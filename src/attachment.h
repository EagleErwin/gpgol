/* attachment.h - Functions for attachment handling
 *    Copyright (C) 2005, 2007 g10 Code GmbH
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
#ifndef ATTACHMENT_H
#define ATTACHMENT_H

#include <windows.h>
#include "oomhelp.h"
#include "mapihelp.h"
#include <string>

/** Helper class for attachment actions. */
class Attachment
{
public:
  /** Creates and opens a new temporary stream. */
  Attachment();

  /** Deletes the attachment and the underlying temporary file. */
  ~Attachment();

  /** Get an assoicated ISteam ptr or NULL. */
  LPSTREAM get_stream();

  /** Writes data to the attachment stream.
   * Calling this method automatically commits the stream.
   *
   * Returns 0 on success. */
  int write(const char *data, size_t size);

  /** Set the display name */
  void set_display_name(const char *name);
  std::string get_display_name() const;

  std::string get_tmp_file_name() const;

  void set_attach_type(attachtype_t type);

  void set_hidden(bool value);
private:
  LPSTREAM m_stream;
  std::string m_utf8FileName;
  std::string m_utf8DisplayName;
  attachtype_t m_type;
  bool m_hidden;
};

#endif // ATTACHMENT_H
