/* @file keycache.cpp
 * @brief Internal keycache
 *
 * Copyright (C) 2018 Intevation GmbH
 *
 * This file is part of GpgOL.
 *
 * GpgOL is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GpgOL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "keycache.h"

#include "common.h"
#include "cpphelp.h"
#include "mail.h"

#include <gpg-error.h>
#include <gpgme++/context.h>
#include <gpgme++/key.h>
#include <gpgme++/data.h>
#include <gpgme++/importresult.h>

#include <windows.h>

#include <set>
#include <unordered_map>
#include <sstream>

GPGRT_LOCK_DEFINE (keycache_lock);
GPGRT_LOCK_DEFINE (fpr_map_lock);
GPGRT_LOCK_DEFINE (update_lock);
GPGRT_LOCK_DEFINE (import_lock);
static KeyCache* singleton = nullptr;

/** At some point we need to set a limit. There
  seems to be no limit on how many recipients a mail
  can have in outlook.

  We would run out of resources or block.

  50 Threads already seems a bit excessive but
  it should really cover most legit use cases.
*/

#define MAX_LOCATOR_THREADS 50
static int s_thread_cnt;

namespace
{
  class LocateArgs
    {
      public:
        LocateArgs (const std::string& mbox, Mail *mail = nullptr):
          m_mbox (mbox),
          m_mail (mail)
        {
          TSTART;
          s_thread_cnt++;
          Mail::lockDelete ();
          if (Mail::isValidPtr (m_mail))
            {
              m_mail->incrementLocateCount ();
            }
          Mail::unlockDelete ();
          TRETURN;
        };

        ~LocateArgs()
        {
          TSTART;
          s_thread_cnt--;
          Mail::lockDelete ();
          if (Mail::isValidPtr (m_mail))
            {
              m_mail->decrementLocateCount ();
            }
          Mail::unlockDelete ();
          TRETURN;
        }

        std::string m_mbox;
        Mail *m_mail;
    };
} // namespace

typedef std::pair<std::string, GpgME::Protocol> update_arg_t;

typedef std::pair<std::unique_ptr<LocateArgs>, std::string> import_arg_t;

static DWORD WINAPI
do_update (LPVOID arg)
{
  TSTART;
  auto args = std::unique_ptr<update_arg_t> ((update_arg_t*) arg);

  log_debug ("%s:%s updating: \"%s\" with protocol %s",
             SRCNAME, __func__, anonstr (args->first.c_str ()),
             to_cstr (args->second));

  auto ctx = std::unique_ptr<GpgME::Context> (GpgME::Context::createForProtocol
                                              (args->second));

  if (!ctx)
    {
      TRACEPOINT;
      KeyCache::instance ()->onUpdateJobDone (args->first.c_str(),
                                              GpgME::Key ());
      TRETURN 0;
    }

  ctx->setKeyListMode (GpgME::KeyListMode::Local |
                       GpgME::KeyListMode::Signatures |
                       GpgME::KeyListMode::Validate |
                       GpgME::KeyListMode::WithTofu);
  GpgME::Error err;
  const auto newKey = ctx->key (args->first.c_str (), err, false);
  TRACEPOINT;

  if (newKey.isNull())
    {
      log_debug ("%s:%s Failed to find key for %s",
                 SRCNAME, __func__, anonstr (args->first.c_str ()));
    }
  if (err)
    {
      log_debug ("%s:%s Failed to find key for %s err: %s",
                 SRCNAME, __func__, anonstr (args->first.c_str()),
                 err.asString ());
    }
  KeyCache::instance ()->onUpdateJobDone (args->first.c_str(),
                                          newKey);
  log_debug ("%s:%s Update job done",
             SRCNAME, __func__);
  TRETURN 0;
}

static DWORD WINAPI
do_import (LPVOID arg)
{
  TSTART;
  auto args = std::unique_ptr<import_arg_t> ((import_arg_t*) arg);

  const std::string mbox = args->first->m_mbox;

  log_debug ("%s:%s importing for: \"%s\" with data \n%s",
             SRCNAME, __func__, anonstr (mbox.c_str ()),
             anonstr (args->second.c_str ()));
  auto ctx = std::unique_ptr<GpgME::Context> (GpgME::Context::createForProtocol
                                              (GpgME::OpenPGP));

  if (!ctx)
    {
      TRACEPOINT;
      TRETURN 0;
    }
  // We want to avoid unneccessary copies. The c_str will be valid
  // until args goes out of scope.
  const char *keyStr = args->second.c_str ();
  GpgME::Data data (keyStr, strlen (keyStr), /* copy */ false);

  if (data.type () != GpgME::Data::PGPKey)
    {
      log_debug ("%s:%s Data for: %s is not a PGP Key",
                 SRCNAME, __func__, anonstr (mbox.c_str ()));
      TRETURN 0;
    }
  data.rewind ();

  const auto result = ctx->importKeys (data);

  std::vector<std::string> fingerprints;
  for (const auto import: result.imports())
    {
      if (import.error())
        {
          log_debug ("%s:%s Error importing: %s",
                     SRCNAME, __func__, import.error().asString());
          continue;
        }
      const char *fpr = import.fingerprint ();
      if (!fpr)
        {
          TRACEPOINT;
          continue;
        }

      update_arg_t * update_args = new update_arg_t;
      update_args->first = std::string (fpr);
      update_args->second = GpgME::OpenPGP;

      // We do it blocking to be sure that when all imports
      // are done they are also part of the keycache.
      do_update ((LPVOID) update_args);

      fingerprints.push_back (fpr);
      log_debug ("%s:%s Imported: %s from addressbook.",
                 SRCNAME, __func__, anonstr (fpr));
    }

  KeyCache::instance ()->onAddrBookImportJobDone (mbox, fingerprints);

  log_debug ("%s:%s Import job done for: %s",
             SRCNAME, __func__, anonstr (mbox.c_str ()));
  TRETURN 0;
}


class KeyCache::Private
{
public:
  Private()
  {

  }

  void setPgpKey(const std::string &mbox, const GpgME::Key &key)
  {
    TSTART;
    gpgol_lock (&keycache_lock);
    auto it = m_pgp_key_map.find (mbox);

    if (it == m_pgp_key_map.end ())
      {
        m_pgp_key_map.insert (std::pair<std::string, GpgME::Key> (mbox, key));
      }
    else
      {
        it->second = key;
      }
    insertOrUpdateInFprMap (key);
    gpgol_unlock (&keycache_lock);
    TRETURN;
  }

  void setSmimeKey(const std::string &mbox, const GpgME::Key &key)
  {
    TSTART;
    gpgol_lock (&keycache_lock);
    auto it = m_smime_key_map.find (mbox);

    if (it == m_smime_key_map.end ())
      {
        m_smime_key_map.insert (std::pair<std::string, GpgME::Key> (mbox, key));
      }
    else
      {
        it->second = key;
      }
    insertOrUpdateInFprMap (key);
    gpgol_unlock (&keycache_lock);
    TRETURN;
  }

  void setPgpKeySecret(const std::string &mbox, const GpgME::Key &key)
  {
    TSTART;
    gpgol_lock (&keycache_lock);
    auto it = m_pgp_skey_map.find (mbox);

    if (it == m_pgp_skey_map.end ())
      {
        m_pgp_skey_map.insert (std::pair<std::string, GpgME::Key> (mbox, key));
      }
    else
      {
        it->second = key;
      }
    insertOrUpdateInFprMap (key);
    gpgol_unlock (&keycache_lock);
    TRETURN;
  }

  void setSmimeKeySecret(const std::string &mbox, const GpgME::Key &key)
  {
    TSTART;
    gpgol_lock (&keycache_lock);
    auto it = m_smime_skey_map.find (mbox);

    if (it == m_smime_skey_map.end ())
      {
        m_smime_skey_map.insert (std::pair<std::string, GpgME::Key> (mbox, key));
      }
    else
      {
        it->second = key;
      }
    insertOrUpdateInFprMap (key);
    gpgol_unlock (&keycache_lock);
    TRETURN;
  }

  std::vector<GpgME::Key> getPGPOverrides (const char *addr)
  {
    TSTART;
    std::vector<GpgME::Key> ret;

    if (!addr)
      {
        TRETURN ret;
      }
    auto mbox = GpgME::UserID::addrSpecFromString (addr);

    gpgol_lock (&keycache_lock);
    const auto it = m_addr_book_overrides.find (mbox);
    if (it == m_addr_book_overrides.end ())
      {
        gpgol_unlock (&keycache_lock);
        TRETURN ret;
      }
    for (const auto fpr: it->second)
      {
        const auto key = getByFpr (fpr.c_str (), false);
        if (key.isNull())
          {
            log_debug ("%s:%s: No key for %s in the cache?!",
                       SRCNAME, __func__, anonstr (fpr.c_str()));
            continue;
          }
        ret.push_back (key);
      }

    gpgol_unlock (&keycache_lock);
    TRETURN ret;
  }

  GpgME::Key getKey (const char *addr, GpgME::Protocol proto)
  {
    TSTART;
    if (!addr)
      {
        TRETURN GpgME::Key();
      }
    auto mbox = GpgME::UserID::addrSpecFromString (addr);

    if (proto == GpgME::OpenPGP)
      {
        gpgol_lock (&keycache_lock);
        const auto it = m_pgp_key_map.find (mbox);

        if (it == m_pgp_key_map.end ())
          {
            gpgol_unlock (&keycache_lock);
            TRETURN GpgME::Key();
          }
        const auto ret = it->second;
        gpgol_unlock (&keycache_lock);

        TRETURN ret;
      }
    gpgol_lock (&keycache_lock);
    const auto it = m_smime_key_map.find (mbox);

    if (it == m_smime_key_map.end ())
      {
        gpgol_unlock (&keycache_lock);
        TRETURN GpgME::Key();
      }
    const auto ret = it->second;
    gpgol_unlock (&keycache_lock);

    TRETURN ret;
  }

  GpgME::Key getSKey (const char *addr, GpgME::Protocol proto)
  {
    TSTART;
    if (!addr)
      {
        TRETURN GpgME::Key();
      }
    auto mbox = GpgME::UserID::addrSpecFromString (addr);

    if (proto == GpgME::OpenPGP)
      {
        gpgol_lock (&keycache_lock);
        const auto it = m_pgp_skey_map.find (mbox);

        if (it == m_pgp_skey_map.end ())
          {
            gpgol_unlock (&keycache_lock);
            TRETURN GpgME::Key();
          }
        const auto ret = it->second;
        gpgol_unlock (&keycache_lock);

        TRETURN ret;
      }
    gpgol_lock (&keycache_lock);
    const auto it = m_smime_skey_map.find (mbox);

    if (it == m_smime_skey_map.end ())
      {
        gpgol_unlock (&keycache_lock);
        TRETURN GpgME::Key();
      }
    const auto ret = it->second;
    gpgol_unlock (&keycache_lock);

    TRETURN ret;
  }

  GpgME::Key getSigningKey (const char *addr, GpgME::Protocol proto)
  {
    TSTART;
    const auto key = getSKey (addr, proto);
    if (key.isNull())
      {
        log_debug ("%s:%s: secret key for %s is null",
                   SRCNAME, __func__, anonstr (addr));
        TRETURN key;
      }
    if (!key.canReallySign())
      {
        log_debug ("%s:%s: Discarding key for %s because it can't sign",
                   SRCNAME, __func__, anonstr (addr));
        TRETURN GpgME::Key();
      }
    if (!key.hasSecret())
      {
        log_debug ("%s:%s: Discarding key for %s because it has no secret",
                   SRCNAME, __func__, anonstr (addr));
        TRETURN GpgME::Key();
      }
    if (in_de_vs_mode () && !key.isDeVs())
      {
        log_debug ("%s:%s: signing key for %s is not deVS",
                   SRCNAME, __func__, anonstr (addr));
        TRETURN GpgME::Key();
      }
    TRETURN key;
  }

  std::vector<GpgME::Key> getEncryptionKeys (const std::vector<std::string>
                                             &recipients,
                                             GpgME::Protocol proto)
  {
    TSTART;
    std::vector<GpgME::Key> ret;
    if (recipients.empty ())
      {
        TRACEPOINT;
        TRETURN ret;
      }
    for (const auto &recip: recipients)
      {
        if (proto == GpgME::OpenPGP)
          {
            const auto overrides = getPGPOverrides (recip.c_str ());

            if (!overrides.empty())
              {
                ret.insert (ret.end (), overrides.begin (), overrides.end ());
                log_debug ("%s:%s: Using overides for %s",
                           SRCNAME, __func__, anonstr (recip.c_str ()));
                continue;
              }
          }
        const auto key = getKey (recip.c_str (), proto);
        if (key.isNull())
          {
            log_debug ("%s:%s: No key for %s. no internal encryption",
                       SRCNAME, __func__, anonstr (recip.c_str ()));
            TRETURN std::vector<GpgME::Key>();
          }

        if (!key.canEncrypt() || key.isRevoked() ||
            key.isExpired() || key.isDisabled() || key.isInvalid())
          {
            log_data ("%s:%s: Invalid key for %s. no internal encryption",
                       SRCNAME, __func__, anonstr (recip.c_str ()));
            TRETURN std::vector<GpgME::Key>();
          }

        if (in_de_vs_mode () && !key.isDeVs ())
          {
            log_data ("%s:%s: key for %s is not deVS",
                      SRCNAME, __func__, anonstr (recip.c_str ()));
            TRETURN std::vector<GpgME::Key>();
          }

        bool validEnough = false;
        /* Here we do the check if the key is valid for this recipient */
        const auto addrSpec = GpgME::UserID::addrSpecFromString (recip.c_str ());
        for (const auto &uid: key.userIDs ())
          {
            if (addrSpec != uid.addrSpec())
              {
                // Ignore unmatching addr specs
                continue;
              }
            if (uid.validity() >= GpgME::UserID::Marginal ||
                uid.origin() == GpgME::Key::OriginWKD)
              {
                validEnough = true;
                break;
              }
          }
        if (!validEnough)
          {
            log_debug ("%s:%s: UID for %s does not have at least marginal trust",
                       SRCNAME, __func__, anonstr (recip.c_str ()));
            TRETURN std::vector<GpgME::Key>();
          }
        // Accepting key
        ret.push_back (key);
      }
    TRETURN ret;
  }

  void insertOrUpdateInFprMap (const GpgME::Key &key)
    {
      TSTART;
      if (key.isNull() || !key.primaryFingerprint())
        {
          TRACEPOINT;
          TRETURN;
        }
      gpgol_lock (&fpr_map_lock);

      /* First ensure that we have the subkeys mapped to the primary
         fpr */
      const char *primaryFpr = key.primaryFingerprint ();

      for (const auto &sub: key.subkeys())
        {
          const char *subFpr = sub.fingerprint();
          auto it = m_sub_fpr_map.find (subFpr);
          if (it == m_sub_fpr_map.end ())
            {
              m_sub_fpr_map.insert (std::make_pair(
                                     std::string (subFpr),
                                     std::string (primaryFpr)));
            }
        }

      auto it = m_fpr_map.find (primaryFpr);

      log_debug ("%s:%s \"%s\" updated.",
                 SRCNAME, __func__, anonstr (primaryFpr));
      if (it == m_fpr_map.end ())
        {
          m_fpr_map.insert (std::make_pair (primaryFpr, key));

          gpgol_unlock (&fpr_map_lock);
          TRETURN;
        }

      if (it->second.hasSecret () && !key.hasSecret())
        {
          log_debug ("%s:%s Lost secret info on update. Merging.",
                     SRCNAME, __func__);
          auto merged = key;
          merged.mergeWith (it->second);
          it->second = merged;
        }
      else
        {
          it->second = key;
        }
      gpgol_unlock (&fpr_map_lock);
      TRETURN;
    }

  GpgME::Key getFromMap (const char *fpr) const
  {
    TSTART;
    if (!fpr)
      {
        TRACEPOINT;
        TRETURN GpgME::Key();
      }

    gpgol_lock (&fpr_map_lock);
    std::string primaryFpr;
    const auto it = m_sub_fpr_map.find (fpr);
    if (it != m_sub_fpr_map.end ())
      {
        log_debug ("%s:%s using \"%s\" for \"%s\"",
                   SRCNAME, __func__, anonstr (it->second.c_str()),
                   anonstr (fpr));
        primaryFpr = it->second;
      }
    else
      {
        primaryFpr = fpr;
      }

    const auto keyIt = m_fpr_map.find (primaryFpr);
    if (keyIt != m_fpr_map.end ())
      {
        const auto ret = keyIt->second;
        gpgol_unlock (&fpr_map_lock);
        TRETURN ret;
      }
    gpgol_unlock (&fpr_map_lock);
    TRETURN GpgME::Key();
  }

  GpgME::Key getByFpr (const char *fpr, bool block) const
    {
      TSTART;
      if (!fpr)
        {
          TRACEPOINT;
          TRETURN GpgME::Key ();
        }

      TRACEPOINT;
      const auto ret = getFromMap (fpr);
      if (ret.isNull())
        {
          // If the key was not found we need to check if there is
          // an update running.
          if (block)
            {
              const std::string sFpr (fpr);
              int i = 0;

              gpgol_lock (&update_lock);
              while (m_update_jobs.find(sFpr) != m_update_jobs.end ())
                {
                  i++;
                  if (i % 100 == 0)
                    {
                      log_debug ("%s:%s Waiting on update for \"%s\"",
                                 SRCNAME, __func__, anonstr (fpr));
                    }
                  gpgol_unlock (&update_lock);
                  Sleep (10);
                  gpgol_lock (&update_lock);
                  if (i == 3000)
                    {
                      /* Just to be on the save side */
                      log_error ("%s:%s Waiting on update for \"%s\" "
                                 "failed! Bug!",
                                 SRCNAME, __func__, anonstr (fpr));
                      break;
                    }
                }
              gpgol_unlock (&update_lock);

              TRACEPOINT;
              const auto ret2 = getFromMap (fpr);
              if (ret2.isNull ())
                {
                  log_debug ("%s:%s Cache miss after blocking check %s.",
                             SRCNAME, __func__, anonstr (fpr));
                }
              else
                {
                  log_debug ("%s:%s Cache hit after wait for %s.",
                             SRCNAME, __func__, anonstr (fpr));
                  TRETURN ret2;
                }
            }
          log_debug ("%s:%s Cache miss for %s.",
                     SRCNAME, __func__, anonstr (fpr));
          TRETURN GpgME::Key();
        }

      log_debug ("%s:%s Cache hit for %s.",
                 SRCNAME, __func__, anonstr (fpr));
      TRETURN ret;
    }

  void update (const char *fpr, GpgME::Protocol proto)
     {
       TSTART;
       if (!fpr)
         {
           TRETURN;
         }
       const std::string sFpr (fpr);
       gpgol_lock (&update_lock);
       if (m_update_jobs.find(sFpr) != m_update_jobs.end ())
         {
           log_debug ("%s:%s Update for \"%s\" already in progress.",
                      SRCNAME, __func__, anonstr (fpr));
           gpgol_unlock (&update_lock);
         }

       m_update_jobs.insert (sFpr);
       gpgol_unlock (&update_lock);
       update_arg_t * args = new update_arg_t;
       args->first = sFpr;
       args->second = proto;
       CloseHandle (CreateThread (NULL, 0, do_update,
                                  (LPVOID) args, 0,
                                  NULL));
       TRETURN;
     }

  void onUpdateJobDone (const char *fpr, const GpgME::Key &key)
    {
      TSTART;
      if (!fpr)
        {
          TRETURN;
        }
      TRACEPOINT;
      insertOrUpdateInFprMap (key);
      gpgol_lock (&update_lock);
      const auto it = m_update_jobs.find(fpr);

      if (it == m_update_jobs.end())
        {
          log_debug ("%s:%s Update for \"%s\" already finished.",
                     SRCNAME, __func__, anonstr (fpr));
          gpgol_unlock (&update_lock);
          TRETURN;
        }
      m_update_jobs.erase (it);
      gpgol_unlock (&update_lock);
      TRACEPOINT;
      TRETURN;
    }

  void importFromAddrBook (const std::string &mbox, const char *data,
                           Mail *mail)
    {
      TSTART;
      if (!data || mbox.empty() || !mail)
        {
          TRACEPOINT;
          TRETURN;
        }
       gpgol_lock (&import_lock);
       if (m_import_jobs.find (mbox) != m_import_jobs.end ())
         {
           log_debug ("%s:%s import for \"%s\" already in progress.",
                      SRCNAME, __func__, anonstr (mbox.c_str ()));
           gpgol_unlock (&import_lock);
         }
       m_import_jobs.insert (mbox);
       gpgol_unlock (&import_lock);

       import_arg_t * args = new import_arg_t;
       args->first = std::unique_ptr<LocateArgs> (new LocateArgs (mbox, mail));
       args->second = std::string (data);
       CloseHandle (CreateThread (NULL, 0, do_import,
                                  (LPVOID) args, 0,
                                  NULL));

      TRETURN;
    }

  void onAddrBookImportJobDone (const std::string &mbox,
                                const std::vector<std::string> &result_fprs)
    {
      TSTART;
      gpgol_lock (&keycache_lock);
      auto it = m_addr_book_overrides.find (mbox);
      if (it != m_addr_book_overrides.end ())
        {
          it->second = result_fprs;
        }
      else
        {
          m_addr_book_overrides.insert (
                std::make_pair (mbox, result_fprs));
        }
      gpgol_unlock (&keycache_lock);
      gpgol_lock (&import_lock);
      const auto job_it = m_import_jobs.find(mbox);

      if (job_it == m_import_jobs.end())
        {
          log_error ("%s:%s import for \"%s\" already finished.",
                     SRCNAME, __func__, anonstr (mbox.c_str ()));
          gpgol_unlock (&import_lock);
          TRETURN;
        }
      m_import_jobs.erase (job_it);
      gpgol_unlock (&import_lock);
      TRETURN;
    }

  std::unordered_map<std::string, GpgME::Key> m_pgp_key_map;
  std::unordered_map<std::string, GpgME::Key> m_smime_key_map;
  std::unordered_map<std::string, GpgME::Key> m_pgp_skey_map;
  std::unordered_map<std::string, GpgME::Key> m_smime_skey_map;
  std::unordered_map<std::string, GpgME::Key> m_fpr_map;
  std::unordered_map<std::string, std::string> m_sub_fpr_map;
  std::unordered_map<std::string, std::vector<std::string> >
    m_addr_book_overrides;
  std::set<std::string> m_update_jobs;
  std::set<std::string> m_import_jobs;
};

KeyCache::KeyCache():
  d(new Private)
{
}

KeyCache *
KeyCache::instance ()
{
  if (!singleton)
    {
      singleton = new KeyCache();
    }
  return singleton;
}

GpgME::Key
KeyCache::getSigningKey (const char *addr, GpgME::Protocol proto) const
{
  return d->getSigningKey (addr, proto);
}

std::vector<GpgME::Key>
KeyCache::getEncryptionKeys (const std::vector<std::string> &recipients, GpgME::Protocol proto) const
{
  return d->getEncryptionKeys (recipients, proto);
}

static GpgME::Key
get_most_valid_key_simple (const std::vector<GpgME::Key> &keys)
{
  GpgME::Key candidate;
  for (const auto &key: keys)
    {
      if (key.isRevoked() || key.isExpired() ||
          key.isDisabled() || key.isInvalid())
        {
          log_debug ("%s:%s: Skipping invalid S/MIME key",
                     SRCNAME, __func__);
          continue;
        }
      if (candidate.isNull() || !candidate.numUserIDs())
        {
          if (key.numUserIDs() &&
              candidate.userID(0).validity() <= key.userID(0).validity())
            {
              candidate = key;
            }
        }
    }
  return candidate;
}

static std::vector<GpgME::Key>
get_local_smime_keys (const std::string &addr)
{
  TSTART;
  std::vector<GpgME::Key> keys;
  auto ctx = std::unique_ptr<GpgME::Context> (
                                              GpgME::Context::createForProtocol (GpgME::CMS));
  if (!ctx)
    {
      TRACEPOINT;
      TRETURN keys;
    }
  // We need to validate here to fetch CRL's
  ctx->setKeyListMode (GpgME::KeyListMode::Local |
                       GpgME::KeyListMode::Validate |
                       GpgME::KeyListMode::Signatures);
  GpgME::Error e = ctx->startKeyListing (addr.c_str());
  if (e)
    {
      TRACEPOINT;
      TRETURN keys;
    }

  GpgME::Error err;
  do {
      keys.push_back(ctx->nextKey(err));
  } while (!err);
  keys.pop_back();

  TRETURN keys;
}

static std::vector<GpgME::Key>
get_extern_smime_keys (const std::string &addr, bool import)
{
  TSTART;
  std::vector<GpgME::Key> keys;
  auto ctx = std::unique_ptr<GpgME::Context> (
                                              GpgME::Context::createForProtocol (GpgME::CMS));
  if (!ctx)
    {
      TRACEPOINT;
      TRETURN keys;
    }
  // We need to validate here to fetch CRL's
  ctx->setKeyListMode (GpgME::KeyListMode::Extern);
  GpgME::Error e = ctx->startKeyListing (addr.c_str());
  if (e)
    {
      TRACEPOINT;
      TRETURN keys;
    }

  GpgME::Error err;
  do
    {
      const auto key = ctx->nextKey (err);
      if (!err && !key.isNull())
        {
          keys.push_back (key);
          log_debug ("%s:%s: Found extern S/MIME key for %s with fpr: %s",
                     SRCNAME, __func__, anonstr (addr.c_str()),
                     anonstr (key.primaryFingerprint()));
        }
    } while (!err);

  if (import && keys.size ())
    {
      const GpgME::ImportResult res = ctx->importKeys(keys);
      log_debug ("%s:%s: Import result for %s: err: %s",
                 SRCNAME, __func__, anonstr (addr.c_str()),
                 res.error ().asString ());

    }

  TRETURN keys;
}

static DWORD WINAPI
do_locate (LPVOID arg)
{
  TSTART;
  if (!arg)
    {
      TRETURN 0;
    }

  auto args = std::unique_ptr<LocateArgs> ((LocateArgs *) arg);

  const auto addr = args->m_mbox;

  log_debug ("%s:%s searching key for addr: \"%s\"",
             SRCNAME, __func__, anonstr (addr.c_str()));

  const auto k = GpgME::Key::locate (addr.c_str());

  if (!k.isNull ())
    {
      log_debug ("%s:%s found key for addr: \"%s\":%s",
                 SRCNAME, __func__, anonstr (addr.c_str()),
                 anonstr (k.primaryFingerprint()));
      KeyCache::instance ()->setPgpKey (addr, k);
    }
  log_debug ("%s:%s pgp locate done",
             SRCNAME, __func__);

  if (opt.enable_smime)
    {
      GpgME::Key candidate = get_most_valid_key_simple (
                                    get_local_smime_keys (addr));
      if (!candidate.isNull())
        {
          log_debug ("%s:%s found SMIME key for addr: \"%s\":%s",
                     SRCNAME, __func__, anonstr (addr.c_str()),
                     anonstr (candidate.primaryFingerprint()));
          KeyCache::instance()->setSmimeKey (addr, candidate);
          TRETURN 0;
        }
      if (!opt.search_smime_servers || (!k.isNull() && !opt.prefer_smime))
        {
          log_debug ("%s:%s Found no S/MIME key locally and external "
                     "search is disabled.", SRCNAME, __func__);
          TRETURN 0;
        }
      /* Search for extern keys and import them */
      const auto externs = get_extern_smime_keys (addr, true);
      if (externs.empty())
        {
          TRETURN 0;
        }
      /* We found and imported external keys. We need to get them
         locally now to ensure that they are valid etc. */
      candidate = get_most_valid_key_simple (
                                    get_local_smime_keys (addr));
      if (!candidate.isNull())
        {
          log_debug ("%s:%s found ext. SMIME key for addr: \"%s\":%s",
                     SRCNAME, __func__, anonstr (addr.c_str()),
                     anonstr (candidate.primaryFingerprint()));
          KeyCache::instance()->setSmimeKey (addr, candidate);
          TRETURN 0;
        }
      else
        {
          log_debug ("%s:%s: Found no valid key in extern S/MIME certs",
                     SRCNAME, __func__);
        }
    }
  TRETURN 0;
}

static void
locate_secret (const char *addr, GpgME::Protocol proto)
{
  TSTART;
  auto ctx = std::unique_ptr<GpgME::Context> (
                      GpgME::Context::createForProtocol (proto));
  if (!ctx)
    {
      TRACEPOINT;
      TRETURN;
    }
  if (!addr)
    {
      TRACEPOINT;
      TRETURN;
    }
  const auto mbox = GpgME::UserID::addrSpecFromString (addr);

  if (mbox.empty())
    {
      log_debug ("%s:%s: Empty mbox for addr %s",
                 SRCNAME, __func__, anonstr (addr));
      TRETURN;
    }

  // We need to validate here to fetch CRL's
  ctx->setKeyListMode (GpgME::KeyListMode::Local |
                       GpgME::KeyListMode::Validate);
  GpgME::Error e = ctx->startKeyListing (mbox.c_str(), true);
  if (e)
    {
      TRACEPOINT;
      TRETURN;
    }

  std::vector<GpgME::Key> keys;
  GpgME::Error err;
  do
    {
      const auto key = ctx->nextKey(err);
      if (key.isNull())
        {
          continue;
        }
      if (key.isRevoked() || key.isExpired() ||
          key.isDisabled() || key.isInvalid())
        {
          if ((opt.enable_debug & DBG_DATA))
            {
              std::stringstream ss;
              ss << key;
              log_data ("%s:%s: Skipping invalid secret key %s",
                        SRCNAME, __func__, ss.str().c_str());
            }
          continue;
        }
      if (proto == GpgME::OpenPGP)
        {
          log_debug ("%s:%s found pgp skey for addr: \"%s\":%s",
                     SRCNAME, __func__, anonstr (mbox.c_str()),
                     anonstr (key.primaryFingerprint()));
          KeyCache::instance()->setPgpKeySecret (mbox, key);
          TRETURN;
        }
      if (proto == GpgME::CMS)
        {
          log_debug ("%s:%s found cms skey for addr: \"%s\":%s",
                     SRCNAME, __func__, anonstr (mbox.c_str ()),
                     anonstr (key.primaryFingerprint()));
          KeyCache::instance()->setSmimeKeySecret (mbox, key);
          TRETURN;
        }
    } while (!err);
  TRETURN;
}

static DWORD WINAPI
do_locate_secret (LPVOID arg)
{
  TSTART;
  auto args = std::unique_ptr<LocateArgs> ((LocateArgs *) arg);

  log_debug ("%s:%s searching secret key for addr: \"%s\"",
             SRCNAME, __func__, anonstr (args->m_mbox.c_str ()));

  locate_secret (args->m_mbox.c_str(), GpgME::OpenPGP);
  if (opt.enable_smime)
    {
      locate_secret (args->m_mbox.c_str(), GpgME::CMS);
    }
  log_debug ("%s:%s locator sthread thread done",
             SRCNAME, __func__);
  TRETURN 0;
}

void
KeyCache::startLocate (const std::vector<std::string> &addrs, Mail *mail) const
{
  for (const auto &addr: addrs)
    {
      startLocate (addr.c_str(), mail);
    }
}

void
KeyCache::startLocate (const char *addr, Mail *mail) const
{
  TSTART;
  if (!addr)
    {
      TRACEPOINT;
      TRETURN;
    }
  std::string recp = GpgME::UserID::addrSpecFromString (addr);
  if (recp.empty ())
    {
      TRETURN;
    }
  gpgol_lock (&keycache_lock);
  if (d->m_pgp_key_map.find (recp) == d->m_pgp_key_map.end ())
    {
      // It's enough to look at the PGP Key map. We marked
      // searched keys there.
      d->m_pgp_key_map.insert (std::pair<std::string, GpgME::Key> (recp, GpgME::Key()));
      log_debug ("%s:%s Creating a locator thread",
                 SRCNAME, __func__);
      const auto args = new LocateArgs(recp, mail);
      HANDLE thread = CreateThread (NULL, 0, do_locate,
                                    args, 0,
                                    NULL);
      CloseHandle (thread);
    }
  gpgol_unlock (&keycache_lock);
  TRETURN;
}

void
KeyCache::startLocateSecret (const char *addr, Mail *mail) const
{
  TSTART;
  if (!addr)
    {
      TRACEPOINT;
      TRETURN;
    }
  std::string recp = GpgME::UserID::addrSpecFromString (addr);
  if (recp.empty ())
    {
      TRETURN;
    }
  gpgol_lock (&keycache_lock);
  if (d->m_pgp_skey_map.find (recp) == d->m_pgp_skey_map.end ())
    {
      // It's enough to look at the PGP Key map. We marked
      // searched keys there.
      d->m_pgp_skey_map.insert (std::pair<std::string, GpgME::Key> (recp, GpgME::Key()));
      log_debug ("%s:%s Creating a locator thread",
                 SRCNAME, __func__);
      const auto args = new LocateArgs(recp, mail);

      HANDLE thread = CreateThread (NULL, 0, do_locate_secret,
                                    (LPVOID) args, 0,
                                    NULL);
      CloseHandle (thread);
    }
  gpgol_unlock (&keycache_lock);
  TRETURN;
}


void
KeyCache::setSmimeKey(const std::string &mbox, const GpgME::Key &key)
{
  d->setSmimeKey(mbox, key);
}

void
KeyCache::setPgpKey(const std::string &mbox, const GpgME::Key &key)
{
  d->setPgpKey(mbox, key);
}

void
KeyCache::setSmimeKeySecret(const std::string &mbox, const GpgME::Key &key)
{
  d->setSmimeKeySecret(mbox, key);
}

void
KeyCache::setPgpKeySecret(const std::string &mbox, const GpgME::Key &key)
{
  d->setPgpKeySecret(mbox, key);
}

bool
KeyCache::isMailResolvable(Mail *mail)
{
  TSTART;
  /* Get the data from the mail. */
  const auto sender = mail->getSender ();
  auto recps = mail->getCachedRecipients ();

  if (sender.empty() || recps.empty())
    {
      log_debug ("%s:%s: Mail has no sender or no recipients.",
                 SRCNAME, __func__);
      TRETURN false;
    }


  std::vector<GpgME::Key> encKeys = getEncryptionKeys (recps,
                                                       GpgME::OpenPGP);

  if (!encKeys.empty())
    {
      TRETURN true;
    }

  if (!opt.enable_smime)
    {
      TRETURN false;
    }

  /* Check S/MIME instead here we need to include the sender
     as we can't just generate a key. */
  recps.push_back (sender);
  GpgME::Key sigKey= getSigningKey (sender.c_str(), GpgME::CMS);
  encKeys = getEncryptionKeys (recps, GpgME::CMS);

  TRETURN !encKeys.empty() && !sigKey.isNull();
}

void
KeyCache::update (const char *fpr, GpgME::Protocol proto)
{
  d->update (fpr, proto);
}

GpgME::Key
KeyCache::getByFpr (const char *fpr, bool block) const
{
  return d->getByFpr (fpr, block);
}

void
KeyCache::onUpdateJobDone (const char *fpr, const GpgME::Key &key)
{
  return d->onUpdateJobDone (fpr, key);
}

void
KeyCache::importFromAddrBook (const std::string &mbox, const char *key_data,
                              Mail *mail) const
{
  return d->importFromAddrBook (mbox, key_data, mail);
}

void
KeyCache::onAddrBookImportJobDone (const std::string &mbox,
                                   const std::vector<std::string> &result_fprs)
{
  return d->onAddrBookImportJobDone (mbox, result_fprs);
}
