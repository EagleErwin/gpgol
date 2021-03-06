/* mymapi.h - MAPI definitions required for GpgOL and Mingw32
 * Copyright (C) 1998 Justin Bradford
 * Copyright (C) 2000 Fran�ois Gouget
 * Copyright (C) 2005, 2007 g10 Code GmbH
 * Copyright (C) 2015 by Bundesamt für Sicherheit in der Informationstechnik
 * Software engineering by Intevation GmbH
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/* This header has been put together from the mingw32 and Wine
   headers.  The first were missing the MAPI definitions and the
   latter one is not compatible to the first one.  We only declare
   stuff we really need for this project.

   Revisions:
   2005-07-26  Initial version (wk at g10code).
   2005-08-14  Tweaked for use with myexchext.h.
   2007-07-19  Add IConverterSession.  Info taken from
                 http://blogs.msdn.com/stephen_griffin/archive/2007/06/22/
                 iconvertersession-do-you-converter-session.aspx
   2007-07-23  More IConverterSession features.
   2007-07-23  Add IMAPISession; taken from WINE.
   2007-07-24  Add IMsgStore, IMAPIContainer and IMAPIFolder taken from specs.
               Reorganized code.
   2008-08-01  Add IMAPIFormContainer taken from specs.
*/

#ifndef MAPI_H
#define MAPI_H

#include "mapierr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Some types */

#ifndef __LHANDLE
#define __LHANDLE
typedef unsigned long           LHANDLE, *LPLHANDLE;
#endif
#define lhSessionNull           ((LHANDLE)0)

typedef unsigned long           FLAGS;


typedef struct _MAPIUID
{
    BYTE ab[sizeof(GUID)];
} MAPIUID, *LPMAPIUID;


struct MapiFileDesc_s
{
  ULONG ulReserved;
  ULONG flFlags;
  ULONG nPosition;
  LPSTR lpszPathName;
  LPSTR lpszFileName;
  LPVOID lpFileType;
};
typedef struct MapiFileDesc_s MapiFileDesc;
typedef struct MapiFileDesc_s *lpMapiFileDesc;

struct MapiRecipDesc_s
{
  ULONG ulReserved;
  ULONG ulRecipClass;
  LPSTR lpszName;
  LPSTR lpszAddress;
  ULONG ulEIDSize;
  LPVOID lpEntryID;
};
typedef struct MapiRecipDesc_s MapiRecipDesc;
typedef struct MapiRecipDesc_s *lpMapiRecipDesc;

struct MapiMessage_s
{
  ULONG ulReserved;
  LPSTR lpszSubject;
  LPSTR lpszNoteText;
  LPSTR lpszMessageType;
  LPSTR lpszDateReceived;
  LPSTR lpszConversationID;
  FLAGS flFlags;
  lpMapiRecipDesc lpOriginator;
  ULONG nRecipCount;
  lpMapiRecipDesc lpRecips;
  ULONG nFileCount;
  lpMapiFileDesc lpFiles;
};
typedef struct MapiMessage_s MapiMessage;
typedef struct MapiMessage_s *lpMapiMessage;


/* Object types.  */
#define MAPI_STORE      0x00000001u
#define MAPI_ADDRBOOK   0x00000002u
#define MAPI_FOLDER     0x00000003u
#define MAPI_ABCONT     0x00000004u
#define MAPI_MESSAGE    0x00000005u
#define MAPI_MAILUSER   0x00000006u
#define MAPI_ATTACH     0x00000007u
#define MAPI_DISTLIST   0x00000008u
#define MAPI_PROFSECT   0x00000009u
#define MAPI_STATUS     0x0000000Au
#define MAPI_SESSION    0x0000000Bu
#define MAPI_FORMINFO   0x0000000Cu


/* MAPILogon */
#define MAPI_LOGON_UI           0x00000001
#define MAPI_PASSWORD_UI        0x00020000
#define MAPI_NEW_SESSION        0x00000002
#define MAPI_FORCE_DOWNLOAD     0x00001000
#define MAPI_EXTENDED           0x00000020


/* MAPISendMail */
#define MAPI_DIALOG             0x00000008


/* Flags for various calls */
#define MAPI_MODIFY                   0x00000001U /* Object can be modified. */
#define MAPI_CREATE                   0x00000002U
#define MAPI_ACCESS_MODIFY            MAPI_MODIFY /* Want write access. */
#define MAPI_ACCESS_READ              0x00000002U /* Want read access. */
#define MAPI_ACCESS_DELETE            0x00000004U /* Want delete access. */
#define MAPI_ACCESS_CREATE_HIERARCHY  0x00000008U
#define MAPI_ACCESS_CREATE_CONTENTS   0x00000010U
#define MAPI_ACCESS_CREATE_ASSOCIATED 0x00000020U
#define MAPI_UNICODE                  0x80000000U /* Strings in this
                                                     call are Unicode. */

#define MAPI_DEFERRED_ERRORS          0x00000008ul
#define MAPI_BEST_ACCESS              0x00000010ul
#define MAPI_CACHE_ONLY               0x00004000ul

#define MDB_NO_DIALOG                 0x00000001ul
#define MDB_WRITE                     0x00000004ul
#define MDB_TEMPORARY                 0x00000020ul
#define MDB_NO_MAIL                   0x00000080ul


#define ATTACH_BY_VALUE       1
#define ATTACH_BY_REFERENCE   2
#define ATTACH_BY_REF_RESOLVE 3
#define ATTACH_BY_REF_ONLY    4
#define ATTACH_EMBEDDED_MSG   5
#define ATTACH_OLE            6


#define KEEP_OPEN_READONLY            0x00000001
#define KEEP_OPEN_READWRITE           0x00000002
#define FORCE_SAVE                    0x00000004

#define SOF_UNIQUEFILENAME            0x80000000

#define RTF_SYNC_RTF_CHANGED          1
#define RTF_SYNC_BODY_CHANGED         2


#define FORCE_SUBMIT                  0x00000001ul

#define MSGFLAG_READ                  0x00000001ul
#define MSGFLAG_UNMODIFIED            0x00000002ul
#define MSGFLAG_SUBMIT                0x00000004ul
#define MSGFLAG_UNSENT                0x00000008ul
#define MSGFLAG_HASATTACH             0x00000010ul
#define MSGFLAG_FROMME                0x00000020ul
#define MSGFLAG_ASSOCIATED            0x00000040ul
#define MSGFLAG_RESEND                0x00000080ul
#define MSGFLAG_RN_PENDING            0x00000100ul
#define MSGFLAG_NRN_PENDING           0x00000200ul

#define SUBMITFLAG_LOCKED             0x00000001ul
#define SUBMITFLAG_PREPROCESS         0x00000002ul

#define HOOK_DELETE                   0x00000001ul
#define HOOK_CANCEL                   0x00000002ul

#define HOOK_INBOUND                  0x00000200ul
#define HOOK_OUTBOUND                 0x00000400ul


#ifndef MAPI_DIM
# define MAPI_DIM 1 /* Default to one dimension for variable length arrays */
#endif

DEFINE_OLEGUID(IID_IABContainer,0x2030D,0,0);
DEFINE_OLEGUID(IID_IABLogon,0x20314,0,0);
DEFINE_OLEGUID(IID_IABProvider,0x20311,0,0);
DEFINE_OLEGUID(IID_IAddrBook,0x20309,0,0);
DEFINE_OLEGUID(IID_IAttachment,0x20308,0,0);
DEFINE_OLEGUID(IID_IDistList,0x2030E,0,0);
DEFINE_OLEGUID(IID_IEnumMAPIFormProp,0x20323,0,0);
DEFINE_OLEGUID(IID_IMailUser,0x2030A,0,0);
DEFINE_OLEGUID(IID_IMAPIAdviseSink,0x20302,0,0);
DEFINE_OLEGUID(IID_IMAPIContainer,0x2030B,0,0);
DEFINE_OLEGUID(IID_IMAPIControl,0x2031B,0,0);
DEFINE_OLEGUID(IID_IMAPIFolder,0x2030C,0,0);
DEFINE_OLEGUID(IID_IMAPIForm,0x20327,0,0);
DEFINE_OLEGUID(IID_IMAPIFormAdviseSink,0x2032F,0,0);
DEFINE_OLEGUID(IID_IMAPIFormContainer,0x2032E,0,0);
DEFINE_OLEGUID(IID_IMAPIFormFactory,0x20350,0,0);
DEFINE_OLEGUID(IID_IMAPIFormInfo,0x20324,0,0);
DEFINE_OLEGUID(IID_IMAPIFormMgr,0x20322,0,0);
DEFINE_OLEGUID(IID_IMAPIFormProp,0x2032D,0,0);
DEFINE_OLEGUID(IID_IMAPIMessageSite,0x20370,0,0);
DEFINE_OLEGUID(IID_IMAPIProgress,0x2031F,0,0);
DEFINE_OLEGUID(IID_IMAPIProp,0x20303,0,0);
DEFINE_OLEGUID(IID_IMAPIPropData,0x2031A,0,0);
DEFINE_OLEGUID(IID_IMAPISession,0x20300,0,0);
DEFINE_OLEGUID(IID_IMAPISpoolerInit,0x20317,0,0);
DEFINE_OLEGUID(IID_IMAPISpoolerService,0x2031E,0,0);
DEFINE_OLEGUID(IID_IMAPISpoolerSession,0x20318,0,0);
DEFINE_OLEGUID(IID_IMAPIStatus,0x20305,0,0);
DEFINE_OLEGUID(IID_IMAPISup,0x2030F,0,0);
DEFINE_OLEGUID(IID_IMAPITable,0x20301,0,0);
DEFINE_OLEGUID(IID_IMAPITableData,0x20316,0,0);
DEFINE_OLEGUID(IID_IMAPIViewAdviseSink,0x2032B,0,0);
DEFINE_OLEGUID(IID_IMAPIViewContext,0x20321,0,0);
DEFINE_OLEGUID(IID_IMessage,0x20307,0,0);
DEFINE_OLEGUID(IID_IMsgServiceAdmin,0x2031D,0,0);
DEFINE_OLEGUID(IID_IMsgStore,0x20306,0,0);
DEFINE_OLEGUID(IID_IMSLogon,0x20313,0,0);
DEFINE_OLEGUID(IID_IMSProvider,0x20310,0,0);
DEFINE_OLEGUID(IID_IPersistMessage,0x2032A,0,0);
DEFINE_OLEGUID(IID_IProfAdmin,0x2031C,0,0);
DEFINE_OLEGUID(IID_IProfSect,0x20304,0,0);
DEFINE_OLEGUID(IID_IProviderAdmin,0x20325,0,0);
DEFINE_OLEGUID(IID_ISpoolerHook,0x20320,0,0);
DEFINE_OLEGUID(IID_IStream, 0x0000c, 0, 0);
DEFINE_OLEGUID(IID_IStreamDocfile,0x2032C,0,0);
DEFINE_OLEGUID(IID_IStreamTnef,0x20330,0,0);
DEFINE_OLEGUID(IID_ITNEF,0x20319,0,0);
DEFINE_OLEGUID(IID_IXPLogon,0x20315,0,0);
DEFINE_OLEGUID(IID_IXPProvider,0x20312,0,0);
DEFINE_OLEGUID(MUID_PROFILE_INSTANCE,0x20385,0,0);
DEFINE_OLEGUID(PS_MAPI,0x20328,0,0);
DEFINE_OLEGUID(PS_PUBLIC_STRINGS,0x20329,0,0);
DEFINE_OLEGUID(PS_ROUTING_ADDRTYPE,0x20381,0,0);
DEFINE_OLEGUID(PS_ROUTING_DISPLAY_NAME,0x20382,0,0);
DEFINE_OLEGUID(PS_ROUTING_EMAIL_ADDRESSES,0x20380,0,0);
DEFINE_OLEGUID(PS_ROUTING_ENTRYID,0x20383,0,0);
DEFINE_OLEGUID(PS_ROUTING_SEARCH_KEY,0x20384,0,0);

DEFINE_GUID(CLSID_IConverterSession, 0x4e3a7680, 0xb77a,
            0x11d0, 0x9d, 0xa5, 0x0, 0xc0, 0x4f, 0xd6, 0x56, 0x85);
DEFINE_GUID(IID_IConverterSession, 0x4b401570, 0xb77b,
            0x11d0, 0x9d, 0xa5, 0x0, 0xc0, 0x4f, 0xd6, 0x56, 0x85);



struct _ENTRYID
{
    BYTE abFlags[4];
    BYTE ab[MAPI_DIM];
};
typedef struct _ENTRYID ENTRYID;
typedef struct _ENTRYID *LPENTRYID;


/* The property tag structure. This describes a list of columns */
typedef struct _SPropTagArray
{
    ULONG cValues;              /* Number of elements in aulPropTag */
    ULONG aulPropTag[MAPI_DIM]; /* Property tags */
} SPropTagArray, *LPSPropTagArray;

#define CbNewSPropTagArray(c) \
               (offsetof(SPropTagArray,aulPropTag)+(c)*sizeof(ULONG))
#define CbSPropTagArray(p)    CbNewSPropTagArray((p)->cValues)
#define SizedSPropTagArray(n,id) \
    struct _SPropTagArray_##id { ULONG cValues; ULONG aulPropTag[n]; } id


/* Multi-valued PT_APPTIME property value */
typedef struct _SAppTimeArray
{
    ULONG   cValues; /* Number of doubles in lpat */
    double *lpat;    /* Pointer to double array of length cValues */
} SAppTimeArray;

/* PT_BINARY property value */
typedef struct _SBinary
{
    ULONG  cb;  /* Number of bytes in lpb */
    LPBYTE lpb; /* Pointer to byte array of length cb */
} SBinary, *LPSBinary;

/* Multi-valued PT_BINARY property value */
typedef struct _SBinaryArray
{
    ULONG    cValues; /* Number of SBinarys in lpbin */
    SBinary *lpbin;   /* Pointer to SBinary array of length cValues */
} SBinaryArray;

typedef SBinaryArray ENTRYLIST, *LPENTRYLIST;

/* Multi-valued PT_CY property value */
typedef struct _SCurrencyArray
{
    ULONG  cValues; /* Number of CYs in lpcu */
    CY    *lpcur;   /* Pointer to CY array of length cValues */
} SCurrencyArray;

/* Multi-valued PT_SYSTIME property value */
typedef struct _SDateTimeArray
{
    ULONG     cValues; /* Number of FILETIMEs in lpft */
    FILETIME *lpft;    /* Pointer to FILETIME array of length cValues */
} SDateTimeArray;

/* Multi-valued PT_DOUBLE property value */
typedef struct _SDoubleArray
{
    ULONG   cValues; /* Number of doubles in lpdbl */
    double *lpdbl;   /* Pointer to double array of length cValues */
} SDoubleArray;

/* Multi-valued PT_CLSID property value */
typedef struct _SGuidArray
{
    ULONG cValues; /* Number of GUIDs in lpguid */
    GUID *lpguid;  /* Pointer to GUID array of length cValues */
} SGuidArray;

/* Multi-valued PT_LONGLONG property value */
typedef struct _SLargeIntegerArray
{
    ULONG          cValues; /* Number of long64s in lpli */
    LARGE_INTEGER *lpli;    /* Pointer to long64 array of length cValues */
} SLargeIntegerArray;

/* Multi-valued PT_LONG property value */
typedef struct _SLongArray
{
    ULONG  cValues; /* Number of longs in lpl */
    LONG  *lpl;     /* Pointer to long array of length cValues */
} SLongArray;

/* Multi-valued PT_STRING8 property value */
typedef struct _SLPSTRArray
{
    ULONG  cValues; /* Number of Ascii strings in lppszA */
    LPSTR *lppszA;  /* Pointer to Ascii string array of length cValues */
} SLPSTRArray;

/* Multi-valued PT_FLOAT property value */
typedef struct _SRealArray
{
    ULONG cValues; /* Number of floats in lpflt */
    float *lpflt;  /* Pointer to float array of length cValues */
} SRealArray;

/* Multi-valued PT_SHORT property value */
typedef struct _SShortArray
{
    ULONG      cValues; /* Number of shorts in lpb */
    short int *lpi;     /* Pointer to short array of length cValues */
} SShortArray;

/* Multi-valued PT_UNICODE property value */
typedef struct _SWStringArray
{
    ULONG   cValues; /* Number of Unicode strings in lppszW */
    LPWSTR *lppszW;  /* Pointer to Unicode string array of length cValues */
} SWStringArray;


/* A property value */
union PV_u
{
    short int          i;
    LONG               l;
    ULONG              ul;
    float              flt;
    double             dbl;
    unsigned short     b;
    CY                 cur;
    double             at;
    FILETIME           ft;
    LPSTR              lpszA;
    SBinary            bin;
    LPWSTR             lpszW;
    LPGUID             lpguid;
    LARGE_INTEGER      li;
    SShortArray        MVi;
    SLongArray         MVl;
    SRealArray         MVflt;
    SDoubleArray       MVdbl;
    SCurrencyArray     MVcur;
    SAppTimeArray      MVat;
    SDateTimeArray     MVft;
    SBinaryArray       MVbin;
    SLPSTRArray        MVszA;
    SWStringArray      MVszW;
    SGuidArray         MVguid;
    SLargeIntegerArray MVli;
    SCODE              err;
    LONG               x;
};
typedef union PV_u uPV;

/* Property value structure. This is essentially a mini-Variant. */
struct SPropValue_s
{
  ULONG     ulPropTag;  /* The property type. */
  ULONG     dwAlignPad; /* Alignment, treat as reserved. */
  uPV       Value;      /* The property value. */
};
typedef struct SPropValue_s SPropValue;
typedef struct SPropValue_s *LPSPropValue;

/* Structure describing a table row (a collection of property values). */
struct SRow_s
{
  ULONG        ulAdrEntryPad; /* Padding, treat as reserved. */
  ULONG        cValues;       /* Count of property values in lpProbs. */
  LPSPropValue lpProps;       /* Pointer to an array of property
                                 values of length cValues. */
};
typedef struct SRow_s SRow;
typedef struct SRow_s *LPSRow;


/* Structure describing a set of table rows. */
struct SRowSet_s
{
  ULONG cRows;          /* Count of rows in aRow. */
  SRow  aRow[MAPI_DIM]; /* Array of rows of length cRows. */
};
typedef struct SRowSet_s *LPSRowSet;


/* Structure describing a problem with a property */
typedef struct _SPropProblem
{
    ULONG ulIndex;   /* Index of the property */
    ULONG ulPropTag; /* Proprty tag of the property */
    SCODE scode;     /* Error code of the problem */
} SPropProblem, *LPSPropProblem;

/* A collection of property problems */
typedef struct _SPropProblemArray
{
    ULONG        cProblem;           /* Number of problems in aProblem */
    SPropProblem aProblem[MAPI_DIM]; /* Array of problems of length cProblem */
} SPropProblemArray, *LPSPropProblemArray;


/* Table bookmarks */
typedef ULONG BOOKMARK;

#define BOOKMARK_BEGINNING ((BOOKMARK)0) /* The first row */
#define BOOKMARK_CURRENT   ((BOOKMARK)1) /* The curent table row */
#define BOOKMARK_END       ((BOOKMARK)2) /* The last row */


/* Row restrictions */
typedef struct _SRestriction* LPSRestriction;


/* Errors. */
typedef struct _MAPIERROR
{
    ULONG  ulVersion;       /* Mapi version */
#if defined (UNICODE) || defined (__WINESRC__)
    LPWSTR lpszError;       /* Error and component strings. These are Ascii */
    LPWSTR lpszComponent;   /* unless the MAPI_UNICODE flag is passed in */
#else
    LPSTR  lpszError;
    LPSTR  lpszComponent;
#endif
    ULONG  ulLowLevelError;
    ULONG  ulContext;
} MAPIERROR, *LPMAPIERROR;


/* Sorting */
#define TABLE_SORT_ASCEND  0U
#define TABLE_SORT_DESCEND 1U
#define TABLE_SORT_COMBINE 2U

typedef struct _SSortOrder
{
    ULONG ulPropTag;
    ULONG ulOrder;
} SSortOrder, *LPSSortOrder;

typedef struct _SSortOrderSet
{
    ULONG      cSorts;
    ULONG      cCategories;
    ULONG      cExpanded;
    SSortOrder aSort[MAPI_DIM];
} SSortOrderSet, * LPSSortOrderSet;



typedef struct _MAPINAMEID
{
    LPGUID lpguid;
    ULONG ulKind;
    union
    {
        LONG lID;
        LPWSTR lpwstrName;
    } Kind;
} MAPINAMEID, *LPMAPINAMEID;

#define MNID_ID     0
#define MNID_STRING 1


typedef struct _ADRENTRY
{
    ULONG        ulReserved1;
    ULONG        cValues;
    LPSPropValue rgPropVals;
} ADRENTRY, *LPADRENTRY;

typedef struct _ADRLIST
{
    ULONG    cEntries;
    ADRENTRY aEntries[MAPI_DIM];
} ADRLIST, *LPADRLIST;


struct IAddrBook;
typedef struct IAddrBook *LPADRBOOK;


/* FIXME: Need to move the HCHARSET definition to some other place.  */
typedef void *HCHARSET;


typedef enum tagCHARSETTYPE
  {
    CHARSET_BODY = 0,
    CHARSET_HEADER = 1,
    CHARSET_WEB = 2
  }
CHARSETTYPE;

typedef enum tagCSETAPPLYTYPE
  {
    CSET_APPLY_UNTAGGED = 0,
    CSET_APPLY_ALL = 1,
    CSET_APPLY_TAG_ALL = 2
  }
CSETAPPLYTYPE;


/* Note that this is just a minimal definition.  */
typedef struct
{
  ULONG ulEventType;  /* Notification type.  */
  ULONG ulAlignPad;   /* Force alignment on a 8 byte.  */
  union {
    struct {
      ULONG           n_entryid;
      LPENTRYID       entryid;
      ULONG           obj_type;
      ULONG           n_parentid;
      LPENTRYID       parentid;
      ULONG           n_oldid;
      LPENTRYID       oldid;
      ULONG           n_oldparentid;
      LPENTRYID       oldparentid;
      LPSPropTagArray prop_tag_array;
    } obj;
  } info;
} NOTIFICATION, FAR *LPNOTIFICATION;


/* Definitions required for IConverterSession. */
typedef enum tagMIMESAVETYPE
  {
    SAVE_RFC822 = 0,    /* Use uuencode for attachments.  */
    SAVE_RFC1521 = 1    /* Regular MIME.  */
  }
MIMESAVETYPE;

typedef enum tagENCODINGTYPE
  {
    IET_BINARY   = 0,
    IET_BASE64   = 1,
    IET_UUENCODE = 2,
    IET_QP       = 3,
    IET_7BIT     = 4,
    IET_8BIT     = 5,
    IET_INETCSET = 6,
    IET_UNICODE  = 7,
    IET_RFC1522  = 8,
    IET_ENCODED  = 9,
    IET_CURRENT  = 10,
    IET_UNKNOWN  = 11,
    IET_BINHEX40 = 12,
    IET_LAST     = 13
  }
ENCODINGTYPE;

#define CCSF_SMTP            0x0002 /* The input is an SMTP message.  */
#define CCSF_NOHEADERS       0x0004 /* Ignore headers outside the message.  */
#define CCSF_USE_TNEF        0x0010 /* Embed TNEF in MIME.  */
#define CCSF_INCLUDE_BCC     0x0020 /* Include BCC recipients.  */
#define CCSF_8BITHEADERS     0x0040 /* Allow 8-bit headers. (mapi->mime) */
#define CCSF_USE_RTF         0x0080 /* Convert HTML to RTF.  */
#define CCSF_PLAIN_TEXT_ONLY 0x1000 /* Create only plain text (mapi->mime) */
#define CCSF_NO_MSGID        0x4000 /* Do not include Message-Id.  */



/**** Class definitions ****/
typedef const IID *LPCIID;


struct IAttach;
typedef struct IAttach *LPATTACH;

struct IMAPIAdviseSink;
typedef struct IMAPIAdviseSink *LPMAPIADVISESINK;

struct IMAPIProgress;
typedef struct IMAPIProgress *LPMAPIPROGRESS;

struct IMAPITable;
typedef struct IMAPITable *LPMAPITABLE;

struct IMAPIProp;
typedef struct IMAPIProp *LPMAPIPROP;

struct IMessage;
typedef struct IMessage *LPMESSAGE;

struct IMAPIContainer;
typedef struct IMAPIContainer *LPMAPICONTAINER;

struct IMAPIFolder;
typedef struct IMAPIFolder *LPMAPIFOLDER;

struct IMsgStore;
typedef struct IMsgStore *LPMDB;

struct IMAPISession;
typedef struct IMAPISession *LPMAPISESSION;

struct IMsgServiceAdmin;
typedef struct IMsgServiceAdmin *LPSERVICEADMIN;

struct IConverterSession;
typedef struct IConverterSession *LPCONVERTERSESSION;

struct IProfSect;
typedef struct IProfSect *LPPROFSECT;

struct ISpoolerHook;
typedef struct ISpoolerHook *LPSPOOLERHOOK;

struct IMAPIFormContainer;
typedef struct IMAPIFormContainer *LPMAPIFORMCONTAINER;


/*** IMAPIProp methods ***/
#define MY_IMAPIPROP_METHODS \
  STDMETHOD(GetLastError)(THIS_ HRESULT, ULONG, LPMAPIERROR FAR*);         \
  STDMETHOD(SaveChanges)(THIS_ ULONG);                                     \
  STDMETHOD(GetProps)(THIS_ LPSPropTagArray, ULONG, ULONG FAR*,            \
                      LPSPropValue FAR*);                                  \
  STDMETHOD(GetPropList)(THIS_ ULONG, LPSPropTagArray FAR *);              \
  STDMETHOD(OpenProperty)(THIS_ ULONG, LPCIID lpiid, ULONG,                \
                          ULONG, LPUNKNOWN FAR*);                          \
  STDMETHOD(SetProps)(THIS_ LONG, LPSPropValue, LPSPropProblemArray FAR*); \
  STDMETHOD(DeleteProps)(THIS_ LPSPropTagArray, LPSPropProblemArray FAR *);\
  STDMETHOD(CopyTo)(THIS_ ULONG, LPCIID, LPSPropTagArray, ULONG,           \
                    LPMAPIPROGRESS, LPCIID lpInterface, LPVOID,            \
                    ULONG, LPSPropProblemArray FAR*);                      \
  STDMETHOD(CopyProps)(THIS_ LPSPropTagArray, ULONG, LPMAPIPROGRESS,       \
                       LPCIID, LPVOID, ULONG, LPSPropProblemArray FAR*);   \
  STDMETHOD(GetNamesFromIDs)(THIS_ LPSPropTagArray FAR*, LPGUID,           \
                             ULONG, ULONG FAR*, LPMAPINAMEID FAR* FAR*);   \
  STDMETHOD(GetIDsFromNames)(THIS_ ULONG, LPMAPINAMEID FAR*, ULONG,        \
                             LPSPropTagArray FAR*)

/*** IMsgstore methods ***/
#define MY_IMSGSTORE_METHODS \
  STDMETHOD(Advise)(THIS_ ULONG n_entryid, LPENTRYID entryid,                \
                    ULONG ulEventMask, LPMAPIADVISESINK lpAdviseSink,        \
                    ULONG FAR *connection) PURE;                             \
  STDMETHOD(Unadvise)(THIS_ ULONG connection) PURE;                          \
  STDMETHOD(CompareEntryIDs)(THIS_  ULONG n_entryid1, LPENTRYID entryid1,    \
                             ULONG n_entryid2, LPENTRYID entryid2,           \
                             ULONG flags, ULONG FAR *lpulResult) PURE;       \
  STDMETHOD(OpenEntry)(THIS_ ULONG n_entryid, LPENTRYID entryid,             \
                       LPCIID lpInterface, ULONG flags,                      \
                       ULONG FAR *obj_type, LPUNKNOWN FAR *lppUnk) PURE;     \
  STDMETHOD(SetReceiveFolder)(THIS_ LPTSTR message_class, ULONG flags,       \
                              ULONG n_entryid, LPENTRYID entryid) PURE;      \
  STDMETHOD(GetReceiveFolder)(THIS_ LPTSTR message_class, ULONG flags,       \
                              ULONG FAR *r_nentryid,LPENTRYID FAR *r_entryid,\
                              LPTSTR FAR *lppszExplicitClass) PURE;          \
  STDMETHOD(GetReceiveFolderTable)(THIS_ ULONG flags,                        \
                                   LPMAPITABLE FAR *lppTable) PURE;          \
  STDMETHOD(StoreLogoff)(THIS_ ULONG FAR *r_flags) PURE;                     \
  STDMETHOD(AbortSubmit)(THIS_ ULONG nentryid, LPENTRYID entryid,            \
                         ULONG flags) PURE;                                  \
  STDMETHOD(GetOutgoingQueue)(THIS_ ULONG flags,LPMAPITABLE FAR *table) PURE;\
  STDMETHOD(SetLockState)(THIS_ LPMESSAGE message, ULONG lock_state) PURE;   \
  STDMETHOD(FinishedMsg)(THIS_ ULONG ulFlags,                                \
                         ULONG nentryid, LPENTRYID entryid) PURE;            \
  STDMETHOD(NotifyNewMail)(THIS_ LPNOTIFICATION notification) PURE


/*** IMessage methods ***/
#define MY_IMESSAGE_METHODS \
  STDMETHOD(GetAttachmentTable)(THIS_ ULONG, LPMAPITABLE FAR*) PURE;         \
  STDMETHOD(OpenAttach)(THIS_ ULONG, LPCIID, ULONG, LPATTACH FAR*) PURE;     \
  STDMETHOD(CreateAttach)(THIS_ LPCIID, ULONG, ULONG FAR*,                   \
                          LPATTACH FAR*) PURE;                               \
  STDMETHOD(DeleteAttach)(THIS_ ULONG, ULONG, LPMAPIPROGRESS, ULONG) PURE;   \
  STDMETHOD(GetRecipientTable)(THIS_ ULONG, LPMAPITABLE FAR*) PURE;          \
  STDMETHOD(ModifyRecipients)(THIS_ ULONG, LPADRLIST) PURE;                  \
  STDMETHOD(SubmitMessage)(THIS_ ULONG) PURE;                                \
  STDMETHOD(SetReadFlag)(THIS_ ULONG) PURE

/*** IMAPIContainer ***/
#define MY_IMAPICONTAINER_METHODS \
  STDMETHOD(GetContentsTable)(THIS_ ULONG flags,                             \
                              LPMAPITABLE FAR *table) PURE;                  \
  STDMETHOD(GetHierarchyTable)(THIS_ ULONG flags,                            \
                               LPMAPITABLE FAR *table) PURE;                 \
  STDMETHOD(OpenEntry)(THIS_ ULONG n_entryid, LPENTRYID entryid,             \
                       LPCIID iface, ULONG flags,                            \
                       ULONG FAR *obj_type, LPUNKNOWN FAR *lppUnk) PURE;     \
  STDMETHOD(SetSearchCriteria)(THIS_ LPSRestriction restriction,             \
                               LPENTRYLIST container_list,                   \
                               ULONG  search_flags) PURE;                    \
  STDMETHOD(GetSearchCriteria)(THIS_ ULONG flags,                            \
                               LPSRestriction FAR *restriction,              \
                               LPENTRYLIST FAR *container_list,              \
                               ULONG FAR *search_state) PURE


/*** IMAPIFolder methods ***/
#define MY_IMAPIFOLDER_METHODS \
  STDMETHOD(CreateMessage)(THIS_ LPCIID iface, ULONG flags,                  \
                           LPMESSAGE FAR *message) PURE;                     \
  STDMETHOD(CopyMessages)(THIS_ LPENTRYLIST msg_list, LPCIID iface,          \
                          LPVOID dest_folder, ULONG ui_param,                \
                          LPMAPIPROGRESS progress, ULONG flags) PURE;        \
  STDMETHOD(DeleteMessages)(THIS_ LPENTRYLIST msg_list, ULONG uiparam,       \
                            LPMAPIPROGRESS progress, ULONG flags) PURE;      \
  STDMETHOD(CreateFolder)(THIS_ ULONG folder_type, LPTSTR folder_name,       \
                          LPTSTR folder_comment, LPCIID iface,               \
                          ULONG flags, LPMAPIFOLDER FAR *folder) PURE;       \
  STDMETHOD(CopyFolder)(THIS_ ULONG n_entryid, LPENTRYID entryid,            \
                        LPCIID iface, LPVOID dest_folder,                    \
                        LPTSTR new_folder_name, ULONG uiparam,               \
                        LPMAPIPROGRESS progress, ULONG flags) PURE;          \
  STDMETHOD(DeleteFolder)(THIS_ ULONG n_entryid, LPENTRYID entryid,          \
                          ULONG uiparam,                                     \
                          LPMAPIPROGRESS progress, ULONG flags) PURE;        \
  STDMETHOD(SetReadFlags)(THIS_ LPENTRYLIST msg_list, ULONG uiparam,         \
                          LPMAPIPROGRESS progress, ULONG flags) PURE;        \
  STDMETHOD(GetMessageStatus)(THIS_ ULONG n_entryid, LPENTRYID entryid,      \
                              ULONG flags, ULONG FAR *message_status) PURE;  \
  STDMETHOD(SetMessageStatus)(THIS_ ULONG n_entryid, LPENTRYID entryid,      \
                              ULONG new_status, ULONG new_status_mask,       \
                              ULONG FAR *r_old_status) PURE;                 \
  STDMETHOD(SaveContentsSort)(THIS_ LPSSortOrderSet sort_criteria,           \
                              ULONG flags) PURE;                             \
  STDMETHOD(EmptyFolder)(THIS_ ULONG uiparam,                                \
                         LPMAPIPROGRESS progress, ULONG flags) PURE


/*** IMAPITable methods ***/
#define MY_IMAPITABLE_METHODS \
  STDMETHOD(GetLastError)(THIS_ HRESULT, ULONG, LPMAPIERROR*) PURE;          \
  STDMETHOD(Advise)(THIS_ ULONG, LPMAPIADVISESINK, ULONG*) PURE;             \
  STDMETHOD(Unadvise)(THIS_ ULONG ulCxn) PURE;                               \
  STDMETHOD(GetStatus)(THIS_ ULONG*, ULONG*) PURE;                           \
  STDMETHOD(SetColumns)(THIS_ LPSPropTagArray, ULONG) PURE;                  \
  STDMETHOD(QueryColumns)(THIS_ ULONG, LPSPropTagArray*) PURE;               \
  STDMETHOD(GetRowCount)(THIS_ ULONG, ULONG *) PURE;                         \
  STDMETHOD(SeekRow)(THIS_ BOOKMARK, LONG, LONG*) PURE;                      \
  STDMETHOD(SeekRowApprox)(THIS_ ULONG, ULONG) PURE;                         \
  STDMETHOD(QueryPosition)(THIS_ ULONG*, ULONG*, ULONG*) PURE;               \
  STDMETHOD(FindRow)(THIS_ LPSRestriction, BOOKMARK, ULONG) PURE;            \
  STDMETHOD(Restrict)(THIS_ LPSRestriction, ULONG) PURE;                     \
  STDMETHOD(CreateBookmark)(THIS_ BOOKMARK*) PURE;                           \
  STDMETHOD(FreeBookmark)(THIS_ BOOKMARK) PURE;                              \
  STDMETHOD(SortTable)(THIS_ LPSSortOrderSet, ULONG) PURE;                   \
  STDMETHOD(QuerySortOrder)(THIS_ LPSSortOrderSet*) PURE;                    \
  STDMETHOD(QueryRows)(THIS_ LONG, ULONG, LPSRowSet*) PURE;                  \
  STDMETHOD(Abort)(THIS) PURE;                                               \
  STDMETHOD(ExpandRow)(THIS_ ULONG, LPBYTE, ULONG, ULONG,                    \
                       LPSRowSet*, ULONG*) PURE;                             \
  STDMETHOD(CollapseRow)(THIS_ ULONG, LPBYTE, ULONG, ULONG*) PURE;           \
  STDMETHOD(WaitForCompletion)(THIS_ ULONG, ULONG, ULONG*) PURE;             \
  STDMETHOD(GetCollapseState)(THIS_ ULONG, ULONG,LPBYTE,ULONG*,LPBYTE*) PURE;\
  STDMETHOD(SetCollapseState)(THIS_ ULONG, ULONG, LPBYTE, BOOKMARK*) PURE



/*** IMAPISession methods ***/
#define MY_IMAPISESSION_METHODS \
  STDMETHOD(GetLastError)(THIS_ HRESULT hResult, ULONG ulFlags,               \
                          LPMAPIERROR *lppMAPIError) PURE;                    \
  STDMETHOD(GetMsgStoresTable)(THIS_ ULONG ulFlags,                           \
                               LPMAPITABLE *lppTable) PURE;                   \
  STDMETHOD(OpenMsgStore)(THIS_ ULONG_PTR ulUIParam, ULONG cbId,              \
                          LPENTRYID lpId, LPCIID lpIFace,                     \
                          ULONG ulFlags, LPMDB *lppMDB) PURE;                 \
  STDMETHOD(OpenAddressBook)(THIS_ ULONG_PTR ulUIParam, LPCIID iid,           \
                             ULONG ulFlags, LPADRBOOK *lppAdrBook) PURE;      \
  STDMETHOD(OpenProfileSection)(THIS_ LPMAPIUID lpUID, LPCIID iid,            \
                                ULONG ulFlags, LPPROFSECT *lppProf) PURE;     \
  STDMETHOD(GetStatusTable)(THIS_ ULONG ulFlags, LPMAPITABLE *lppTable) PURE; \
  STDMETHOD(OpenEntry)(THIS_ ULONG cbId, LPENTRYID lpId, LPCIID iid,          \
                       ULONG ulFlags, ULONG *lpType, LPUNKNOWN *lppUnk) PURE; \
  STDMETHOD(CompareEntryIDs)(THIS_ ULONG cbLID, LPENTRYID lpLID, ULONG cbRID, \
                             LPENTRYID lpRID, ULONG ulFlags,                  \
                             ULONG *lpRes) PURE;                              \
  STDMETHOD(Advise)(THIS_ ULONG cbId, LPENTRYID lpId, ULONG ulMask,           \
                    LPMAPIADVISESINK lpSink, ULONG *lpCxn) PURE;              \
  STDMETHOD(Unadvise)(THIS_ ULONG ulConnection) PURE;                         \
  STDMETHOD(MessageOptions)(THIS_ ULONG_PTR ulUIParam, ULONG ulFlags,         \
                            LPSTR lpszAddr, LPMESSAGE lpMsg) PURE;            \
  STDMETHOD(QueryDefaultMessageOpt)(THIS_ LPSTR lpszAddr, ULONG ulFlags,      \
                                    ULONG *lpcVals,                           \
                                    LPSPropValue *lppOpts) PURE;              \
  STDMETHOD(EnumAdrTypes)(THIS_ ULONG ulFlags, ULONG *lpcTypes,               \
                          LPSTR **lpppszTypes) PURE;                          \
  STDMETHOD(QueryIdentity)(THIS_ ULONG *lpcbId, LPENTRYID *lppEntryID) PURE;  \
  STDMETHOD(Logoff)(THIS_ ULONG_PTR ulUIParam, ULONG ulFlags,                 \
                    ULONG ulReserved) PURE;                                   \
  STDMETHOD(SetDefaultStore)(THIS_ ULONG ulFlags, ULONG cbId,                 \
                             LPENTRYID lpId) PURE;                            \
  STDMETHOD(AdminServices)(THIS_ ULONG ulFlags, LPSERVICEADMIN *lppAdmin)PURE;\
  STDMETHOD(ShowForm)(THIS_ ULONG_PTR ulUIParam, LPMDB lpStore,               \
                      LPMAPIFOLDER lpParent, LPCIID iid, ULONG ulToken,       \
                      LPMESSAGE lpSent, ULONG ulFlags, ULONG ulStatus,        \
                      ULONG ulMsgFlags, ULONG ulAccess, LPSTR lpszClass)PURE; \
  STDMETHOD(PrepareForm)(THIS_ LPCIID lpIFace, LPMESSAGE lpMsg,               \
                         ULONG *lpToken) PURE;


/*** ISpoolerHook methods ***/
#define MY_ISPOOLERHOOK_METHODS \
  STDMETHOD(InboundMsgHook)(THIS_ LPMESSAGE message, LPMAPIFOLDER folder,     \
                            LPMDB mdb, ULONG FAR *r_flags,                    \
                            ULONG FAR *n_entryid, LPBYTE FAR *entryid) PURE;  \
  STDMETHOD(OutboundMsgHook)(THIS_ LPMESSAGE message, LPMAPIFOLDER folder,    \
                             LPMDB mdb, ULONG FAR *r_flags,                   \
                             ULONG FAR *n_entryid, LPBYTE FAR *entryid) PURE

/* In difference to the MY_ macros the DECLARE_ macros are not undefined
   in this header. */

/*** IUnknown methods ***/
#define DECLARE_IUNKNOWN_METHODS                                              \
  STDMETHOD(QueryInterface)(THIS_ REFIID, PVOID*) PURE;                       \
  STDMETHOD_(ULONG,AddRef)(THIS) PURE;                                        \
  STDMETHOD_(ULONG,Release)(THIS) PURE

/*** IDispatch methods ***/
#define DECLARE_IDISPATCH_METHODS                                             \
  STDMETHOD(GetTypeInfoCount)(THIS_ UINT*) PURE;                              \
  STDMETHOD(GetTypeInfo)(THIS_ UINT, LCID, LPTYPEINFO*) PURE;                 \
  STDMETHOD(GetIDsOfNames)(THIS_ REFIID, LPOLESTR*, UINT, LCID, DISPID*) PURE;\
  STDMETHOD(Invoke)(THIS_ DISPID, REFIID, LCID, WORD,                         \
                    DISPPARAMS*, VARIANT*, EXCEPINFO*, UINT*) PURE



EXTERN_C const IID IID_IMAPIProp;
#undef INTERFACE
#define INTERFACE IMAPIProp
DECLARE_INTERFACE_(IMAPIProp,IUnknown)
{
  DECLARE_IUNKNOWN_METHODS;
  MY_IMAPIPROP_METHODS;
};



EXTERN_C const IID IID_IMsgStore;
#undef INTERFACE
#define INTERFACE IMsgStore
DECLARE_INTERFACE_(IMsgStore,IMAPIProp)
{
  DECLARE_IUNKNOWN_METHODS;
  MY_IMAPIPROP_METHODS;
  MY_IMSGSTORE_METHODS;
};


EXTERN_C const IID IID_IMessage;
#undef INTERFACE
#define INTERFACE IMessage
DECLARE_INTERFACE_(IMessage,IMAPIProp)
{
  DECLARE_IUNKNOWN_METHODS;
  MY_IMAPIPROP_METHODS;
  MY_IMESSAGE_METHODS;
};


EXTERN_C const IID IID_IMAPIContainer;
#undef INTERFACE
#define INTERFACE IMAPIContainer
DECLARE_INTERFACE_(IMAPIContainer,IMAPIProp)
{
  DECLARE_IUNKNOWN_METHODS;
  MY_IMAPIPROP_METHODS;
  MY_IMAPICONTAINER_METHODS;
};


EXTERN_C const IID IID_IMAPIFolder;
#undef INTERFACE
#define INTERFACE IMAPIFolder
DECLARE_INTERFACE_(IMAPIFolder,IMAPIContainer)
{
  DECLARE_IUNKNOWN_METHODS;
  MY_IMAPIPROP_METHODS;
  MY_IMAPICONTAINER_METHODS;
  MY_IMAPIFOLDER_METHODS;
};


EXTERN_C const IID IID_IAttachment;
#undef INTERFACE
#define INTERFACE IAttach
DECLARE_INTERFACE_(IAttach, IMAPIProp)
{
  DECLARE_IUNKNOWN_METHODS;
  MY_IMAPIPROP_METHODS;
  /*** IAttach methods ***/
  /* No methods */
};


EXTERN_C const IID IID_IMAPITableData;
#undef INTERFACE
#define INTERFACE IMAPITable
DECLARE_INTERFACE_(IMAPITable,IUnknown)
{
  DECLARE_IUNKNOWN_METHODS;
  MY_IMAPITABLE_METHODS;
};


EXTERN_C const IID IID_IMAPISession;
#undef INTERFACE
#define INTERFACE IMAPISession
DECLARE_INTERFACE_(IMAPISession, IUnknown)
{
  DECLARE_IUNKNOWN_METHODS;
  MY_IMAPISESSION_METHODS;
};




EXTERN_C const IID IID_IConverterSession;
#undef INTERFACE
#define INTERFACE IConverterSession
DECLARE_INTERFACE_(IConverterSession, IUnknown)
{
  DECLARE_IUNKNOWN_METHODS;

  /*** IConverterSession ***/

  /* Pass a MAPI address book to be used for name resultion to the
     converter.  */
  STDMETHOD(SetAdrBook)(THIS_ LPADRBOOK pab);
  /* For an encoding for the outermost MIME container.  */
  STDMETHOD(SetEncoding)(THIS_ ENCODINGTYPE);
  STDMETHOD(PlaceHolder3)(THIS);
  /* Convert a MIME stream to a MAPI message.  SRC_SRV needs to be
     NULL. */
  STDMETHOD(MIMEToMAPI)(THIS_ LPSTREAM pstm, LPMESSAGE pmsg,
                        LPCSTR src_srv, ULONG flags);
  /* Convert a MAPI message to a MIME stream. */
  STDMETHOD(MAPIToMIMEStm)(THIS_ LPMESSAGE pmsg, LPSTREAM pstm, ULONG flags);
  STDMETHOD(PlaceHolder6)(THIS);
  STDMETHOD(PlaceHolder7)(THIS);
  STDMETHOD(PlaceHolder8)(THIS);
  /* Enable text wrapping and the maximum line length.  */
  STDMETHOD(SetTextWrapping)(THIS_ BOOL, ULONG);
  /* Set the MIME save format.  The default is SAVE_RFC1521 (MIME) but
     my be changed using this function to SAVE_RFC822 (uuencode).  */
  STDMETHOD(SetSaveFormat)(THIS_ MIMESAVETYPE);
  STDMETHOD(PlaceHolder11)(THIS);
  /* Tell which character set to use.  HCHARSET is either NULL or a
     handle retrieved by (e.g.) MimeOleGetCodePageCharset.  If FAPPLY
     is false, HCHARSET is not used.  */
  STDMETHOD(SetCharset)(THIS_ BOOL fApply, HCHARSET hcharset,
                        CSETAPPLYTYPE csetapplytype);
};


EXTERN_C const IID IID_ISpoolerHook;
#undef INTERFACE
#define INTERFACE ISpoolerHook
DECLARE_INTERFACE_(ISpoolerHook, IUnknown)
{
  DECLARE_IUNKNOWN_METHODS;
  MY_ISPOOLERHOOK_METHODS;
};



/* IMAPIFormContainer */

#define MAPIFORM_INSTALL_OVERWRITEONCONFLICT  0x10

typedef struct _SMAPIFormPropEnumVal
{
  LPTSTR pszDisplayName;
  ULONG nVal;
} SMAPIFormPropEnumVal, *LPMAPIFORMPROPENUMVAL;


typedef struct _SMAPIFormProp
{
  ULONG      ulFlags;
  ULONG      nPropType;
  MAPINAMEID nmid;
  LPTSTR     pszDisplayName;
  ULONG      nSpecialType;
  union {
    struct {
      MAPINAMEID nmidIdx;
      ULONG cfpevAvailable;
      LPMAPIFORMPROPENUMVAL pfpevAvailable;
    } s1;
  } u;
} SMAPIFormProp, *LPMAPIFORMPROP;

typedef struct _SMAPIFormPropArray
{
  ULONG cProps;
  ULONG ulPad;
  SMAPIFormProp aFormProp[MAPI_DIM];
} SMAPIFormPropArray, *LPMAPIFORMPROPARRAY;

typedef struct _SMessageClassArray
{
  ULONG  cValues;
  LPCSTR aMessageClass[MAPI_DIM];
} SMessageClassArray, *LPSMESSAGECLASSARRAY;


/* Fixme: The void ptr in ResolveMessageClass and SMAPIFormInfoArray
   should be a LPMAPIFORMINFO, but we have not yet defined the
   corresponding class. */
typedef struct _SMAPIFormInfoArray
{
  ULONG cForms;
  void * aFormInfo[MAPI_DIM];
} SMAPIFormInfoArray, *LPSMAPIFORMINFOARRAY;

#define MY_IMAPIFORMCONTAINER_METHODS                                         \
  STDMETHOD(GetLastError)(THIS_ HRESULT, ULONG, LPMAPIERROR FAR*) PURE;       \
  STDMETHOD(InstallForm)(THIS_ ULONG ulUIParam, ULONG ulFlags,                \
                         LPCTSTR szCfgPathName) PURE;                         \
  STDMETHOD(RemoveForm)(THIS_ LPCSTR szMessageClass) PURE;                    \
  STDMETHOD(ResolveMessageClass) (THIS_ LPCSTR szMessageClass, ULONG ulFlags, \
                                  void * FAR *pforminfo) PURE;                \
  STDMETHOD(ResolveMultipleMessageClasses)                                    \
    (THIS_ LPSMESSAGECLASSARRAY pMsgClassArray, ULONG ulFlags,                \
     LPSMAPIFORMINFOARRAY FAR *ppfrminfoarray) PURE;                          \
  STDMETHOD(CalcFormPropSet)(THIS_  ULONG ulFlags,                            \
                             LPMAPIFORMPROPARRAY FAR *ppResults) PURE;        \
  STDMETHOD(GetDisplay)(THIS_ ULONG ulFlags,                                  \
                        LPTSTR FAR *pszDisplayName) PURE;


EXTERN_C const IID IID_IMAPIFormContainer;
#undef INTERFACE
#define INTERFACE IMAPIFormContainer
DECLARE_INTERFACE_(IMAPIFormContainer, IUnknown)
{
  DECLARE_IUNKNOWN_METHODS;
  MY_IMAPIFORMCONTAINER_METHODS;
};



#undef MY_IMAPIPROP_METHODS
#undef MY_IMSGSTORE_METHODS
#undef MY_IMESSAGE_METHODS
#undef MY_IMAPICONTAINER_METHODS
#undef MY_IMAPIFOLDER_METHODS
#undef MY_IMAPITABLE_METHODS
#undef MY_IMAPISESSION_METHODS
#undef MY_ISPOOLERHOOK_METHODS
#undef MY_IMAPIFORMCONTAINER_METHODS



/**** C Wrapper ****/
#ifdef COBJMACROS

#define IMessage_CreateAttach(This,a,b,c,d) \
                (This)->lpVtbl->CreateAttach ((This),(a),(b),(c),(d))
#define IMessage_DeleteAttach(This,a,b,c,d) \
                (This)->lpVtbl->DeleteAttach ((This),(a),(b),(c),(d))
#define IMessage_SetProps(This,a,b,c) \
                (This)->lpVtbl->SetProps ((This),(a),(b),(c))
#define IMessage_DeleteProps(This,a,b) \
                (This)->lpVtbl->DeleteProps ((This),(a),(b))
#define IMessage_SaveChanges(This,a) (This)->lpVtbl->SaveChanges ((This),(a))

#define IAttach_Release(This)  (This)->lpVtbl->Release ((This))
#define IAttach_SaveChanges(This,a) (This)->lpVtbl->SaveChanges ((This),(a))
#define IAttach_OpenProperty(This,a,b,c,d,e) \
                (This)->lpVtbl->OpenProperty ((This),(a),(b),(c),(d),(e))




#endif /*COBJMACROS*/


/****  Function prototypes. *****/

ULONG   WINAPI UlAddRef(void*);
ULONG   WINAPI UlRelease(void*);
HRESULT WINAPI HrGetOneProp(LPMAPIPROP,ULONG,LPSPropValue*);
HRESULT WINAPI HrSetOneProp(LPMAPIPROP,LPSPropValue);
BOOL    WINAPI FPropExists(LPMAPIPROP,ULONG);
void    WINAPI FreePadrlist(LPADRLIST);
void    WINAPI FreeProws(LPSRowSet);
HRESULT WINAPI HrQueryAllRows(LPMAPITABLE,LPSPropTagArray,LPSRestriction,
                              LPSSortOrderSet,LONG,LPSRowSet*);
LPSPropValue WINAPI PpropFindProp(LPSPropValue,ULONG,ULONG);

HRESULT WINAPI RTFSync (LPMESSAGE, ULONG, BOOL FAR*);


/* Memory allocation routines */
typedef SCODE (WINAPI ALLOCATEBUFFER)(ULONG,LPVOID*);
typedef SCODE (WINAPI ALLOCATEMORE)(ULONG,LPVOID,LPVOID*);
typedef ULONG (WINAPI FREEBUFFER)(LPVOID);
typedef ALLOCATEBUFFER *LPALLOCATEBUFFER;
typedef ALLOCATEMORE *LPALLOCATEMORE;
typedef FREEBUFFER *LPFREEBUFFER;

SCODE WINAPI MAPIAllocateBuffer (ULONG, LPVOID FAR *);
ULONG WINAPI MAPIFreeBuffer (LPVOID);

void    MAPIUninitialize (void);
HRESULT MAPIInitialize (LPVOID lpMapiInit);

#if defined (UNICODE)
HRESULT WINAPI OpenStreamOnFile(LPALLOCATEBUFFER,LPFREEBUFFER,
                                ULONG,LPWSTR,LPWSTR,LPSTREAM*);
#else
HRESULT WINAPI OpenStreamOnFile(LPALLOCATEBUFFER,LPFREEBUFFER,
                                ULONG,LPSTR,LPSTR,LPSTREAM*);
#endif

/* IMAPISecureMessage */
struct IMAPISecureMessage;
typedef struct IMAPISecureMessage *LPMAPISECUREMESSAGE;

#undef INTERFACE
#define INTERFACE IMAPISecureMessage

DECLARE_INTERFACE_(IMAPISecureMessage, IUnknown)
{
  DECLARE_IUNKNOWN_METHODS;

  STDMETHOD(Unknown1)(void) PURE;
  STDMETHOD(Unknown2)(void) PURE;
  STDMETHOD(Unknown3)(void) PURE;
  STDMETHOD(Unknown4)(void) PURE;
  STDMETHOD(Unknown5)(void) PURE;
  STDMETHOD(GetBaseMessage)(LPMESSAGE FAR *) PURE;
};

STDAPI MAPIOpenLocalFormContainer (LPMAPIFORMCONTAINER FAR *ppfcnt);
/* IMAPIFormContainer*/
struct MAPIFormMgr;
typedef struct MAPIFormMgr *LPMAPIFORMMGR;

typedef ULONG HFRMREG;
#define HFRMREG_DEFAULT  0
#define HFRMREG_LOCAL    1
#define HFRMREG_PERSONAL 2
#define HFRMREG_FOLDER   3

#undef INTERFACE
#define INTERFACE MAPIFormMgr

DECLARE_INTERFACE_(MAPIFormMgr, IUnknown)
{
  DECLARE_IUNKNOWN_METHODS;

  STDMETHOD(LoadForm)(void) PURE;
  STDMETHOD(ResolveMessageClass)(void) PURE;
  STDMETHOD(ResolveMultipleMessageClasses)(void) PURE;
  STDMETHOD(CalcFormPropSet)(void) PURE;
  STDMETHOD(CreateForm)(void) PURE;
  STDMETHOD(SelectForm)(void) PURE;
  STDMETHOD(SelectMultipleForms)(void) PURE;
  STDMETHOD(SelectFormContainer)(void) PURE;
  STDMETHOD(OpenFormContainer)(HFRMREG, LPUNKNOWN, LPMAPIFORMCONTAINER FAR *) PURE;
  STDMETHOD(PrepareForm)(void) PURE;
  STDMETHOD(IsInConflict)(void) PURE;
  STDMETHOD(GetLastError)(void) PURE;
};

STDAPI MAPIOpenFormMgr (LPMAPISESSION, LPMAPIFORMMGR FAR *);

#ifdef __cplusplus
}
#endif
#endif /* MAPI_H */
