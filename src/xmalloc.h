/* xmalloc.h - xmalloc prototypes
 *	Copyright (C) 2006 g10 Code GmbH
 *
 * This file is part of GPGol.
 * 
 * GPGol is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * GPGol is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef XMALLOC_H
#define XMALLOC_H

#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif

/*-- common.c --*/
void* xmalloc (size_t n);
void* xcalloc (size_t m, size_t n);
char* xstrdup (const char *s);
void  xfree (void *p);
void out_of_core (void);


#ifdef __cplusplus
}
#endif
#endif /*XMALLOC_H*/