

/* this ALWAYS GENERATED file contains the IIDs and CLSIDs */

/* link this file in with the server and any clients */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for ../../chrome/updater/server/win/updater_idl.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.xx.xxxx 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#pragma warning( disable: 4049 )  /* more than 64k source lines */


#ifdef __cplusplus
extern "C"{
#endif 


#include <rpc.h>
#include <rpcndr.h>

#ifdef _MIDL_USE_GUIDDEF_

#ifndef INITGUID
#define INITGUID
#include <guiddef.h>
#undef INITGUID
#else
#include <guiddef.h>
#endif

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        DEFINE_GUID(name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8)

#else // !_MIDL_USE_GUIDDEF_

#ifndef __IID_DEFINED__
#define __IID_DEFINED__

typedef struct _IID
{
    unsigned long x;
    unsigned short s1;
    unsigned short s2;
    unsigned char  c[8];
} IID;

#endif // __IID_DEFINED__

#ifndef CLSID_DEFINED
#define CLSID_DEFINED
typedef IID CLSID;
#endif // CLSID_DEFINED

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        EXTERN_C __declspec(selectany) const type name = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}

#endif // !_MIDL_USE_GUIDDEF_

MIDL_DEFINE_GUID(IID, IID_ICurrentState,0x247954F9,0x9EDC,0x4E68,0x8C,0xC3,0x15,0x0C,0x2B,0x89,0xEA,0xDF);


MIDL_DEFINE_GUID(IID, IID_IGoogleUpdate3Web,0x494B20CF,0x282E,0x4BDD,0x9F,0x5D,0xB7,0x0C,0xB0,0x9D,0x35,0x1E);


MIDL_DEFINE_GUID(IID, IID_IAppBundleWeb,0xDD42475D,0x6D46,0x496a,0x92,0x4E,0xBD,0x56,0x30,0xB4,0xCB,0xBA);


MIDL_DEFINE_GUID(IID, IID_IAppWeb,0x18D0F672,0x18B4,0x48e6,0xAD,0x36,0x6E,0x6B,0xF0,0x1D,0xBB,0xC4);


MIDL_DEFINE_GUID(IID, IID_ICompleteStatus,0x2FCD14AF,0xB645,0x4351,0x83,0x59,0xE8,0x0A,0x0E,0x20,0x2A,0x0B);


MIDL_DEFINE_GUID(IID, IID_IUpdaterObserver,0x7B416CFD,0x4216,0x4FD6,0xBD,0x83,0x7C,0x58,0x60,0x54,0x67,0x6E);


MIDL_DEFINE_GUID(IID, IID_IUpdater,0x63B8FFB1,0x5314,0x48C9,0x9C,0x57,0x93,0xEC,0x8B,0xC6,0x18,0x4B);


MIDL_DEFINE_GUID(IID, LIBID_UpdaterLib,0x69464FF0,0xD9EC,0x4037,0xA3,0x5F,0x8A,0xE4,0x35,0x81,0x06,0xCC);


MIDL_DEFINE_GUID(CLSID, CLSID_UpdaterClass,0x158428A4,0x6014,0x4978,0x83,0xBA,0x9F,0xAD,0x0D,0xAB,0xE7,0x91);


MIDL_DEFINE_GUID(CLSID, CLSID_UpdaterServiceClass,0x415FD747,0xD79E,0x42D7,0x93,0xAC,0x1B,0xA6,0xE5,0xFD,0x4E,0x93);


MIDL_DEFINE_GUID(CLSID, CLSID_GoogleUpdate3WebUserClass,0x22181302,0xA8A6,0x4f84,0xA5,0x41,0xE5,0xCB,0xFC,0x70,0xCC,0x43);

#undef MIDL_DEFINE_GUID

#ifdef __cplusplus
}
#endif



