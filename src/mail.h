/* @file mail.h
 * @brief High level class to work with Outlook Mailitems.
 *
 *    Copyright (C) 2015 Intevation GmbH
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
#ifndef MAIL_H
#define MAIL_H

#include "oomhelp.h"

/** @brief Data wrapper around a mailitem.
 *
 * This class is intended to bundle all that we know about
 * a Mail. Due to the restrictions in Outlook we sometimes may
 * need additional information that is not available at the time
 * like the sender address of an exchange account in the afterWrite
 * event.
 *
 * This class bundles such information and also provides a way to
 * access the event handler of a mail.
 */
class Mail
{
public:
  /** @brief Construct a mail object for the item.
    *
    * This also installs the event sink for this item.
    *
    * The mail object takes ownership of the mailitem
    * reference. Do not Release it! */
  Mail (LPDISPATCH mailitem);

  ~Mail ();

  /** @brief looks for existing Mail objects for the OOM mailitem.

    @returns A reference to an existing mailitem or NULL in case none
    could be found.
  */
  static Mail* get_mail_for_item (LPDISPATCH mailitem);

  /** @brief Reference to the mailitem. Do not Release! */
  LPDISPATCH item () { return m_mailitem; }

  /** @brief Process the message. Ususally to be called from BeforeRead.
   *
   * This function assumes that the base message interface can be accessed
   * and calles the MAPI Message handling which creates the GpgOL style
   * attachments and sets up the message class etc.
   *
   * Sets the was_encrypted / processed variables.
   *
   * @returns 0 on success.
   */
  int process_message ();

  /** @brief Replace the body with the plaintext and session decrypts
   * attachments.
   *
   * Sets the needs_wipe variable.
   *
   * @returns 0 on success. */
  int insert_plaintext ();

  /** @brief do crypto operations as selected by the user.
   *
   * Initiates the crypto operations according to the gpgol
   * draft info flags.
   *
   * @returns 0 on success. */
  int do_crypto ();

  /** @brief Necessary crypto operations were completed successfully. */
  bool crypto_successful () { return !needs_crypto() || m_crypt_successful; }

  /** @brief Message should be encrypted and or signed. */
  bool needs_crypto ();

  /** @brief wipe the plaintext from the message and ecnrypt attachments.
   *
   * @returns 0 on success; */
  int wipe ();

private:
  LPDISPATCH m_mailitem;
  LPDISPATCH m_event_sink;
  char * m_sender_addr;
  bool m_processed,    /* The message has been porcessed by us.  */
       m_needs_wipe,   /* We have added plaintext to the mesage. */
       m_crypt_successful; /* We successfuly performed crypto on the item. */
};
#endif // MAIL_H
