/* gpgmsg.cpp - Implementation of the GpgMsg class
 *	Copyright (C) 2005, 2006 g10 Code GmbH
 *
 * This file is part of GpgOL.
 * 
 * GpgOL is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * GpgOL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#error not anymore used
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <windows.h>
#include <assert.h>
#include <string.h>

#include "mymapi.h"
#include "mymapitags.h"
#include "myexchext.h"
#include "common.h"
#include "util.h"
#include "msgcache.h"
#include "pgpmime.h"
#include "engine.h"
#include "ol-ext-callback.h"
#include "display.h"
#include "rfc822parse.h"


static const char oid_mimetag[] =
  {0x2A, 0x86, 0x48, 0x86, 0xf7, 0x14, 0x03, 0x0a, 0x04};


#define TRACEPOINT() do { log_debug ("%s:%s:%d: tracepoint\n", \
                                       SRCNAME, __func__, __LINE__); \
                        } while (0)

/* Constants to describe the PGP armor types. */
typedef enum 
  {
    ARMOR_NONE = 0,
    ARMOR_MESSAGE,
    ARMOR_SIGNATURE,
    ARMOR_SIGNED,
    ARMOR_FILE,     
    ARMOR_PUBKEY,
    ARMOR_SECKEY
  }
armor_t;


struct attach_info
{
  int end_of_table;  /* True if this is the last plus one entry of the
                        table. */
  int invalid;       /* Invalid table entry - usally ignored. */
   
  int is_encrypted;  /* This is an encrypted attchment. */
  int is_signed;     /* This is a signed attachment. */
  unsigned int sig_pos; /* For signed attachments the index of the
                           attachment with the detached signature. */
  
  int method;        /* MAPI attachmend method. */
  char *filename;    /* Malloced filename of this attachment or NULL. */
  char *content_type;/* Malloced string with the mime attrib or NULL.
                        Parameters are stripped off thus a compare
                        against "type/subtype" is sufficient. */
  const char *content_type_parms; /* If not NULL the parameters of the
                                     content_type. */
  armor_t armor_type;   /* 0 or the type of the PGP armor. */
};
typedef struct attach_info *attach_info_t;


static int get_attach_method (LPATTACH obj);
static char *get_short_attach_data (LPATTACH obj);
static char *get_long_attach_data (LPMESSAGE msg, attach_info_t table,int pos);
static bool set_x_header (LPMESSAGE msg, const char *name, const char *val);



/*
   The implementation class of GpgMsg.  
 */
class GpgMsgImpl : public GpgMsg
{
public:    
  GpgMsgImpl () 
  {
    message = NULL;
    exchange_cb = NULL;
    is_pgpmime_enc = false;
    has_attestation = false;
    preview = false;

    got_message_ct = false;
    media_type = NULL;
    media_subtype = NULL;
    ct_protocol = NULL;
    transport_message_headers = NULL;

    attestation = NULL;

    attach.att_table = NULL;
    attach.rows = NULL;
  }

  ~GpgMsgImpl ()
  {
    xfree (media_type);
    xfree (media_subtype);
    xfree (ct_protocol);
    xfree (transport_message_headers);

    if (message)
      message->Release ();

    if (attestation)
      gpgme_data_release (attestation);

    free_attach_info ();
  }

  void destroy ()
  {
    delete this;
  }

  void operator delete (void *p) 
  {
    ::operator delete (p);
  }

  void setMapiMessage (LPMESSAGE msg)
  {
    if (message)
      {
        message->Release ();
        message = NULL;
        got_message_ct = false;
      }
    if (msg)
      {
        msg->AddRef ();
        message = msg;
      }
  }

  /* Set the callback for Exchange. */
  void setExchangeCallback (void *cb)
  {
    exchange_cb = cb;
  }
  
  void setPreview (bool value)
  {
    preview = value;
  }

  openpgp_t getMessageType (const char *s);
  bool hasAttachments (void);
  const char *getPlainText (void);

  int decrypt (HWND hwnd, bool info_only);
  int sign (HWND hwnd, bool want_html);
  int encrypt (HWND hwnd, bool want_html)
  {
    return encrypt_and_sign (hwnd, want_html, false);
  }
  int signEncrypt (HWND hwnd, bool want_html)
  {
    return encrypt_and_sign (hwnd, want_html, true);
  }
  int attachPublicKey (const char *keyid);

  char **getRecipients (void);
  unsigned int getAttachments (void);
  void verifyAttachment (HWND hwnd, attach_info_t table,
                         unsigned int pos_data,
                         unsigned int pos_sig);
  void decryptAttachment (HWND hwnd, int pos, bool save_plaintext, int ttl,
                          const char *filename);
  void signAttachment (HWND hwnd, int pos, gpgme_key_t sign_key, int ttl);
  int encryptAttachment (HWND hwnd, int pos, gpgme_key_t *keys,
                         gpgme_key_t sign_key, int ttl);


private:
  LPMESSAGE message;  /* Pointer to the message. */
  void *exchange_cb;  /* Call back used with the display function. */
  bool is_pgpmime_enc;/* True if the message is a PGP/MIME encrypted one. */
  bool has_attestation;/* True if we found an attestation attachment. */
  bool preview;       /* Don't pop up message boxes and run only a
                         body decryption. */

  /* If not NULL, collect attestation information here. */
  gpgme_data_t attestation;

  /* Store information from the messages headers. */ 
  bool got_message_ct;  /* Flag indicating whether we retrieved the info. */
  char *media_type;     /* Media type from the content-type or NULL. */
  char *media_subtype;  /* Media sybtype from the content-type or NULL. */
  char *ct_protocol;    /* Protocol from the content_type or NULL. */
  char *transport_message_headers;  /* Al the message headers as one string. */

  /* This structure collects the information about attachments. */
  struct 
  {
    LPMAPITABLE att_table;/* The loaded attachment table or NULL. */
    LPSRowSet   rows;     /* The retrieved set of rows from the table. */
  } attach;
  
  void free_attach_info (void);
  char *loadBody (bool want_html);
  void get_msg_content_type (void);
  bool isPgpmimeVersionPart (int pos);
  void writeAttestation (void);
  gpg_error_t createHtmlAttachment (const char *text);
  attach_info_t gatherAttachmentInfo (void);
  int encrypt_and_sign (HWND hwnd, bool want_html, bool sign);
};


/* Return a new instance and initialize with the MAPI message object
   MSG. */
GpgMsg *
CreateGpgMsg (LPMESSAGE msg)
{
  GpgMsg *m = new GpgMsgImpl ();
  if (!m)
    out_of_core ();
  m->setMapiMessage (msg);
  return m;
}

void
GpgMsgImpl::free_attach_info (void)
{
  if (attach.att_table)
    {
      attach.att_table->Release ();
      attach.att_table = NULL;
    }
  if (attach.rows)
    {
      FreeProws (attach.rows);
      attach.rows = NULL;
    }
}

/* Release an array of GPGME keys. */
static void 
free_key_array (gpgme_key_t *keys)
{
  if (keys)
    {
      for (int i = 0; keys[i]; i++) 
	gpgme_key_release (keys[i]);
      xfree (keys);
    }
}

/* Release an array of strings. */
static void
free_string_array (char **strings)
{
  if (strings)
    {
      for (int i=0; strings[i]; i++) 
	xfree (strings[i]);	
      xfree (strings);
    }
}

/* Release a table with attachments infos. */
static void
release_attach_info (attach_info_t table)
{
  int i;

  if (!table)
    return;
  for (i=0; !table[i].end_of_table; i++)
    {
      xfree (table[i].filename);
      xfree (table[i].content_type);
    }
  xfree (table);
}


/* Return the number of strings in the array STRINGS. */
static size_t
count_strings (char **strings)
{
  size_t i;
  
  for (i=0; strings[i]; i++)
    ;
  return i;
}

static size_t
count_keys (gpgme_key_t *keys)
{
  size_t i;
  
  for (i=0; keys[i]; i++)
    ;
  return i;
}


/* Return a string suitable for displaying in a message box.  The
   function takes FORMAT and replaces the string "@LIST@" with the
   names of the attachmets. Depending on the set bits in WHAT only
   certain attachments are inserted. 

   Defined bits in MODE are:
      0 = Any attachment
      1 = signed attachments
      2 = encrypted attachments

   Caller must free the returned value.  Routine is guaranteed to
   return a string.
*/
static char *
text_from_attach_info (attach_info_t table, const char *format,
                       unsigned int what)
{
  int pos;
  size_t length;
  char *buffer, *p;
  const char *marker;

  marker = strstr (format, "@LIST@");
  if (!marker)
    return xstrdup (format);

#define CONDITION  (table[pos].filename \
                    && ( (what&1) \
                         || ((what & 2) && table[pos].is_signed) \
                         || ((what & 4) && table[pos].is_encrypted)))

  for (length=0, pos=0; !table[pos].end_of_table; pos++)
    if (CONDITION)
      length += 2 + strlen (table[pos].filename) + 1;

  length += strlen (format);
  buffer = p = (char*)xmalloc (length+1);

  strncpy (p, format, marker - format);
  p += marker - format;

  for (pos=0; !table[pos].end_of_table; pos++)
    if (CONDITION)
      {
        if (table[pos].is_signed)
          p = stpcpy (p, "S ");
        else if (table[pos].is_encrypted)
          p = stpcpy (p, "E ");
        else
          p = stpcpy (p, "* ");
        p = stpcpy (p, table[pos].filename);
        p = stpcpy (p, "\n");
      }
  strcpy (p, marker+6);
#undef CONDITION

  return buffer;
}



/* Load the body from the MAPI and return it as an UTF8 string.
   Returns NULL on error.  */
char *
GpgMsgImpl::loadBody (bool want_html)
{
  HRESULT hr;
  LPSPropValue lpspvFEID = NULL;
  LPSTREAM stream;
  STATSTG statInfo;
  ULONG nread;
  char *body = NULL;

  if (!message)
    return NULL;

  hr = HrGetOneProp ((LPMAPIPROP)message,
                     want_html? PR_BODY_HTML : PR_BODY, &lpspvFEID);
  if (SUCCEEDED (hr))
    { /* Message is small enough to be retrieved this way. */
      switch ( PROP_TYPE (lpspvFEID->ulPropTag) )
        {
        case PT_UNICODE:
          body = wchar_to_utf8 (lpspvFEID->Value.lpszW);
          if (!body)
            log_debug ("%s: error converting to utf8\n", __func__);
          break;
          
        case PT_STRING8:
          body = xstrdup (lpspvFEID->Value.lpszA);
          break;
          
        default:
          log_debug ("%s: proptag=0x%08lx not supported\n",
                     __func__, lpspvFEID->ulPropTag);
          break;
        }
      MAPIFreeBuffer (lpspvFEID);
    }
  else /* Message is large; Use a stream to read it. */
    {
      hr = message->OpenProperty (want_html? PR_BODY_HTML : PR_BODY,
                                  &IID_IStream, 0, 0, (LPUNKNOWN*)&stream);
      if ( hr != S_OK )
        {
          log_debug ("%s:%s: OpenProperty failed: hr=%#lx",
                     SRCNAME, __func__, hr);
          if (want_html)
            {
              log_debug ("%s:%s: trying to read it from the OOM\n",
                         SRCNAME, __func__);
              body = get_outlook_property (exchange_cb, "HTMLBody");
              if (body)
                goto ready;
            }
          
          return NULL;
        }
      
      hr = stream->Stat (&statInfo, STATFLAG_NONAME);
      if ( hr != S_OK )
        {
          log_debug ("%s:%s: Stat failed: hr=%#lx", SRCNAME, __func__, hr);
          stream->Release ();
          return NULL;
        }
      
      /* Fixme: We might want to read only the first 1k to decide
         whether this is actually an OpenPGP message and only then
         continue reading.  This requires some changes in this
         module. */
      body = (char*)xmalloc ((size_t)statInfo.cbSize.QuadPart + 2);
      hr = stream->Read (body, (size_t)statInfo.cbSize.QuadPart, &nread);
      if ( hr != S_OK )
        {
          log_debug ("%s:%s: Read failed: hr=%#lx", SRCNAME, __func__, hr);
          xfree (body);
          stream->Release ();
          return NULL;
        }
      body[nread] = 0;
      body[nread+1] = 0;
      if (nread != statInfo.cbSize.QuadPart)
        {
          log_debug ("%s:%s: not enough bytes returned\n", SRCNAME, __func__);
          xfree (body);
          stream->Release ();
          return NULL;
        }
      stream->Release ();
      
      /* FIXME: We should to optimize this. */
      {
        char *tmp;
        tmp = wchar_to_utf8 ((wchar_t*)body);
        if (!tmp)
          log_debug ("%s: error converting to utf8\n", __func__);
        else
          {
            xfree (body);
            body = tmp;
          }
      }
    }

 ready:
  if (body)
    log_debug ("%s:%s: loaded body %d bytes of body at %p\n",
               SRCNAME, __func__, strlen (body), body);
  

//   prop.ulPropTag = PR_ACCESS;
//   prop.Value.l = MAPI_ACCESS_MODIFY;
//   hr = HrSetOneProp (message, &prop);
//   if (FAILED (hr))
//     log_debug ("%s:%s: updating message access to 0x%08lx failed: hr=%#lx",
//                    SRCNAME, __func__, prop.Value.l, hr);
  return body;
}


/* Return the subject of the message or NULL if it does not
   exists.  Caller must free. */
#if 0
static char *
get_subject (LPMESSAGE obj)
{
  HRESULT hr;
  LPSPropValue propval = NULL;
  char *name;

  hr = HrGetOneProp ((LPMAPIPROP)obj, PR_SUBJECT, &propval);
  if (FAILED (hr))
    {
      log_error ("%s:%s: error getting the subject: hr=%#lx",
                 SRCNAME, __func__, hr);
      return NULL; 
    }
  switch ( PROP_TYPE (propval->ulPropTag) )
    {
    case PT_UNICODE:
      name = wchar_to_utf8 (propval->Value.lpszW);
      if (!name)
        log_debug ("%s:%s: error converting to utf8\n", SRCNAME, __func__);
      break;
      
    case PT_STRING8:
      name = xstrdup (propval->Value.lpszA);
      break;
      
    default:
      log_debug ("%s:%s: proptag=%#lx not supported\n",
                 SRCNAME, __func__, propval->ulPropTag);
      name = NULL;
      break;
    }
  MAPIFreeBuffer (propval);
  return name;
}
#endif

/* Set the subject of the message OBJ to STRING. Returns 0 on
   success. */
#if 0
static int
set_subject (LPMESSAGE obj, const char *string)
{
  HRESULT hr;
  SPropValue prop;
  const char *s;
  
  /* Decide whether we ned to use the Unicode version. */
  for (s=string; *s && !(*s & 0x80); s++)
    ;
  if (*s)
    {
      prop.ulPropTag = PR_SUBJECT_W;
      prop.Value.lpszW = utf8_to_wchar (string);
      hr = HrSetOneProp (obj, &prop);
      xfree (prop.Value.lpszW);
    }
  else /* Only plain ASCII. */
    {
      prop.ulPropTag = PR_SUBJECT_A;
      prop.Value.lpszA = (CHAR*)string;
      hr = HrSetOneProp (obj, &prop);
    }
  if (hr != S_OK)
    {
      log_debug ("%s:%s: HrSetOneProp failed: hr=%#lx\n",
                 SRCNAME, __func__, hr); 
      return gpg_error (GPG_ERR_GENERAL);
    }
  return 0;
}
#endif



/* Helper for get_msg_content_type() */
static int
get_msg_content_type_cb (void *dummy_arg,
                         rfc822parse_event_t event, rfc822parse_t msg)
{
  if (event == RFC822PARSE_T2BODY)
    return 42; /* Hack to stop immediately the parsing.  This is
                  required because the code would else prepare for
                  MIME handling and we don't want this to happen. In
                  general it would be better to do any parsing of the
                  headers here but we need to access instance
                  variables and it is more complex to do this in a
                  callback. */
  return 0;
}


/* Find Content-Type of the current message.  The result will be put
   into instance variables.  FIXME: This function is basically
   duplicated in mapihelp.cpp - either remove it here or mak use of
   the other implementation. */
void
GpgMsgImpl::get_msg_content_type (void)
{
  HRESULT hr;
  LPSPropValue propval = NULL;
  rfc822parse_t msg;
  const char *header_lines, *s;
  rfc822parse_field_t ctx;
  size_t length;

  if (got_message_ct)
    return;
  got_message_ct = 1;

  xfree (media_type);
  media_type = NULL;
  xfree (media_subtype);
  media_subtype = NULL;
  xfree (ct_protocol);
  ct_protocol = NULL;
  xfree (transport_message_headers);
  transport_message_headers = NULL;

  hr = HrGetOneProp ((LPMAPIPROP)message,
                     PR_TRANSPORT_MESSAGE_HEADERS_A, &propval);
  if (FAILED (hr))
    {
      log_error ("%s:%s: error getting the headers lines: hr=%#lx",
                 SRCNAME, __func__, hr);
      return; 
    }
  if ( PROP_TYPE (propval->ulPropTag) != PT_STRING8 )
    {
      /* As per rfc822, header lines must be plain ascii, so no need to
         cope withy unicode etc. */
      log_error ("%s:%s: proptag=%#lx not supported\n",
                 SRCNAME, __func__, propval->ulPropTag);
      MAPIFreeBuffer (propval);
      return;
    }

  header_lines = propval->Value.lpszA;

  /* Save the header lines in case we need them for signature
     verification. */
  transport_message_headers = xstrdup (header_lines);

  /* Read the headers into an rfc822 object. */
  msg = rfc822parse_open (get_msg_content_type_cb, NULL);
  if (!msg)
    {
      log_error ("%s:%s: rfc822parse_open failed\n", SRCNAME, __func__);
      MAPIFreeBuffer (propval);
      return;
    }
  
  while ((s = strchr (header_lines, '\n')))
    {
      length = (s - header_lines);
      if (length && s[-1] == '\r')
        length--;
      rfc822parse_insert (msg, (const unsigned char*)header_lines, length);
      header_lines = s+1;
    }
  
  /* Parse the content-type field. */
  ctx = rfc822parse_parse_field (msg, "Content-Type", -1);
  if (ctx)
    {
      const char *s1, *s2;
      s1 = rfc822parse_query_media_type (ctx, &s2);
      if (s1)
        {
          media_type = xstrdup (s1);
          media_subtype = xstrdup (s2);
          s = rfc822parse_query_parameter (ctx, "protocol", 0);
          if (s)
            ct_protocol = xstrdup (s);
        }
      rfc822parse_release_field (ctx);
    }

  rfc822parse_close (msg);
  MAPIFreeBuffer (propval);
}



/* Return the type of a message with the body text in TEXT. */
openpgp_t
GpgMsgImpl::getMessageType (const char *text)
{
  const char *s;

  if (!text || !(s = strstr (text, "BEGIN PGP ")))
    return OPENPGP_NONE;

  /* (The extra strstr() above is just a simple optimization.) */
  if (strstr (text, "BEGIN PGP MESSAGE"))
    return OPENPGP_MSG;
  else if (strstr (text, "BEGIN PGP SIGNED MESSAGE"))
    return OPENPGP_CLEARSIG;
  else if (strstr (text, "BEGIN PGP SIGNATURE"))
    return OPENPGP_SIG;
  else if (strstr (text, "BEGIN PGP PUBLIC KEY"))
    return OPENPGP_PUBKEY;
  else if (strstr (text, "BEGIN PGP PRIVATE KEY"))
    return OPENPGP_SECKEY;
  else
    return OPENPGP_NONE;
}



/* Return an array of strings with the recipients of the message. On
   success a malloced array is returned containing allocated strings
   for each recipient.  The end of the array is marked by NULL.
   Caller is responsible for releasing the array.  On failure NULL is
   returned.  */
char ** 
GpgMsgImpl::getRecipients ()
{
  static SizedSPropTagArray (1L, PropRecipientNum) = {1L, {PR_EMAIL_ADDRESS}};
  HRESULT hr;
  LPMAPITABLE lpRecipientTable = NULL;
  LPSRowSet lpRecipientRows = NULL;
  char **rset;
  int i, j;

  if (!message)
    return NULL;

  hr = message->GetRecipientTable (0, &lpRecipientTable);
  if (FAILED (hr)) 
    {
      log_debug_w32 (-1, "%s:%s: GetRecipientTable failed",
                     SRCNAME, __func__);
      return NULL;
    }

  hr = HrQueryAllRows (lpRecipientTable, (LPSPropTagArray) &PropRecipientNum,
                       NULL, NULL, 0L, &lpRecipientRows);
  if (FAILED (hr)) 
    {
      log_debug_w32 (-1, "%s:%s: GHrQueryAllRows failed", SRCNAME, __func__);
      if (lpRecipientTable)
        lpRecipientTable->Release();
      return NULL;
    }

  rset = (char**)xcalloc (lpRecipientRows->cRows+1, sizeof *rset);

  for (i = j = 0; (unsigned int)i < lpRecipientRows->cRows; i++)
    {
      LPSPropValue row;

      if (!lpRecipientRows->aRow[j].cValues)
        continue;
      row = lpRecipientRows->aRow[j].lpProps;

      switch ( PROP_TYPE (row->ulPropTag) )
        {
        case PT_UNICODE:
          rset[j] = wchar_to_utf8 (row->Value.lpszW);
          if (rset[j])
            j++;
          else
            log_debug ("%s:%s: error converting recipient to utf8\n",
                       SRCNAME, __func__);
          break;
      
        case PT_STRING8: /* Assume Ascii. */
          rset[j++] = xstrdup (row->Value.lpszA);
          break;
          
        default:
          log_debug ("%s:%s: proptag=0x%08lx not supported\n",
                     SRCNAME, __func__, row->ulPropTag);
          break;
        }
    }

  if (lpRecipientTable)
    lpRecipientTable->Release();
  if (lpRecipientRows)
    FreeProws(lpRecipientRows);	
  
  log_debug ("%s:%s: got %d recipients:\n",
             SRCNAME, __func__, j);
  for (i=0; rset[i]; i++)
    log_debug ("%s:%s: \t`%s'\n", SRCNAME, __func__, rset[i]);

  return rset;
}


/* Write an Attestation to the current message. */
void
GpgMsgImpl::writeAttestation (void)
{
  HRESULT hr;
  ULONG newpos;
  SPropValue prop;
  LPATTACH newatt = NULL;
  LPSTREAM to = NULL;
  char *buffer = NULL;
  char *p, *pend;
  ULONG nwritten;

  if (!message || !attestation)
    return;

  hr = message->CreateAttach (NULL, 0, &newpos, &newatt);
  if (hr != S_OK)
    {
      log_error ("%s:%s: can't create attachment: hr=%#lx\n",
                 SRCNAME, __func__, hr); 
      goto leave;
    }
          
  prop.ulPropTag = PR_ATTACH_METHOD;
  prop.Value.ul = ATTACH_BY_VALUE;
  hr = HrSetOneProp (newatt, &prop);
  if (hr != S_OK)
    {
      log_error ("%s:%s: can't set attach method: hr=%#lx\n",
                 SRCNAME, __func__, hr); 
      goto leave;
    }
  
  /* It seem that we need to insert a short filename.  Without it the
     _displayed_ list of attachments won't get updated although the
     attachment has been created. */
  prop.ulPropTag = PR_ATTACH_FILENAME_A;
  prop.Value.lpszA = "gpgtstt0.txt";
  hr = HrSetOneProp (newatt, &prop);
  if (hr != S_OK)
    {
      log_error ("%s:%s: can't set attach filename: hr=%#lx\n",
                 SRCNAME, __func__, hr); 
      goto leave;
    }

  /* And now for the real name. */
  prop.ulPropTag = PR_ATTACH_LONG_FILENAME_A;
  prop.Value.lpszA = "GPGol-Attestation.txt";
  hr = HrSetOneProp (newatt, &prop);
  if (hr != S_OK)
    {
      log_error ("%s:%s: can't set attach filename: hr=%#lx\n",
                 SRCNAME, __func__, hr); 
      goto leave;
    }

  prop.ulPropTag = PR_ATTACH_TAG;
  prop.Value.bin.cb  = sizeof oid_mimetag;
  prop.Value.bin.lpb = (LPBYTE)oid_mimetag;
  hr = HrSetOneProp (newatt, &prop);
  if (hr != S_OK)
    {
      log_error ("%s:%s: can't set attach tag: hr=%#lx\n",
                 SRCNAME, __func__, hr); 
      goto leave;
    }

  prop.ulPropTag = PR_ATTACH_MIME_TAG_A;
  prop.Value.lpszA = "text/plain; charset=utf-8";
  hr = HrSetOneProp (newatt, &prop);
  if (hr != S_OK)
    {
      log_error ("%s:%s: can't set attach mime tag: hr=%#lx\n",
                 SRCNAME, __func__, hr); 
      goto leave;
    }

  hr = newatt->OpenProperty (PR_ATTACH_DATA_BIN, &IID_IStream, 0,
                             MAPI_CREATE|MAPI_MODIFY, (LPUNKNOWN*)&to);
  if (FAILED (hr)) 
    {
      log_error ("%s:%s: can't create output stream: hr=%#lx\n",
                 SRCNAME, __func__, hr); 
      goto leave;
    }
  

  if (gpgme_data_write (attestation, "", 1) != 1
      || !(buffer = gpgme_data_release_and_get_mem (attestation, NULL)))
    {
      attestation = NULL;
      log_error ("%s:%s: gpgme_data_write failed\n", SRCNAME, __func__); 
      goto leave;
    }
  attestation = NULL;

  if (!*buffer)
    goto leave;

  log_debug ("writing attestation `%s'\n", buffer);
  hr = S_OK;
  for (p=buffer; hr == S_OK && (pend = strchr (p, '\n')); p = pend+1)
    {
      hr = to->Write (p, pend - p, &nwritten);
      if (hr == S_OK)
        hr = to->Write ("\r\n", 2, &nwritten);
    }
  if (*p && hr == S_OK)
    hr = to->Write (p, strlen (p), &nwritten);
  if (hr != S_OK)
    {
      log_debug ("%s:%s: Write failed: hr=%#lx", SRCNAME, __func__, hr);
      goto leave;
    }
      
  
  to->Commit (0);
  to->Release ();
  to = NULL;
  
  hr = newatt->SaveChanges (0);
  if (hr != S_OK)
    {
      log_error ("%s:%s: SaveChanges(attachment) failed: hr=%#lx\n",
                 SRCNAME, __func__, hr); 
      goto leave;
    }
  hr = message->SaveChanges (KEEP_OPEN_READWRITE|FORCE_SAVE);
  if (hr != S_OK)
    {
      log_error ("%s:%s: SaveChanges(message) failed: hr=%#lx\n",
                 SRCNAME, __func__, hr); 
      goto leave;
    }


 leave:
  if (to)
    {
      to->Revert ();
      to->Release ();
    }
  if (newatt)
    newatt->Release ();
  gpgme_free (buffer);
}


/* Create a new HTML attachment from TEXT and store it as the standard
   HTML attachment (according to PGP rules).  */
gpg_error_t
GpgMsgImpl::createHtmlAttachment (const char *text)
{
  HRESULT hr;
  ULONG newpos;
  SPropValue prop;
  LPATTACH newatt = NULL;
  LPSTREAM to = NULL;
  ULONG nwritten;
  gpg_error_t err = gpg_error (GPG_ERR_GENERAL);
  
  hr = message->CreateAttach (NULL, 0, &newpos, &newatt);
  if (hr != S_OK)
    {
      log_error ("%s:%s: can't create HTML attachment: hr=%#lx\n",
                 SRCNAME, __func__, hr); 
      goto leave;
    }
          
  prop.ulPropTag = PR_ATTACH_METHOD;
  prop.Value.ul = ATTACH_BY_VALUE;
  hr = HrSetOneProp (newatt, &prop);
  if (hr != S_OK)
    {
      log_error ("%s:%s: can't set HTML attach method: hr=%#lx\n",
                 SRCNAME, __func__, hr); 
      goto leave;
    }
  
  prop.ulPropTag = PR_ATTACH_LONG_FILENAME_A;
  prop.Value.lpszA = "PGPexch.htm";
  hr = HrSetOneProp (newatt, &prop);
  if (hr != S_OK)
    {
      log_error ("%s:%s: can't set HTML attach filename: hr=%#lx\n",
                 SRCNAME, __func__, hr); 
      goto leave;
    }

  prop.ulPropTag = PR_ATTACH_TAG;
  prop.Value.bin.cb  = sizeof oid_mimetag;
  prop.Value.bin.lpb = (LPBYTE)oid_mimetag;
  hr = HrSetOneProp (newatt, &prop);
  if (hr != S_OK)
    {
      log_error ("%s:%s: can't set HTML attach tag: hr=%#lx\n",
                 SRCNAME, __func__, hr); 
      goto leave;
    }

  prop.ulPropTag = PR_ATTACH_MIME_TAG_A;
  prop.Value.lpszA = "text/html";
  hr = HrSetOneProp (newatt, &prop);
  if (hr != S_OK)
    {
      log_error ("%s:%s: can't set HTML attach mime tag: hr=%#lx\n",
                 SRCNAME, __func__, hr); 
      goto leave;
    }

  hr = newatt->OpenProperty (PR_ATTACH_DATA_BIN, &IID_IStream, 0,
                             MAPI_CREATE|MAPI_MODIFY, (LPUNKNOWN*)&to);
  if (FAILED (hr)) 
    {
      log_error ("%s:%s: can't create output stream: hr=%#lx\n",
                 SRCNAME, __func__, hr); 
      goto leave;
    }
  
  hr = to->Write (text, strlen (text), &nwritten);
  if (hr != S_OK)
    {
      log_debug ("%s:%s: Write failed: hr=%#lx", SRCNAME, __func__, hr);
      goto leave;
    }
      
  
  to->Commit (0);
  to->Release ();
  to = NULL;
  
  hr = newatt->SaveChanges (0);
  if (hr != S_OK)
    {
      log_error ("%s:%s: SaveChanges(attachment) failed: hr=%#lx\n",
                 SRCNAME, __func__, hr); 
      goto leave;
    }
  hr = message->SaveChanges (KEEP_OPEN_READWRITE|FORCE_SAVE);
  if (hr != S_OK)
    {
      log_error ("%s:%s: SaveChanges(message) failed: hr=%#lx\n",
                 SRCNAME, __func__, hr); 
      goto leave;
    }

  err = 0;

 leave:
  if (to)
    {
      to->Revert ();
      to->Release ();
    }
  if (newatt)
    newatt->Release ();
  return err;
}



/* Decrypt the message MSG and update the window.  HWND identifies the
   current window.  With INFO_ONLY set, the function will only update
   the display to indicate that a PGP/MIME message has been
   detected. */
int 
GpgMsgImpl::decrypt (HWND hwnd, bool info_only)
{
  log_debug ("%s:%s: enter\n", SRCNAME, __func__);
  openpgp_t mtype;
  char *plaintext = NULL;
  attach_info_t table = NULL;
  int err;
  unsigned int pos;
  unsigned int n_attach = 0;
  unsigned int n_encrypted = 0;
  unsigned int n_signed = 0;
  int is_pgpmime_sig = 0;
  int have_pgphtml_sig = 0;
  int have_pgphtml_enc = 0;
  unsigned int pgphtml_pos = 0;
  unsigned int pgphtml_pos_sig = 0;
  HRESULT hr;
  int pgpmime_succeeded = 0;
  int is_html = 0;
  char *body;

  get_msg_content_type ();
  log_debug ("%s:%s: parsed content-type: media=%s/%s protocol=%s\n",
             SRCNAME, __func__,
             media_type? media_type:"[none]",
             media_subtype? media_subtype:"[none]",
             ct_protocol? ct_protocol : "[none]");
  if (media_type && media_subtype && ct_protocol
      && !strcmp (media_type, "multipart")
      && !strcmp (media_subtype, "signed")
      && !strcmp (ct_protocol, "application/pgp-signature"))
    {
      /* This is a PGP/MIME signature.  */
      is_pgpmime_sig = 1;
    }
  
  /* Load the body text into BODY.  Note that body may be NULL but in
     this case MTYPE will be OPENPGP_NONE. */
  body = loadBody (false);
  mtype = getMessageType (body);

  /* Check whether this possibly encrypted message has encrypted
     attachments.  We check right now because we need to get into the
     decryption code even if the body is not encrypted but attachments
     are available. */
  table = is_pgpmime_sig? NULL : gatherAttachmentInfo ();
  if (table)
    {
      /* Fixup for the special pgphtml attachment. */
      for (pos=0; !table[pos].end_of_table; pos++)
        if (table[pos].is_encrypted)
          {
            if (!have_pgphtml_enc && !have_pgphtml_sig
                && table[pos].filename
                && !strcmp (table[pos].filename, "PGPexch.htm.pgp")
                && table[pos].content_type  
                && !strcmp (table[pos].content_type,
                            "application/pgp-encrypted"))
              {
                have_pgphtml_enc = 1;
                pgphtml_pos = pos;
              }
            else
              n_encrypted++;
          }
        else if (table[pos].is_signed)
          {
            if (!have_pgphtml_sig && !have_pgphtml_enc
                && table[pos].filename
                && !strcmp (table[pos].filename, "PGPexch.htm")
                && table[pos].content_type  
                && !strcmp (table[pos].content_type, "text/html")
                && table[pos].sig_pos != pos)
              {
                have_pgphtml_sig = 1;
                pgphtml_pos = pos;
                pgphtml_pos_sig = table[pos].sig_pos;
              }
            else
              n_signed++;
          }
      n_attach = pos;
    }
  log_debug ("%s:%s: message has %u attachments with "
             "%u signed and %d encrypted\n",
             SRCNAME, __func__, n_attach, n_signed, n_encrypted);
  if (have_pgphtml_enc)
    log_debug ("%s:%s: pgphtml encrypted attachment found at pos %d\n",
               SRCNAME, __func__, pgphtml_pos);
  if (have_pgphtml_sig)
    log_debug ("%s:%s: pgphtml signature attachment found at pos %d\n",
               SRCNAME, __func__, pgphtml_pos);


  if (mtype == OPENPGP_NONE && !n_encrypted && !n_signed
      && !have_pgphtml_enc && !have_pgphtml_sig && !is_pgpmime_sig) 
     {
      /* Because we usually work around the OL object model, it can't
         notice that we changed the windows's text behind its back (by
         means of update_display and the SetWindowText API).  Thus it
         happens sometimes that the ciphertext is still displayed
         although the MAPI calls in loadBody returned the plaintext
         (because we once used set_message_body).  The effect is that
         when clicking the decrypt button, we won't have any
         ciphertext to decrypt and thus get to here.  We try solving
         this by updating the window if we also have a cached entry.

         Another solution would be to always update the windows's text
         using a cached plaintext (in OnRead). I have some fear that
         this might lead to unexpected behaviour in certain cases, so
         we better only do it on demand and only if the old reply hack
         has been enabled. */
      void *refhandle;
      const char *s;

      if (!opt.compat.old_reply_hack
          && (s = msgcache_get_from_mapi (message, &refhandle)))
        {
          update_display (hwnd, exchange_cb, is_html_body (s), s);
          msgcache_unref (refhandle);
          log_debug ("%s:%s: leave (already decrypted)\n", SRCNAME, __func__);
        }
      else
        {
          if (!preview)
            MessageBox (hwnd, _("No valid OpenPGP data found."),
                        _("Decryption"), MB_ICONWARNING|MB_OK);
          log_debug ("%s:%s: leave (no OpenPGP data)\n", SRCNAME, __func__);
        }
      
      release_attach_info (table);
      xfree (body);
      return 0;
    }


  if (info_only)
    {
      /* Note, that we don't use the exchange_cb in the updatedisplay
         because this might lead to storing the new text in the
         message.  */
      if (is_pgpmime_sig || is_pgpmime_enc)
        {
          char *tmp = native_to_utf8 
            (_("[This is a PGP/MIME message]\r\n\r\n"
               "[Use the \"Decrypt\" button in the message window "
               "to show its content.]"));        
          update_display (hwnd, NULL, 0, tmp);
          xfree (tmp);
        }
      
      release_attach_info (table);
      xfree (body);
      return 0;
    }
  


  /* We always want an attestation.  Note that we ignore any error
     because that would anyway be a out of core situation and thus we
     can't do much about it. */
  if (has_attestation)
    {
      if (attestation)
        gpgme_data_release (attestation);
      attestation = NULL;
      log_debug ("%s:%s: we already have an attestation\n",
                 SRCNAME, __func__);
    }
  else if (!attestation && !opt.compat.no_attestation && !preview)
    gpgme_data_new (&attestation);
  
  /* Process according to type of message. */
  if (is_pgpmime_sig)
    {
      static int warning_shown;
      
      /* We need to do duplicate some work: For retrieving the headers
         we already used our own rfc822 parser.  For actually
         verifying the signature we need to concatentate the body with
         these hesaders and passs it down to pgpmime.c where they will
         be parsed again. Probably easier to maintain than merging the
         MAPI access with our rc822 parser code. */
      const char *mybody = body? body: "";
      char *tmp;
      
      assert (transport_message_headers);
      tmp = (char*)xmalloc (strlen (transport_message_headers)
                            + strlen (mybody));
      strcpy (stpcpy (tmp, transport_message_headers), mybody);

      /* Note, that we don't do an attestation.  This is becuase we
         don't run the code to check for duplicate attestations. */
      err = pgpmime_verify (tmp,
                            opt.passwd_ttl, &plaintext, NULL,
                            hwnd, preview);
      xfree (tmp);

      if (err && !warning_shown)
        {
          warning_shown = 1;
          MessageBox
            (hwnd, _("Note: This is a PGP/MIME signed message.  The GPGol "
                     "plugin is not always able to verify such a message "
                     "due to missing support in Outlook.\n\n"
                     "(This message will be shown only once per session)"),
                      _("Verification"), MB_ICONWARNING|MB_OK);
        }
      
      if (!err)
        pgpmime_succeeded = 1;
    }
  else if (is_pgpmime_enc)
    {
      LPATTACH att;
      int method;
      LPSTREAM from;
      
      /* If there is no body text (this should be the case for
         PGP/MIME), display a message to indicate that this is such a
         message.  This is useful in case of such messages with
         longish attachments which might take long to decrypt. */
      if (!body || !*body)
        {
          char *tmp = native_to_utf8 (_("[This is a PGP/MIME message]"));
          update_display (hwnd, exchange_cb, 0, tmp);
          xfree (tmp);
        }
      
      
      hr = message->OpenAttach (1, NULL, MAPI_BEST_ACCESS, &att);	
      if (FAILED (hr))
        {
          log_error ("%s:%s: can't open PGP/MIME attachment 2: hr=%#lx",
                     SRCNAME, __func__, hr);
          if (!preview)
            MessageBox (hwnd, _("Problem decrypting PGP/MIME message"),
                        _("Decryption"), MB_ICONERROR|MB_OK);
          log_debug ("%s:%s: leave (PGP/MIME problem)\n", SRCNAME, __func__);
          release_attach_info (table);
          xfree (body);
          return gpg_error (GPG_ERR_GENERAL);
        }

      method = get_attach_method (att);
      if (method != ATTACH_BY_VALUE)
        {
          log_error ("%s:%s: unsupported method %d for PGP/MIME attachment 2",
                     SRCNAME, __func__, method);
          if (!preview)
            MessageBox (hwnd, _("Problem decrypting PGP/MIME message"),
                        _("Decryption"), MB_ICONERROR|MB_OK);
          log_debug ("%s:%s: leave (bad PGP/MIME method)\n",SRCNAME,__func__);
          att->Release ();
          release_attach_info (table);
          xfree (body);
          return gpg_error (GPG_ERR_GENERAL);
        }

      hr = att->OpenProperty (PR_ATTACH_DATA_BIN, &IID_IStream, 
                              0, 0, (LPUNKNOWN*) &from);
      if (FAILED (hr))
        {
          log_error ("%s:%s: can't open data of attachment 2: hr=%#lx",
                     SRCNAME, __func__, hr);
          if (!preview)
            MessageBox (hwnd, _("Problem decrypting PGP/MIME message"),
                        _("Decryption"), MB_ICONERROR|MB_OK);
          log_debug ("%s:%s: leave (OpenProperty failed)\n",SRCNAME,__func__);
          att->Release ();
          release_attach_info (table);
          xfree (body);
          return gpg_error (GPG_ERR_GENERAL);
        }

      err = pgpmime_decrypt (from, opt.passwd_ttl, &plaintext, attestation,
                             hwnd, preview);
      
      from->Release ();
      att->Release ();
      if (!err)
        pgpmime_succeeded = 1;
    }
  else if (mtype == OPENPGP_CLEARSIG && !(have_pgphtml_sig && opt.prefer_html))
    {
      /* Cleartext signature.  */

      assert (body);
      err = preview? 0 : op_verify (body, NULL, NULL, attestation);
    }
  else if ( (body && *body)
            && !((have_pgphtml_enc||have_pgphtml_sig) && opt.prefer_html))
    {
      /* Standard encrypted body.  We do not enter this if we also
         have an pgphtml encrypted attachment and the prefer_html
         option is activ.  */

      err = op_decrypt (body, &plaintext, opt.passwd_ttl, NULL,
                        attestation, preview);
    }
  else if ((mtype == OPENPGP_NONE || opt.prefer_html) && have_pgphtml_sig)
    {
      /* There is no body but a pgphtml signed attachment - decrypt
         that one. */
      if (preview)
        err = 0;
      else
        {
          /* Note that we don't access the HTML body.  It seems that
             Outlooks creates that one on the fly and it will break
             the signature.  It is better to use the attachment
             directly. */
          LPATTACH att;

          plaintext = get_long_attach_data (message, table, pgphtml_pos);
          
          if (plaintext && *plaintext)
            {
              is_html = 1;
              hr = message->OpenAttach (pgphtml_pos_sig, NULL,
                                        MAPI_BEST_ACCESS, &att);	
              if (FAILED (hr))
                {
                  log_error ("%s:%s: can't open attachment %d (sig): hr=%#lx",
                             SRCNAME, __func__, pgphtml_pos_sig, hr);
                  err = gpg_error (GPG_ERR_GENERAL);
                }
              else if (table[pgphtml_pos_sig].method != ATTACH_BY_VALUE)
                {
                  log_error ("%s:%s: HTML attachment: method not supported",
                             SRCNAME, __func__);
                  att->Release ();
                  err = gpg_error (GPG_ERR_GENERAL);
                }
              else
                {
                  char *sigpart = get_short_attach_data (att);
                  att->Release ();
                  if (!sigpart)
                    err = gpg_error (GPG_ERR_GENERAL);
                  else
                    {
                      err = op_verify_detached_sig_mem (plaintext, sigpart,
                                                        NULL, attestation);
                      xfree (sigpart);
                    }
                }
            }
          else
            err = gpg_error (GPG_ERR_NO_DATA);
        }
    }
  else if ((mtype == OPENPGP_NONE || opt.prefer_html) && have_pgphtml_enc)
    {
      /* There is no body but a pgphtml encrypted attachment - decrypt
         that one. */
      LPATTACH att;
      LPSTREAM from;

      is_html = 1;
      hr = message->OpenAttach (pgphtml_pos, NULL,
                                MAPI_BEST_ACCESS, &att);	
      if (FAILED (hr))
        {
          log_error ("%s:%s: can't open attachment %d (sig): hr=%#lx",
                     SRCNAME, __func__, pgphtml_pos, hr);
          err = gpg_error (GPG_ERR_GENERAL);
        }
      else if (table[pgphtml_pos].method != ATTACH_BY_VALUE)
        {
          log_error ("%s:%s: HTML attachment: method not supported",
                     SRCNAME, __func__);
          att->Release ();
          err = gpg_error (GPG_ERR_GENERAL);
        }
      else if (FAILED(hr = att->OpenProperty (PR_ATTACH_DATA_BIN,
                                              &IID_IStream, 
                                              0, 0, (LPUNKNOWN*) &from)))
        {
          log_error ("%s:%s: can't open data stream of HTML attachment: "
                     "hr=%#lx", SRCNAME, __func__, hr);
          att->Release ();
          err = gpg_error (GPG_ERR_GENERAL);
        }
      else
        {
          err = op_decrypt_stream_to_buffer (from, &plaintext, opt.passwd_ttl,
                                             NULL, attestation);
          from->Release ();
          att->Release ();
        }
    }
  else
    err = gpg_error (GPG_ERR_NO_DATA);

  if (err)
    {
      if (!is_pgpmime_enc && n_attach && gpg_err_code (err) == GPG_ERR_NO_DATA)
        ;
      else if (mtype == OPENPGP_CLEARSIG)
        MessageBox (hwnd, op_strerror (err),
                    _("Verification Failure"), MB_ICONERROR|MB_OK);
      else if (!preview)
        MessageBox (hwnd, op_strerror (err),
                    _("Decryption Failure"), MB_ICONERROR|MB_OK);
    }
  else if (plaintext && *plaintext)
    {	
      log_debug ("decrypt isHtml=%d\n", is_html);

      /* Do we really need to set the body?  update_display below
         should be sufficient.  The problem with this is that we did
         changes in the MAPI and OL will later ask whether to save
         them.  The original reason for this kludge was to get the
         plaintext into the reply (by setting the property without
         calling SaveChanges) - with OL2003 it didn't worked reliable
         and thus we implemented the trick with the msgcache. For now
         we will disable it but add a compatibility flag to re-enable
         it. */
      if (opt.compat.old_reply_hack)
        set_message_body (message, plaintext, is_html);

      msgcache_put (plaintext, 0, message);

      if (preview)
        update_display (hwnd, exchange_cb, is_html, plaintext);
      else if (opt.save_decrypted_attach)
        {
          /* User wants us to replace the encrypted message with the
             plaintext version. */
          hr = message->SaveChanges (KEEP_OPEN_READWRITE|FORCE_SAVE);
          if (FAILED (hr))
            log_debug ("%s:%s: SaveChanges failed: hr=%#lx",
                       SRCNAME, __func__, hr);
          update_display (hwnd, exchange_cb, is_html, plaintext);
          
        }
      else if (update_display (hwnd, exchange_cb, is_html, plaintext))
        {
          const char *s = 
            _("The message text cannot be displayed.\n"
              "You have to save the decrypted message to view it.\n"
              "Then you need to re-open the message.\n\n"
              "Do you want to save the decrypted message?");
          int what;
          
          what = MessageBox (hwnd, s, _("Decryption"),
                             MB_YESNO|MB_ICONWARNING);
          if (what == IDYES) 
            {
              log_debug ("decrypt: saving plaintext message.\n");
              hr = message->SaveChanges (KEEP_OPEN_READWRITE|FORCE_SAVE);
              if (FAILED (hr))
                log_debug ("%s:%s: SaveChanges failed: hr=%#lx",
                           SRCNAME, __func__, hr);
            }
	}
    }


  /* If we have signed attachments.  Ask whether the signatures should
     be verified; we do this is case of large attachments where
     verification might take long. */
  if (!preview && n_signed && !pgpmime_succeeded)
    {
      /* TRANSLATORS: Keep the @LIST@ verbatim on a separate line; it
         will be expanded to a list of atatchment names. */
      const char *s = _("Signed attachments found.\n\n"
                        "@LIST@\n"
                        "Do you want to verify the signatures?");
      int what;
      char *text;

      text = text_from_attach_info (table, s, 2);
      
      what = MessageBox (hwnd, text, _("Attachment Verification"),
                         MB_YESNO|MB_ICONINFORMATION);
      xfree (text);
      if (what == IDYES) 
        {
          for (pos=0; !table[pos].end_of_table; pos++)
            if ((have_pgphtml_sig || have_pgphtml_enc)
                && pos == pgphtml_pos)
              ; /* We already processed this attachment. */
            else if (table[pos].is_signed)
              {
                assert (table[pos].sig_pos < n_attach);
                verifyAttachment (hwnd, table, pos, table[pos].sig_pos);
              }
        }
    }

  if (!preview && n_encrypted && !pgpmime_succeeded)
    {
      /* TRANSLATORS: Keep the @LIST@ verbatim on a separate line; it
         will be expanded to a list of atatchment names. */
      const char *s = _("Encrypted attachments found.\n\n"
                        "@LIST@\n"
                        "Do you want to decrypt and save them?");
      int what;
      char *text;

      text = text_from_attach_info (table, s, 4);
      what = MessageBox (hwnd, text, _("Attachment Decryption"),
                         MB_YESNO|MB_ICONINFORMATION);
      xfree (text);
      if (what == IDYES) 
        {
          for (pos=0; !table[pos].end_of_table; pos++)
            if ((have_pgphtml_sig || have_pgphtml_enc)
                && pos == pgphtml_pos)
              ; /* We already processed this attachment. */
            else if (table[pos].is_encrypted)
              decryptAttachment (hwnd, pos, true, opt.passwd_ttl,
                                 table[pos].filename);
        }
    }

  if (!preview)
    writeAttestation ();

  release_attach_info (table);
  xfree (plaintext);
  xfree (body);
  log_debug ("%s:%s: leave (rc=%d)\n", SRCNAME, __func__, err);
  return err;
}





/* Sign the current message. Returns 0 on success. */
int
GpgMsgImpl::sign (HWND hwnd, bool want_html)
{
  HRESULT hr;
  char *plaintext;
  char *signedtext = NULL;
  int err = 0;
  gpgme_key_t sign_key = NULL;
  SPropValue prop;
  int have_html_attach = 0;

  log_debug ("%s:%s: enter message=%p\n", SRCNAME, __func__, message);
  
  /* We don't sign an empty body - a signature on a zero length string
     is pretty much useless.  We assume that a HTML message always
     comes with a text/plain alternative. */
  plaintext = loadBody (false);
  if ( (!plaintext || !*plaintext) && !hasAttachments ()) 
    {
      log_debug ("%s:%s: leave (empty)", SRCNAME, __func__);
      xfree (plaintext);
      return 0; 
    }

  /* Pop up a dialog box to ask for the signer of the message. */
  if (signer_dialog_box (&sign_key, NULL, 0) == -1)
    {
      log_debug ("%s.%s: leave (dialog failed)\n", SRCNAME, __func__);
      xfree (plaintext);
      return gpg_error (GPG_ERR_CANCELED);  
    }

  if (plaintext && *plaintext)
    {
      err = op_sign (plaintext, &signedtext, 
                     OP_SIG_CLEAR, sign_key, opt.passwd_ttl);
      if (err)
        {
          MessageBox (hwnd, op_strerror (err),
                      _("Signing Failure"), MB_ICONERROR|MB_OK);
          goto leave;
        }
    }

  
  /* If those brain dead html mails are requested we now figure out
     whether a HTML body is actually available and move it to an
     attachment so that the code below will sign it as a regular
     attachments.  */
  if (want_html)
    {
      char *htmltext = loadBody (true);
      
      if (htmltext && *htmltext)
        {
          if (!createHtmlAttachment (htmltext))
            have_html_attach = 1;
        }
      xfree (htmltext);

      /* If we got a new attachment we need to release the loaded
         attachment info so that the next getAttachment call will read
         fresh info. */
      if (have_html_attach)
        free_attach_info ();
    }


  /* Note, there is a side-effect when we have HTML mails: The
     auto-sign-attch option is ignored. I regard auto-sign-atatch as a
     silly option anyway. */
  if ((opt.auto_sign_attach || have_html_attach) && hasAttachments ())
    {
      unsigned int n;
      
      n = getAttachments ();
      log_debug ("%s:%s: message has %u attachments\n", SRCNAME, __func__, n);
      for (unsigned int i=0; i < n; i++) 
        signAttachment (hwnd, i, sign_key, opt.passwd_ttl);
      /* FIXME: we should throw an error if signing of any attachment
         failed. */
    }

  set_x_header (message, "GPGOL-VERSION", PACKAGE_VERSION);

  /* Now that we successfully processed the attachments, we can save
     the changes to the body.  */
  if (plaintext && *plaintext)
    {
      err = set_message_body (message, signedtext, 0);
      if (err)
        goto leave;

      /* In case we don't have attachments, Outlook will really insert
         the following content type into the header.  We use this to
         declare that the encrypted content of the message is utf-8
         encoded. */
      prop.ulPropTag=PR_CONTENT_TYPE_A;
      prop.Value.lpszA="text/plain; charset=utf-8"; 
      hr = HrSetOneProp (message, &prop);
      if (hr != S_OK)
        {
          log_error ("%s:%s: can't set content type: hr=%#lx\n",
                     SRCNAME, __func__, hr);
        }
    }
  
  hr = message->SaveChanges (KEEP_OPEN_READWRITE|FORCE_SAVE);
  if (hr != S_OK)
    {
      log_error ("%s:%s: SaveChanges(message) failed: hr=%#lx\n",
                 SRCNAME, __func__, hr); 
      err = gpg_error (GPG_ERR_GENERAL);
      goto leave;
    }

 leave:
  xfree (signedtext);
  gpgme_key_release (sign_key);
  xfree (plaintext);
  log_debug ("%s:%s: leave (err=%s)\n", SRCNAME, __func__, op_strerror (err));
  return err;
}



/* Encrypt and optionally sign (if SIGN_FLAG is true) the entire
   message including all attachments.  If WANT_HTML is true, the text
   to encrypt will also be taken from the html property. Returns 0 on
   success. */
int 
GpgMsgImpl::encrypt_and_sign (HWND hwnd, bool want_html, bool sign_flag)
{
  log_debug ("%s:%s: enter\n", SRCNAME, __func__);
  HRESULT hr;
  gpgme_key_t *keys = NULL;
  gpgme_key_t sign_key = NULL;
  char *plaintext;
  char *ciphertext = NULL;
  char **recipients = NULL;
  char **unknown = NULL;
  int err = 0;
  size_t n_keys, n_unknown, n_recp;
  SPropValue prop;
  int have_html_attach = 0;
    
  plaintext = loadBody (false);
  if ( (!plaintext || !*plaintext) && !hasAttachments ()) 
    {
      log_debug ("%s:%s: leave (empty)", SRCNAME, __func__);
      xfree (plaintext);
      return 0; 
    }

  /* Pop up a dialog box to ask for the signer of the message. */
  if (sign_flag)
    {
      if (signer_dialog_box (&sign_key, NULL, 1) == -1)
        {
          log_debug ("%s.%s: leave (dialog failed)\n", SRCNAME, __func__);
          xfree (plaintext);
          return gpg_error (GPG_ERR_CANCELED);  
        }
    }

  /* Gather the keys for the recipients. */
  recipients = getRecipients ();
  if ( op_lookup_keys (recipients, &keys, &unknown) )
    {
      log_debug ("%s.%s: leave (lookup keys failed)\n", SRCNAME, __func__);
      return gpg_error (GPG_ERR_GENERAL);  
    }
  n_recp = count_strings (recipients);
  n_keys = count_keys (keys);
  n_unknown = count_strings (unknown);

  
  log_debug ("%s:%s: found %d recipients, need %d, unknown=%d\n",
             SRCNAME, __func__, (int)n_keys, (int)n_recp, (int)n_unknown);
  
  if (n_keys != n_recp)
    {
      unsigned int opts;
      gpgme_key_t *keys2;

      log_debug ("%s:%s: calling recipient_dialog_box2", SRCNAME, __func__);
      opts = recipient_dialog_box2 (keys, unknown, &keys2);
      free_key_array (keys);
      keys = keys2;
      if (opts & OPT_FLAG_CANCEL) 
        {
          err = gpg_error (GPG_ERR_CANCELED);
          goto leave;
	}
    }


  /* If a default key has been set, add it to the list of keys.  Check
     that the key is actually available. */
  if (opt.enable_default_key && opt.default_key && *opt.default_key)
    {
      gpgme_key_t defkey;

      defkey = op_get_one_key (opt.default_key);
      if (!defkey)
        {
          MessageBox (hwnd,
                      _("The configured default encryption key is not "
                        "available or does not unambigiously specify a key. "
                        "Please fix this in the option dialog.\n\n"
                        "This message won't be be encrypted to this key!"),
                      _("Encryption"), MB_ICONWARNING|MB_OK);
        }
      else
        {
          gpgme_key_t *tmpkeys;
          int i;

          n_keys = count_keys (keys) + 1;
          tmpkeys = (gpgme_key_t *)xcalloc (n_keys+1, sizeof *tmpkeys);
          for (i = 0; keys[i]; i++) 
            {
              tmpkeys[i] = keys[i];
              gpgme_key_ref (tmpkeys[i]);
            }
          tmpkeys[i++] = defkey;
          tmpkeys[i] = NULL;
          free_key_array (keys);
          keys = tmpkeys;
        }
    }
  

  /* Show  some debug info. */
  if (sign_key)
    log_debug ("%s:%s: signer: 0x%s %s\n",  SRCNAME, __func__,
               keyid_from_key (sign_key), userid_from_key (sign_key));
  else
    log_debug ("%s:%s: no signer\n", SRCNAME, __func__);
  if (keys)
    {
      for (int i=0; keys[i] != NULL; i++)
        log_debug ("%s.%s: recp.%d 0x%s %s\n", SRCNAME, __func__,
                   i, keyid_from_key (keys[i]), userid_from_key (keys[i]));
    }

  /* Do the encryption.  */
  if (plaintext && *plaintext)
    {
      err = op_encrypt (plaintext, &ciphertext, 
                        keys, sign_key, opt.passwd_ttl);
      if (err)
        {
          MessageBox (hwnd, op_strerror (err),
                      _("Encryption Failure"), MB_ICONERROR|MB_OK);
          goto leave;
        }

//       {
//         SPropValue prop;
//         prop.ulPropTag=PR_MESSAGE_CLASS_A;
//         prop.Value.lpszA="IPM.Note.OPENPGP";
//         hr = HrSetOneProp (message, &prop);
//         if (hr != S_OK)
//           {
//             log_error ("%s:%s: can't set message class: hr=%#lx\n",
//                        SRCNAME, __func__, hr); 
//           }
//       }

    }


  /* If those brain dead html mails are requested we now figure out
     whether a HTML body is actually available and move it to an
     attachment so that the code below will sign it as a regular
     attachments.  Note that the orginal HTML body will be deletated
     in the code calling us. */
  if (want_html)
    {
      char *htmltext = loadBody (true);
      
      if (htmltext && *htmltext)
        {
          if (!createHtmlAttachment (htmltext))
            have_html_attach = 1;
        }
      xfree (htmltext);

      /* If we got a new attachment we need to release the loaded
         attachment info so that the next getAttachment call will read
         fresh info. */
      if (have_html_attach)
        free_attach_info ();
    }


  if (hasAttachments ())
    {
      unsigned int n;
      
      n = getAttachments ();
      log_debug ("%s:%s: message has %u attachments\n", SRCNAME, __func__, n);
      for (unsigned int i=0; !err && i < n; i++) 
        err = encryptAttachment (hwnd, i, keys, NULL, 0);
      if (err)
        {
          MessageBox (hwnd, op_strerror (err),
                      _("Attachment Encryption Failure"), MB_ICONERROR|MB_OK);
          goto leave;
        }
    }

  set_x_header (message, "GPGOL-VERSION", PACKAGE_VERSION);

  /* Now that we successfully processed the attachments, we can save
     the changes to the body.  */
  if (plaintext && *plaintext)
    {
      if (want_html)
        {
          /* We better update the body of the OOM too. */
          if (put_outlook_property (exchange_cb, "Body", ciphertext))
            log_error ("%s:%s: put OOM property Body failed\n",
                       SRCNAME, __func__);
          /* And set the format to plain text. */
          if (put_outlook_property_int (exchange_cb, "BodyFormat", 1))
            log_error ("%s:%s: put OOM property BodyFormat failed\n",
                       SRCNAME, __func__);
        }


      err = set_message_body (message, ciphertext, 0);
      if (err)
        goto leave;

      /* In case we don't have attachments, Outlook will really insert
         the following content type into the header.  We use this to
         declare that the encrypted content of the message is utf-8
         encoded.  Note that we use plain/text even for HTML because
         it is base64 encoded. */
      prop.ulPropTag=PR_CONTENT_TYPE_A;
      prop.Value.lpszA="text/plain; charset=utf-8"; 
      hr = HrSetOneProp (message, &prop);
      if (hr != S_OK)
        {
          log_error ("%s:%s: can't set content type: hr=%#lx\n",
                     SRCNAME, __func__, hr);
        }

    }
  
  hr = message->SaveChanges (KEEP_OPEN_READWRITE|FORCE_SAVE);
  if (hr != S_OK)
    {
      log_error ("%s:%s: SaveChanges(message) failed: hr=%#lx\n",
                 SRCNAME, __func__, hr); 
      err = gpg_error (GPG_ERR_GENERAL);
      goto leave;
    }

 leave:
  /* FIXME: What to do with already encrypted attachments if some of
     the encrypted (or other operations) failed? */

  free_key_array (keys);
  free_string_array (recipients);
  free_string_array (unknown);
  xfree (ciphertext);
  xfree (plaintext);
  log_debug ("%s:%s: leave (err=%s)\n", SRCNAME, __func__, op_strerror (err));
  return err;
}




/* Attach a public key to a message. */
int 
GpgMsgImpl::attachPublicKey (const char *keyid)
{
    /* @untested@ */
#if 0
    const char *patt[1];
    char *keyfile;
    int err, pos = 0;
    LPATTACH newatt;

    keyfile = generateTempname (keyid);
    patt[0] = xstrdup (keyid);
    err = op_export_keys (patt, keyfile);

    newatt = createAttachment (NULL/*FIXME*/,pos);
    setAttachMethod (newatt, ATTACH_BY_VALUE);
    setAttachFilename (newatt, keyfile, false);
    /* XXX: set proper RFC3156 MIME types. */

    if (streamFromFile (keyfile, newatt)) {
	log_debug ("attachPublicKey: commit changes.\n");
	newatt->SaveChanges (FORCE_SAVE);
    }
    releaseAttachment (newatt);
    xfree (keyfile);
    xfree ((void *)patt[0]);
    return err;
#endif
    return -1;
}





/* Returns whether the message has any attachments. */
bool
GpgMsgImpl::hasAttachments (void)
{
  return !!getAttachments ();
}


/* Reads the attachment information and returns the number of
   attachments. */
unsigned int
GpgMsgImpl::getAttachments (void)
{
  SizedSPropTagArray (1L, propAttNum) = {
    1L, {PR_ATTACH_NUM}
  };
  HRESULT hr;    
  LPMAPITABLE table;
  LPSRowSet   rows;

  if (!message)
    return 0;

  if (!attach.att_table)
    {
      hr = message->GetAttachmentTable (0, &table);
      if (FAILED (hr))
        {
          log_debug ("%s:%s: GetAttachmentTable failed: hr=%#lx",
                     SRCNAME, __func__, hr);
          return 0;
        }
      
      hr = HrQueryAllRows (table, (LPSPropTagArray)&propAttNum,
                           NULL, NULL, 0, &rows);
      if (FAILED (hr))
        {
          log_debug ("%s:%s: HrQueryAllRows failed: hr=%#lx",
                     SRCNAME, __func__, hr);
          table->Release ();
          return 0;
        }
      attach.att_table = table;
      attach.rows = rows;
    }

  return attach.rows->cRows > 0? attach.rows->cRows : 0;
}



/* Return the attachment method for attachment OBJ. In case of error we
   return 0 which happens to be not defined. */
static int
get_attach_method (LPATTACH obj)
{
  HRESULT hr;
  LPSPropValue propval = NULL;
  int method ;

  hr = HrGetOneProp ((LPMAPIPROP)obj, PR_ATTACH_METHOD, &propval);
  if (FAILED (hr))
    {
      log_error ("%s:%s: error getting attachment method: hr=%#lx",
                 SRCNAME, __func__, hr);
      return 0; 
    }
  /* We don't bother checking whether we really get a PT_LONG ulong
     back; if not the system is seriously damaged and we can't do
     further harm by returning a possible random value. */
  method = propval->Value.l;
  MAPIFreeBuffer (propval);
  return method;
}


/* Return the content-type of the attachment OBJ or NULL if it does not
   exists.  Caller must free. */
static char *
get_attach_mime_tag (LPATTACH obj)
{
  HRESULT hr;
  LPSPropValue propval = NULL;
  char *name;

  hr = HrGetOneProp ((LPMAPIPROP)obj, PR_ATTACH_MIME_TAG_A, &propval);
  if (FAILED (hr))
    {
      log_error ("%s:%s: error getting attachment's MIME tag: hr=%#lx",
                 SRCNAME, __func__, hr);
      return NULL; 
    }
  switch ( PROP_TYPE (propval->ulPropTag) )
    {
    case PT_UNICODE:
      name = wchar_to_utf8 (propval->Value.lpszW);
      if (!name)
        log_debug ("%s:%s: error converting to utf8\n", SRCNAME, __func__);
      break;
      
    case PT_STRING8:
      name = xstrdup (propval->Value.lpszA);
      break;
      
    default:
      log_debug ("%s:%s: proptag=%#lx not supported\n",
                 SRCNAME, __func__, propval->ulPropTag);
      name = NULL;
      break;
    }
  MAPIFreeBuffer (propval);
  return name;
}


/* Return the data property of an attachments or NULL in case of an
   error.  Caller must free.  Note, that this routine should only be
   used for short data objects like detached signatures. */
static char *
get_short_attach_data (LPATTACH obj)
{
  HRESULT hr;
  LPSPropValue propval = NULL;
  char *data;

  hr = HrGetOneProp ((LPMAPIPROP)obj, PR_ATTACH_DATA_BIN, &propval);
  if (FAILED (hr))
    {
      log_error ("%s:%s: error getting attachment's data: hr=%#lx",
                 SRCNAME, __func__, hr);
      return NULL; 
    }
  switch ( PROP_TYPE (propval->ulPropTag) )
    {
    case PT_BINARY:
      /* This is a binary object but we know that it must be plain
         ASCII due to the armored format.  */
      data = (char*)xmalloc (propval->Value.bin.cb + 1);
      memcpy (data, propval->Value.bin.lpb, propval->Value.bin.cb);
      data[propval->Value.bin.cb] = 0;
      break;
      
    default:
      log_debug ("%s:%s: proptag=%#lx not supported\n",
                 SRCNAME, __func__, propval->ulPropTag);
      data = NULL;
      break;
    }
  MAPIFreeBuffer (propval);
  return data;
}


/* Get an statchment as one long C string.  We assume that there are
   no binary nuls in it.  Returns NULL on failure. */
static char *
get_long_attach_data (LPMESSAGE msg, attach_info_t table, int pos)
{
  HRESULT hr;
  LPATTACH att;
  LPSTREAM stream;
  STATSTG statInfo;
  ULONG nread;
  char *buffer;

  hr = msg->OpenAttach (pos, NULL, MAPI_BEST_ACCESS, &att);	
  if (FAILED (hr))
    {
      log_error ("%s:%s: can't open attachment %d: hr=%#lx",
                 SRCNAME, __func__, pos, hr);
      return NULL;
    }
  if (table[pos].method != ATTACH_BY_VALUE)
    {
      log_error ("%s:%s: attachment: method not supported", SRCNAME, __func__);
      att->Release ();
      return NULL;
    }

  hr = att->OpenProperty (PR_ATTACH_DATA_BIN, &IID_IStream, 
                          0, 0, (LPUNKNOWN*) &stream);
  if (FAILED (hr))
    {
      log_error ("%s:%s: can't open data stream of attachment: hr=%#lx",
                 SRCNAME, __func__, hr);
      att->Release ();
      return NULL;
    }

  hr = stream->Stat (&statInfo, STATFLAG_NONAME);
  if ( hr != S_OK )
    {
      log_error ("%s:%s: Stat failed: hr=%#lx", SRCNAME, __func__, hr);
      stream->Release ();
      att->Release ();
      return NULL;
    }
      
  buffer = (char*)xmalloc ((size_t)statInfo.cbSize.QuadPart + 2);
  hr = stream->Read (buffer, (size_t)statInfo.cbSize.QuadPart, &nread);
  if ( hr != S_OK )
    {
      log_error ("%s:%s: Read failed: hr=%#lx", SRCNAME, __func__, hr);
      xfree (buffer);
      stream->Release ();
      att->Release ();
      return NULL;
    }
  buffer[nread] = 0;
  buffer[nread+1] = 0;
  if (nread != statInfo.cbSize.QuadPart)
    {
      log_error ("%s:%s: not enough bytes returned\n", SRCNAME, __func__);
      xfree (buffer);
      buffer = NULL;
    }
  stream->Release ();
  att->Release ();
      
  return buffer;
}



/* Check whether the attachment at position POS in the attachment
   table is the first part of a PGP/MIME message.  This routine should
   only be called if it has already been checked that the content-type
   of the attachment is application/pgp-encrypted. */
bool
GpgMsgImpl::isPgpmimeVersionPart (int pos)
{
  HRESULT hr;
  LPATTACH att;
  LPSPropValue propval = NULL;
  bool result = false;

  hr = message->OpenAttach (pos, NULL, MAPI_BEST_ACCESS, &att);	
  if (FAILED(hr))
    return false;

  hr = HrGetOneProp ((LPMAPIPROP)att, PR_ATTACH_SIZE, &propval);
  if (FAILED (hr))
    {
      att->Release ();
      return false;
    }
  if ( PROP_TYPE (propval->ulPropTag) != PT_LONG
      || propval->Value.l < 10 || propval->Value.l > 1000 )
    {
      MAPIFreeBuffer (propval);
      att->Release ();
      return false;
    }
  MAPIFreeBuffer (propval);

  hr = HrGetOneProp ((LPMAPIPROP)att, PR_ATTACH_DATA_BIN, &propval);
  if (SUCCEEDED (hr))
    {
      if (PROP_TYPE (propval->ulPropTag) == PT_BINARY)
        {
          if (propval->Value.bin.cb > 10 && propval->Value.bin.cb < 15 
              && !memcmp (propval->Value.bin.lpb, "Version: 1", 10)
              && ( propval->Value.bin.lpb[10] == '\r'
                   || propval->Value.bin.lpb[10] == '\n'))
            result = true;
        }
      MAPIFreeBuffer (propval);
    }
  att->Release ();
  return result;
}



/* Set an arbitary header in the message MSG with NAME to the value
   VAL. */
static bool 
set_x_header (LPMESSAGE msg, const char *name, const char *val)
{  
  HRESULT hr;
  LPSPropTagArray pProps = NULL;
  SPropValue pv;
  MAPINAMEID mnid, *pmnid;	
  /* {00020386-0000-0000-C000-000000000046}  ->  GUID For X-Headers */
  GUID guid = {0x00020386, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00, 0x46} };

  if (!msg)
    return false;

  memset (&mnid, 0, sizeof mnid);
  mnid.lpguid = &guid;
  mnid.ulKind = MNID_STRING;
  mnid.Kind.lpwstrName = utf8_to_wchar (name);
  pmnid = &mnid;
  hr = msg->GetIDsFromNames (1, &pmnid, MAPI_CREATE, &pProps);
  xfree (mnid.Kind.lpwstrName);
  if (FAILED (hr)) 
    {
      log_error ("%s:%s: can't get mapping for header `%s': hr=%#lx\n",
                 SRCNAME, __func__, name, hr); 
      return false;
    }
    
  pv.ulPropTag = (pProps->aulPropTag[0] & 0xFFFF0000) | PT_STRING8;
  pv.Value.lpszA = (char *)val;
  hr = HrSetOneProp(msg, &pv);	
  if (hr != S_OK)
    {
      log_error ("%s:%s: can't set header `%s': hr=%#lx\n",
                 SRCNAME, __func__, name, hr); 
      return false;
    }
  return true;
}



/* Return the filename from the attachment as a malloced string.  The
   encoding we return will be utf8, however the MAPI docs declare that
   MAPI does only handle plain ANSI and thus we don't really care
   later on.  In fact we would need to convert the filename back to
   wchar and use the Unicode versions of the file API.  Returns NULL
   on error or if no filename is available. */
static char *
get_attach_filename (LPATTACH obj)
{
  HRESULT hr;
  LPSPropValue propval;
  char *name = NULL;

  hr = HrGetOneProp ((LPMAPIPROP)obj, PR_ATTACH_LONG_FILENAME, &propval);
  if (FAILED(hr)) 
    hr = HrGetOneProp ((LPMAPIPROP)obj, PR_ATTACH_FILENAME, &propval);
  if (FAILED(hr))
    {
      log_debug ("%s:%s: no filename property found", SRCNAME, __func__);
      return NULL;
    }

  switch ( PROP_TYPE (propval->ulPropTag) )
    {
    case PT_UNICODE:
      name = wchar_to_utf8 (propval->Value.lpszW);
      if (!name)
        log_debug ("%s:%s: error converting to utf8\n", SRCNAME, __func__);
      break;
      
    case PT_STRING8:
      name = xstrdup (propval->Value.lpszA);
      break;
      
    default:
      log_debug ("%s:%s: proptag=%#lx not supported\n",
                 SRCNAME, __func__, propval->ulPropTag);
      name = NULL;
      break;
    }
  MAPIFreeBuffer (propval);
  return name;
}




/* Read the attachment ATT and try to detect whether this is a PGP
   Armored message.  METHOD is the attach method of ATT.  Returns 0 if
   it is not a PGP attachment. */
static armor_t
get_pgp_armor_type (LPATTACH att, int method)
{
  HRESULT hr;
  LPSTREAM stream;
  char buffer [128];
  ULONG nread;
  const char *s;

  if (method != ATTACH_BY_VALUE)
    return ARMOR_NONE;
  
  hr = att->OpenProperty (PR_ATTACH_DATA_BIN, &IID_IStream, 
                          0, 0, (LPUNKNOWN*) &stream);
  if (FAILED (hr))
    {
      log_debug ("%s:%s: can't attachment data: hr=%#lx",
                 SRCNAME, __func__,  hr);
      return ARMOR_NONE;
    }

  hr = stream->Read (buffer, sizeof buffer -1, &nread);
  if ( hr != S_OK )
    {
      log_debug ("%s:%s: Read failed: hr=%#lx", SRCNAME, __func__, hr);
      stream->Release ();
      return ARMOR_NONE;
    }
  buffer[nread] = 0;
  stream->Release ();

  s = strstr (buffer, "-----BEGIN PGP ");
  if (!s)
    return ARMOR_NONE;
  s += 15;
  if (!strncmp (s, "MESSAGE-----", 12))
    return ARMOR_MESSAGE;
  else if (!strncmp (s, "SIGNATURE-----", 14))
    return ARMOR_SIGNATURE;
  else if (!strncmp (s, "SIGNED MESSAGE-----", 19))
    return ARMOR_SIGNED;
  else if (!strncmp (s, "ARMORED FILE-----", 17))
    return ARMOR_FILE;
  else if (!strncmp (s, "PUBLIC KEY BLOCK-----", 21))
    return ARMOR_PUBKEY;
  else if (!strncmp (s, "PRIVATE KEY BLOCK-----", 22))
    return ARMOR_SECKEY;
  else if (!strncmp (s, "SECRET KEY BLOCK-----", 21))
    return ARMOR_SECKEY;
  else
    return ARMOR_NONE;
}


/* Gather information about attachments and return a new object with
   these information.  Caller must release the returned information.
   The routine will return NULL in case of an error or if no
   attachments are available. */
attach_info_t
GpgMsgImpl::gatherAttachmentInfo (void)
{    
  HRESULT hr;
  attach_info_t table;
  unsigned int pos, n_attach;
  const char *s;
  unsigned int attestation_count = 0;
  unsigned int invalid_count = 0;

  is_pgpmime_enc = false;
  has_attestation = false;
  n_attach = getAttachments ();
  log_debug ("%s:%s: message has %u attachments\n",
             SRCNAME, __func__, n_attach);
  if (!n_attach)
      return NULL;

  table = (attach_info_t)xcalloc (n_attach+1, sizeof *table);
  for (pos=0; pos < n_attach; pos++) 
    {
      LPATTACH att;

      hr = message->OpenAttach (pos, NULL, MAPI_BEST_ACCESS, &att);	
      if (FAILED (hr))
        {
          log_error ("%s:%s: can't open attachment %d: hr=%#lx",
                     SRCNAME, __func__, pos, hr);
          table[pos].invalid = 1;
          invalid_count++;
          continue;
        }

      table[pos].method = get_attach_method (att);
      table[pos].filename = get_attach_filename (att);
      table[pos].content_type = get_attach_mime_tag (att);
      if (table[pos].content_type)
        {
          char *p = strchr (table[pos].content_type, ';');
          if (p)
            {
              *p++ = 0;
              trim_trailing_spaces (table[pos].content_type);
              while (strchr (" \t\r\n", *p))
                p++;
              trim_trailing_spaces (p);
              table[pos].content_type_parms = p;
            }
          if (!stricmp (table[pos].content_type, "text/plain")
              && table[pos].filename 
              && (s = strrchr (table[pos].filename, '.'))
              && !stricmp (s, ".asc"))
            table[pos].armor_type = get_pgp_armor_type (att,table[pos].method);
        }
      if (table[pos].filename
          && !stricmp (table[pos].filename, "GPGol-Attestation.txt")
          && table[pos].content_type
          && !stricmp (table[pos].content_type, "text/plain"))
        {
          has_attestation = true;
          attestation_count++;
        }

      att->Release ();
    }
  table[pos].end_of_table = 1;

  /* Figure out whether there are encrypted attachments. */
  for (pos=0; !table[pos].end_of_table; pos++)
    {
      if (table[pos].invalid)
        continue;
      if (table[pos].armor_type == ARMOR_MESSAGE)
        table[pos].is_encrypted = 1;
      else if (table[pos].filename && (s = strrchr (table[pos].filename, '.'))
               &&  (!stricmp (s, ".pgp") || !stricmp (s, ".gpg")))
        table[pos].is_encrypted = 1;
      else if (table[pos].content_type  
               && ( !stricmp (table[pos].content_type,
                              "application/pgp-encrypted")
                   || (!stricmp (table[pos].content_type,
                                 "multipart/encrypted")
                       && table[pos].content_type_parms
                       && strstr (table[pos].content_type_parms,
                                  "application/pgp-encrypted"))
                   || (!stricmp (table[pos].content_type,
                                 "application/pgp")
                       && table[pos].content_type_parms
                       && strstr (table[pos].content_type_parms,
                                  "x-action=encrypt"))))
        table[pos].is_encrypted = 1;
    }
     
  /* Figure out what attachments are signed. */
  for (pos=0; !table[pos].end_of_table; pos++)
    {
      if (table[pos].invalid)
        continue;
      if (table[pos].filename && (s = strrchr (table[pos].filename, '.'))
          && !stricmp (s, ".asc")
          && table[pos].content_type  
          && !stricmp (table[pos].content_type, "application/pgp-signature"))
        {
          size_t len = (s - table[pos].filename);

          /* We mark the actual file, assuming that the .asc is a
             detached signature.  To correlate the data file and the
             signature we keep track of the POS. */
          for (unsigned int i=0; !table[i].end_of_table; i++)
            {
              if (table[i].invalid)
                continue;
              if (i != pos && table[i].filename 
                  && strlen (table[i].filename) == len
                  && !strncmp (table[i].filename, table[pos].filename, len))
                {
                  table[i].is_signed = 1;
                  table[i].sig_pos = pos;
                }
            }
          
        }
      else if (table[pos].content_type  
               && (!stricmp (table[pos].content_type, "application/pgp")
                   && table[pos].content_type_parms
                   && strstr (table[pos].content_type_parms,"x-action=sign")))
        table[pos].is_signed = 1;
    }

  log_debug ("%s:%s: attachment info:\n", SRCNAME, __func__);
  for (pos=0; !table[pos].end_of_table; pos++)
    {
      if (table[pos].invalid)
        continue;
      log_debug ("\t%d %d %d %u %d `%s' `%s' `%s'\n",
                 pos, table[pos].is_encrypted,
                 table[pos].is_signed, table[pos].sig_pos,
                 table[pos].armor_type,
                 table[pos].filename, table[pos].content_type,
                 table[pos].content_type_parms);
    }

  /* Simple check whether this is PGP/MIME encrypted.  At least with
     OL2003 the content-type of the body is also correctly set but we
     don't make use of this as it is not clear whether this is true
     for other storage providers.  We use a hack to ignore extra
     attestation attachments: Those are assumed to come after the
     both PGP/MIME parts. */
  if (opt.compat.no_pgpmime)
    ;
  else if (pos == 2 + attestation_count + invalid_count
           && table[0].content_type && table[1].content_type
           && !stricmp (table[0].content_type, "application/pgp-encrypted")
           && !stricmp (table[1].content_type, "application/octet-stream")
           && isPgpmimeVersionPart (0))
    {
      log_debug ("\tThis is a PGP/MIME encrypted message - table adjusted");
      table[0].is_encrypted = 0;
      table[1].is_encrypted = 1;
      is_pgpmime_enc = true;
    }

  return table;
}




/* Verify the attachment as recorded in TABLE and at table position
   POS_DATA against the signature at position POS_SIG.  Display the
   status for each signature. */
void
GpgMsgImpl::verifyAttachment (HWND hwnd, attach_info_t table,
                              unsigned int pos_data,
                              unsigned int pos_sig)

{    
  HRESULT hr;
  LPATTACH att;
  int err;
  char *sig_data;

  log_debug ("%s:%s: verifying attachment %d/%d",
             SRCNAME, __func__, pos_data, pos_sig);

  assert (table);
  assert (message);

  /* First we copy the actual signature into a memory buffer.  Such a
     signature is expected to be small enough to be readable directly
     (i.e.less that 16k as suggested by the MS MAPI docs). */
  hr = message->OpenAttach (pos_sig, NULL, MAPI_BEST_ACCESS, &att);	
  if (FAILED (hr))
    {
      log_error ("%s:%s: can't open attachment %d (sig): hr=%#lx",
                 SRCNAME, __func__, pos_sig, hr);
      return;
    }

  if ( table[pos_sig].method == ATTACH_BY_VALUE )
    sig_data = get_short_attach_data (att);
  else
    {
      log_error ("%s:%s: attachment %d (sig): method %d not supported",
                 SRCNAME, __func__, pos_sig, table[pos_sig].method);
      att->Release ();
      return;
    }
  att->Release ();
  if (!sig_data)
    return; /* Problem getting signature; error has already been
               logged. */

  /* Now get on with the actual signed data. */
  hr = message->OpenAttach (pos_data, NULL, MAPI_BEST_ACCESS, &att);	
  if (FAILED (hr))
    {
      log_error ("%s:%s: can't open attachment %d (data): hr=%#lx",
                 SRCNAME, __func__, pos_data, hr);
      xfree (sig_data);
      return;
    }

  if ( table[pos_data].method == ATTACH_BY_VALUE )
    {
      LPSTREAM stream;

      hr = att->OpenProperty (PR_ATTACH_DATA_BIN, &IID_IStream, 
                              0, 0, (LPUNKNOWN*) &stream);
      if (FAILED (hr))
        {
          log_error ("%s:%s: can't open data of attachment %d: hr=%#lx",
                     SRCNAME, __func__, pos_data, hr);
          goto leave;
        }
      err = op_verify_detached_sig (stream, sig_data,
                                    table[pos_data].filename, attestation);
      if (err)
        {
          log_debug ("%s:%s: verify detached signature failed: %s",
                     SRCNAME, __func__, op_strerror (err)); 
          MessageBox (hwnd, op_strerror (err),
                      _("Attachment Verification Failure"),
                      MB_ICONERROR|MB_OK);
        }
      stream->Release ();
    }
  else
    {
      log_error ("%s:%s: attachment %d (data): method %d not supported",
                 SRCNAME, __func__, pos_data, table[pos_data].method);
    }

 leave:
  /* Close this attachment. */
  xfree (sig_data);
  att->Release ();
}


/* Decrypt the attachment with the internal number POS.
   SAVE_PLAINTEXT must be true to save the attachemnt; displaying a
   attachment is not yet supported.  If FILENAME is not NULL it will
   be displayed along with status outputs. */
void
GpgMsgImpl::decryptAttachment (HWND hwnd, int pos, bool save_plaintext,
                               int ttl, const char *filename)
{    
  HRESULT hr;
  LPATTACH att;
  int method, err;
  LPATTACH newatt = NULL;
  char *outname = NULL;
  

  log_debug ("%s:%s: processing attachment %d", SRCNAME, __func__, pos);

  /* Make sure that we can access the attachment table. */
  if (!message || !getAttachments ())
    {
      log_debug ("%s:%s: no attachemnts at all", SRCNAME, __func__);
      return;
    }

  if (!save_plaintext)
    {
      log_error ("%s:%s: save_plaintext not requested", SRCNAME, __func__);
      return;
    }

  hr = message->OpenAttach (pos, NULL, MAPI_BEST_ACCESS, &att);	
  if (FAILED (hr))
    {
      log_debug ("%s:%s: can't open attachment %d: hr=%#lx",
                 SRCNAME, __func__, pos, hr);
      return;
    }

  method = get_attach_method (att);
  if ( method == ATTACH_EMBEDDED_MSG)
    {
      /* This is an embedded message.  The orginal G-DATA plugin
         decrypted the message and then updated the attachemnt;
         i.e. stored the plaintext.  This seemed to ensure that the
         attachemnt message was properly displayed.  I am not sure
         what we should do - it might be necessary to have a callback
         to allow displaying the attachment.  Needs further
         experiments. */
      LPMESSAGE emb;
      
      hr = att->OpenProperty (PR_ATTACH_DATA_OBJ, &IID_IMessage, 0, 
                              MAPI_MODIFY, (LPUNKNOWN*)&emb);
      if (FAILED (hr))
        {
          log_error ("%s:%s: can't open data obj of attachment %d: hr=%#lx",
                     SRCNAME, __func__, pos, hr);
          goto leave;
        }

      //FIXME  Not sure what to do here.  Did it ever work?
      // 	setWindow (hwnd);
      // 	setMessage (emb);
      //if (doCmdAttach (action))
      //  success = FALSE;
      //XXX;
      //emb->SaveChanges (FORCE_SAVE);
      //att->SaveChanges (FORCE_SAVE);
      emb->Release ();
    }
  else if (method == ATTACH_BY_VALUE)
    {
      char *s;
      char *suggested_name;
      LPSTREAM from, to;

      suggested_name = get_attach_filename (att);
      if (suggested_name)
        log_debug ("%s:%s: attachment %d, filename `%s'", 
                   SRCNAME, __func__, pos, suggested_name);
      /* Strip of know extensions or use a default name. */
      if (!suggested_name)
        {
          xfree (suggested_name);
          suggested_name = (char*)xmalloc (50);
          snprintf (suggested_name, 49, "unnamed-%d.dat", pos);
        }
      else if ((s = strrchr (suggested_name, '.'))
               && (!stricmp (s, ".pgp") 
                   || !stricmp (s, ".gpg") 
                   || !stricmp (s, ".asc")) )
        {
          *s = 0;
        }
      if (opt.save_decrypted_attach)
        outname = suggested_name;
      else
        {
          outname = get_save_filename (hwnd, suggested_name);
          xfree (suggested_name);
        }
      
      hr = att->OpenProperty (PR_ATTACH_DATA_BIN, &IID_IStream, 
                              0, 0, (LPUNKNOWN*) &from);
      if (FAILED (hr))
        {
          log_error ("%s:%s: can't open data of attachment %d: hr=%#lx",
                     SRCNAME, __func__, pos, hr);
          goto leave;
        }


      if (opt.save_decrypted_attach) /* Decrypt and save in the MAPI. */
        {
          ULONG newpos;
          SPropValue prop;

          hr = message->CreateAttach (NULL, 0, &newpos, &newatt);
          if (hr != S_OK)
            {
              log_error ("%s:%s: can't create attachment: hr=%#lx\n",
                         SRCNAME, __func__, hr); 
              goto leave;
            }
          
          prop.ulPropTag = PR_ATTACH_METHOD;
          prop.Value.ul = ATTACH_BY_VALUE;
          hr = HrSetOneProp (newatt, &prop);
          if (hr != S_OK)
            {
              log_error ("%s:%s: can't set attach method: hr=%#lx\n",
                         SRCNAME, __func__, hr); 
              goto leave;
            }
          
          prop.ulPropTag = PR_ATTACH_LONG_FILENAME_A;
          prop.Value.lpszA = outname;   
          hr = HrSetOneProp (newatt, &prop);
          if (hr != S_OK)
            {
              log_error ("%s:%s: can't set attach filename: hr=%#lx\n",
                         SRCNAME, __func__, hr); 
              goto leave;
            }
          log_debug ("%s:%s: setting filename of attachment %d/%ld to `%s'",
                     SRCNAME, __func__, pos, newpos, outname);
          

          hr = newatt->OpenProperty (PR_ATTACH_DATA_BIN, &IID_IStream, 0,
                                     MAPI_CREATE|MAPI_MODIFY, (LPUNKNOWN*)&to);
          if (FAILED (hr)) 
            {
              log_error ("%s:%s: can't create output stream: hr=%#lx\n",
                         SRCNAME, __func__, hr); 
              goto leave;
            }
      
          err = op_decrypt_stream (from, to, ttl, filename, attestation);
          if (err)
            {
              log_debug ("%s:%s: decrypt stream failed: %s",
                         SRCNAME, __func__, op_strerror (err)); 
              to->Revert ();
              to->Release ();
              from->Release ();
              MessageBox (hwnd, op_strerror (err),
                          _("Attachment Decryption Failure"),
                          MB_ICONERROR|MB_OK);
              goto leave;
            }
        
          to->Commit (0);
          to->Release ();
          from->Release ();

          hr = newatt->SaveChanges (0);
          if (hr != S_OK)
            {
              log_error ("%s:%s: SaveChanges failed: hr=%#lx\n",
                         SRCNAME, __func__, hr); 
              goto leave;
            }

          /* Delete the orginal attachment. FIXME: Should we really do
             that or better just mark it in the table and delete
             later? */
          att->Release ();
          att = NULL;
          if (message->DeleteAttach (pos, 0, NULL, 0) == S_OK)
            log_error ("%s:%s: failed to delete attachment %d: %s",
                       SRCNAME, __func__, pos, op_strerror (err)); 
          
        }
      else  /* Save attachment to a file. */
        {
          hr = OpenStreamOnFile (MAPIAllocateBuffer, MAPIFreeBuffer,
                                 (STGM_CREATE | STGM_READWRITE),
                                 outname, NULL, &to); 
          if (FAILED (hr)) 
            {
              log_error ("%s:%s: can't create stream for `%s': hr=%#lx\n",
                         SRCNAME, __func__, outname, hr); 
              from->Release ();
              goto leave;
            }
      
          err = op_decrypt_stream (from, to, ttl, filename, attestation);
          if (err)
            {
              log_debug ("%s:%s: decrypt stream failed: %s",
                         SRCNAME, __func__, op_strerror (err)); 
              to->Revert ();
              to->Release ();
              from->Release ();
              MessageBox (hwnd, op_strerror (err),
                          _("Attachment Decryption Failure"),
                          MB_ICONERROR|MB_OK);
              /* FIXME: We might need to delete outname now.  However a
                 sensible implementation of the stream object should have
                 done it through the Revert call. */
              goto leave;
            }
        
          to->Commit (0);
          to->Release ();
          from->Release ();
        }
      
    }
  else
    {
      log_error ("%s:%s: attachment %d: method %d not supported",
                 SRCNAME, __func__, pos, method);
    }

 leave:
  xfree (outname);
  if (newatt)
    newatt->Release ();
  if (att)
    att->Release ();
}


/* Sign the attachment with the internal number POS.  TTL is the caching
   time for a required passphrase. */
void
GpgMsgImpl::signAttachment (HWND hwnd, int pos, gpgme_key_t sign_key, int ttl)
{    
  HRESULT hr;
  LPATTACH att;
  int method, err;
  LPSTREAM from = NULL;
  LPSTREAM to = NULL;
  char *signame = NULL;
  LPATTACH newatt = NULL;

  /* Make sure that we can access the attachment table. */
  if (!message || !getAttachments ())
    {
      log_debug ("%s:%s: no attachemnts at all", SRCNAME, __func__);
      return;
    }

  hr = message->OpenAttach (pos, NULL, MAPI_BEST_ACCESS, &att);	
  if (FAILED (hr))
    {
      log_debug ("%s:%s: can't open attachment %d: hr=%#lx",
                 SRCNAME, __func__, pos, hr);
      return;
    }

  /* Construct a filename for the new attachment. */
  {
    char *tmpname = get_attach_filename (att);
    if (!tmpname)
      {
        signame = (char*)xmalloc (70);
        snprintf (signame, 70, "gpg-signature-%d.asc", pos);
      }
    else
      {
        signame = (char*)xmalloc (strlen (tmpname) + 4 + 1);
        strcpy (stpcpy (signame, tmpname), ".asc");
        xfree (tmpname);
      }
  }

  method = get_attach_method (att);
  if (method == ATTACH_EMBEDDED_MSG)
    {
      log_debug ("%s:%s: signing embedded attachments is not supported",
                 SRCNAME, __func__);
    }
  else if (method == ATTACH_BY_VALUE)
    {
      ULONG newpos;
      SPropValue prop;

      hr = att->OpenProperty (PR_ATTACH_DATA_BIN, &IID_IStream, 
                              0, 0, (LPUNKNOWN*)&from);
      if (FAILED (hr))
        {
          log_error ("%s:%s: can't open data of attachment %d: hr=%#lx",
                     SRCNAME, __func__, pos, hr);
          goto leave;
        }

      hr = message->CreateAttach (NULL, 0, &newpos, &newatt);
      if (hr != S_OK)
        {
          log_error ("%s:%s: can't create attachment: hr=%#lx\n",
                     SRCNAME, __func__, hr); 
          goto leave;
        }

      prop.ulPropTag = PR_ATTACH_METHOD;
      prop.Value.ul = ATTACH_BY_VALUE;
      hr = HrSetOneProp (newatt, &prop);
      if (hr != S_OK)
        {
          log_error ("%s:%s: can't set attach method: hr=%#lx\n",
                     SRCNAME, __func__, hr); 
          goto leave;
        }
      
      prop.ulPropTag = PR_ATTACH_LONG_FILENAME_A;
      prop.Value.lpszA = signame;   
      hr = HrSetOneProp (newatt, &prop);
      if (hr != S_OK)
        {
          log_error ("%s:%s: can't set attach filename: hr=%#lx\n",
                     SRCNAME, __func__, hr); 
          goto leave;
        }
      log_debug ("%s:%s: setting filename of attachment %d/%ld to `%s'",
                 SRCNAME, __func__, pos, newpos, signame);

      prop.ulPropTag = PR_ATTACH_EXTENSION_A;
      prop.Value.lpszA = ".pgpsig";   
      hr = HrSetOneProp (newatt, &prop);
      if (hr != S_OK)
        {
          log_error ("%s:%s: can't set attach extension: hr=%#lx\n",
                     SRCNAME, __func__, hr); 
          goto leave;
        }

      prop.ulPropTag = PR_ATTACH_TAG;
      prop.Value.bin.cb  = sizeof oid_mimetag;
      prop.Value.bin.lpb = (LPBYTE)oid_mimetag;
      hr = HrSetOneProp (newatt, &prop);
      if (hr != S_OK)
        {
          log_error ("%s:%s: can't set attach tag: hr=%#lx\n",
                     SRCNAME, __func__, hr); 
          goto leave;
        }

      prop.ulPropTag = PR_ATTACH_MIME_TAG_A;
      prop.Value.lpszA = "application/pgp-signature";
      hr = HrSetOneProp (newatt, &prop);
      if (hr != S_OK)
        {
          log_error ("%s:%s: can't set attach mime tag: hr=%#lx\n",
                     SRCNAME, __func__, hr); 
          goto leave;
        }

      hr = newatt->OpenProperty (PR_ATTACH_DATA_BIN, &IID_IStream, 0,
                                 MAPI_CREATE | MAPI_MODIFY, (LPUNKNOWN*)&to);
      if (FAILED (hr)) 
        {
          log_error ("%s:%s: can't create output stream: hr=%#lx\n",
                     SRCNAME, __func__, hr); 
          goto leave;
        }
      
      err = op_sign_stream (from, to, OP_SIG_DETACH, sign_key, ttl);
      if (err)
        {
          log_debug ("%s:%s: sign stream failed: %s",
                     SRCNAME, __func__, op_strerror (err)); 
          to->Revert ();
          MessageBox (hwnd, op_strerror (err),
                      _("Attachment Signing Failure"), MB_ICONERROR|MB_OK);
          goto leave;
        }
      from->Release ();
      from = NULL;
      to->Commit (0);
      to->Release ();
      to = NULL;

      hr = newatt->SaveChanges (0);
      if (hr != S_OK)
        {
          log_error ("%s:%s: SaveChanges failed: hr=%#lx\n",
                     SRCNAME, __func__, hr); 
          goto leave;
        }

    }
  else
    {
      log_error ("%s:%s: attachment %d: method %d not supported",
                 SRCNAME, __func__, pos, method);
    }

 leave:
  if (from)
    from->Release ();
  if (to)
    to->Release ();
  xfree (signame);
  if (newatt)
    newatt->Release ();

  att->Release ();
}

/* Encrypt the attachment with the internal number POS.  KEYS is a
   NULL terminates array with recipients to whom the message should be
   encrypted.  If SIGN_KEY is not NULL the attachment will also get
   signed. TTL is the passphrase caching time and only used if
   SIGN_KEY is not NULL. Returns 0 on success. */
int
GpgMsgImpl::encryptAttachment (HWND hwnd, int pos, gpgme_key_t *keys,
                               gpgme_key_t sign_key, int ttl)
{    
  HRESULT hr;
  LPATTACH att;
  int method, err;
  LPSTREAM from = NULL;
  LPSTREAM to = NULL;
  char *filename = NULL;
  LPATTACH newatt = NULL;

  /* Make sure that we can access the attachment table. */
  if (!message || !getAttachments ())
    {
      log_debug ("%s:%s: no attachemnts at all", SRCNAME, __func__);
      return 0;
    }

  hr = message->OpenAttach (pos, NULL, MAPI_BEST_ACCESS, &att);	
  if (FAILED (hr))
    {
      log_debug ("%s:%s: can't open attachment %d: hr=%#lx",
                 SRCNAME, __func__, pos, hr);
      err = gpg_error (GPG_ERR_GENERAL);
      return err;
    }

  /* Construct a filename for the new attachment. */
  {
    char *tmpname = get_attach_filename (att);
    if (!tmpname)
      {
        filename = (char*)xmalloc (70);
        snprintf (filename, 70, "gpg-encrypted-%d.pgp", pos);
      }
    else
      {
        filename = (char*)xmalloc (strlen (tmpname) + 4 + 1);
        strcpy (stpcpy (filename, tmpname), ".pgp");
        xfree (tmpname);
      }
  }

  method = get_attach_method (att);
  if (method == ATTACH_EMBEDDED_MSG)
    {
      log_debug ("%s:%s: encrypting embedded attachments is not supported",
                 SRCNAME, __func__);
      err = gpg_error (GPG_ERR_NOT_SUPPORTED);
    }
  else if (method == ATTACH_BY_VALUE)
    {
      ULONG newpos;
      SPropValue prop;

      hr = att->OpenProperty (PR_ATTACH_DATA_BIN, &IID_IStream, 
                              0, 0, (LPUNKNOWN*)&from);
      if (FAILED (hr))
        {
          log_error ("%s:%s: can't open data of attachment %d: hr=%#lx",
                     SRCNAME, __func__, pos, hr);
          err = gpg_error (GPG_ERR_GENERAL);
          goto leave;
        }

      hr = message->CreateAttach (NULL, 0, &newpos, &newatt);
      if (hr != S_OK)
        {
          log_error ("%s:%s: can't create attachment: hr=%#lx\n",
                     SRCNAME, __func__, hr); 
          err = gpg_error (GPG_ERR_GENERAL);
          goto leave;
        }

      prop.ulPropTag = PR_ATTACH_METHOD;
      prop.Value.ul = ATTACH_BY_VALUE;
      hr = HrSetOneProp (newatt, &prop);
      if (hr != S_OK)
        {
          log_error ("%s:%s: can't set attach method: hr=%#lx\n",
                     SRCNAME, __func__, hr); 
          err = gpg_error (GPG_ERR_GENERAL);
          goto leave;
        }
      
      prop.ulPropTag = PR_ATTACH_LONG_FILENAME_A;
      prop.Value.lpszA = filename;   
      hr = HrSetOneProp (newatt, &prop);
      if (hr != S_OK)
        {
          log_error ("%s:%s: can't set attach filename: hr=%#lx\n",
                     SRCNAME, __func__, hr); 
          err = gpg_error (GPG_ERR_GENERAL);
          goto leave;
        }
      log_debug ("%s:%s: setting filename of attachment %d/%ld to `%s'",
                 SRCNAME, __func__, pos, newpos, filename);

      prop.ulPropTag = PR_ATTACH_EXTENSION_A;
      prop.Value.lpszA = ".pgpenc";   
      hr = HrSetOneProp (newatt, &prop);
      if (hr != S_OK)
        {
          log_error ("%s:%s: can't set attach extension: hr=%#lx\n",
                     SRCNAME, __func__, hr); 
          err = gpg_error (GPG_ERR_GENERAL);
          goto leave;
        }

      prop.ulPropTag = PR_ATTACH_TAG;
      prop.Value.bin.cb  = sizeof oid_mimetag;
      prop.Value.bin.lpb = (LPBYTE)oid_mimetag;
      hr = HrSetOneProp (newatt, &prop);
      if (hr != S_OK)
        {
          log_error ("%s:%s: can't set attach tag: hr=%#lx\n",
                     SRCNAME, __func__, hr); 
          err = gpg_error (GPG_ERR_GENERAL);
          goto leave;
        }

      prop.ulPropTag = PR_ATTACH_MIME_TAG_A;
      prop.Value.lpszA = "application/pgp-encrypted";
      hr = HrSetOneProp (newatt, &prop);
      if (hr != S_OK)
        {
          log_error ("%s:%s: can't set attach mime tag: hr=%#lx\n",
                     SRCNAME, __func__, hr); 
          err = gpg_error (GPG_ERR_GENERAL);
          goto leave;
        }

      hr = newatt->OpenProperty (PR_ATTACH_DATA_BIN, &IID_IStream, 0,
                                 MAPI_CREATE | MAPI_MODIFY, (LPUNKNOWN*)&to);
      if (FAILED (hr)) 
        {
          log_error ("%s:%s: can't create output stream: hr=%#lx\n",
                     SRCNAME, __func__, hr); 
          err = gpg_error (GPG_ERR_GENERAL);
          goto leave;
        }
      
      err = op_encrypt_stream (from, to, keys, sign_key, ttl);
      if (err)
        {
          log_debug ("%s:%s: encrypt stream failed: %s",
                     SRCNAME, __func__, op_strerror (err)); 
          to->Revert ();
          MessageBox (hwnd, op_strerror (err),
                      _("Attachment Encryption Failure"), MB_ICONERROR|MB_OK);
          goto leave;
        }
      from->Release ();
      from = NULL;
      to->Commit (0);
      to->Release ();
      to = NULL;

      hr = newatt->SaveChanges (0);
      if (hr != S_OK)
        {
          log_error ("%s:%s: SaveChanges failed: hr=%#lx\n",
                     SRCNAME, __func__, hr); 
          err = gpg_error (GPG_ERR_GENERAL);
          goto leave;
        }

      hr = message->DeleteAttach (pos, 0, NULL, 0);
      if (hr != S_OK)
        {
          log_error ("%s:%s: DeleteAtatch failed: hr=%#lx\n",
                     SRCNAME, __func__, hr); 
          err = gpg_error (GPG_ERR_GENERAL);
          goto leave;
        }

    }
  else
    {
      log_error ("%s:%s: attachment %d: method %d not supported",
                 SRCNAME, __func__, pos, method);
      err = gpg_error (GPG_ERR_NOT_SUPPORTED);
    }

 leave:
  if (from)
    from->Release ();
  if (to)
    to->Release ();
  xfree (filename);
  if (newatt)
    newatt->Release ();

  att->Release ();
  return err;
}
