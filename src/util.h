/* util.h - Common functions.
 *	Copyright (C) 2005 g10 Code GmbH
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

#ifndef UTIL_H
#define UTIL_H

#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif

#if __GNUC__ >= 4 
# define GPGOL_GCC_A_SENTINEL(a) __attribute__ ((sentinel(a)))
#else
# define GPGOL_GCC_A_SENTINEL(a) 
#endif


/* To avoid that a compiler optimizes certain memset calls away, these
   macros may be used instead. */
#define wipememory2(_ptr,_set,_len) do { \
              volatile char *_vptr=(volatile char *)(_ptr); \
              size_t _vlen=(_len); \
              while(_vlen) { *_vptr=(_set); _vptr++; _vlen--; } \
                  } while(0)
#define wipememory(_ptr,_len) wipememory2(_ptr,0,_len)
#define wipestring(_ptr) do { \
              volatile char *_vptr=(volatile char *)(_ptr); \
              while(*_vptr) { *_vptr=0; _vptr++; } \
                  } while(0)

#include <windows.h>

/* i18n stuff */
#include "w32-gettext.h"
#define _(a) gettext (a)
#define N_(a) gettext_noop (a)


/*-- common.c --*/

#include "xmalloc.h"

void fatal_error (const char *format, ...);

char *wchar_to_utf8_2 (const wchar_t *string, size_t len);
wchar_t *utf8_to_wchar2 (const char *string, size_t len);
char *latin1_to_utf8 (const char *string);

char *mem2str (char *dest, const void *src, size_t n);

char *trim_spaces (char *string);
char *trim_trailing_spaces (char *string);
char *read_w32_registry_string (const char *root, const char *dir,
                                const char *name);
char *percent_escape (const char *str, const char *extra);

void fix_linebreaks (char *str, int *len);

/*-- main.c --*/
const void *get_128bit_session_key (void);
const void *get_64bit_session_marker (void);
void *create_initialization_vector (size_t nbytes);

#define debug_oom        (opt.enable_debug & DBG_OOM)
#define debug_oom_extra  (opt.enable_debug & DBG_OOM_EXTRA)
void log_debug (const char *fmt, ...) __attribute__ ((format (printf,1,2)));
void log_error (const char *fmt, ...) __attribute__ ((format (printf,1,2)));
void log_vdebug (const char *fmt, va_list a);
void log_debug_w32 (int w32err, const char *fmt,
                    ...) __attribute__ ((format (printf,2,3)));
void log_error_w32 (int w32err, const char *fmt,
                    ...) __attribute__ ((format (printf,2,3)));
void log_hexdump (const void *buf, size_t buflen, const char *fmt, 
                  ...)  __attribute__ ((format (printf,3,4)));
void log_window_hierarchy (HWND window, const char *fmt, 
                           ...) __attribute__ ((format (printf,2,3)));

#define log_oom if (opt.enable_debug & DBG_OOM) log_debug
#define log_oom_extra if (opt.enable_debug & DBG_OOM_EXTRA) log_debug
#define gpgol_release(X) \
{ \
  if (X && opt.enable_debug & DBG_OOM_EXTRA) \
    { \
      log_debug ("%s:%s: Object: %p released ref: %lu \n", \
                 SRCNAME, __func__, X, X->Release()); \
    } \
  else if (X) \
    { \
      X->Release(); \
    } \
}

const char *log_srcname (const char *s);
#define SRCNAME log_srcname (__FILE__)

#define TRACEPOINT log_debug ("%s:%s:%d: tracepoint\n", \
                              SRCNAME, __func__, __LINE__);

const char *get_log_file (void);
void set_log_file (const char *name);
void set_default_key (const char *name);
void read_options (void);
int write_options (void);

/*-- Convenience macros. -- */
#define DIM(v)		     (sizeof(v)/sizeof((v)[0]))
#define DIMof(type,member)   DIM(((type *)0)->member)

/*-- Macros to replace ctype ones to avoid locale problems. --*/
#define spacep(p)   (*(p) == ' ' || *(p) == '\t')
#define digitp(p)   (*(p) >= '0' && *(p) <= '9')
#define hexdigitp(a) (digitp (a)                     \
                      || (*(a) >= 'A' && *(a) <= 'F')  \
                      || (*(a) >= 'a' && *(a) <= 'f'))
  /* Note this isn't identical to a C locale isspace() without \f and
     \v, but works for the purposes used here. */
#define ascii_isspace(a) ((a)==' ' || (a)=='\n' || (a)=='\r' || (a)=='\t')

/* The atoi macros assume that the buffer has only valid digits. */
#define atoi_1(p)   (*(p) - '0' )
#define atoi_2(p)   ((atoi_1(p) * 10) + atoi_1((p)+1))
#define atoi_4(p)   ((atoi_2(p) * 100) + atoi_2((p)+2))
#define xtoi_1(p)   (*(p) <= '9'? (*(p)- '0'): \
                     *(p) <= 'F'? (*(p)-'A'+10):(*(p)-'a'+10))
#define xtoi_2(p)   ((xtoi_1(p) * 16) + xtoi_1((p)+1))
#define xtoi_4(p)   ((xtoi_2(p) * 256) + xtoi_2((p)+2))

#define tohex(n) ((n) < 10 ? ((n) + '0') : (((n) - 10) + 'A'))

#define tohex_lower(n) ((n) < 10 ? ((n) + '0') : (((n) - 10) + 'a'))
/***** Inline functions.  ****/

/* Return true if LINE consists only of white space (up to and
   including the LF). */
static inline int
trailing_ws_p (const char *line)
{
  for ( ; *line && *line != '\n'; line++)
    if (*line != ' ' && *line != '\t' && *line != '\r')
      return 0;
  return 1;
}

/* An strcmp variant with the compare ending at the end of B.  */
static inline int
tagcmp (const char *a, const char *b)
{
  return strncmp (a, b, strlen (b));
}


/*****  Missing functions.  ****/

#ifndef HAVE_STPCPY
static inline char *
_gpgol_stpcpy (char *a, const char *b)
{
  while (*b)
    *a++ = *b++;
  *a = 0;
  return a;
}
#define stpcpy(a,b) _gpgol_stpcpy ((a), (b))
#endif /*!HAVE_STPCPY*/

extern int g_ol_version_major;

#ifdef WIN64
#define SIZE_T_FORMAT "%I64u"
#else
#define SIZE_T_FORMAT "%u"
#endif


#ifdef __cplusplus
}
#endif
#endif /*UTIL_H*/
