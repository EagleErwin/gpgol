/* gpgmsg.hh - The GpgMsg class
 *	Copyright (C) 2005 g10 Code GmbH
 *
 * This file is part of OutlGPG.
 * 
 * OutlGPG is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * OutlGPG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#ifndef GPGMSG_HH
#define GPGMSG_HH

#include <gpgme.h>

/* Type of a message. */
typedef enum 
  {
    OPENPGP_NONE = 0,
    OPENPGP_MSG,
    OPENPGP_SIG,
    OPENPGP_CLEARSIG,
    OPENPGP_PUBKEY,   /* Note, that this type is only partly supported */
    OPENPGP_SECKEY    /* Note, that this type is only partly supported */
  }
openpgp_t;



/* To manage a message we use our own class to keep track about all
   the information we known on the content of a message.  This is
   useful to remember the state of conversion (sometimes we need to
   copy between utf8 and the native character set) and to parse the
   message down into the MIME structure. */

class GpgMsg
{
public:    
  virtual void destroy () = 0;
  void operator delete (void *p)
    {
      if (p)
        {
          GpgMsg *m = (GpgMsg*)(p);
          m->destroy();
        }
    }

  /* Set a new MAPI message into the object. */
  virtual void setMapiMessage (LPMESSAGE msg);

  /* Return the type of the message. */
  virtual openpgp_t getMessageType (void);

  /* Returns whether the message has any attachments. */
  virtual bool hasAttachments (void);

  /* Return the body text as received or composed.  This is guaranteed
     to never return NULL.  Usually getMessageType is used to check
     whether there is a suitable message. */
  virtual const char *getOrigText (void);

  /* Return the text of the message to be used for the display.  The
     message objects has intrinsic knowledge about the correct
     text.  */
  virtual const char *getDisplayText (void);

  /* Save STRING as the plaintext version of the message.  WARNING:
     ownership of STRING is transferred to this object. */
  virtual void setPlainText (char *string);

  /* Save STRING as the ciphertext version of the message.  WARNING:
     ownership of STRING is transferred to this object. */
  virtual void setCipherText (char *string, bool html);

  /* Save STRING as the signed version of the message.  WARNING:
     ownership of STRING is transferred to this object. */
  virtual void setSignedText (char *string);
  
  /* Save the changes made to the message.  With PERMANENT set to true
     they are really stored, when not set they are only saved
     temporary. */
  virtual void saveChanges (bool permanent);

  /* Return true if STRING matches the actual message. */ 
  virtual bool matchesString (const char *string);

  /* Return a malloced array of malloced strings with the recipients
     of the message. Caller is responsible for freeing this array and
     the strings.  On failure NULL is returned.  */
  virtual char **getRecipients (void);


  /* Encrypt the entire message including any attachments. Return 0 on
     success. */
  virtual int encrypt (HWND hwnd);

  virtual int sign (HWND hwnd);


  /* Return the number of attachments. */
  virtual unsigned int getAttachments (void);

  /* Decrypt the attachment with the internal number POS.
     SAVE_PLAINTEXT must be true to save the attachemnt; displaying a
     attachemnt is not yet supported. */
  virtual void decryptAttachment (HWND hwnd, int pos, bool save_plaintext,
                                  int ttl);

  virtual void signAttachment (HWND hwnd, int pos,
                               gpgme_key_t sign_key, int ttl);

  virtual int encryptAttachment (HWND hwnd, int pos, gpgme_key_t *keys,
                                 gpgme_key_t sign_key, int ttl);

};


/* Create a new instance and initialize with the MAPI message object
   MSG. */
GpgMsg *CreateGpgMsg (LPMESSAGE msg);

#endif /*GPGMSG_HH*/
