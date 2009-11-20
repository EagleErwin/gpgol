/* oomhelp.h - Defs for helper functions for the Outlook Object Model
 *	Copyright (C) 2009 g10 Code GmbH
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

#ifndef OOMHELP_H
#define OOMHELP_H

#include <unknwn.h>


#define MSOCONTROLBUTTON    1
#define MSOCONTROLEDIT      2
#define MSOCONTROLDROPDOWN  3
#define MSOCONTROLCOMBOBOX  4
#define MSOCONTROLPOPUP    10

enum 
  {
    msoButtonAutomatic = 0,
    msoButtonIcon = 1,
    msoButtonCaption = 2,
    msoButtonIconAndCaption = 3,
    msoButtonIconAndWrapCaption = 7,
    msoButtonIconAndCaptionBelow = 11,
    msoButtonWrapCaption = 14,
    msoButtonIconAndWrapCaptionBelow = 15 
  };



DEFINE_GUID(GUID_NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

DEFINE_GUID(IID_IConnectionPoint, 
            0xb196b286, 0xbab4, 0x101a,
            0xb6, 0x9c, 0x00, 0xaa, 0x00, 0x34, 0x1d, 0x07);
DEFINE_GUID(IID_IConnectionPointContainer, 
            0xb196b284, 0xbab4, 0x101a,
            0xb6, 0x9c, 0x00, 0xaa, 0x00, 0x34, 0x1d, 0x07);
DEFINE_GUID(IID_IPictureDisp,
            0x7bf80981, 0xbf32, 0x101a,
            0x8b, 0xbb, 0x00, 0xaa, 0x00, 0x30, 0x0c, 0xab);

DEFINE_OLEGUID(IID_IUnknown,                  0x00000000, 0, 0);
DEFINE_OLEGUID(IID_IDispatch,                 0x00020400, 0, 0);


#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif

/* Return the malloced name of an COM+ object.  */
char *get_object_name (LPUNKNOWN obj);

/* Helper to lookup a dispid.  */
DISPID lookup_oom_dispid (LPDISPATCH pDisp, const char *name);

/* Return the OOM object's IDispatch interface described by FULLNAME.  */
LPDISPATCH get_oom_object (LPDISPATCH pStart, const char *fullname);

/* Set the Icon of a CommandBarControl.  */
int put_oom_icon (LPDISPATCH pDisp, int rsource_id, int size);

/* Set the boolean property NAME to VALUE.  */
int put_oom_bool (LPDISPATCH pDisp, const char *name, int value);

/* Set the property NAME to VALUE.  */
int put_oom_int (LPDISPATCH pDisp, const char *name, int value);

/* Set the property NAME to STRING.  */
int put_oom_string (LPDISPATCH pDisp, const char *name, const char *string);

/* Get the boolean property NAME of the object PDISP.  */
int get_oom_bool (LPDISPATCH pDisp, const char *name);

/* Get the string property NAME of the object PDISP.  */
char *get_oom_string (LPDISPATCH pDisp, const char *name);

/* Return the control object with tag property value TAG.  */
LPDISPATCH get_oom_control_bytag (LPDISPATCH pObj, const char *tag);

/* Add a new button to an object which supports the add method.
   Returns the new object or NULL on error.  */
LPDISPATCH add_oom_button (LPDISPATCH pObj);


#ifdef __cplusplus
}
#endif
#endif /*OOMHELP_H*/