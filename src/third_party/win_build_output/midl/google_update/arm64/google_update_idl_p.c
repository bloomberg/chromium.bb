

/* this ALWAYS GENERATED file contains the proxy stub code */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for ../../google_update/google_update_idl.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=ARM64 8.01.0622 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#if defined(_M_ARM64)


#pragma warning( disable: 4049 )  /* more than 64k source lines */
#if _MSC_VER >= 1200
#pragma warning(push)
#endif

#pragma warning( disable: 4211 )  /* redefine extern to static */
#pragma warning( disable: 4232 )  /* dllimport identity*/
#pragma warning( disable: 4024 )  /* array to pointer mapping*/
#pragma warning( disable: 4152 )  /* function/data pointer conversion in expression */

#define USE_STUBLESS_PROXY


/* verify that the <rpcproxy.h> version is high enough to compile this file*/
#ifndef __REDQ_RPCPROXY_H_VERSION__
#define __REQUIRED_RPCPROXY_H_VERSION__ 475
#endif


#include "rpcproxy.h"
#ifndef __RPCPROXY_H_VERSION__
#error this stub requires an updated version of <rpcproxy.h>
#endif /* __RPCPROXY_H_VERSION__ */


#include "google_update_idl.h"

#define TYPE_FORMAT_STRING_SIZE   1117                              
#define PROC_FORMAT_STRING_SIZE   6003                              
#define EXPR_FORMAT_STRING_SIZE   1                                 
#define TRANSMIT_AS_TABLE_SIZE    0            
#define WIRE_MARSHAL_TABLE_SIZE   2            

typedef struct _google_update_idl_MIDL_TYPE_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ TYPE_FORMAT_STRING_SIZE ];
    } google_update_idl_MIDL_TYPE_FORMAT_STRING;

typedef struct _google_update_idl_MIDL_PROC_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ PROC_FORMAT_STRING_SIZE ];
    } google_update_idl_MIDL_PROC_FORMAT_STRING;

typedef struct _google_update_idl_MIDL_EXPR_FORMAT_STRING
    {
    long          Pad;
    unsigned char  Format[ EXPR_FORMAT_STRING_SIZE ];
    } google_update_idl_MIDL_EXPR_FORMAT_STRING;


static const RPC_SYNTAX_IDENTIFIER  _RpcTransferSyntax = 
{{0x8A885D04,0x1CEB,0x11C9,{0x9F,0xE8,0x08,0x00,0x2B,0x10,0x48,0x60}},{2,0}};


extern const google_update_idl_MIDL_TYPE_FORMAT_STRING google_update_idl__MIDL_TypeFormatString;
extern const google_update_idl_MIDL_PROC_FORMAT_STRING google_update_idl__MIDL_ProcFormatString;
extern const google_update_idl_MIDL_EXPR_FORMAT_STRING google_update_idl__MIDL_ExprFormatString;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IGoogleUpdate3_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IGoogleUpdate3_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAppBundle_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAppBundle_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IApp_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IApp_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IApp2_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IApp2_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAppCommand_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAppCommand_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAppCommand2_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAppCommand2_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAppVersion_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAppVersion_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IPackage_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IPackage_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO ICurrentState_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO ICurrentState_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IRegistrationUpdateHook_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IRegistrationUpdateHook_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO ICredentialDialog_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO ICredentialDialog_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IPolicyStatus_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IPolicyStatus_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IGoogleUpdate3Web_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IGoogleUpdate3Web_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IGoogleUpdate3WebSecurity_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IGoogleUpdate3WebSecurity_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAppBundleWeb_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAppBundleWeb_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAppWeb_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAppWeb_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAppCommandWeb_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAppCommandWeb_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAppVersionWeb_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAppVersionWeb_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO ICoCreateAsyncStatus_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO ICoCreateAsyncStatus_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO ICoCreateAsync_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO ICoCreateAsync_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IBrowserHttpRequest2_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IBrowserHttpRequest2_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IProcessLauncher_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IProcessLauncher_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IProcessLauncher2_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IProcessLauncher2_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IOneClickProcessLauncher_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IOneClickProcessLauncher_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IProgressWndEvents_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IProgressWndEvents_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IJobObserver_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IJobObserver_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IJobObserver2_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IJobObserver2_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IGoogleUpdate_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IGoogleUpdate_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IGoogleUpdateCore_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IGoogleUpdateCore_ProxyInfo;


extern const USER_MARSHAL_ROUTINE_QUADRUPLE UserMarshalRoutines[ WIRE_MARSHAL_TABLE_SIZE ];

#if !defined(__RPC_ARM64__)
#error  Invalid build platform for this stub.
#endif

static const google_update_idl_MIDL_PROC_FORMAT_STRING google_update_idl__MIDL_ProcFormatString =
    {
        0,
        {

	/* Procedure get_status */


	/* Procedure get_lastCheckPeriodMinutes */


	/* Procedure get_stateValue */


	/* Procedure get_Count */

			0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
/*  6 */	NdrFcShort( 0x7 ),	/* 7 */
/*  8 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 10 */	NdrFcShort( 0x0 ),	/* 0 */
/* 12 */	NdrFcShort( 0x24 ),	/* 36 */
/* 14 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 16 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 18 */	NdrFcShort( 0x0 ),	/* 0 */
/* 20 */	NdrFcShort( 0x0 ),	/* 0 */
/* 22 */	NdrFcShort( 0x0 ),	/* 0 */
/* 24 */	NdrFcShort( 0x2 ),	/* 2 */
/* 26 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 28 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IAppCommandWeb0000 */


	/* Parameter minutes */


	/* Parameter __MIDL__ICurrentState0000 */


	/* Parameter count */

/* 30 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 32 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 34 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 36 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 38 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 40 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_Item */

/* 42 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 44 */	NdrFcLong( 0x0 ),	/* 0 */
/* 48 */	NdrFcShort( 0x8 ),	/* 8 */
/* 50 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 52 */	NdrFcShort( 0x8 ),	/* 8 */
/* 54 */	NdrFcShort( 0x8 ),	/* 8 */
/* 56 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 58 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 60 */	NdrFcShort( 0x0 ),	/* 0 */
/* 62 */	NdrFcShort( 0x0 ),	/* 0 */
/* 64 */	NdrFcShort( 0x0 ),	/* 0 */
/* 66 */	NdrFcShort( 0x3 ),	/* 3 */
/* 68 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 70 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter index */

/* 72 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 74 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 76 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter bundle */

/* 78 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 80 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 82 */	NdrFcShort( 0x6 ),	/* Type Offset=6 */

	/* Return value */

/* 84 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 86 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 88 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_createdInstance */


	/* Procedure get_nextVersionWeb */


	/* Procedure createAppBundle */

/* 90 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 92 */	NdrFcLong( 0x0 ),	/* 0 */
/* 96 */	NdrFcShort( 0x9 ),	/* 9 */
/* 98 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 100 */	NdrFcShort( 0x0 ),	/* 0 */
/* 102 */	NdrFcShort( 0x8 ),	/* 8 */
/* 104 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 106 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 108 */	NdrFcShort( 0x0 ),	/* 0 */
/* 110 */	NdrFcShort( 0x0 ),	/* 0 */
/* 112 */	NdrFcShort( 0x0 ),	/* 0 */
/* 114 */	NdrFcShort( 0x2 ),	/* 2 */
/* 116 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 118 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter instance */


	/* Parameter next */


	/* Parameter app_bundle */

/* 120 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 122 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 124 */	NdrFcShort( 0x6 ),	/* Type Offset=6 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 126 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 128 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 130 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_version */


	/* Procedure get_appId */


	/* Procedure get_version */


	/* Procedure get_displayName */

/* 132 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 134 */	NdrFcLong( 0x0 ),	/* 0 */
/* 138 */	NdrFcShort( 0x7 ),	/* 7 */
/* 140 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 142 */	NdrFcShort( 0x0 ),	/* 0 */
/* 144 */	NdrFcShort( 0x8 ),	/* 8 */
/* 146 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 148 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 150 */	NdrFcShort( 0x1 ),	/* 1 */
/* 152 */	NdrFcShort( 0x0 ),	/* 0 */
/* 154 */	NdrFcShort( 0x0 ),	/* 0 */
/* 156 */	NdrFcShort( 0x2 ),	/* 2 */
/* 158 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 160 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IAppVersionWeb0000 */


	/* Parameter __MIDL__IAppWeb0000 */


	/* Parameter __MIDL__IAppVersion0000 */


	/* Parameter __MIDL__IAppBundle0000 */

/* 162 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 164 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 166 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 168 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 170 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 172 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure createInstalledApp */


	/* Procedure put_displayName */

/* 174 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 176 */	NdrFcLong( 0x0 ),	/* 0 */
/* 180 */	NdrFcShort( 0x8 ),	/* 8 */
/* 182 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 184 */	NdrFcShort( 0x0 ),	/* 0 */
/* 186 */	NdrFcShort( 0x8 ),	/* 8 */
/* 188 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 190 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 192 */	NdrFcShort( 0x0 ),	/* 0 */
/* 194 */	NdrFcShort( 0x1 ),	/* 1 */
/* 196 */	NdrFcShort( 0x0 ),	/* 0 */
/* 198 */	NdrFcShort( 0x2 ),	/* 2 */
/* 200 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 202 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter app_id */


	/* Parameter __MIDL__IAppBundle0001 */

/* 204 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 206 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 208 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */


	/* Return value */

/* 210 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 212 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 214 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_output */


	/* Procedure get_downloadPreferenceGroupPolicy */


	/* Procedure get_filename */


	/* Procedure get_appId */


	/* Procedure get_displayLanguage */

/* 216 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 218 */	NdrFcLong( 0x0 ),	/* 0 */
/* 222 */	NdrFcShort( 0x9 ),	/* 9 */
/* 224 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 226 */	NdrFcShort( 0x0 ),	/* 0 */
/* 228 */	NdrFcShort( 0x8 ),	/* 8 */
/* 230 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 232 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 234 */	NdrFcShort( 0x1 ),	/* 1 */
/* 236 */	NdrFcShort( 0x0 ),	/* 0 */
/* 238 */	NdrFcShort( 0x0 ),	/* 0 */
/* 240 */	NdrFcShort( 0x2 ),	/* 2 */
/* 242 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 244 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IAppCommandWeb0002 */


	/* Parameter pref */


	/* Parameter __MIDL__IPackage0001 */


	/* Parameter __MIDL__IApp0000 */


	/* Parameter __MIDL__IAppBundle0002 */

/* 246 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 248 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 250 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 252 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 254 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 256 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_displayLanguage */

/* 258 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 260 */	NdrFcLong( 0x0 ),	/* 0 */
/* 264 */	NdrFcShort( 0xa ),	/* 10 */
/* 266 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 268 */	NdrFcShort( 0x0 ),	/* 0 */
/* 270 */	NdrFcShort( 0x8 ),	/* 8 */
/* 272 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 274 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 276 */	NdrFcShort( 0x0 ),	/* 0 */
/* 278 */	NdrFcShort( 0x1 ),	/* 1 */
/* 280 */	NdrFcShort( 0x0 ),	/* 0 */
/* 282 */	NdrFcShort( 0x2 ),	/* 2 */
/* 284 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 286 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IAppBundle0003 */

/* 288 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 290 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 292 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */

/* 294 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 296 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 298 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_output */


	/* Procedure get_installSource */

/* 300 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 302 */	NdrFcLong( 0x0 ),	/* 0 */
/* 306 */	NdrFcShort( 0xb ),	/* 11 */
/* 308 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 310 */	NdrFcShort( 0x0 ),	/* 0 */
/* 312 */	NdrFcShort( 0x8 ),	/* 8 */
/* 314 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 316 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 318 */	NdrFcShort( 0x1 ),	/* 1 */
/* 320 */	NdrFcShort( 0x0 ),	/* 0 */
/* 322 */	NdrFcShort( 0x0 ),	/* 0 */
/* 324 */	NdrFcShort( 0x2 ),	/* 2 */
/* 326 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 328 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IAppCommand20000 */


	/* Parameter __MIDL__IAppBundle0004 */

/* 330 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 332 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 334 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */


	/* Return value */

/* 336 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 338 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 340 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_installSource */

/* 342 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 344 */	NdrFcLong( 0x0 ),	/* 0 */
/* 348 */	NdrFcShort( 0xc ),	/* 12 */
/* 350 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 352 */	NdrFcShort( 0x0 ),	/* 0 */
/* 354 */	NdrFcShort( 0x8 ),	/* 8 */
/* 356 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 358 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 360 */	NdrFcShort( 0x0 ),	/* 0 */
/* 362 */	NdrFcShort( 0x1 ),	/* 1 */
/* 364 */	NdrFcShort( 0x0 ),	/* 0 */
/* 366 */	NdrFcShort( 0x2 ),	/* 2 */
/* 368 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 370 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IAppBundle0005 */

/* 372 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 374 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 376 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */

/* 378 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 380 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 382 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_originURL */

/* 384 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 386 */	NdrFcLong( 0x0 ),	/* 0 */
/* 390 */	NdrFcShort( 0xd ),	/* 13 */
/* 392 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 394 */	NdrFcShort( 0x0 ),	/* 0 */
/* 396 */	NdrFcShort( 0x8 ),	/* 8 */
/* 398 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 400 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 402 */	NdrFcShort( 0x1 ),	/* 1 */
/* 404 */	NdrFcShort( 0x0 ),	/* 0 */
/* 406 */	NdrFcShort( 0x0 ),	/* 0 */
/* 408 */	NdrFcShort( 0x2 ),	/* 2 */
/* 410 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 412 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IAppBundle0006 */

/* 414 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 416 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 418 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */

/* 420 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 422 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 424 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_originURL */

/* 426 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 428 */	NdrFcLong( 0x0 ),	/* 0 */
/* 432 */	NdrFcShort( 0xe ),	/* 14 */
/* 434 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 436 */	NdrFcShort( 0x0 ),	/* 0 */
/* 438 */	NdrFcShort( 0x8 ),	/* 8 */
/* 440 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 442 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 444 */	NdrFcShort( 0x0 ),	/* 0 */
/* 446 */	NdrFcShort( 0x1 ),	/* 1 */
/* 448 */	NdrFcShort( 0x0 ),	/* 0 */
/* 450 */	NdrFcShort( 0x2 ),	/* 2 */
/* 452 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 454 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IAppBundle0007 */

/* 456 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 458 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 460 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */

/* 462 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 464 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 466 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_serverInstallDataIndex */


	/* Procedure get_offlineDirectory */

/* 468 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 470 */	NdrFcLong( 0x0 ),	/* 0 */
/* 474 */	NdrFcShort( 0xf ),	/* 15 */
/* 476 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 478 */	NdrFcShort( 0x0 ),	/* 0 */
/* 480 */	NdrFcShort( 0x8 ),	/* 8 */
/* 482 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 484 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 486 */	NdrFcShort( 0x1 ),	/* 1 */
/* 488 */	NdrFcShort( 0x0 ),	/* 0 */
/* 490 */	NdrFcShort( 0x0 ),	/* 0 */
/* 492 */	NdrFcShort( 0x2 ),	/* 2 */
/* 494 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 496 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IAppWeb0001 */


	/* Parameter offline_dir */

/* 498 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 500 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 502 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */


	/* Return value */

/* 504 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 506 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 508 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_serverInstallDataIndex */


	/* Procedure put_offlineDirectory */

/* 510 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 512 */	NdrFcLong( 0x0 ),	/* 0 */
/* 516 */	NdrFcShort( 0x10 ),	/* 16 */
/* 518 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 520 */	NdrFcShort( 0x0 ),	/* 0 */
/* 522 */	NdrFcShort( 0x8 ),	/* 8 */
/* 524 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 526 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 528 */	NdrFcShort( 0x0 ),	/* 0 */
/* 530 */	NdrFcShort( 0x1 ),	/* 1 */
/* 532 */	NdrFcShort( 0x0 ),	/* 0 */
/* 534 */	NdrFcShort( 0x2 ),	/* 2 */
/* 536 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 538 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IAppWeb0002 */


	/* Parameter offline_dir */

/* 540 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 542 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 544 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */


	/* Return value */

/* 546 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 548 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 550 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_sessionId */

/* 552 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 554 */	NdrFcLong( 0x0 ),	/* 0 */
/* 558 */	NdrFcShort( 0x11 ),	/* 17 */
/* 560 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 562 */	NdrFcShort( 0x0 ),	/* 0 */
/* 564 */	NdrFcShort( 0x8 ),	/* 8 */
/* 566 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 568 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 570 */	NdrFcShort( 0x1 ),	/* 1 */
/* 572 */	NdrFcShort( 0x0 ),	/* 0 */
/* 574 */	NdrFcShort( 0x0 ),	/* 0 */
/* 576 */	NdrFcShort( 0x2 ),	/* 2 */
/* 578 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 580 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter session_id */

/* 582 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 584 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 586 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */

/* 588 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 590 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 592 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_sessionId */

/* 594 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 596 */	NdrFcLong( 0x0 ),	/* 0 */
/* 600 */	NdrFcShort( 0x12 ),	/* 18 */
/* 602 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 604 */	NdrFcShort( 0x0 ),	/* 0 */
/* 606 */	NdrFcShort( 0x8 ),	/* 8 */
/* 608 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 610 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 612 */	NdrFcShort( 0x0 ),	/* 0 */
/* 614 */	NdrFcShort( 0x1 ),	/* 1 */
/* 616 */	NdrFcShort( 0x0 ),	/* 0 */
/* 618 */	NdrFcShort( 0x2 ),	/* 2 */
/* 620 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 622 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter session_id */

/* 624 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 626 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 628 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */

/* 630 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 632 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 634 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_sendPings */

/* 636 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 638 */	NdrFcLong( 0x0 ),	/* 0 */
/* 642 */	NdrFcShort( 0x13 ),	/* 19 */
/* 644 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 646 */	NdrFcShort( 0x0 ),	/* 0 */
/* 648 */	NdrFcShort( 0x22 ),	/* 34 */
/* 650 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 652 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 654 */	NdrFcShort( 0x0 ),	/* 0 */
/* 656 */	NdrFcShort( 0x0 ),	/* 0 */
/* 658 */	NdrFcShort( 0x0 ),	/* 0 */
/* 660 */	NdrFcShort( 0x2 ),	/* 2 */
/* 662 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 664 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter send_pings */

/* 666 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 668 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 670 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */

/* 672 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 674 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 676 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_sendPings */

/* 678 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 680 */	NdrFcLong( 0x0 ),	/* 0 */
/* 684 */	NdrFcShort( 0x14 ),	/* 20 */
/* 686 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 688 */	NdrFcShort( 0x6 ),	/* 6 */
/* 690 */	NdrFcShort( 0x8 ),	/* 8 */
/* 692 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 694 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 696 */	NdrFcShort( 0x0 ),	/* 0 */
/* 698 */	NdrFcShort( 0x0 ),	/* 0 */
/* 700 */	NdrFcShort( 0x0 ),	/* 0 */
/* 702 */	NdrFcShort( 0x2 ),	/* 2 */
/* 704 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 706 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter send_pings */

/* 708 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 710 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 712 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */

/* 714 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 716 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 718 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_priority */

/* 720 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 722 */	NdrFcLong( 0x0 ),	/* 0 */
/* 726 */	NdrFcShort( 0x15 ),	/* 21 */
/* 728 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 730 */	NdrFcShort( 0x0 ),	/* 0 */
/* 732 */	NdrFcShort( 0x24 ),	/* 36 */
/* 734 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 736 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 738 */	NdrFcShort( 0x0 ),	/* 0 */
/* 740 */	NdrFcShort( 0x0 ),	/* 0 */
/* 742 */	NdrFcShort( 0x0 ),	/* 0 */
/* 744 */	NdrFcShort( 0x2 ),	/* 2 */
/* 746 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 748 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter priority */

/* 750 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 752 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 754 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 756 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 758 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 760 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_priority */

/* 762 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 764 */	NdrFcLong( 0x0 ),	/* 0 */
/* 768 */	NdrFcShort( 0x16 ),	/* 22 */
/* 770 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 772 */	NdrFcShort( 0x8 ),	/* 8 */
/* 774 */	NdrFcShort( 0x8 ),	/* 8 */
/* 776 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 778 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 780 */	NdrFcShort( 0x0 ),	/* 0 */
/* 782 */	NdrFcShort( 0x0 ),	/* 0 */
/* 784 */	NdrFcShort( 0x0 ),	/* 0 */
/* 786 */	NdrFcShort( 0x2 ),	/* 2 */
/* 788 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 790 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter priority */

/* 792 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 794 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 796 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 798 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 800 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 802 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_postInstallAction */


	/* Procedure get_Count */

/* 804 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 806 */	NdrFcLong( 0x0 ),	/* 0 */
/* 810 */	NdrFcShort( 0x17 ),	/* 23 */
/* 812 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 814 */	NdrFcShort( 0x0 ),	/* 0 */
/* 816 */	NdrFcShort( 0x24 ),	/* 36 */
/* 818 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 820 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 822 */	NdrFcShort( 0x0 ),	/* 0 */
/* 824 */	NdrFcShort( 0x0 ),	/* 0 */
/* 826 */	NdrFcShort( 0x0 ),	/* 0 */
/* 828 */	NdrFcShort( 0x2 ),	/* 2 */
/* 830 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 832 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentState0015 */


	/* Parameter count */

/* 834 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 836 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 838 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 840 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 842 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 844 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_Item */

/* 846 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 848 */	NdrFcLong( 0x0 ),	/* 0 */
/* 852 */	NdrFcShort( 0x18 ),	/* 24 */
/* 854 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 856 */	NdrFcShort( 0x8 ),	/* 8 */
/* 858 */	NdrFcShort( 0x8 ),	/* 8 */
/* 860 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 862 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 864 */	NdrFcShort( 0x0 ),	/* 0 */
/* 866 */	NdrFcShort( 0x0 ),	/* 0 */
/* 868 */	NdrFcShort( 0x0 ),	/* 0 */
/* 870 */	NdrFcShort( 0x3 ),	/* 3 */
/* 872 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 874 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter index */

/* 876 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 878 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 880 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter app */

/* 882 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 884 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 886 */	NdrFcShort( 0x6 ),	/* Type Offset=6 */

	/* Return value */

/* 888 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 890 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 892 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_altTokens */

/* 894 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 896 */	NdrFcLong( 0x0 ),	/* 0 */
/* 900 */	NdrFcShort( 0x19 ),	/* 25 */
/* 902 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 904 */	NdrFcShort( 0x18 ),	/* 24 */
/* 906 */	NdrFcShort( 0x8 ),	/* 8 */
/* 908 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 910 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 912 */	NdrFcShort( 0x0 ),	/* 0 */
/* 914 */	NdrFcShort( 0x0 ),	/* 0 */
/* 916 */	NdrFcShort( 0x0 ),	/* 0 */
/* 918 */	NdrFcShort( 0x4 ),	/* 4 */
/* 920 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 922 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 924 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter impersonation_token */

/* 926 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 928 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 930 */	0xb9,		/* FC_UINT3264 */
			0x0,		/* 0 */

	/* Parameter primary_token */

/* 932 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 934 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 936 */	0xb9,		/* FC_UINT3264 */
			0x0,		/* 0 */

	/* Parameter caller_proc_id */

/* 938 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 940 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 942 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 944 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 946 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 948 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_parentHWND */

/* 950 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 952 */	NdrFcLong( 0x0 ),	/* 0 */
/* 956 */	NdrFcShort( 0x1a ),	/* 26 */
/* 958 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 960 */	NdrFcShort( 0x8 ),	/* 8 */
/* 962 */	NdrFcShort( 0x8 ),	/* 8 */
/* 964 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 966 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 968 */	NdrFcShort( 0x0 ),	/* 0 */
/* 970 */	NdrFcShort( 0x0 ),	/* 0 */
/* 972 */	NdrFcShort( 0x0 ),	/* 0 */
/* 974 */	NdrFcShort( 0x2 ),	/* 2 */
/* 976 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 978 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter hwnd */

/* 980 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 982 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 984 */	0xb9,		/* FC_UINT3264 */
			0x0,		/* 0 */

	/* Return value */

/* 986 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 988 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 990 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure initialize */

/* 992 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 994 */	NdrFcLong( 0x0 ),	/* 0 */
/* 998 */	NdrFcShort( 0x1b ),	/* 27 */
/* 1000 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1002 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1004 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1006 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1008 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1010 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1012 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1014 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1016 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1018 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 1020 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1022 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1024 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_command */


	/* Procedure createApp */

/* 1026 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1028 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1032 */	NdrFcShort( 0x1c ),	/* 28 */
/* 1034 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1036 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1038 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1040 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 1042 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 1044 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1046 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1048 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1050 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1052 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 1054 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter command_id */


	/* Parameter app_id */

/* 1056 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1058 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1060 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Parameter command */


	/* Parameter app */

/* 1062 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 1064 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1066 */	NdrFcShort( 0x6 ),	/* Type Offset=6 */

	/* Return value */


	/* Return value */

/* 1068 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1070 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1072 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure createInstalledApp */

/* 1074 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1076 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1080 */	NdrFcShort( 0x1d ),	/* 29 */
/* 1082 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1084 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1086 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1088 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 1090 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 1092 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1094 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1096 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1098 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1100 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 1102 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 1104 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1106 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1108 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Parameter app */

/* 1110 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 1112 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1114 */	NdrFcShort( 0x6 ),	/* Type Offset=6 */

	/* Return value */

/* 1116 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1118 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1120 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure createAllInstalledApps */

/* 1122 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1124 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1128 */	NdrFcShort( 0x1e ),	/* 30 */
/* 1130 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1132 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1134 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1136 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1138 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1140 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1142 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1144 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1146 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1148 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 1150 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1152 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1154 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure checkForUpdate */

/* 1156 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1158 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1162 */	NdrFcShort( 0x1f ),	/* 31 */
/* 1164 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1166 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1168 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1170 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1172 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1174 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1176 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1178 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1180 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1182 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 1184 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1186 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1188 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure download */

/* 1190 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1192 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1196 */	NdrFcShort( 0x20 ),	/* 32 */
/* 1198 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1200 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1202 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1204 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1206 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1208 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1210 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1212 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1214 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1216 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 1218 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1220 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1222 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure install */

/* 1224 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1226 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1230 */	NdrFcShort( 0x21 ),	/* 33 */
/* 1232 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1234 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1236 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1238 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1240 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1242 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1244 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1246 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1248 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1250 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 1252 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1254 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1256 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure updateAllApps */

/* 1258 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1260 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1264 */	NdrFcShort( 0x22 ),	/* 34 */
/* 1266 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1268 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1270 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1272 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1274 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1276 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1278 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1280 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1282 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1284 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 1286 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1288 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1290 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure stop */

/* 1292 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1294 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1298 */	NdrFcShort( 0x23 ),	/* 35 */
/* 1300 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1302 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1304 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1306 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1308 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1310 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1312 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1314 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1316 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1318 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 1320 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1322 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1324 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure pause */

/* 1326 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1328 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1332 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1334 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1336 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1338 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1340 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1342 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1344 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1346 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1348 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1350 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1352 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 1354 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1356 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1358 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure resume */

/* 1360 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1362 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1366 */	NdrFcShort( 0x25 ),	/* 37 */
/* 1368 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1370 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1372 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1374 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 1376 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1378 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1380 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1382 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1384 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1386 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 1388 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1390 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1392 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure isBusy */

/* 1394 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1396 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1400 */	NdrFcShort( 0x26 ),	/* 38 */
/* 1402 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1404 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1406 */	NdrFcShort( 0x22 ),	/* 34 */
/* 1408 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 1410 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1412 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1414 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1416 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1418 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1420 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1422 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter is_busy */

/* 1424 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1426 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1428 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */

/* 1430 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1432 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1434 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure downloadPackage */

/* 1436 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1438 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1442 */	NdrFcShort( 0x27 ),	/* 39 */
/* 1444 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1446 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1448 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1450 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 1452 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 1454 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1456 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1458 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1460 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1462 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 1464 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 1466 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1468 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1470 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Parameter package_name */

/* 1472 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1474 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1476 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */

/* 1478 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1480 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1482 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_currentState */

/* 1484 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1486 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1490 */	NdrFcShort( 0x28 ),	/* 40 */
/* 1492 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1494 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1496 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1498 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1500 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1502 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1504 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1506 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1508 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1510 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1512 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter current_state */

/* 1514 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 1516 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1518 */	NdrFcShort( 0x3fa ),	/* Type Offset=1018 */

	/* Return value */

/* 1520 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1522 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1524 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure createAppBundleWeb */


	/* Procedure get_currentVersion */

/* 1526 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1528 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1532 */	NdrFcShort( 0x7 ),	/* 7 */
/* 1534 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1536 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1538 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1540 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1542 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1544 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1546 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1548 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1550 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1552 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1554 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter app_bundle_web */


	/* Parameter current */

/* 1556 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 1558 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1560 */	NdrFcShort( 0x6 ),	/* Type Offset=6 */

	/* Return value */


	/* Return value */

/* 1562 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1564 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1566 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_currentVersionWeb */


	/* Procedure get_nextVersion */

/* 1568 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1570 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1574 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1576 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1578 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1580 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1582 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1584 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1586 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1588 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1590 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1592 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1594 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1596 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter current */


	/* Parameter next */

/* 1598 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 1600 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1602 */	NdrFcShort( 0x6 ),	/* Type Offset=6 */

	/* Return value */


	/* Return value */

/* 1604 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1606 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1608 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_displayLanguage */


	/* Procedure get_displayName */

/* 1610 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1612 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1616 */	NdrFcShort( 0xa ),	/* 10 */
/* 1618 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1620 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1622 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1624 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1626 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1628 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1630 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1632 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1634 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1636 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1638 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IAppBundleWeb0000 */


	/* Parameter __MIDL__IApp0001 */

/* 1640 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 1642 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1644 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */


	/* Return value */

/* 1646 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1648 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1650 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_displayLanguage */


	/* Procedure put_displayName */

/* 1652 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1654 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1658 */	NdrFcShort( 0xb ),	/* 11 */
/* 1660 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1662 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1664 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1666 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1668 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 1670 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1672 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1674 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1676 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1678 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1680 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IAppBundleWeb0001 */


	/* Parameter __MIDL__IApp0002 */

/* 1682 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1684 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1686 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */


	/* Return value */

/* 1688 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1690 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1692 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_language */

/* 1694 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1696 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1700 */	NdrFcShort( 0xc ),	/* 12 */
/* 1702 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1704 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1706 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1708 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1710 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1712 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1714 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1716 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1718 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1720 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1722 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0003 */

/* 1724 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 1726 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1728 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */

/* 1730 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1732 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1734 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_language */

/* 1736 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1738 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1742 */	NdrFcShort( 0xd ),	/* 13 */
/* 1744 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1746 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1748 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1750 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1752 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 1754 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1756 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1758 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1760 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1762 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1764 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0004 */

/* 1766 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1768 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1770 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */

/* 1772 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1774 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1776 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_ap */

/* 1778 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1780 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1784 */	NdrFcShort( 0xe ),	/* 14 */
/* 1786 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1788 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1790 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1792 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1794 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1796 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1798 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1800 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1802 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1804 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1806 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0005 */

/* 1808 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 1810 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1812 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */

/* 1814 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1816 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1818 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_ap */

/* 1820 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1822 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1826 */	NdrFcShort( 0xf ),	/* 15 */
/* 1828 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1830 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1832 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1834 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1836 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 1838 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1840 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1842 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1844 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1846 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1848 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0006 */

/* 1850 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1852 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1854 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */

/* 1856 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1858 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1860 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_ttToken */

/* 1862 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1864 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1868 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1870 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1872 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1874 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1876 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1878 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1880 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1882 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1884 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1886 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1888 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1890 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0007 */

/* 1892 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 1894 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1896 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */

/* 1898 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1900 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1902 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_ttToken */

/* 1904 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1906 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1910 */	NdrFcShort( 0x11 ),	/* 17 */
/* 1912 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1914 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1916 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1918 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1920 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 1922 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1924 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1926 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1928 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1930 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1932 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0008 */

/* 1934 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1936 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1938 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */

/* 1940 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1942 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1944 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_completionMessage */


	/* Procedure get_iid */

/* 1946 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1948 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1952 */	NdrFcShort( 0x12 ),	/* 18 */
/* 1954 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1956 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1958 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1960 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1962 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1964 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1966 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1968 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1970 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1972 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1974 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentState0010 */


	/* Parameter __MIDL__IApp0009 */

/* 1976 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 1978 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1980 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */


	/* Return value */

/* 1982 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1984 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1986 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_iid */

/* 1988 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1990 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1994 */	NdrFcShort( 0x13 ),	/* 19 */
/* 1996 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1998 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2000 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2002 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 2004 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 2006 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2008 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2010 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2012 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2014 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2016 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0010 */

/* 2018 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 2020 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2022 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */

/* 2024 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2026 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2028 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_brandCode */

/* 2030 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2032 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2036 */	NdrFcShort( 0x14 ),	/* 20 */
/* 2038 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2040 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2042 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2044 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 2046 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2048 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2050 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2052 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2054 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2056 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2058 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0011 */

/* 2060 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2062 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2064 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */

/* 2066 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2068 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2070 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_brandCode */

/* 2072 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2074 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2078 */	NdrFcShort( 0x15 ),	/* 21 */
/* 2080 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2082 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2084 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2086 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 2088 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 2090 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2092 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2094 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2096 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2098 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2100 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0012 */

/* 2102 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 2104 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2106 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */

/* 2108 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2110 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2112 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_postInstallUrl */


	/* Procedure get_clientId */

/* 2114 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2116 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2120 */	NdrFcShort( 0x16 ),	/* 22 */
/* 2122 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2124 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2126 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2128 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 2130 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2132 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2134 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2136 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2138 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2140 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2142 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentState0014 */


	/* Parameter __MIDL__IApp0013 */

/* 2144 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2146 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2148 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */


	/* Return value */

/* 2150 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2152 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2154 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_clientId */

/* 2156 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2158 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2162 */	NdrFcShort( 0x17 ),	/* 23 */
/* 2164 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2166 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2168 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2170 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 2172 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 2174 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2176 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2178 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2180 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2182 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2184 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0014 */

/* 2186 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 2188 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2190 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */

/* 2192 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2194 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2196 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_labels */

/* 2198 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2200 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2204 */	NdrFcShort( 0x18 ),	/* 24 */
/* 2206 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2208 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2210 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2212 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 2214 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2216 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2218 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2220 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2222 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2224 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2226 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0015 */

/* 2228 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2230 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2232 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */

/* 2234 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2236 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2238 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_labels */

/* 2240 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2242 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2246 */	NdrFcShort( 0x19 ),	/* 25 */
/* 2248 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2250 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2252 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2254 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 2256 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 2258 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2260 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2262 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2264 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2266 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2268 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0016 */

/* 2270 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 2272 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2274 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */

/* 2276 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2278 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2280 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_referralId */

/* 2282 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2284 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2288 */	NdrFcShort( 0x1a ),	/* 26 */
/* 2290 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2292 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2294 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2296 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 2298 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2300 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2302 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2304 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2306 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2308 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2310 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0017 */

/* 2312 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2314 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2316 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */

/* 2318 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2320 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2322 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_referralId */

/* 2324 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2326 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2330 */	NdrFcShort( 0x1b ),	/* 27 */
/* 2332 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2334 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2336 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2338 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 2340 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 2342 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2344 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2346 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2348 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2350 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2352 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0018 */

/* 2354 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 2356 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2358 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */

/* 2360 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2362 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2364 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_browserType */

/* 2366 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2368 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2372 */	NdrFcShort( 0x1d ),	/* 29 */
/* 2374 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2376 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2378 */	NdrFcShort( 0x24 ),	/* 36 */
/* 2380 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 2382 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2384 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2386 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2388 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2390 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2392 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2394 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0019 */

/* 2396 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2398 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2400 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2402 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2404 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2406 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_browserType */

/* 2408 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2410 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2414 */	NdrFcShort( 0x1e ),	/* 30 */
/* 2416 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2418 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2420 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2422 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 2424 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2426 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2428 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2430 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2432 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2434 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2436 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0020 */

/* 2438 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2440 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2442 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2444 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2446 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2448 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_clientInstallData */

/* 2450 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2452 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2456 */	NdrFcShort( 0x1f ),	/* 31 */
/* 2458 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2460 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2462 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2464 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 2466 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2468 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2470 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2472 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2474 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2476 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2478 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0021 */

/* 2480 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2482 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2484 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */

/* 2486 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2488 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2490 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_clientInstallData */

/* 2492 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2494 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2498 */	NdrFcShort( 0x20 ),	/* 32 */
/* 2500 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2502 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2504 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2506 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 2508 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 2510 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2512 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2514 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2516 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2518 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2520 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0022 */

/* 2522 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 2524 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2526 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */

/* 2528 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2530 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2532 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_serverInstallDataIndex */

/* 2534 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2536 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2540 */	NdrFcShort( 0x21 ),	/* 33 */
/* 2542 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2544 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2546 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2548 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 2550 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2552 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2554 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2556 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2558 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2560 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2562 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0023 */

/* 2564 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2566 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2568 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */

/* 2570 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2572 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2574 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_serverInstallDataIndex */

/* 2576 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2578 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2582 */	NdrFcShort( 0x22 ),	/* 34 */
/* 2584 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2586 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2588 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2590 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 2592 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 2594 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2596 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2598 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2600 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2602 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2604 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0024 */

/* 2606 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 2608 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2610 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */

/* 2612 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2614 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2616 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isEulaAccepted */

/* 2618 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2620 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2624 */	NdrFcShort( 0x23 ),	/* 35 */
/* 2626 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2628 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2630 */	NdrFcShort( 0x22 ),	/* 34 */
/* 2632 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 2634 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2636 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2638 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2640 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2642 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2644 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2646 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0025 */

/* 2648 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2650 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2652 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */

/* 2654 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2656 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2658 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_isEulaAccepted */

/* 2660 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2662 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2666 */	NdrFcShort( 0x24 ),	/* 36 */
/* 2668 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2670 */	NdrFcShort( 0x6 ),	/* 6 */
/* 2672 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2674 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 2676 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2678 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2680 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2682 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2684 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2686 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2688 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0026 */

/* 2690 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2692 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2694 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */

/* 2696 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2698 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2700 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_usageStatsEnable */

/* 2702 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2704 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2708 */	NdrFcShort( 0x25 ),	/* 37 */
/* 2710 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2712 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2714 */	NdrFcShort( 0x24 ),	/* 36 */
/* 2716 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 2718 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2720 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2722 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2724 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2726 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2728 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2730 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0027 */

/* 2732 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2734 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2736 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2738 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2740 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2742 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_usageStatsEnable */

/* 2744 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2746 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2750 */	NdrFcShort( 0x26 ),	/* 38 */
/* 2752 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2754 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2756 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2758 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 2760 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2762 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2764 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2766 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2768 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2770 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2772 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0028 */

/* 2774 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2776 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2778 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2780 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2782 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2784 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_installTimeDiffSec */

/* 2786 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2788 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2792 */	NdrFcShort( 0x27 ),	/* 39 */
/* 2794 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2796 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2798 */	NdrFcShort( 0x24 ),	/* 36 */
/* 2800 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 2802 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2804 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2806 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2808 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2810 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2812 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2814 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0029 */

/* 2816 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2818 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2820 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2822 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2824 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2826 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_currentState */

/* 2828 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2830 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2834 */	NdrFcShort( 0x28 ),	/* 40 */
/* 2836 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2838 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2840 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2842 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 2844 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2846 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2848 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2850 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2852 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2854 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2856 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp0030 */

/* 2858 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 2860 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2862 */	NdrFcShort( 0x6 ),	/* Type Offset=6 */

	/* Return value */

/* 2864 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2866 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2868 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_untrustedData */

/* 2870 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2872 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2876 */	NdrFcShort( 0x29 ),	/* 41 */
/* 2878 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2880 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2882 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2884 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 2886 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2888 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2890 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2892 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2894 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2896 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2898 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp20000 */

/* 2900 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2902 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2904 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */

/* 2906 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2908 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2910 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_untrustedData */

/* 2912 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2914 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2918 */	NdrFcShort( 0x2a ),	/* 42 */
/* 2920 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2922 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2924 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2926 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 2928 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 2930 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2932 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2934 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2936 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2938 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2940 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IApp20001 */

/* 2942 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 2944 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2946 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */

/* 2948 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2950 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2952 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isDone */


	/* Procedure get_isWebAccessible */

/* 2954 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2956 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2960 */	NdrFcShort( 0x7 ),	/* 7 */
/* 2962 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2964 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2966 */	NdrFcShort( 0x22 ),	/* 34 */
/* 2968 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 2970 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2972 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2974 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2976 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2978 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2980 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2982 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter is_done */


	/* Parameter __MIDL__IAppCommand0000 */

/* 2984 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2986 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2988 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 2990 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2992 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2994 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_completionHResult */


	/* Procedure get_packageCount */


	/* Procedure get_exitCode */


	/* Procedure get_packageCount */


	/* Procedure get_status */

/* 2996 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2998 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3002 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3004 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3006 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3008 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3010 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3012 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3014 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3016 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3018 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3020 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3022 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3024 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter hr */


	/* Parameter count */


	/* Parameter __MIDL__IAppCommandWeb0001 */


	/* Parameter count */


	/* Parameter __MIDL__IAppCommand0001 */

/* 3026 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3028 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3030 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 3032 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3034 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3036 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_bytesDownloaded */


	/* Procedure get_exitCode */

/* 3038 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3040 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3044 */	NdrFcShort( 0x9 ),	/* 9 */
/* 3046 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3048 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3050 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3052 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3054 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3056 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3058 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3060 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3062 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3064 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3066 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentState0002 */


	/* Parameter __MIDL__IAppCommand0002 */

/* 3068 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3070 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3072 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 3074 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3076 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3078 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure execute */


	/* Procedure execute */

/* 3080 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3082 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3086 */	NdrFcShort( 0xa ),	/* 10 */
/* 3088 */	NdrFcShort( 0x58 ),	/* ARM64 Stack size/offset = 88 */
/* 3090 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3092 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3094 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0xa,		/* 10 */
/* 3096 */	0x16,		/* 22 */
			0x85,		/* Ext Flags:  new corr desc, srv corr check, has big byval param */
/* 3098 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3100 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3102 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3104 */	NdrFcShort( 0xa ),	/* 10 */
/* 3106 */	0xa,		/* 10 */
			0x80,		/* 128 */
/* 3108 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 3110 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 3112 */	0x85,		/* 133 */
			0x86,		/* 134 */
/* 3114 */	0x87,		/* 135 */
			0xf8,		/* 248 */
/* 3116 */	0xf8,		/* 248 */
			0x0,		/* 0 */

	/* Parameter arg1 */


	/* Parameter arg1 */

/* 3118 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 3120 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3122 */	NdrFcShort( 0x40c ),	/* Type Offset=1036 */

	/* Parameter arg2 */


	/* Parameter arg2 */

/* 3124 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 3126 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3128 */	NdrFcShort( 0x40c ),	/* Type Offset=1036 */

	/* Parameter arg3 */


	/* Parameter arg3 */

/* 3130 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 3132 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3134 */	NdrFcShort( 0x40c ),	/* Type Offset=1036 */

	/* Parameter arg4 */


	/* Parameter arg4 */

/* 3136 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 3138 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3140 */	NdrFcShort( 0x40c ),	/* Type Offset=1036 */

	/* Parameter arg5 */


	/* Parameter arg5 */

/* 3142 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 3144 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 3146 */	NdrFcShort( 0x40c ),	/* Type Offset=1036 */

	/* Parameter arg6 */


	/* Parameter arg6 */

/* 3148 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 3150 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 3152 */	NdrFcShort( 0x40c ),	/* Type Offset=1036 */

	/* Parameter arg7 */


	/* Parameter arg7 */

/* 3154 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 3156 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 3158 */	NdrFcShort( 0x40c ),	/* Type Offset=1036 */

	/* Parameter arg8 */


	/* Parameter arg8 */

/* 3160 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 3162 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 3164 */	NdrFcShort( 0x40c ),	/* Type Offset=1036 */

	/* Parameter arg9 */


	/* Parameter arg9 */

/* 3166 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 3168 */	NdrFcShort( 0x48 ),	/* ARM64 Stack size/offset = 72 */
/* 3170 */	NdrFcShort( 0x40c ),	/* Type Offset=1036 */

	/* Return value */


	/* Return value */

/* 3172 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3174 */	NdrFcShort( 0x50 ),	/* ARM64 Stack size/offset = 80 */
/* 3176 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_packageWeb */


	/* Procedure get_package */

/* 3178 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3180 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3184 */	NdrFcShort( 0x9 ),	/* 9 */
/* 3186 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3188 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3190 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3192 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 3194 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3196 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3198 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3200 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3202 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3204 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3206 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter index */


	/* Parameter index */

/* 3208 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3210 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3212 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter package */


	/* Parameter package */

/* 3214 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3216 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3218 */	NdrFcShort( 0x6 ),	/* Type Offset=6 */

	/* Return value */


	/* Return value */

/* 3220 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3222 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3224 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get */

/* 3226 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3228 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3232 */	NdrFcShort( 0x7 ),	/* 7 */
/* 3234 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3236 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3238 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3240 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 3242 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 3244 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3246 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3248 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3250 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3252 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3254 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter dir */

/* 3256 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 3258 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3260 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */

/* 3262 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3264 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3266 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isAvailable */

/* 3268 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3270 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3274 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3276 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3278 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3280 */	NdrFcShort( 0x22 ),	/* 34 */
/* 3282 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3284 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3286 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3288 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3290 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3292 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3294 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3296 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__IPackage0000 */

/* 3298 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3300 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3302 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */

/* 3304 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3306 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3308 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_availableVersion */

/* 3310 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3312 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3316 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3318 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3320 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3322 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3324 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 3326 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 3328 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3330 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3332 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3334 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3336 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3338 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentState0001 */

/* 3340 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 3342 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3344 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */

/* 3346 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3348 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3350 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_packageCacheSizeLimitMBytes */


	/* Procedure get_totalBytesToDownload */

/* 3352 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3354 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3358 */	NdrFcShort( 0xa ),	/* 10 */
/* 3360 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3362 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3364 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3366 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3368 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3370 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3372 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3374 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3376 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3378 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3380 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter limit */


	/* Parameter __MIDL__ICurrentState0003 */

/* 3382 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3384 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3386 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 3388 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3390 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3392 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_packageCacheExpirationTimeDays */


	/* Procedure get_downloadTimeRemainingMs */

/* 3394 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3396 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3400 */	NdrFcShort( 0xb ),	/* 11 */
/* 3402 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3404 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3406 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3408 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3410 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3412 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3414 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3416 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3418 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3420 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3422 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter days */


	/* Parameter __MIDL__ICurrentState0004 */

/* 3424 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3426 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3428 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 3430 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3432 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3434 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nextRetryTime */

/* 3436 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3438 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3442 */	NdrFcShort( 0xc ),	/* 12 */
/* 3444 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3446 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3448 */	NdrFcShort( 0x2c ),	/* 44 */
/* 3450 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3452 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3454 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3456 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3458 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3460 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3462 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3464 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentState0005 */

/* 3466 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3468 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3470 */	0xb,		/* FC_HYPER */
			0x0,		/* 0 */

	/* Return value */

/* 3472 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3474 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3476 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_length */


	/* Procedure get_installProgress */

/* 3478 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3480 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3484 */	NdrFcShort( 0xd ),	/* 13 */
/* 3486 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3488 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3490 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3492 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3494 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3496 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3498 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3500 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3502 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3504 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3506 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter index */


	/* Parameter __MIDL__ICurrentState0006 */

/* 3508 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3510 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3512 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 3514 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3516 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3518 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_installTimeRemainingMs */

/* 3520 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3522 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3526 */	NdrFcShort( 0xe ),	/* 14 */
/* 3528 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3530 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3532 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3534 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3536 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3538 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3540 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3542 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3544 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3546 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3548 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentState0007 */

/* 3550 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3552 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3554 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3556 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3558 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3560 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isCanceled */

/* 3562 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3564 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3568 */	NdrFcShort( 0xf ),	/* 15 */
/* 3570 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3572 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3574 */	NdrFcShort( 0x22 ),	/* 34 */
/* 3576 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3578 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3580 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3582 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3584 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3586 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3588 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3590 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter is_canceled */

/* 3592 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3594 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3596 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */

/* 3598 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3600 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3602 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_errorCode */

/* 3604 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3606 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3610 */	NdrFcShort( 0x10 ),	/* 16 */
/* 3612 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3614 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3616 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3618 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3620 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3622 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3624 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3626 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3628 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3630 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3632 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentState0008 */

/* 3634 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3636 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3638 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3640 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3642 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3644 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_extraCode1 */

/* 3646 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3648 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3652 */	NdrFcShort( 0x11 ),	/* 17 */
/* 3654 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3656 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3658 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3660 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3662 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3664 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3666 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3668 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3670 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3672 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3674 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentState0009 */

/* 3676 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3678 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3680 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3682 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3684 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3686 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_installerResultCode */

/* 3688 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3690 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3694 */	NdrFcShort( 0x13 ),	/* 19 */
/* 3696 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3698 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3700 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3702 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3704 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3706 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3708 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3710 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3712 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3714 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3716 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentState0011 */

/* 3718 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3720 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3722 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3724 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3726 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3728 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_installerResultExtraCode1 */

/* 3730 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3732 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3736 */	NdrFcShort( 0x14 ),	/* 20 */
/* 3738 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3740 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3742 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3744 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3746 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3748 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3750 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3752 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3754 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3756 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3758 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentState0012 */

/* 3760 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3762 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3764 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3766 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3768 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3770 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_postInstallLaunchCommandLine */

/* 3772 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3774 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3778 */	NdrFcShort( 0x15 ),	/* 21 */
/* 3780 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3782 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3784 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3786 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 3788 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 3790 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3792 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3794 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3796 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3798 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3800 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter __MIDL__ICurrentState0013 */

/* 3802 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 3804 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3806 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */

/* 3808 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3810 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3812 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure UpdateRegistry */

/* 3814 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3816 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3820 */	NdrFcShort( 0x7 ),	/* 7 */
/* 3822 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3824 */	NdrFcShort( 0x6 ),	/* 6 */
/* 3826 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3828 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 3830 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 3832 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3834 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3836 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3838 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3840 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3842 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 3844 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 3846 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3848 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Parameter is_machine */

/* 3850 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3852 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3854 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */

/* 3856 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3858 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3860 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure QueryUserForCredentials */

/* 3862 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3864 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3868 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3870 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 3872 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3874 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3876 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x6,		/* 6 */
/* 3878 */	0x12,		/* 18 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 3880 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3882 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3884 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3886 */	NdrFcShort( 0x6 ),	/* 6 */
/* 3888 */	0x6,		/* 6 */
			0x80,		/* 128 */
/* 3890 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 3892 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 3894 */	0x85,		/* 133 */
			0x0,		/* 0 */

	/* Parameter owner_hwnd */

/* 3896 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3898 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3900 */	0xb9,		/* FC_UINT3264 */
			0x0,		/* 0 */

	/* Parameter server */

/* 3902 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 3904 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3906 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Parameter message */

/* 3908 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 3910 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3912 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Parameter username */

/* 3914 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 3916 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3918 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Parameter password */

/* 3920 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 3922 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 3924 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */

/* 3926 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3928 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 3930 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_updatesSuppressedTimes */

/* 3932 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3934 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3938 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3940 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 3942 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3944 */	NdrFcShort( 0x76 ),	/* 118 */
/* 3946 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x5,		/* 5 */
/* 3948 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3950 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3952 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3954 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3956 */	NdrFcShort( 0x5 ),	/* 5 */
/* 3958 */	0x5,		/* 5 */
			0x80,		/* 128 */
/* 3960 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 3962 */	0x83,		/* 131 */
			0x84,		/* 132 */

	/* Parameter start_hour */

/* 3964 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3966 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3968 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter start_min */

/* 3970 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3972 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3974 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter duration_min */

/* 3976 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3978 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3980 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter are_updates_suppressed */

/* 3982 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3984 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3986 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */

/* 3988 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3990 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 3992 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_effectivePolicyForAppInstalls */

/* 3994 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3996 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4000 */	NdrFcShort( 0xc ),	/* 12 */
/* 4002 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4004 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4006 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4008 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 4010 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 4012 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4014 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4016 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4018 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4020 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4022 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 4024 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 4026 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4028 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Parameter policy */

/* 4030 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4032 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4034 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4036 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4038 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4040 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_effectivePolicyForAppUpdates */

/* 4042 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4044 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4048 */	NdrFcShort( 0xd ),	/* 13 */
/* 4050 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4052 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4054 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4056 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 4058 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 4060 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4062 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4064 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4066 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4068 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4070 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 4072 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 4074 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4076 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Parameter policy */

/* 4078 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4080 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4082 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4084 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4086 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4088 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_targetVersionPrefix */

/* 4090 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4092 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4096 */	NdrFcShort( 0xe ),	/* 14 */
/* 4098 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4100 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4102 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4104 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 4106 */	0xe,		/* 14 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 4108 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4110 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4112 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4114 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4116 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4118 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 4120 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 4122 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4124 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Parameter prefix */

/* 4126 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 4128 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4130 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */

/* 4132 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4134 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4136 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isRollbackToTargetVersionAllowed */

/* 4138 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4140 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4144 */	NdrFcShort( 0xf ),	/* 15 */
/* 4146 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4148 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4150 */	NdrFcShort( 0x22 ),	/* 34 */
/* 4152 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 4154 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 4156 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4158 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4160 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4162 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4164 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4166 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 4168 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 4170 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4172 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Parameter rollback_allowed */

/* 4174 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4176 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4178 */	0x6,		/* FC_SHORT */
			0x0,		/* 0 */

	/* Return value */

/* 4180 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4182 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4184 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure setOriginURL */

/* 4186 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4188 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4192 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4194 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4196 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4198 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4200 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 4202 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 4204 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4206 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4208 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4210 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4212 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4214 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter origin_url */

/* 4216 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 4218 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4220 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */

/* 4222 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4224 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4226 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure createApp */

/* 4228 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4230 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4234 */	NdrFcShort( 0x7 ),	/* 7 */
/* 4236 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 4238 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4240 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4242 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 4244 */	0x10,		/* 16 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 4246 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4248 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4250 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4252 */	NdrFcShort( 0x5 ),	/* 5 */
/* 4254 */	0x5,		/* 5 */
			0x80,		/* 128 */
/* 4256 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 4258 */	0x83,		/* 131 */
			0x84,		/* 132 */

	/* Parameter app_guid */

/* 4260 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 4262 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4264 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Parameter brand_code */

/* 4266 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 4268 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4270 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Parameter language */

/* 4272 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 4274 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4276 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Parameter ap */

/* 4278 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 4280 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4282 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */

/* 4284 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4286 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 4288 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure OnInstalling */


	/* Procedure createAllInstalledApps */

/* 4290 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4292 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4296 */	NdrFcShort( 0x9 ),	/* 9 */
/* 4298 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4300 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4302 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4304 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 4306 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4308 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4310 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4312 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4314 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4316 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */


	/* Return value */

/* 4318 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4320 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4322 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure put_parentHWND */

/* 4324 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4326 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4330 */	NdrFcShort( 0xc ),	/* 12 */
/* 4332 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4334 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4336 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4338 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4340 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4342 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4344 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4346 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4348 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4350 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4352 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter hwnd */

/* 4354 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4356 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4358 */	0xb9,		/* FC_UINT3264 */
			0x0,		/* 0 */

	/* Return value */

/* 4360 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4362 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4364 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_appWeb */

/* 4366 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4368 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4372 */	NdrFcShort( 0xe ),	/* 14 */
/* 4374 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4376 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4378 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4380 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 4382 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4384 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4386 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4388 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4390 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4392 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4394 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter index */

/* 4396 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4398 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4400 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter app_web */

/* 4402 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 4404 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4406 */	NdrFcShort( 0x6 ),	/* Type Offset=6 */

	/* Return value */

/* 4408 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4410 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4412 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure initialize */

/* 4414 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4416 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4420 */	NdrFcShort( 0xf ),	/* 15 */
/* 4422 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4424 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4426 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4428 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 4430 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4432 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4434 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4436 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4438 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4440 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 4442 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4444 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4446 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure checkForUpdate */

/* 4448 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4450 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4454 */	NdrFcShort( 0x10 ),	/* 16 */
/* 4456 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4458 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4460 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4462 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 4464 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4466 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4468 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4470 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4472 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4474 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 4476 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4478 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4480 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure download */

/* 4482 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4484 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4488 */	NdrFcShort( 0x11 ),	/* 17 */
/* 4490 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4492 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4494 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4496 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 4498 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4500 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4502 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4504 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4506 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4508 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 4510 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4512 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4514 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure install */

/* 4516 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4518 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4522 */	NdrFcShort( 0x12 ),	/* 18 */
/* 4524 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4526 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4528 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4530 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 4532 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4534 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4536 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4538 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4540 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4542 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 4544 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4546 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4548 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure pause */

/* 4550 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4552 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4556 */	NdrFcShort( 0x13 ),	/* 19 */
/* 4558 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4560 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4562 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4564 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 4566 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4568 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4570 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4572 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4574 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4576 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 4578 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4580 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4582 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure resume */

/* 4584 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4586 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4590 */	NdrFcShort( 0x14 ),	/* 20 */
/* 4592 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4594 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4596 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4598 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 4600 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4602 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4604 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4606 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4608 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4610 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 4612 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4614 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4616 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure cancel */

/* 4618 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4620 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4624 */	NdrFcShort( 0x15 ),	/* 21 */
/* 4626 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4628 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4630 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4632 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 4634 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4636 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4638 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4640 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4642 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4644 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 4646 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4648 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4650 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure downloadPackage */

/* 4652 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4654 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4658 */	NdrFcShort( 0x16 ),	/* 22 */
/* 4660 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4662 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4664 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4666 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 4668 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 4670 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4672 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4674 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4676 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4678 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4680 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_id */

/* 4682 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 4684 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4686 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Parameter package_name */

/* 4688 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 4690 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4692 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Return value */

/* 4694 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4696 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4698 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_currentState */

/* 4700 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4702 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4706 */	NdrFcShort( 0x17 ),	/* 23 */
/* 4708 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4710 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4712 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4714 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 4716 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 4718 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4720 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4722 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4724 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4726 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4728 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter current_state */

/* 4730 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 4732 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4734 */	NdrFcShort( 0x3fa ),	/* Type Offset=1018 */

	/* Return value */

/* 4736 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4738 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4740 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_command */

/* 4742 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4744 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4748 */	NdrFcShort( 0xa ),	/* 10 */
/* 4750 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4752 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4754 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4756 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 4758 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 4760 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4762 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4764 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4766 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4768 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4770 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter command_id */

/* 4772 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 4774 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4776 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Parameter command */

/* 4778 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 4780 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4782 */	NdrFcShort( 0x6 ),	/* Type Offset=6 */

	/* Return value */

/* 4784 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4786 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4788 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure cancel */

/* 4790 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4792 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4796 */	NdrFcShort( 0xb ),	/* 11 */
/* 4798 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4800 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4802 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4804 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 4806 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4808 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4810 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4812 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4814 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4816 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 4818 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4820 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4822 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_currentState */

/* 4824 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4826 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4830 */	NdrFcShort( 0xc ),	/* 12 */
/* 4832 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4834 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4836 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4838 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 4840 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4842 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4844 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4846 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4848 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4850 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4852 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter current_state */

/* 4854 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 4856 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4858 */	NdrFcShort( 0x6 ),	/* Type Offset=6 */

	/* Return value */

/* 4860 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4862 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4864 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure launch */

/* 4866 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4868 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4872 */	NdrFcShort( 0xd ),	/* 13 */
/* 4874 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4876 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4878 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4880 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 4882 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4884 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4886 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4888 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4890 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4892 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 4894 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4896 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4898 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure uninstall */

/* 4900 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4902 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4906 */	NdrFcShort( 0xe ),	/* 14 */
/* 4908 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4910 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4912 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4914 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 4916 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4918 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4920 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4922 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4924 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4926 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 4928 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4930 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4932 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure createOmahaMachineServerAsync */

/* 4934 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4936 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4940 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4942 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 4944 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4946 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4948 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x4,		/* 4 */
/* 4950 */	0x10,		/* 16 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 4952 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4954 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4956 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4958 */	NdrFcShort( 0x4 ),	/* 4 */
/* 4960 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 4962 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 4964 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter origin_url */

/* 4966 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 4968 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4970 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Parameter create_elevated */

/* 4972 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4974 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4976 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter status */

/* 4978 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 4980 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4982 */	NdrFcShort( 0x41a ),	/* Type Offset=1050 */

	/* Return value */

/* 4984 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4986 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4988 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Send */

/* 4990 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4992 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4996 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4998 */	NdrFcShort( 0x48 ),	/* ARM64 Stack size/offset = 72 */
/* 5000 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5002 */	NdrFcShort( 0x24 ),	/* 36 */
/* 5004 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x8,		/* 8 */
/* 5006 */	0x14,		/* 20 */
			0x87,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, has big byval param */
/* 5008 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5010 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5012 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5014 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5016 */	0x8,		/* 8 */
			0x80,		/* 128 */
/* 5018 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 5020 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 5022 */	0x85,		/* 133 */
			0x86,		/* 134 */
/* 5024 */	0x87,		/* 135 */
			0x0,		/* 0 */

	/* Parameter url */

/* 5026 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 5028 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5030 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Parameter post_data */

/* 5032 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 5034 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5036 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Parameter request_headers */

/* 5038 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 5040 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5042 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Parameter response_headers_needed */

/* 5044 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 5046 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5048 */	NdrFcShort( 0x40c ),	/* Type Offset=1036 */

	/* Parameter response_headers */

/* 5050 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 5052 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 5054 */	NdrFcShort( 0x3fa ),	/* Type Offset=1018 */

	/* Parameter response_code */

/* 5056 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5058 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 5060 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter cache_filename */

/* 5062 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 5064 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 5066 */	NdrFcShort( 0x3a ),	/* Type Offset=58 */

	/* Return value */

/* 5068 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5070 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 5072 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure LaunchCmdLine */

/* 5074 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5076 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5080 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5082 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5084 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5086 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5088 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 5090 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5092 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5094 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5096 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5098 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5100 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5102 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter cmd_line */

/* 5104 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 5106 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5108 */	NdrFcShort( 0x432 ),	/* Type Offset=1074 */

	/* Return value */

/* 5110 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5112 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5114 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure LaunchBrowser */

/* 5116 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5118 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5122 */	NdrFcShort( 0x4 ),	/* 4 */
/* 5124 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5126 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5128 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5130 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 5132 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5134 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5136 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5138 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5140 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5142 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5144 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter browser_type */

/* 5146 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5148 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5150 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter url */

/* 5152 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 5154 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5156 */	NdrFcShort( 0x432 ),	/* Type Offset=1074 */

	/* Return value */

/* 5158 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5160 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5162 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure LaunchCmdElevated */

/* 5164 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5166 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5170 */	NdrFcShort( 0x5 ),	/* 5 */
/* 5172 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 5174 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5176 */	NdrFcShort( 0x24 ),	/* 36 */
/* 5178 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 5180 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5182 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5184 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5186 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5188 */	NdrFcShort( 0x5 ),	/* 5 */
/* 5190 */	0x5,		/* 5 */
			0x80,		/* 128 */
/* 5192 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 5194 */	0x83,		/* 131 */
			0x84,		/* 132 */

	/* Parameter app_guid */

/* 5196 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 5198 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5200 */	NdrFcShort( 0x432 ),	/* Type Offset=1074 */

	/* Parameter cmd_id */

/* 5202 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 5204 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5206 */	NdrFcShort( 0x432 ),	/* Type Offset=1074 */

	/* Parameter caller_proc_id */

/* 5208 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5210 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5212 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter proc_handle */

/* 5214 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5216 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5218 */	0xb9,		/* FC_UINT3264 */
			0x0,		/* 0 */

	/* Return value */

/* 5220 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5222 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 5224 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure LaunchCmdLineEx */

/* 5226 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5228 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5232 */	NdrFcShort( 0x6 ),	/* 6 */
/* 5234 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 5236 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5238 */	NdrFcShort( 0x5c ),	/* 92 */
/* 5240 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 5242 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5244 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5246 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5248 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5250 */	NdrFcShort( 0x5 ),	/* 5 */
/* 5252 */	0x5,		/* 5 */
			0x80,		/* 128 */
/* 5254 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 5256 */	0x83,		/* 131 */
			0x84,		/* 132 */

	/* Parameter cmd_line */

/* 5258 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 5260 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5262 */	NdrFcShort( 0x432 ),	/* Type Offset=1074 */

	/* Parameter server_proc_id */

/* 5264 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5266 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5268 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter proc_handle */

/* 5270 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5272 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5274 */	0xb9,		/* FC_UINT3264 */
			0x0,		/* 0 */

	/* Parameter stdout_handle */

/* 5276 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5278 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5280 */	0xb9,		/* FC_UINT3264 */
			0x0,		/* 0 */

	/* Return value */

/* 5282 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5284 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 5286 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure LaunchAppCommand */

/* 5288 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5290 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5294 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5296 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5298 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5300 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5302 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 5304 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5306 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5308 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5310 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5312 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5314 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5316 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter app_guid */

/* 5318 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 5320 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5322 */	NdrFcShort( 0x432 ),	/* Type Offset=1074 */

	/* Parameter cmd_id */

/* 5324 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 5326 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5328 */	NdrFcShort( 0x432 ),	/* Type Offset=1074 */

	/* Return value */

/* 5330 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5332 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5334 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure OnShow */


	/* Procedure DoClose */

/* 5336 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5338 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5342 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5344 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5346 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5348 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5350 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 5352 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5354 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5356 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5358 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5360 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5362 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */


	/* Return value */

/* 5364 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5366 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5368 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure OnCheckingForUpdate */


	/* Procedure DoPause */

/* 5370 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5372 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5376 */	NdrFcShort( 0x4 ),	/* 4 */
/* 5378 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5380 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5382 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5384 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 5386 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5388 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5390 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5392 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5394 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5396 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */


	/* Return value */

/* 5398 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5400 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5402 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure DoResume */

/* 5404 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5406 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5410 */	NdrFcShort( 0x5 ),	/* 5 */
/* 5412 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5414 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5416 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5418 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 5420 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5422 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5424 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5426 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5428 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5430 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 5432 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5434 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5436 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure OnWaitingToDownload */


	/* Procedure DoRestartBrowsers */

/* 5438 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5440 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5444 */	NdrFcShort( 0x6 ),	/* 6 */
/* 5446 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5448 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5450 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5452 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 5454 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5456 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5458 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5460 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5462 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5464 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */


	/* Return value */

/* 5466 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5468 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5470 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure DoReboot */

/* 5472 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5474 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5478 */	NdrFcShort( 0x7 ),	/* 7 */
/* 5480 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5482 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5484 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5486 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 5488 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5490 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5492 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5494 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5496 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5498 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 5500 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5502 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5504 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure DoLaunchBrowser */

/* 5506 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5508 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5512 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5514 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5516 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5518 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5520 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 5522 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5524 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5526 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5528 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5530 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5532 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5534 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter url */

/* 5536 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 5538 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5540 */	NdrFcShort( 0x432 ),	/* Type Offset=1074 */

	/* Return value */

/* 5542 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5544 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5546 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure OnUpdateAvailable */

/* 5548 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5550 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5554 */	NdrFcShort( 0x5 ),	/* 5 */
/* 5556 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5558 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5560 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5562 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 5564 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5566 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5568 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5570 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5572 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5574 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5576 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter version_string */

/* 5578 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 5580 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5582 */	NdrFcShort( 0x432 ),	/* Type Offset=1074 */

	/* Return value */

/* 5584 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5586 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5588 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure OnDownloading */

/* 5590 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5592 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5596 */	NdrFcShort( 0x7 ),	/* 7 */
/* 5598 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5600 */	NdrFcShort( 0x10 ),	/* 16 */
/* 5602 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5604 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 5606 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5608 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5610 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5612 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5614 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5616 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5618 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter time_remaining_ms */

/* 5620 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5622 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5624 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter pos */

/* 5626 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5628 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5630 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5632 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5634 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5636 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure OnWaitingToInstall */

/* 5638 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5640 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5644 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5646 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5648 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5650 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5652 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 5654 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5656 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5658 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5660 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5662 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5664 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 5666 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5668 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5670 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure OnPause */

/* 5672 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5674 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5678 */	NdrFcShort( 0xa ),	/* 10 */
/* 5680 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5682 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5684 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5686 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x1,		/* 1 */
/* 5688 */	0xc,		/* 12 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5690 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5692 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5694 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5696 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5698 */	0x1,		/* 1 */
			0x80,		/* 128 */

	/* Return value */

/* 5700 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5702 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5704 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure OnComplete */

/* 5706 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5708 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5712 */	NdrFcShort( 0xb ),	/* 11 */
/* 5714 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5716 */	NdrFcShort( 0x6 ),	/* 6 */
/* 5718 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5720 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 5722 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5724 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5726 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5728 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5730 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5732 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5734 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter code */

/* 5736 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5738 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5740 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter completion_text */

/* 5742 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 5744 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5746 */	NdrFcShort( 0x432 ),	/* Type Offset=1074 */

	/* Return value */

/* 5748 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5750 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5752 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure SetEventSink */

/* 5754 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5756 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5760 */	NdrFcShort( 0xc ),	/* 12 */
/* 5762 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5764 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5766 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5768 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 5770 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5772 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5774 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5776 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5778 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5780 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5782 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter ui_sink */

/* 5784 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 5786 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5788 */	NdrFcShort( 0x438 ),	/* Type Offset=1080 */

	/* Return value */

/* 5790 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5792 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5794 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure OnInstalling2 */

/* 5796 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5798 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5802 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5804 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5806 */	NdrFcShort( 0x10 ),	/* 16 */
/* 5808 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5810 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 5812 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5814 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5816 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5818 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5820 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5822 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5824 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter time_remaining_ms */

/* 5826 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5828 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5830 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter pos */

/* 5832 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5834 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5836 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5838 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5840 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5842 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure CheckForUpdate */

/* 5844 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5846 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5850 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5852 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5854 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5856 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5858 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 5860 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5862 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5864 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5866 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5868 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5870 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5872 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter guid */

/* 5874 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 5876 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5878 */	NdrFcShort( 0x432 ),	/* Type Offset=1074 */

	/* Parameter observer */

/* 5880 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 5882 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5884 */	NdrFcShort( 0x44a ),	/* Type Offset=1098 */

	/* Return value */

/* 5886 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5888 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5890 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Update */

/* 5892 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5894 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5898 */	NdrFcShort( 0x4 ),	/* 4 */
/* 5900 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5902 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5904 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5906 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 5908 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5910 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5912 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5914 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5916 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5918 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5920 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter guid */

/* 5922 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 5924 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5926 */	NdrFcShort( 0x432 ),	/* Type Offset=1074 */

	/* Parameter observer */

/* 5928 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 5930 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5932 */	NdrFcShort( 0x44a ),	/* Type Offset=1098 */

	/* Return value */

/* 5934 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5936 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5938 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure LaunchCmdElevated */

/* 5940 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5942 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5946 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5948 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 5950 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5952 */	NdrFcShort( 0x24 ),	/* 36 */
/* 5954 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 5956 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5958 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5960 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5962 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5964 */	NdrFcShort( 0x5 ),	/* 5 */
/* 5966 */	0x5,		/* 5 */
			0x80,		/* 128 */
/* 5968 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 5970 */	0x83,		/* 131 */
			0x84,		/* 132 */

	/* Parameter app_guid */

/* 5972 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 5974 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5976 */	NdrFcShort( 0x432 ),	/* Type Offset=1074 */

	/* Parameter cmd_id */

/* 5978 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 5980 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5982 */	NdrFcShort( 0x432 ),	/* Type Offset=1074 */

	/* Parameter caller_proc_id */

/* 5984 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5986 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5988 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter proc_handle */

/* 5990 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5992 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5994 */	0xb9,		/* FC_UINT3264 */
			0x0,		/* 0 */

	/* Return value */

/* 5996 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5998 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 6000 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

			0x0
        }
    };

static const google_update_idl_MIDL_TYPE_FORMAT_STRING google_update_idl__MIDL_TypeFormatString =
    {
        0,
        {
			NdrFcShort( 0x0 ),	/* 0 */
/*  2 */	
			0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
/*  4 */	0x8,		/* FC_LONG */
			0x5c,		/* FC_PAD */
/*  6 */	
			0x11, 0x10,	/* FC_RP [pointer_deref] */
/*  8 */	NdrFcShort( 0x2 ),	/* Offset= 2 (10) */
/* 10 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 12 */	NdrFcLong( 0x20400 ),	/* 132096 */
/* 16 */	NdrFcShort( 0x0 ),	/* 0 */
/* 18 */	NdrFcShort( 0x0 ),	/* 0 */
/* 20 */	0xc0,		/* 192 */
			0x0,		/* 0 */
/* 22 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 24 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 26 */	0x0,		/* 0 */
			0x46,		/* 70 */
/* 28 */	
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/* 30 */	NdrFcShort( 0x1c ),	/* Offset= 28 (58) */
/* 32 */	
			0x13, 0x0,	/* FC_OP */
/* 34 */	NdrFcShort( 0xe ),	/* Offset= 14 (48) */
/* 36 */	
			0x1b,		/* FC_CARRAY */
			0x1,		/* 1 */
/* 38 */	NdrFcShort( 0x2 ),	/* 2 */
/* 40 */	0x9,		/* Corr desc: FC_ULONG */
			0x0,		/*  */
/* 42 */	NdrFcShort( 0xfffc ),	/* -4 */
/* 44 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 46 */	0x6,		/* FC_SHORT */
			0x5b,		/* FC_END */
/* 48 */	
			0x17,		/* FC_CSTRUCT */
			0x3,		/* 3 */
/* 50 */	NdrFcShort( 0x8 ),	/* 8 */
/* 52 */	NdrFcShort( 0xfff0 ),	/* Offset= -16 (36) */
/* 54 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 56 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 58 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 60 */	NdrFcShort( 0x0 ),	/* 0 */
/* 62 */	NdrFcShort( 0x8 ),	/* 8 */
/* 64 */	NdrFcShort( 0x0 ),	/* 0 */
/* 66 */	NdrFcShort( 0xffde ),	/* Offset= -34 (32) */
/* 68 */	
			0x12, 0x0,	/* FC_UP */
/* 70 */	NdrFcShort( 0xffea ),	/* Offset= -22 (48) */
/* 72 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 74 */	NdrFcShort( 0x0 ),	/* 0 */
/* 76 */	NdrFcShort( 0x8 ),	/* 8 */
/* 78 */	NdrFcShort( 0x0 ),	/* 0 */
/* 80 */	NdrFcShort( 0xfff4 ),	/* Offset= -12 (68) */
/* 82 */	
			0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
/* 84 */	0x6,		/* FC_SHORT */
			0x5c,		/* FC_PAD */
/* 86 */	
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/* 88 */	NdrFcShort( 0x3a2 ),	/* Offset= 930 (1018) */
/* 90 */	
			0x13, 0x0,	/* FC_OP */
/* 92 */	NdrFcShort( 0x38a ),	/* Offset= 906 (998) */
/* 94 */	
			0x2b,		/* FC_NON_ENCAPSULATED_UNION */
			0x9,		/* FC_ULONG */
/* 96 */	0x7,		/* Corr desc: FC_USHORT */
			0x0,		/*  */
/* 98 */	NdrFcShort( 0xfff8 ),	/* -8 */
/* 100 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 102 */	NdrFcShort( 0x2 ),	/* Offset= 2 (104) */
/* 104 */	NdrFcShort( 0x10 ),	/* 16 */
/* 106 */	NdrFcShort( 0x2f ),	/* 47 */
/* 108 */	NdrFcLong( 0x14 ),	/* 20 */
/* 112 */	NdrFcShort( 0x800b ),	/* Simple arm type: FC_HYPER */
/* 114 */	NdrFcLong( 0x3 ),	/* 3 */
/* 118 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 120 */	NdrFcLong( 0x11 ),	/* 17 */
/* 124 */	NdrFcShort( 0x8001 ),	/* Simple arm type: FC_BYTE */
/* 126 */	NdrFcLong( 0x2 ),	/* 2 */
/* 130 */	NdrFcShort( 0x8006 ),	/* Simple arm type: FC_SHORT */
/* 132 */	NdrFcLong( 0x4 ),	/* 4 */
/* 136 */	NdrFcShort( 0x800a ),	/* Simple arm type: FC_FLOAT */
/* 138 */	NdrFcLong( 0x5 ),	/* 5 */
/* 142 */	NdrFcShort( 0x800c ),	/* Simple arm type: FC_DOUBLE */
/* 144 */	NdrFcLong( 0xb ),	/* 11 */
/* 148 */	NdrFcShort( 0x8006 ),	/* Simple arm type: FC_SHORT */
/* 150 */	NdrFcLong( 0xa ),	/* 10 */
/* 154 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 156 */	NdrFcLong( 0x6 ),	/* 6 */
/* 160 */	NdrFcShort( 0xe8 ),	/* Offset= 232 (392) */
/* 162 */	NdrFcLong( 0x7 ),	/* 7 */
/* 166 */	NdrFcShort( 0x800c ),	/* Simple arm type: FC_DOUBLE */
/* 168 */	NdrFcLong( 0x8 ),	/* 8 */
/* 172 */	NdrFcShort( 0xff74 ),	/* Offset= -140 (32) */
/* 174 */	NdrFcLong( 0xd ),	/* 13 */
/* 178 */	NdrFcShort( 0xdc ),	/* Offset= 220 (398) */
/* 180 */	NdrFcLong( 0x9 ),	/* 9 */
/* 184 */	NdrFcShort( 0xff52 ),	/* Offset= -174 (10) */
/* 186 */	NdrFcLong( 0x2000 ),	/* 8192 */
/* 190 */	NdrFcShort( 0xe2 ),	/* Offset= 226 (416) */
/* 192 */	NdrFcLong( 0x24 ),	/* 36 */
/* 196 */	NdrFcShort( 0x2d8 ),	/* Offset= 728 (924) */
/* 198 */	NdrFcLong( 0x4024 ),	/* 16420 */
/* 202 */	NdrFcShort( 0x2d2 ),	/* Offset= 722 (924) */
/* 204 */	NdrFcLong( 0x4011 ),	/* 16401 */
/* 208 */	NdrFcShort( 0x2d0 ),	/* Offset= 720 (928) */
/* 210 */	NdrFcLong( 0x4002 ),	/* 16386 */
/* 214 */	NdrFcShort( 0x2ce ),	/* Offset= 718 (932) */
/* 216 */	NdrFcLong( 0x4003 ),	/* 16387 */
/* 220 */	NdrFcShort( 0x2cc ),	/* Offset= 716 (936) */
/* 222 */	NdrFcLong( 0x4014 ),	/* 16404 */
/* 226 */	NdrFcShort( 0x2ca ),	/* Offset= 714 (940) */
/* 228 */	NdrFcLong( 0x4004 ),	/* 16388 */
/* 232 */	NdrFcShort( 0x2c8 ),	/* Offset= 712 (944) */
/* 234 */	NdrFcLong( 0x4005 ),	/* 16389 */
/* 238 */	NdrFcShort( 0x2c6 ),	/* Offset= 710 (948) */
/* 240 */	NdrFcLong( 0x400b ),	/* 16395 */
/* 244 */	NdrFcShort( 0x2b0 ),	/* Offset= 688 (932) */
/* 246 */	NdrFcLong( 0x400a ),	/* 16394 */
/* 250 */	NdrFcShort( 0x2ae ),	/* Offset= 686 (936) */
/* 252 */	NdrFcLong( 0x4006 ),	/* 16390 */
/* 256 */	NdrFcShort( 0x2b8 ),	/* Offset= 696 (952) */
/* 258 */	NdrFcLong( 0x4007 ),	/* 16391 */
/* 262 */	NdrFcShort( 0x2ae ),	/* Offset= 686 (948) */
/* 264 */	NdrFcLong( 0x4008 ),	/* 16392 */
/* 268 */	NdrFcShort( 0x2b0 ),	/* Offset= 688 (956) */
/* 270 */	NdrFcLong( 0x400d ),	/* 16397 */
/* 274 */	NdrFcShort( 0x2ae ),	/* Offset= 686 (960) */
/* 276 */	NdrFcLong( 0x4009 ),	/* 16393 */
/* 280 */	NdrFcShort( 0x2ac ),	/* Offset= 684 (964) */
/* 282 */	NdrFcLong( 0x6000 ),	/* 24576 */
/* 286 */	NdrFcShort( 0x2aa ),	/* Offset= 682 (968) */
/* 288 */	NdrFcLong( 0x400c ),	/* 16396 */
/* 292 */	NdrFcShort( 0x2a8 ),	/* Offset= 680 (972) */
/* 294 */	NdrFcLong( 0x10 ),	/* 16 */
/* 298 */	NdrFcShort( 0x8002 ),	/* Simple arm type: FC_CHAR */
/* 300 */	NdrFcLong( 0x12 ),	/* 18 */
/* 304 */	NdrFcShort( 0x8006 ),	/* Simple arm type: FC_SHORT */
/* 306 */	NdrFcLong( 0x13 ),	/* 19 */
/* 310 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 312 */	NdrFcLong( 0x15 ),	/* 21 */
/* 316 */	NdrFcShort( 0x800b ),	/* Simple arm type: FC_HYPER */
/* 318 */	NdrFcLong( 0x16 ),	/* 22 */
/* 322 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 324 */	NdrFcLong( 0x17 ),	/* 23 */
/* 328 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 330 */	NdrFcLong( 0xe ),	/* 14 */
/* 334 */	NdrFcShort( 0x286 ),	/* Offset= 646 (980) */
/* 336 */	NdrFcLong( 0x400e ),	/* 16398 */
/* 340 */	NdrFcShort( 0x28a ),	/* Offset= 650 (990) */
/* 342 */	NdrFcLong( 0x4010 ),	/* 16400 */
/* 346 */	NdrFcShort( 0x288 ),	/* Offset= 648 (994) */
/* 348 */	NdrFcLong( 0x4012 ),	/* 16402 */
/* 352 */	NdrFcShort( 0x244 ),	/* Offset= 580 (932) */
/* 354 */	NdrFcLong( 0x4013 ),	/* 16403 */
/* 358 */	NdrFcShort( 0x242 ),	/* Offset= 578 (936) */
/* 360 */	NdrFcLong( 0x4015 ),	/* 16405 */
/* 364 */	NdrFcShort( 0x240 ),	/* Offset= 576 (940) */
/* 366 */	NdrFcLong( 0x4016 ),	/* 16406 */
/* 370 */	NdrFcShort( 0x236 ),	/* Offset= 566 (936) */
/* 372 */	NdrFcLong( 0x4017 ),	/* 16407 */
/* 376 */	NdrFcShort( 0x230 ),	/* Offset= 560 (936) */
/* 378 */	NdrFcLong( 0x0 ),	/* 0 */
/* 382 */	NdrFcShort( 0x0 ),	/* Offset= 0 (382) */
/* 384 */	NdrFcLong( 0x1 ),	/* 1 */
/* 388 */	NdrFcShort( 0x0 ),	/* Offset= 0 (388) */
/* 390 */	NdrFcShort( 0xffff ),	/* Offset= -1 (389) */
/* 392 */	
			0x15,		/* FC_STRUCT */
			0x7,		/* 7 */
/* 394 */	NdrFcShort( 0x8 ),	/* 8 */
/* 396 */	0xb,		/* FC_HYPER */
			0x5b,		/* FC_END */
/* 398 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 400 */	NdrFcLong( 0x0 ),	/* 0 */
/* 404 */	NdrFcShort( 0x0 ),	/* 0 */
/* 406 */	NdrFcShort( 0x0 ),	/* 0 */
/* 408 */	0xc0,		/* 192 */
			0x0,		/* 0 */
/* 410 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 412 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 414 */	0x0,		/* 0 */
			0x46,		/* 70 */
/* 416 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 418 */	NdrFcShort( 0x2 ),	/* Offset= 2 (420) */
/* 420 */	
			0x13, 0x0,	/* FC_OP */
/* 422 */	NdrFcShort( 0x1e4 ),	/* Offset= 484 (906) */
/* 424 */	
			0x2a,		/* FC_ENCAPSULATED_UNION */
			0x89,		/* 137 */
/* 426 */	NdrFcShort( 0x20 ),	/* 32 */
/* 428 */	NdrFcShort( 0xa ),	/* 10 */
/* 430 */	NdrFcLong( 0x8 ),	/* 8 */
/* 434 */	NdrFcShort( 0x50 ),	/* Offset= 80 (514) */
/* 436 */	NdrFcLong( 0xd ),	/* 13 */
/* 440 */	NdrFcShort( 0x70 ),	/* Offset= 112 (552) */
/* 442 */	NdrFcLong( 0x9 ),	/* 9 */
/* 446 */	NdrFcShort( 0x90 ),	/* Offset= 144 (590) */
/* 448 */	NdrFcLong( 0xc ),	/* 12 */
/* 452 */	NdrFcShort( 0xb0 ),	/* Offset= 176 (628) */
/* 454 */	NdrFcLong( 0x24 ),	/* 36 */
/* 458 */	NdrFcShort( 0x102 ),	/* Offset= 258 (716) */
/* 460 */	NdrFcLong( 0x800d ),	/* 32781 */
/* 464 */	NdrFcShort( 0x11e ),	/* Offset= 286 (750) */
/* 466 */	NdrFcLong( 0x10 ),	/* 16 */
/* 470 */	NdrFcShort( 0x138 ),	/* Offset= 312 (782) */
/* 472 */	NdrFcLong( 0x2 ),	/* 2 */
/* 476 */	NdrFcShort( 0x14e ),	/* Offset= 334 (810) */
/* 478 */	NdrFcLong( 0x3 ),	/* 3 */
/* 482 */	NdrFcShort( 0x164 ),	/* Offset= 356 (838) */
/* 484 */	NdrFcLong( 0x14 ),	/* 20 */
/* 488 */	NdrFcShort( 0x17a ),	/* Offset= 378 (866) */
/* 490 */	NdrFcShort( 0xffff ),	/* Offset= -1 (489) */
/* 492 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 494 */	NdrFcShort( 0x0 ),	/* 0 */
/* 496 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 498 */	NdrFcShort( 0x0 ),	/* 0 */
/* 500 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 502 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 506 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 508 */	
			0x13, 0x0,	/* FC_OP */
/* 510 */	NdrFcShort( 0xfe32 ),	/* Offset= -462 (48) */
/* 512 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 514 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 516 */	NdrFcShort( 0x10 ),	/* 16 */
/* 518 */	NdrFcShort( 0x0 ),	/* 0 */
/* 520 */	NdrFcShort( 0x6 ),	/* Offset= 6 (526) */
/* 522 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 524 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 526 */	
			0x11, 0x0,	/* FC_RP */
/* 528 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (492) */
/* 530 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 532 */	NdrFcShort( 0x0 ),	/* 0 */
/* 534 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 536 */	NdrFcShort( 0x0 ),	/* 0 */
/* 538 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 540 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 544 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 546 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 548 */	NdrFcShort( 0xff6a ),	/* Offset= -150 (398) */
/* 550 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 552 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 554 */	NdrFcShort( 0x10 ),	/* 16 */
/* 556 */	NdrFcShort( 0x0 ),	/* 0 */
/* 558 */	NdrFcShort( 0x6 ),	/* Offset= 6 (564) */
/* 560 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 562 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 564 */	
			0x11, 0x0,	/* FC_RP */
/* 566 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (530) */
/* 568 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 570 */	NdrFcShort( 0x0 ),	/* 0 */
/* 572 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 574 */	NdrFcShort( 0x0 ),	/* 0 */
/* 576 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 578 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 582 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 584 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 586 */	NdrFcShort( 0xfdc0 ),	/* Offset= -576 (10) */
/* 588 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 590 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 592 */	NdrFcShort( 0x10 ),	/* 16 */
/* 594 */	NdrFcShort( 0x0 ),	/* 0 */
/* 596 */	NdrFcShort( 0x6 ),	/* Offset= 6 (602) */
/* 598 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 600 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 602 */	
			0x11, 0x0,	/* FC_RP */
/* 604 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (568) */
/* 606 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 608 */	NdrFcShort( 0x0 ),	/* 0 */
/* 610 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 612 */	NdrFcShort( 0x0 ),	/* 0 */
/* 614 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 616 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 620 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 622 */	
			0x13, 0x0,	/* FC_OP */
/* 624 */	NdrFcShort( 0x176 ),	/* Offset= 374 (998) */
/* 626 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 628 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 630 */	NdrFcShort( 0x10 ),	/* 16 */
/* 632 */	NdrFcShort( 0x0 ),	/* 0 */
/* 634 */	NdrFcShort( 0x6 ),	/* Offset= 6 (640) */
/* 636 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 638 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 640 */	
			0x11, 0x0,	/* FC_RP */
/* 642 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (606) */
/* 644 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 646 */	NdrFcLong( 0x2f ),	/* 47 */
/* 650 */	NdrFcShort( 0x0 ),	/* 0 */
/* 652 */	NdrFcShort( 0x0 ),	/* 0 */
/* 654 */	0xc0,		/* 192 */
			0x0,		/* 0 */
/* 656 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 658 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 660 */	0x0,		/* 0 */
			0x46,		/* 70 */
/* 662 */	
			0x1b,		/* FC_CARRAY */
			0x0,		/* 0 */
/* 664 */	NdrFcShort( 0x1 ),	/* 1 */
/* 666 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 668 */	NdrFcShort( 0x4 ),	/* 4 */
/* 670 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 672 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */
/* 674 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 676 */	NdrFcShort( 0x18 ),	/* 24 */
/* 678 */	NdrFcShort( 0x0 ),	/* 0 */
/* 680 */	NdrFcShort( 0xa ),	/* Offset= 10 (690) */
/* 682 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 684 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 686 */	NdrFcShort( 0xffd6 ),	/* Offset= -42 (644) */
/* 688 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 690 */	
			0x13, 0x0,	/* FC_OP */
/* 692 */	NdrFcShort( 0xffe2 ),	/* Offset= -30 (662) */
/* 694 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 696 */	NdrFcShort( 0x0 ),	/* 0 */
/* 698 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 700 */	NdrFcShort( 0x0 ),	/* 0 */
/* 702 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 704 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 708 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 710 */	
			0x13, 0x0,	/* FC_OP */
/* 712 */	NdrFcShort( 0xffda ),	/* Offset= -38 (674) */
/* 714 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 716 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 718 */	NdrFcShort( 0x10 ),	/* 16 */
/* 720 */	NdrFcShort( 0x0 ),	/* 0 */
/* 722 */	NdrFcShort( 0x6 ),	/* Offset= 6 (728) */
/* 724 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 726 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 728 */	
			0x11, 0x0,	/* FC_RP */
/* 730 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (694) */
/* 732 */	
			0x1d,		/* FC_SMFARRAY */
			0x0,		/* 0 */
/* 734 */	NdrFcShort( 0x8 ),	/* 8 */
/* 736 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */
/* 738 */	
			0x15,		/* FC_STRUCT */
			0x3,		/* 3 */
/* 740 */	NdrFcShort( 0x10 ),	/* 16 */
/* 742 */	0x8,		/* FC_LONG */
			0x6,		/* FC_SHORT */
/* 744 */	0x6,		/* FC_SHORT */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 746 */	0x0,		/* 0 */
			NdrFcShort( 0xfff1 ),	/* Offset= -15 (732) */
			0x5b,		/* FC_END */
/* 750 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 752 */	NdrFcShort( 0x20 ),	/* 32 */
/* 754 */	NdrFcShort( 0x0 ),	/* 0 */
/* 756 */	NdrFcShort( 0xa ),	/* Offset= 10 (766) */
/* 758 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 760 */	0x36,		/* FC_POINTER */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 762 */	0x0,		/* 0 */
			NdrFcShort( 0xffe7 ),	/* Offset= -25 (738) */
			0x5b,		/* FC_END */
/* 766 */	
			0x11, 0x0,	/* FC_RP */
/* 768 */	NdrFcShort( 0xff12 ),	/* Offset= -238 (530) */
/* 770 */	
			0x1b,		/* FC_CARRAY */
			0x0,		/* 0 */
/* 772 */	NdrFcShort( 0x1 ),	/* 1 */
/* 774 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 776 */	NdrFcShort( 0x0 ),	/* 0 */
/* 778 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 780 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */
/* 782 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 784 */	NdrFcShort( 0x10 ),	/* 16 */
/* 786 */	NdrFcShort( 0x0 ),	/* 0 */
/* 788 */	NdrFcShort( 0x6 ),	/* Offset= 6 (794) */
/* 790 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 792 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 794 */	
			0x13, 0x0,	/* FC_OP */
/* 796 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (770) */
/* 798 */	
			0x1b,		/* FC_CARRAY */
			0x1,		/* 1 */
/* 800 */	NdrFcShort( 0x2 ),	/* 2 */
/* 802 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 804 */	NdrFcShort( 0x0 ),	/* 0 */
/* 806 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 808 */	0x6,		/* FC_SHORT */
			0x5b,		/* FC_END */
/* 810 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 812 */	NdrFcShort( 0x10 ),	/* 16 */
/* 814 */	NdrFcShort( 0x0 ),	/* 0 */
/* 816 */	NdrFcShort( 0x6 ),	/* Offset= 6 (822) */
/* 818 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 820 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 822 */	
			0x13, 0x0,	/* FC_OP */
/* 824 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (798) */
/* 826 */	
			0x1b,		/* FC_CARRAY */
			0x3,		/* 3 */
/* 828 */	NdrFcShort( 0x4 ),	/* 4 */
/* 830 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 832 */	NdrFcShort( 0x0 ),	/* 0 */
/* 834 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 836 */	0x8,		/* FC_LONG */
			0x5b,		/* FC_END */
/* 838 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 840 */	NdrFcShort( 0x10 ),	/* 16 */
/* 842 */	NdrFcShort( 0x0 ),	/* 0 */
/* 844 */	NdrFcShort( 0x6 ),	/* Offset= 6 (850) */
/* 846 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 848 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 850 */	
			0x13, 0x0,	/* FC_OP */
/* 852 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (826) */
/* 854 */	
			0x1b,		/* FC_CARRAY */
			0x7,		/* 7 */
/* 856 */	NdrFcShort( 0x8 ),	/* 8 */
/* 858 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 860 */	NdrFcShort( 0x0 ),	/* 0 */
/* 862 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 864 */	0xb,		/* FC_HYPER */
			0x5b,		/* FC_END */
/* 866 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 868 */	NdrFcShort( 0x10 ),	/* 16 */
/* 870 */	NdrFcShort( 0x0 ),	/* 0 */
/* 872 */	NdrFcShort( 0x6 ),	/* Offset= 6 (878) */
/* 874 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 876 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 878 */	
			0x13, 0x0,	/* FC_OP */
/* 880 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (854) */
/* 882 */	
			0x15,		/* FC_STRUCT */
			0x3,		/* 3 */
/* 884 */	NdrFcShort( 0x8 ),	/* 8 */
/* 886 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 888 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 890 */	
			0x1b,		/* FC_CARRAY */
			0x3,		/* 3 */
/* 892 */	NdrFcShort( 0x8 ),	/* 8 */
/* 894 */	0x7,		/* Corr desc: FC_USHORT */
			0x0,		/*  */
/* 896 */	NdrFcShort( 0xffc8 ),	/* -56 */
/* 898 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 900 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 902 */	NdrFcShort( 0xffec ),	/* Offset= -20 (882) */
/* 904 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 906 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 908 */	NdrFcShort( 0x38 ),	/* 56 */
/* 910 */	NdrFcShort( 0xffec ),	/* Offset= -20 (890) */
/* 912 */	NdrFcShort( 0x0 ),	/* Offset= 0 (912) */
/* 914 */	0x6,		/* FC_SHORT */
			0x6,		/* FC_SHORT */
/* 916 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 918 */	0x40,		/* FC_STRUCTPAD4 */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 920 */	0x0,		/* 0 */
			NdrFcShort( 0xfe0f ),	/* Offset= -497 (424) */
			0x5b,		/* FC_END */
/* 924 */	
			0x13, 0x0,	/* FC_OP */
/* 926 */	NdrFcShort( 0xff04 ),	/* Offset= -252 (674) */
/* 928 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 930 */	0x1,		/* FC_BYTE */
			0x5c,		/* FC_PAD */
/* 932 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 934 */	0x6,		/* FC_SHORT */
			0x5c,		/* FC_PAD */
/* 936 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 938 */	0x8,		/* FC_LONG */
			0x5c,		/* FC_PAD */
/* 940 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 942 */	0xb,		/* FC_HYPER */
			0x5c,		/* FC_PAD */
/* 944 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 946 */	0xa,		/* FC_FLOAT */
			0x5c,		/* FC_PAD */
/* 948 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 950 */	0xc,		/* FC_DOUBLE */
			0x5c,		/* FC_PAD */
/* 952 */	
			0x13, 0x0,	/* FC_OP */
/* 954 */	NdrFcShort( 0xfdce ),	/* Offset= -562 (392) */
/* 956 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 958 */	NdrFcShort( 0xfc62 ),	/* Offset= -926 (32) */
/* 960 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 962 */	NdrFcShort( 0xfdcc ),	/* Offset= -564 (398) */
/* 964 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 966 */	NdrFcShort( 0xfc44 ),	/* Offset= -956 (10) */
/* 968 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 970 */	NdrFcShort( 0xfdd6 ),	/* Offset= -554 (416) */
/* 972 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 974 */	NdrFcShort( 0x2 ),	/* Offset= 2 (976) */
/* 976 */	
			0x13, 0x0,	/* FC_OP */
/* 978 */	NdrFcShort( 0x14 ),	/* Offset= 20 (998) */
/* 980 */	
			0x15,		/* FC_STRUCT */
			0x7,		/* 7 */
/* 982 */	NdrFcShort( 0x10 ),	/* 16 */
/* 984 */	0x6,		/* FC_SHORT */
			0x1,		/* FC_BYTE */
/* 986 */	0x1,		/* FC_BYTE */
			0x8,		/* FC_LONG */
/* 988 */	0xb,		/* FC_HYPER */
			0x5b,		/* FC_END */
/* 990 */	
			0x13, 0x0,	/* FC_OP */
/* 992 */	NdrFcShort( 0xfff4 ),	/* Offset= -12 (980) */
/* 994 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 996 */	0x2,		/* FC_CHAR */
			0x5c,		/* FC_PAD */
/* 998 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x7,		/* 7 */
/* 1000 */	NdrFcShort( 0x20 ),	/* 32 */
/* 1002 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1004 */	NdrFcShort( 0x0 ),	/* Offset= 0 (1004) */
/* 1006 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 1008 */	0x6,		/* FC_SHORT */
			0x6,		/* FC_SHORT */
/* 1010 */	0x6,		/* FC_SHORT */
			0x6,		/* FC_SHORT */
/* 1012 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1014 */	NdrFcShort( 0xfc68 ),	/* Offset= -920 (94) */
/* 1016 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1018 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 1020 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1022 */	NdrFcShort( 0x18 ),	/* 24 */
/* 1024 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1026 */	NdrFcShort( 0xfc58 ),	/* Offset= -936 (90) */
/* 1028 */	
			0x11, 0x0,	/* FC_RP */
/* 1030 */	NdrFcShort( 0x6 ),	/* Offset= 6 (1036) */
/* 1032 */	
			0x12, 0x0,	/* FC_UP */
/* 1034 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (998) */
/* 1036 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 1038 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1040 */	NdrFcShort( 0x18 ),	/* 24 */
/* 1042 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1044 */	NdrFcShort( 0xfff4 ),	/* Offset= -12 (1032) */
/* 1046 */	
			0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
/* 1048 */	0xb,		/* FC_HYPER */
			0x5c,		/* FC_PAD */
/* 1050 */	
			0x11, 0x10,	/* FC_RP [pointer_deref] */
/* 1052 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1054) */
/* 1054 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1056 */	NdrFcLong( 0x2e629606 ),	/* 778212870 */
/* 1060 */	NdrFcShort( 0x312a ),	/* 12586 */
/* 1062 */	NdrFcShort( 0x482f ),	/* 18479 */
/* 1064 */	0x9b,		/* 155 */
			0x12,		/* 18 */
/* 1066 */	0x2c,		/* 44 */
			0x4a,		/* 74 */
/* 1068 */	0xbf,		/* 191 */
			0x6f,		/* 111 */
/* 1070 */	0xb,		/* 11 */
			0x6d,		/* 109 */
/* 1072 */	
			0x11, 0x8,	/* FC_RP [simple_pointer] */
/* 1074 */	
			0x25,		/* FC_C_WSTRING */
			0x5c,		/* FC_PAD */
/* 1076 */	
			0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
/* 1078 */	0xb9,		/* FC_UINT3264 */
			0x5c,		/* FC_PAD */
/* 1080 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1082 */	NdrFcLong( 0x1c642ced ),	/* 476327149 */
/* 1086 */	NdrFcShort( 0xca3b ),	/* -13765 */
/* 1088 */	NdrFcShort( 0x4013 ),	/* 16403 */
/* 1090 */	0xa9,		/* 169 */
			0xdf,		/* 223 */
/* 1092 */	0xca,		/* 202 */
			0x6c,		/* 108 */
/* 1094 */	0xe5,		/* 229 */
			0xff,		/* 255 */
/* 1096 */	0x65,		/* 101 */
			0x3,		/* 3 */
/* 1098 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1100 */	NdrFcLong( 0x49d7563b ),	/* 1238849083 */
/* 1104 */	NdrFcShort( 0x2ddb ),	/* 11739 */
/* 1106 */	NdrFcShort( 0x4831 ),	/* 18481 */
/* 1108 */	0x88,		/* 136 */
			0xc8,		/* 200 */
/* 1110 */	0x76,		/* 118 */
			0x8a,		/* 138 */
/* 1112 */	0x53,		/* 83 */
			0x83,		/* 131 */
/* 1114 */	0x38,		/* 56 */
			0x37,		/* 55 */

			0x0
        }
    };

static const USER_MARSHAL_ROUTINE_QUADRUPLE UserMarshalRoutines[ WIRE_MARSHAL_TABLE_SIZE ] = 
        {
            
            {
            BSTR_UserSize
            ,BSTR_UserMarshal
            ,BSTR_UserUnmarshal
            ,BSTR_UserFree
            },
            {
            VARIANT_UserSize
            ,VARIANT_UserMarshal
            ,VARIANT_UserUnmarshal
            ,VARIANT_UserFree
            }

        };



/* Standard interface: __MIDL_itf_google_update_idl_0000_0000, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IUnknown, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}} */


/* Object interface: IDispatch, ver. 0.0,
   GUID={0x00020400,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}} */


/* Object interface: IGoogleUpdate3, ver. 0.0,
   GUID={0x6DB17455,0x4E85,0x46e7,{0x9D,0x23,0xE5,0x55,0xE4,0xB0,0x05,0xAF}} */

#pragma code_seg(".orpc")
static const unsigned short IGoogleUpdate3_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    0,
    42,
    90
    };

static const MIDL_STUBLESS_PROXY_INFO IGoogleUpdate3_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IGoogleUpdate3_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IGoogleUpdate3_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IGoogleUpdate3_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(10) _IGoogleUpdate3ProxyVtbl = 
{
    &IGoogleUpdate3_ProxyInfo,
    &IID_IGoogleUpdate3,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IGoogleUpdate3::get_Count */ ,
    (void *) (INT_PTR) -1 /* IGoogleUpdate3::get_Item */ ,
    (void *) (INT_PTR) -1 /* IGoogleUpdate3::createAppBundle */
};


static const PRPC_STUB_FUNCTION IGoogleUpdate3_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IGoogleUpdate3StubVtbl =
{
    &IID_IGoogleUpdate3,
    &IGoogleUpdate3_ServerInfo,
    10,
    &IGoogleUpdate3_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IAppBundle, ver. 0.0,
   GUID={0xfe908cdd,0x22bb,0x472a,{0x98,0x70,0x1a,0x03,0x90,0xe4,0x2f,0x36}} */

#pragma code_seg(".orpc")
static const unsigned short IAppBundle_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    132,
    174,
    216,
    258,
    300,
    342,
    384,
    426,
    468,
    510,
    552,
    594,
    636,
    678,
    720,
    762,
    804,
    846,
    894,
    950,
    992,
    1026,
    1074,
    1122,
    1156,
    1190,
    1224,
    1258,
    1292,
    1326,
    1360,
    1394,
    1436,
    1484
    };

static const MIDL_STUBLESS_PROXY_INFO IAppBundle_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IAppBundle_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAppBundle_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IAppBundle_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(41) _IAppBundleProxyVtbl = 
{
    &IAppBundle_ProxyInfo,
    &IID_IAppBundle,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::get_displayName */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::put_displayName */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::get_displayLanguage */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::put_displayLanguage */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::get_installSource */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::put_installSource */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::get_originURL */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::put_originURL */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::get_offlineDirectory */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::put_offlineDirectory */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::get_sessionId */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::put_sessionId */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::get_sendPings */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::put_sendPings */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::get_priority */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::put_priority */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::get_Count */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::get_Item */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::put_altTokens */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::put_parentHWND */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::initialize */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::createApp */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::createInstalledApp */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::createAllInstalledApps */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::checkForUpdate */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::download */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::install */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::updateAllApps */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::stop */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::pause */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::resume */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::isBusy */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::downloadPackage */ ,
    (void *) (INT_PTR) -1 /* IAppBundle::get_currentState */
};


static const PRPC_STUB_FUNCTION IAppBundle_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAppBundleStubVtbl =
{
    &IID_IAppBundle,
    &IAppBundle_ServerInfo,
    41,
    &IAppBundle_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IApp, ver. 0.0,
   GUID={0x76F7B787,0xA67C,0x4c73,{0x82,0xC7,0x31,0xF5,0xE3,0xAA,0xBC,0x5C}} */

#pragma code_seg(".orpc")
static const unsigned short IApp_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    1526,
    1568,
    216,
    1610,
    1652,
    1694,
    1736,
    1778,
    1820,
    1862,
    1904,
    1946,
    1988,
    2030,
    2072,
    2114,
    2156,
    2198,
    2240,
    2282,
    2324,
    1026,
    2366,
    2408,
    2450,
    2492,
    2534,
    2576,
    2618,
    2660,
    2702,
    2744,
    2786,
    2828
    };

static const MIDL_STUBLESS_PROXY_INFO IApp_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IApp_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IApp_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IApp_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(41) _IAppProxyVtbl = 
{
    &IApp_ProxyInfo,
    &IID_IApp,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IApp::get_currentVersion */ ,
    (void *) (INT_PTR) -1 /* IApp::get_nextVersion */ ,
    (void *) (INT_PTR) -1 /* IApp::get_appId */ ,
    (void *) (INT_PTR) -1 /* IApp::get_displayName */ ,
    (void *) (INT_PTR) -1 /* IApp::put_displayName */ ,
    (void *) (INT_PTR) -1 /* IApp::get_language */ ,
    (void *) (INT_PTR) -1 /* IApp::put_language */ ,
    (void *) (INT_PTR) -1 /* IApp::get_ap */ ,
    (void *) (INT_PTR) -1 /* IApp::put_ap */ ,
    (void *) (INT_PTR) -1 /* IApp::get_ttToken */ ,
    (void *) (INT_PTR) -1 /* IApp::put_ttToken */ ,
    (void *) (INT_PTR) -1 /* IApp::get_iid */ ,
    (void *) (INT_PTR) -1 /* IApp::put_iid */ ,
    (void *) (INT_PTR) -1 /* IApp::get_brandCode */ ,
    (void *) (INT_PTR) -1 /* IApp::put_brandCode */ ,
    (void *) (INT_PTR) -1 /* IApp::get_clientId */ ,
    (void *) (INT_PTR) -1 /* IApp::put_clientId */ ,
    (void *) (INT_PTR) -1 /* IApp::get_labels */ ,
    (void *) (INT_PTR) -1 /* IApp::put_labels */ ,
    (void *) (INT_PTR) -1 /* IApp::get_referralId */ ,
    (void *) (INT_PTR) -1 /* IApp::put_referralId */ ,
    (void *) (INT_PTR) -1 /* IApp::get_command */ ,
    (void *) (INT_PTR) -1 /* IApp::get_browserType */ ,
    (void *) (INT_PTR) -1 /* IApp::put_browserType */ ,
    (void *) (INT_PTR) -1 /* IApp::get_clientInstallData */ ,
    (void *) (INT_PTR) -1 /* IApp::put_clientInstallData */ ,
    (void *) (INT_PTR) -1 /* IApp::get_serverInstallDataIndex */ ,
    (void *) (INT_PTR) -1 /* IApp::put_serverInstallDataIndex */ ,
    (void *) (INT_PTR) -1 /* IApp::get_isEulaAccepted */ ,
    (void *) (INT_PTR) -1 /* IApp::put_isEulaAccepted */ ,
    (void *) (INT_PTR) -1 /* IApp::get_usageStatsEnable */ ,
    (void *) (INT_PTR) -1 /* IApp::put_usageStatsEnable */ ,
    (void *) (INT_PTR) -1 /* IApp::get_installTimeDiffSec */ ,
    (void *) (INT_PTR) -1 /* IApp::get_currentState */
};


static const PRPC_STUB_FUNCTION IApp_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAppStubVtbl =
{
    &IID_IApp,
    &IApp_ServerInfo,
    41,
    &IApp_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IApp2, ver. 0.0,
   GUID={0x084D78A8,0xB084,0x4E14,{0xA6,0x29,0xA2,0xC4,0x19,0xB0,0xE3,0xD9}} */

#pragma code_seg(".orpc")
static const unsigned short IApp2_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    1526,
    1568,
    216,
    1610,
    1652,
    1694,
    1736,
    1778,
    1820,
    1862,
    1904,
    1946,
    1988,
    2030,
    2072,
    2114,
    2156,
    2198,
    2240,
    2282,
    2324,
    1026,
    2366,
    2408,
    2450,
    2492,
    2534,
    2576,
    2618,
    2660,
    2702,
    2744,
    2786,
    2828,
    2870,
    2912
    };

static const MIDL_STUBLESS_PROXY_INFO IApp2_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IApp2_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IApp2_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IApp2_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(43) _IApp2ProxyVtbl = 
{
    &IApp2_ProxyInfo,
    &IID_IApp2,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IApp::get_currentVersion */ ,
    (void *) (INT_PTR) -1 /* IApp::get_nextVersion */ ,
    (void *) (INT_PTR) -1 /* IApp::get_appId */ ,
    (void *) (INT_PTR) -1 /* IApp::get_displayName */ ,
    (void *) (INT_PTR) -1 /* IApp::put_displayName */ ,
    (void *) (INT_PTR) -1 /* IApp::get_language */ ,
    (void *) (INT_PTR) -1 /* IApp::put_language */ ,
    (void *) (INT_PTR) -1 /* IApp::get_ap */ ,
    (void *) (INT_PTR) -1 /* IApp::put_ap */ ,
    (void *) (INT_PTR) -1 /* IApp::get_ttToken */ ,
    (void *) (INT_PTR) -1 /* IApp::put_ttToken */ ,
    (void *) (INT_PTR) -1 /* IApp::get_iid */ ,
    (void *) (INT_PTR) -1 /* IApp::put_iid */ ,
    (void *) (INT_PTR) -1 /* IApp::get_brandCode */ ,
    (void *) (INT_PTR) -1 /* IApp::put_brandCode */ ,
    (void *) (INT_PTR) -1 /* IApp::get_clientId */ ,
    (void *) (INT_PTR) -1 /* IApp::put_clientId */ ,
    (void *) (INT_PTR) -1 /* IApp::get_labels */ ,
    (void *) (INT_PTR) -1 /* IApp::put_labels */ ,
    (void *) (INT_PTR) -1 /* IApp::get_referralId */ ,
    (void *) (INT_PTR) -1 /* IApp::put_referralId */ ,
    (void *) (INT_PTR) -1 /* IApp::get_command */ ,
    (void *) (INT_PTR) -1 /* IApp::get_browserType */ ,
    (void *) (INT_PTR) -1 /* IApp::put_browserType */ ,
    (void *) (INT_PTR) -1 /* IApp::get_clientInstallData */ ,
    (void *) (INT_PTR) -1 /* IApp::put_clientInstallData */ ,
    (void *) (INT_PTR) -1 /* IApp::get_serverInstallDataIndex */ ,
    (void *) (INT_PTR) -1 /* IApp::put_serverInstallDataIndex */ ,
    (void *) (INT_PTR) -1 /* IApp::get_isEulaAccepted */ ,
    (void *) (INT_PTR) -1 /* IApp::put_isEulaAccepted */ ,
    (void *) (INT_PTR) -1 /* IApp::get_usageStatsEnable */ ,
    (void *) (INT_PTR) -1 /* IApp::put_usageStatsEnable */ ,
    (void *) (INT_PTR) -1 /* IApp::get_installTimeDiffSec */ ,
    (void *) (INT_PTR) -1 /* IApp::get_currentState */ ,
    (void *) (INT_PTR) -1 /* IApp2::get_untrustedData */ ,
    (void *) (INT_PTR) -1 /* IApp2::put_untrustedData */
};


static const PRPC_STUB_FUNCTION IApp2_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IApp2StubVtbl =
{
    &IID_IApp2,
    &IApp2_ServerInfo,
    43,
    &IApp2_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IAppCommand, ver. 0.0,
   GUID={0x4DE778FE,0xF195,0x4ee3,{0x9D,0xAB,0xFE,0x44,0x6C,0x23,0x92,0x21}} */

#pragma code_seg(".orpc")
static const unsigned short IAppCommand_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    2954,
    2996,
    3038,
    3080
    };

static const MIDL_STUBLESS_PROXY_INFO IAppCommand_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IAppCommand_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAppCommand_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IAppCommand_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(11) _IAppCommandProxyVtbl = 
{
    &IAppCommand_ProxyInfo,
    &IID_IAppCommand,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IAppCommand::get_isWebAccessible */ ,
    (void *) (INT_PTR) -1 /* IAppCommand::get_status */ ,
    (void *) (INT_PTR) -1 /* IAppCommand::get_exitCode */ ,
    (void *) (INT_PTR) -1 /* IAppCommand::execute */
};


static const PRPC_STUB_FUNCTION IAppCommand_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAppCommandStubVtbl =
{
    &IID_IAppCommand,
    &IAppCommand_ServerInfo,
    11,
    &IAppCommand_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IAppCommand2, ver. 0.0,
   GUID={0x3D05F64F,0x71E3,0x48A5,{0xBF,0x6B,0x83,0x31,0x5B,0xC8,0xAE,0x1F}} */

#pragma code_seg(".orpc")
static const unsigned short IAppCommand2_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    2954,
    2996,
    3038,
    3080,
    300
    };

static const MIDL_STUBLESS_PROXY_INFO IAppCommand2_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IAppCommand2_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAppCommand2_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IAppCommand2_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(12) _IAppCommand2ProxyVtbl = 
{
    &IAppCommand2_ProxyInfo,
    &IID_IAppCommand2,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IAppCommand::get_isWebAccessible */ ,
    (void *) (INT_PTR) -1 /* IAppCommand::get_status */ ,
    (void *) (INT_PTR) -1 /* IAppCommand::get_exitCode */ ,
    (void *) (INT_PTR) -1 /* IAppCommand::execute */ ,
    (void *) (INT_PTR) -1 /* IAppCommand2::get_output */
};


static const PRPC_STUB_FUNCTION IAppCommand2_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAppCommand2StubVtbl =
{
    &IID_IAppCommand2,
    &IAppCommand2_ServerInfo,
    12,
    &IAppCommand2_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IAppVersion, ver. 0.0,
   GUID={0xBCDCB538,0x01C0,0x46d1,{0xA6,0xA7,0x52,0xF4,0xD0,0x21,0xC2,0x72}} */

#pragma code_seg(".orpc")
static const unsigned short IAppVersion_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    132,
    2996,
    3178
    };

static const MIDL_STUBLESS_PROXY_INFO IAppVersion_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IAppVersion_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAppVersion_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IAppVersion_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(10) _IAppVersionProxyVtbl = 
{
    &IAppVersion_ProxyInfo,
    &IID_IAppVersion,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IAppVersion::get_version */ ,
    (void *) (INT_PTR) -1 /* IAppVersion::get_packageCount */ ,
    (void *) (INT_PTR) -1 /* IAppVersion::get_package */
};


static const PRPC_STUB_FUNCTION IAppVersion_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAppVersionStubVtbl =
{
    &IID_IAppVersion,
    &IAppVersion_ServerInfo,
    10,
    &IAppVersion_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IPackage, ver. 0.0,
   GUID={0xDCAB8386,0x4F03,0x4dbd,{0xA3,0x66,0xD9,0x0B,0xC9,0xF6,0x8D,0xE6}} */

#pragma code_seg(".orpc")
static const unsigned short IPackage_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    3226,
    3268,
    216
    };

static const MIDL_STUBLESS_PROXY_INFO IPackage_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IPackage_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IPackage_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IPackage_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(10) _IPackageProxyVtbl = 
{
    &IPackage_ProxyInfo,
    &IID_IPackage,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IPackage::get */ ,
    (void *) (INT_PTR) -1 /* IPackage::get_isAvailable */ ,
    (void *) (INT_PTR) -1 /* IPackage::get_filename */
};


static const PRPC_STUB_FUNCTION IPackage_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IPackageStubVtbl =
{
    &IID_IPackage,
    &IPackage_ServerInfo,
    10,
    &IPackage_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: ICurrentState, ver. 0.0,
   GUID={0x247954F9,0x9EDC,0x4E68,{0x8C,0xC3,0x15,0x0C,0x2B,0x89,0xEA,0xDF}} */

#pragma code_seg(".orpc")
static const unsigned short ICurrentState_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    0,
    3310,
    3038,
    3352,
    3394,
    3436,
    3478,
    3520,
    3562,
    3604,
    3646,
    1946,
    3688,
    3730,
    3772,
    2114,
    804
    };

static const MIDL_STUBLESS_PROXY_INFO ICurrentState_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &ICurrentState_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO ICurrentState_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &ICurrentState_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(24) _ICurrentStateProxyVtbl = 
{
    &ICurrentState_ProxyInfo,
    &IID_ICurrentState,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_stateValue */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_availableVersion */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_bytesDownloaded */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_totalBytesToDownload */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_downloadTimeRemainingMs */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_nextRetryTime */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_installProgress */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_installTimeRemainingMs */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_isCanceled */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_errorCode */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_extraCode1 */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_completionMessage */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_installerResultCode */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_installerResultExtraCode1 */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_postInstallLaunchCommandLine */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_postInstallUrl */ ,
    (void *) (INT_PTR) -1 /* ICurrentState::get_postInstallAction */
};


static const PRPC_STUB_FUNCTION ICurrentState_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _ICurrentStateStubVtbl =
{
    &IID_ICurrentState,
    &ICurrentState_ServerInfo,
    24,
    &ICurrentState_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IRegistrationUpdateHook, ver. 0.0,
   GUID={0x4E223325,0xC16B,0x4eeb,{0xAE,0xDC,0x19,0xAA,0x99,0xA2,0x37,0xFA}} */

#pragma code_seg(".orpc")
static const unsigned short IRegistrationUpdateHook_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    3814
    };

static const MIDL_STUBLESS_PROXY_INFO IRegistrationUpdateHook_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IRegistrationUpdateHook_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IRegistrationUpdateHook_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IRegistrationUpdateHook_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(8) _IRegistrationUpdateHookProxyVtbl = 
{
    &IRegistrationUpdateHook_ProxyInfo,
    &IID_IRegistrationUpdateHook,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IRegistrationUpdateHook::UpdateRegistry */
};


static const PRPC_STUB_FUNCTION IRegistrationUpdateHook_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2
};

CInterfaceStubVtbl _IRegistrationUpdateHookStubVtbl =
{
    &IID_IRegistrationUpdateHook,
    &IRegistrationUpdateHook_ServerInfo,
    8,
    &IRegistrationUpdateHook_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: ICredentialDialog, ver. 0.0,
   GUID={0xb3a47570,0x0a85,0x4aea,{0x82,0x70,0x52,0x9d,0x47,0x89,0x96,0x03}} */

#pragma code_seg(".orpc")
static const unsigned short ICredentialDialog_FormatStringOffsetTable[] =
    {
    3862
    };

static const MIDL_STUBLESS_PROXY_INFO ICredentialDialog_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &ICredentialDialog_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO ICredentialDialog_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &ICredentialDialog_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _ICredentialDialogProxyVtbl = 
{
    &ICredentialDialog_ProxyInfo,
    &IID_ICredentialDialog,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* ICredentialDialog::QueryUserForCredentials */
};

const CInterfaceStubVtbl _ICredentialDialogStubVtbl =
{
    &IID_ICredentialDialog,
    &ICredentialDialog_ServerInfo,
    4,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IPolicyStatus, ver. 0.0,
   GUID={0xF63F6F8B,0xACD5,0x413C,{0xA4,0x4B,0x04,0x09,0x13,0x6D,0x26,0xCB}} */

#pragma code_seg(".orpc")
static const unsigned short IPolicyStatus_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    0,
    3932,
    216,
    3352,
    3394,
    3994,
    4042,
    4090,
    4138
    };

static const MIDL_STUBLESS_PROXY_INFO IPolicyStatus_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatus_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IPolicyStatus_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IPolicyStatus_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(16) _IPolicyStatusProxyVtbl = 
{
    &IPolicyStatus_ProxyInfo,
    &IID_IPolicyStatus,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus::get_lastCheckPeriodMinutes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus::get_updatesSuppressedTimes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus::get_downloadPreferenceGroupPolicy */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus::get_packageCacheSizeLimitMBytes */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus::get_packageCacheExpirationTimeDays */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus::get_effectivePolicyForAppInstalls */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus::get_effectivePolicyForAppUpdates */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus::get_targetVersionPrefix */ ,
    (void *) (INT_PTR) -1 /* IPolicyStatus::get_isRollbackToTargetVersionAllowed */
};


static const PRPC_STUB_FUNCTION IPolicyStatus_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IPolicyStatusStubVtbl =
{
    &IID_IPolicyStatus,
    &IPolicyStatus_ServerInfo,
    16,
    &IPolicyStatus_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IGoogleUpdate3Web, ver. 0.0,
   GUID={0x494B20CF,0x282E,0x4BDD,{0x9F,0x5D,0xB7,0x0C,0xB0,0x9D,0x35,0x1E}} */

#pragma code_seg(".orpc")
static const unsigned short IGoogleUpdate3Web_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    1526
    };

static const MIDL_STUBLESS_PROXY_INFO IGoogleUpdate3Web_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IGoogleUpdate3Web_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IGoogleUpdate3Web_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IGoogleUpdate3Web_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(8) _IGoogleUpdate3WebProxyVtbl = 
{
    &IGoogleUpdate3Web_ProxyInfo,
    &IID_IGoogleUpdate3Web,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IGoogleUpdate3Web::createAppBundleWeb */
};


static const PRPC_STUB_FUNCTION IGoogleUpdate3Web_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2
};

CInterfaceStubVtbl _IGoogleUpdate3WebStubVtbl =
{
    &IID_IGoogleUpdate3Web,
    &IGoogleUpdate3Web_ServerInfo,
    8,
    &IGoogleUpdate3Web_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IGoogleUpdate3WebSecurity, ver. 0.0,
   GUID={0x2D363682,0x561D,0x4c3a,{0x81,0xC6,0xF2,0xF8,0x21,0x07,0x56,0x2A}} */

#pragma code_seg(".orpc")
static const unsigned short IGoogleUpdate3WebSecurity_FormatStringOffsetTable[] =
    {
    4186
    };

static const MIDL_STUBLESS_PROXY_INFO IGoogleUpdate3WebSecurity_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IGoogleUpdate3WebSecurity_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IGoogleUpdate3WebSecurity_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IGoogleUpdate3WebSecurity_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IGoogleUpdate3WebSecurityProxyVtbl = 
{
    &IGoogleUpdate3WebSecurity_ProxyInfo,
    &IID_IGoogleUpdate3WebSecurity,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IGoogleUpdate3WebSecurity::setOriginURL */
};

const CInterfaceStubVtbl _IGoogleUpdate3WebSecurityStubVtbl =
{
    &IID_IGoogleUpdate3WebSecurity,
    &IGoogleUpdate3WebSecurity_ServerInfo,
    4,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAppBundleWeb, ver. 0.0,
   GUID={0xDD42475D,0x6D46,0x496a,{0x92,0x4E,0xBD,0x56,0x30,0xB4,0xCB,0xBA}} */

#pragma code_seg(".orpc")
static const unsigned short IAppBundleWeb_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    4228,
    174,
    4290,
    1610,
    1652,
    4324,
    3478,
    4366,
    4414,
    4448,
    4482,
    4516,
    4550,
    4584,
    4618,
    4652,
    4700
    };

static const MIDL_STUBLESS_PROXY_INFO IAppBundleWeb_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IAppBundleWeb_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAppBundleWeb_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IAppBundleWeb_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(24) _IAppBundleWebProxyVtbl = 
{
    &IAppBundleWeb_ProxyInfo,
    &IID_IAppBundleWeb,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::createApp */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::createInstalledApp */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::createAllInstalledApps */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::get_displayLanguage */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::put_displayLanguage */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::put_parentHWND */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::get_length */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::get_appWeb */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::initialize */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::checkForUpdate */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::download */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::install */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::pause */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::resume */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::cancel */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::downloadPackage */ ,
    (void *) (INT_PTR) -1 /* IAppBundleWeb::get_currentState */
};


static const PRPC_STUB_FUNCTION IAppBundleWeb_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAppBundleWebStubVtbl =
{
    &IID_IAppBundleWeb,
    &IAppBundleWeb_ServerInfo,
    24,
    &IAppBundleWeb_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IAppWeb, ver. 0.0,
   GUID={0x18D0F672,0x18B4,0x48e6,{0xAD,0x36,0x6E,0x6B,0xF0,0x1D,0xBB,0xC4}} */

#pragma code_seg(".orpc")
static const unsigned short IAppWeb_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    132,
    1568,
    90,
    4742,
    4790,
    4824,
    4866,
    4900,
    468,
    510
    };

static const MIDL_STUBLESS_PROXY_INFO IAppWeb_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IAppWeb_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAppWeb_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IAppWeb_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(17) _IAppWebProxyVtbl = 
{
    &IAppWeb_ProxyInfo,
    &IID_IAppWeb,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IAppWeb::get_appId */ ,
    (void *) (INT_PTR) -1 /* IAppWeb::get_currentVersionWeb */ ,
    (void *) (INT_PTR) -1 /* IAppWeb::get_nextVersionWeb */ ,
    (void *) (INT_PTR) -1 /* IAppWeb::get_command */ ,
    (void *) (INT_PTR) -1 /* IAppWeb::cancel */ ,
    (void *) (INT_PTR) -1 /* IAppWeb::get_currentState */ ,
    (void *) (INT_PTR) -1 /* IAppWeb::launch */ ,
    (void *) (INT_PTR) -1 /* IAppWeb::uninstall */ ,
    (void *) (INT_PTR) -1 /* IAppWeb::get_serverInstallDataIndex */ ,
    (void *) (INT_PTR) -1 /* IAppWeb::put_serverInstallDataIndex */
};


static const PRPC_STUB_FUNCTION IAppWeb_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAppWebStubVtbl =
{
    &IID_IAppWeb,
    &IAppWeb_ServerInfo,
    17,
    &IAppWeb_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IAppCommandWeb, ver. 0.0,
   GUID={0x8476CE12,0xAE1F,0x4198,{0x80,0x5C,0xBA,0x0F,0x9B,0x78,0x3F,0x57}} */

#pragma code_seg(".orpc")
static const unsigned short IAppCommandWeb_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    0,
    2996,
    216,
    3080
    };

static const MIDL_STUBLESS_PROXY_INFO IAppCommandWeb_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IAppCommandWeb_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAppCommandWeb_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IAppCommandWeb_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(11) _IAppCommandWebProxyVtbl = 
{
    &IAppCommandWeb_ProxyInfo,
    &IID_IAppCommandWeb,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IAppCommandWeb::get_status */ ,
    (void *) (INT_PTR) -1 /* IAppCommandWeb::get_exitCode */ ,
    (void *) (INT_PTR) -1 /* IAppCommandWeb::get_output */ ,
    (void *) (INT_PTR) -1 /* IAppCommandWeb::execute */
};


static const PRPC_STUB_FUNCTION IAppCommandWeb_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAppCommandWebStubVtbl =
{
    &IID_IAppCommandWeb,
    &IAppCommandWeb_ServerInfo,
    11,
    &IAppCommandWeb_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IAppVersionWeb, ver. 0.0,
   GUID={0x0CD01D1E,0x4A1C,0x489d,{0x93,0xB9,0x9B,0x66,0x72,0x87,0x7C,0x57}} */

#pragma code_seg(".orpc")
static const unsigned short IAppVersionWeb_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    132,
    2996,
    3178
    };

static const MIDL_STUBLESS_PROXY_INFO IAppVersionWeb_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IAppVersionWeb_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAppVersionWeb_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IAppVersionWeb_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(10) _IAppVersionWebProxyVtbl = 
{
    &IAppVersionWeb_ProxyInfo,
    &IID_IAppVersionWeb,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* IAppVersionWeb::get_version */ ,
    (void *) (INT_PTR) -1 /* IAppVersionWeb::get_packageCount */ ,
    (void *) (INT_PTR) -1 /* IAppVersionWeb::get_packageWeb */
};


static const PRPC_STUB_FUNCTION IAppVersionWeb_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _IAppVersionWebStubVtbl =
{
    &IID_IAppVersionWeb,
    &IAppVersionWeb_ServerInfo,
    10,
    &IAppVersionWeb_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: ICoCreateAsyncStatus, ver. 0.0,
   GUID={0x2E629606,0x312A,0x482f,{0x9B,0x12,0x2C,0x4A,0xBF,0x6F,0x0B,0x6D}} */

#pragma code_seg(".orpc")
static const unsigned short ICoCreateAsyncStatus_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    2954,
    2996,
    90
    };

static const MIDL_STUBLESS_PROXY_INFO ICoCreateAsyncStatus_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &ICoCreateAsyncStatus_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO ICoCreateAsyncStatus_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &ICoCreateAsyncStatus_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(10) _ICoCreateAsyncStatusProxyVtbl = 
{
    &ICoCreateAsyncStatus_ProxyInfo,
    &IID_ICoCreateAsyncStatus,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    (void *) (INT_PTR) -1 /* ICoCreateAsyncStatus::get_isDone */ ,
    (void *) (INT_PTR) -1 /* ICoCreateAsyncStatus::get_completionHResult */ ,
    (void *) (INT_PTR) -1 /* ICoCreateAsyncStatus::get_createdInstance */
};


static const PRPC_STUB_FUNCTION ICoCreateAsyncStatus_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    NdrStubCall2,
    NdrStubCall2,
    NdrStubCall2
};

CInterfaceStubVtbl _ICoCreateAsyncStatusStubVtbl =
{
    &IID_ICoCreateAsyncStatus,
    &ICoCreateAsyncStatus_ServerInfo,
    10,
    &ICoCreateAsyncStatus_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: ICoCreateAsync, ver. 0.0,
   GUID={0xDAB1D343,0x1B2A,0x47f9,{0xB4,0x45,0x93,0xDC,0x50,0x70,0x4B,0xFE}} */

#pragma code_seg(".orpc")
static const unsigned short ICoCreateAsync_FormatStringOffsetTable[] =
    {
    4934
    };

static const MIDL_STUBLESS_PROXY_INFO ICoCreateAsync_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &ICoCreateAsync_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO ICoCreateAsync_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &ICoCreateAsync_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _ICoCreateAsyncProxyVtbl = 
{
    &ICoCreateAsync_ProxyInfo,
    &IID_ICoCreateAsync,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* ICoCreateAsync::createOmahaMachineServerAsync */
};

const CInterfaceStubVtbl _ICoCreateAsyncStubVtbl =
{
    &IID_ICoCreateAsync,
    &ICoCreateAsync_ServerInfo,
    4,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IBrowserHttpRequest2, ver. 0.0,
   GUID={0x5B25A8DC,0x1780,0x4178,{0xA6,0x29,0x6B,0xE8,0xB8,0xDE,0xFA,0xA2}} */

#pragma code_seg(".orpc")
static const unsigned short IBrowserHttpRequest2_FormatStringOffsetTable[] =
    {
    4990
    };

static const MIDL_STUBLESS_PROXY_INFO IBrowserHttpRequest2_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IBrowserHttpRequest2_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IBrowserHttpRequest2_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IBrowserHttpRequest2_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IBrowserHttpRequest2ProxyVtbl = 
{
    &IBrowserHttpRequest2_ProxyInfo,
    &IID_IBrowserHttpRequest2,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IBrowserHttpRequest2::Send */
};

const CInterfaceStubVtbl _IBrowserHttpRequest2StubVtbl =
{
    &IID_IBrowserHttpRequest2,
    &IBrowserHttpRequest2_ServerInfo,
    4,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IProcessLauncher, ver. 0.0,
   GUID={0x128C2DA6,0x2BC0,0x44c0,{0xB3,0xF6,0x4E,0xC2,0x2E,0x64,0x79,0x64}} */

#pragma code_seg(".orpc")
static const unsigned short IProcessLauncher_FormatStringOffsetTable[] =
    {
    5074,
    5116,
    5164
    };

static const MIDL_STUBLESS_PROXY_INFO IProcessLauncher_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IProcessLauncher_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IProcessLauncher_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IProcessLauncher_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(6) _IProcessLauncherProxyVtbl = 
{
    &IProcessLauncher_ProxyInfo,
    &IID_IProcessLauncher,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IProcessLauncher::LaunchCmdLine */ ,
    (void *) (INT_PTR) -1 /* IProcessLauncher::LaunchBrowser */ ,
    (void *) (INT_PTR) -1 /* IProcessLauncher::LaunchCmdElevated */
};

const CInterfaceStubVtbl _IProcessLauncherStubVtbl =
{
    &IID_IProcessLauncher,
    &IProcessLauncher_ServerInfo,
    6,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IProcessLauncher2, ver. 0.0,
   GUID={0xD106AB5F,0xA70E,0x400E,{0xA2,0x1B,0x96,0x20,0x8C,0x1D,0x8D,0xBB}} */

#pragma code_seg(".orpc")
static const unsigned short IProcessLauncher2_FormatStringOffsetTable[] =
    {
    5074,
    5116,
    5164,
    5226
    };

static const MIDL_STUBLESS_PROXY_INFO IProcessLauncher2_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IProcessLauncher2_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IProcessLauncher2_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IProcessLauncher2_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(7) _IProcessLauncher2ProxyVtbl = 
{
    &IProcessLauncher2_ProxyInfo,
    &IID_IProcessLauncher2,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IProcessLauncher::LaunchCmdLine */ ,
    (void *) (INT_PTR) -1 /* IProcessLauncher::LaunchBrowser */ ,
    (void *) (INT_PTR) -1 /* IProcessLauncher::LaunchCmdElevated */ ,
    (void *) (INT_PTR) -1 /* IProcessLauncher2::LaunchCmdLineEx */
};

const CInterfaceStubVtbl _IProcessLauncher2StubVtbl =
{
    &IID_IProcessLauncher2,
    &IProcessLauncher2_ServerInfo,
    7,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IOneClickProcessLauncher, ver. 0.0,
   GUID={0x5CCCB0EF,0x7073,0x4516,{0x80,0x28,0x4C,0x62,0x8D,0x0C,0x8A,0xAB}} */

#pragma code_seg(".orpc")
static const unsigned short IOneClickProcessLauncher_FormatStringOffsetTable[] =
    {
    5288
    };

static const MIDL_STUBLESS_PROXY_INFO IOneClickProcessLauncher_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IOneClickProcessLauncher_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IOneClickProcessLauncher_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IOneClickProcessLauncher_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IOneClickProcessLauncherProxyVtbl = 
{
    &IOneClickProcessLauncher_ProxyInfo,
    &IID_IOneClickProcessLauncher,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IOneClickProcessLauncher::LaunchAppCommand */
};

const CInterfaceStubVtbl _IOneClickProcessLauncherStubVtbl =
{
    &IID_IOneClickProcessLauncher,
    &IOneClickProcessLauncher_ServerInfo,
    4,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Standard interface: __MIDL_itf_google_update_idl_0000_0024, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IProgressWndEvents, ver. 0.0,
   GUID={0x1C642CED,0xCA3B,0x4013,{0xA9,0xDF,0xCA,0x6C,0xE5,0xFF,0x65,0x03}} */

#pragma code_seg(".orpc")
static const unsigned short IProgressWndEvents_FormatStringOffsetTable[] =
    {
    5336,
    5370,
    5404,
    5438,
    5472,
    5506
    };

static const MIDL_STUBLESS_PROXY_INFO IProgressWndEvents_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IProgressWndEvents_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IProgressWndEvents_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IProgressWndEvents_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(9) _IProgressWndEventsProxyVtbl = 
{
    &IProgressWndEvents_ProxyInfo,
    &IID_IProgressWndEvents,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IProgressWndEvents::DoClose */ ,
    (void *) (INT_PTR) -1 /* IProgressWndEvents::DoPause */ ,
    (void *) (INT_PTR) -1 /* IProgressWndEvents::DoResume */ ,
    (void *) (INT_PTR) -1 /* IProgressWndEvents::DoRestartBrowsers */ ,
    (void *) (INT_PTR) -1 /* IProgressWndEvents::DoReboot */ ,
    (void *) (INT_PTR) -1 /* IProgressWndEvents::DoLaunchBrowser */
};

const CInterfaceStubVtbl _IProgressWndEventsStubVtbl =
{
    &IID_IProgressWndEvents,
    &IProgressWndEvents_ServerInfo,
    9,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IJobObserver, ver. 0.0,
   GUID={0x49D7563B,0x2DDB,0x4831,{0x88,0xC8,0x76,0x8A,0x53,0x83,0x38,0x37}} */

#pragma code_seg(".orpc")
static const unsigned short IJobObserver_FormatStringOffsetTable[] =
    {
    5336,
    5370,
    5548,
    5438,
    5590,
    5638,
    4290,
    5672,
    5706,
    5754
    };

static const MIDL_STUBLESS_PROXY_INFO IJobObserver_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IJobObserver_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IJobObserver_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IJobObserver_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(13) _IJobObserverProxyVtbl = 
{
    &IJobObserver_ProxyInfo,
    &IID_IJobObserver,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IJobObserver::OnShow */ ,
    (void *) (INT_PTR) -1 /* IJobObserver::OnCheckingForUpdate */ ,
    (void *) (INT_PTR) -1 /* IJobObserver::OnUpdateAvailable */ ,
    (void *) (INT_PTR) -1 /* IJobObserver::OnWaitingToDownload */ ,
    (void *) (INT_PTR) -1 /* IJobObserver::OnDownloading */ ,
    (void *) (INT_PTR) -1 /* IJobObserver::OnWaitingToInstall */ ,
    (void *) (INT_PTR) -1 /* IJobObserver::OnInstalling */ ,
    (void *) (INT_PTR) -1 /* IJobObserver::OnPause */ ,
    (void *) (INT_PTR) -1 /* IJobObserver::OnComplete */ ,
    (void *) (INT_PTR) -1 /* IJobObserver::SetEventSink */
};

const CInterfaceStubVtbl _IJobObserverStubVtbl =
{
    &IID_IJobObserver,
    &IJobObserver_ServerInfo,
    13,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IJobObserver2, ver. 0.0,
   GUID={0x19692F10,0xADD2,0x4EFF,{0xBE,0x54,0xE6,0x1C,0x62,0xE4,0x0D,0x13}} */

#pragma code_seg(".orpc")
static const unsigned short IJobObserver2_FormatStringOffsetTable[] =
    {
    5796
    };

static const MIDL_STUBLESS_PROXY_INFO IJobObserver2_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IJobObserver2_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IJobObserver2_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IJobObserver2_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IJobObserver2ProxyVtbl = 
{
    &IJobObserver2_ProxyInfo,
    &IID_IJobObserver2,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IJobObserver2::OnInstalling2 */
};

const CInterfaceStubVtbl _IJobObserver2StubVtbl =
{
    &IID_IJobObserver2,
    &IJobObserver2_ServerInfo,
    4,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IGoogleUpdate, ver. 0.0,
   GUID={0x31AC3F11,0xE5EA,0x4a85,{0x8A,0x3D,0x8E,0x09,0x5A,0x39,0xC2,0x7B}} */

#pragma code_seg(".orpc")
static const unsigned short IGoogleUpdate_FormatStringOffsetTable[] =
    {
    5844,
    5892
    };

static const MIDL_STUBLESS_PROXY_INFO IGoogleUpdate_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IGoogleUpdate_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IGoogleUpdate_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IGoogleUpdate_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(5) _IGoogleUpdateProxyVtbl = 
{
    &IGoogleUpdate_ProxyInfo,
    &IID_IGoogleUpdate,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IGoogleUpdate::CheckForUpdate */ ,
    (void *) (INT_PTR) -1 /* IGoogleUpdate::Update */
};

const CInterfaceStubVtbl _IGoogleUpdateStubVtbl =
{
    &IID_IGoogleUpdate,
    &IGoogleUpdate_ServerInfo,
    5,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IGoogleUpdateCore, ver. 0.0,
   GUID={0x909489C2,0x85A6,0x4322,{0xAA,0x56,0xD2,0x52,0x78,0x64,0x9D,0x67}} */

#pragma code_seg(".orpc")
static const unsigned short IGoogleUpdateCore_FormatStringOffsetTable[] =
    {
    5940
    };

static const MIDL_STUBLESS_PROXY_INFO IGoogleUpdateCore_ProxyInfo =
    {
    &Object_StubDesc,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IGoogleUpdateCore_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IGoogleUpdateCore_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    google_update_idl__MIDL_ProcFormatString.Format,
    &IGoogleUpdateCore_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IGoogleUpdateCoreProxyVtbl = 
{
    &IGoogleUpdateCore_ProxyInfo,
    &IID_IGoogleUpdateCore,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IGoogleUpdateCore::LaunchCmdElevated */
};

const CInterfaceStubVtbl _IGoogleUpdateCoreStubVtbl =
{
    &IID_IGoogleUpdateCore,
    &IGoogleUpdateCore_ServerInfo,
    4,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};

static const MIDL_STUB_DESC Object_StubDesc = 
    {
    0,
    NdrOleAllocate,
    NdrOleFree,
    0,
    0,
    0,
    0,
    0,
    google_update_idl__MIDL_TypeFormatString.Format,
    1, /* -error bounds_check flag */
    0x50002, /* Ndr library version */
    0,
    0x801026e, /* MIDL Version 8.1.622 */
    0,
    UserMarshalRoutines,
    0,  /* notify & notify_flag routine table */
    0x1, /* MIDL flag */
    0, /* cs routines */
    0,   /* proxy/server info */
    0
    };

const CInterfaceProxyVtbl * const _google_update_idl_ProxyVtblList[] = 
{
    ( CInterfaceProxyVtbl *) &_ICoCreateAsyncStatusProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IJobObserver2ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IGoogleUpdateProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAppCommandWebProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAppVersionWebProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IRegistrationUpdateHookProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAppVersionProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IJobObserverProxyVtbl,
    ( CInterfaceProxyVtbl *) &_ICoCreateAsyncProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAppCommand2ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IGoogleUpdate3ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAppBundleWebProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IProcessLauncher2ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_ICredentialDialogProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAppWebProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IGoogleUpdate3WebSecurityProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IPackageProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAppProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IPolicyStatusProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IProcessLauncherProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IApp2ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IGoogleUpdateCoreProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IGoogleUpdate3WebProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IBrowserHttpRequest2ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAppBundleProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IProgressWndEventsProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IOneClickProcessLauncherProxyVtbl,
    ( CInterfaceProxyVtbl *) &_ICurrentStateProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAppCommandProxyVtbl,
    0
};

const CInterfaceStubVtbl * const _google_update_idl_StubVtblList[] = 
{
    ( CInterfaceStubVtbl *) &_ICoCreateAsyncStatusStubVtbl,
    ( CInterfaceStubVtbl *) &_IJobObserver2StubVtbl,
    ( CInterfaceStubVtbl *) &_IGoogleUpdateStubVtbl,
    ( CInterfaceStubVtbl *) &_IAppCommandWebStubVtbl,
    ( CInterfaceStubVtbl *) &_IAppVersionWebStubVtbl,
    ( CInterfaceStubVtbl *) &_IRegistrationUpdateHookStubVtbl,
    ( CInterfaceStubVtbl *) &_IAppVersionStubVtbl,
    ( CInterfaceStubVtbl *) &_IJobObserverStubVtbl,
    ( CInterfaceStubVtbl *) &_ICoCreateAsyncStubVtbl,
    ( CInterfaceStubVtbl *) &_IAppCommand2StubVtbl,
    ( CInterfaceStubVtbl *) &_IGoogleUpdate3StubVtbl,
    ( CInterfaceStubVtbl *) &_IAppBundleWebStubVtbl,
    ( CInterfaceStubVtbl *) &_IProcessLauncher2StubVtbl,
    ( CInterfaceStubVtbl *) &_ICredentialDialogStubVtbl,
    ( CInterfaceStubVtbl *) &_IAppWebStubVtbl,
    ( CInterfaceStubVtbl *) &_IGoogleUpdate3WebSecurityStubVtbl,
    ( CInterfaceStubVtbl *) &_IPackageStubVtbl,
    ( CInterfaceStubVtbl *) &_IAppStubVtbl,
    ( CInterfaceStubVtbl *) &_IPolicyStatusStubVtbl,
    ( CInterfaceStubVtbl *) &_IProcessLauncherStubVtbl,
    ( CInterfaceStubVtbl *) &_IApp2StubVtbl,
    ( CInterfaceStubVtbl *) &_IGoogleUpdateCoreStubVtbl,
    ( CInterfaceStubVtbl *) &_IGoogleUpdate3WebStubVtbl,
    ( CInterfaceStubVtbl *) &_IBrowserHttpRequest2StubVtbl,
    ( CInterfaceStubVtbl *) &_IAppBundleStubVtbl,
    ( CInterfaceStubVtbl *) &_IProgressWndEventsStubVtbl,
    ( CInterfaceStubVtbl *) &_IOneClickProcessLauncherStubVtbl,
    ( CInterfaceStubVtbl *) &_ICurrentStateStubVtbl,
    ( CInterfaceStubVtbl *) &_IAppCommandStubVtbl,
    0
};

PCInterfaceName const _google_update_idl_InterfaceNamesList[] = 
{
    "ICoCreateAsyncStatus",
    "IJobObserver2",
    "IGoogleUpdate",
    "IAppCommandWeb",
    "IAppVersionWeb",
    "IRegistrationUpdateHook",
    "IAppVersion",
    "IJobObserver",
    "ICoCreateAsync",
    "IAppCommand2",
    "IGoogleUpdate3",
    "IAppBundleWeb",
    "IProcessLauncher2",
    "ICredentialDialog",
    "IAppWeb",
    "IGoogleUpdate3WebSecurity",
    "IPackage",
    "IApp",
    "IPolicyStatus",
    "IProcessLauncher",
    "IApp2",
    "IGoogleUpdateCore",
    "IGoogleUpdate3Web",
    "IBrowserHttpRequest2",
    "IAppBundle",
    "IProgressWndEvents",
    "IOneClickProcessLauncher",
    "ICurrentState",
    "IAppCommand",
    0
};

const IID *  const _google_update_idl_BaseIIDList[] = 
{
    &IID_IDispatch,
    0,
    0,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    0,
    0,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    0,
    0,
    &IID_IDispatch,
    0,
    &IID_IDispatch,
    &IID_IDispatch,
    &IID_IDispatch,
    0,
    &IID_IDispatch,
    0,
    &IID_IDispatch,
    0,
    &IID_IDispatch,
    0,
    0,
    &IID_IDispatch,
    &IID_IDispatch,
    0
};


#define _google_update_idl_CHECK_IID(n)	IID_GENERIC_CHECK_IID( _google_update_idl, pIID, n)

int __stdcall _google_update_idl_IID_Lookup( const IID * pIID, int * pIndex )
{
    IID_BS_LOOKUP_SETUP

    IID_BS_LOOKUP_INITIAL_TEST( _google_update_idl, 29, 16 )
    IID_BS_LOOKUP_NEXT_TEST( _google_update_idl, 8 )
    IID_BS_LOOKUP_NEXT_TEST( _google_update_idl, 4 )
    IID_BS_LOOKUP_NEXT_TEST( _google_update_idl, 2 )
    IID_BS_LOOKUP_NEXT_TEST( _google_update_idl, 1 )
    IID_BS_LOOKUP_RETURN_RESULT( _google_update_idl, 29, *pIndex )
    
}

const ExtendedProxyFileInfo google_update_idl_ProxyFileInfo = 
{
    (PCInterfaceProxyVtblList *) & _google_update_idl_ProxyVtblList,
    (PCInterfaceStubVtblList *) & _google_update_idl_StubVtblList,
    (const PCInterfaceName * ) & _google_update_idl_InterfaceNamesList,
    (const IID ** ) & _google_update_idl_BaseIIDList,
    & _google_update_idl_IID_Lookup, 
    29,
    2,
    0, /* table of [async_uuid] interfaces */
    0, /* Filler1 */
    0, /* Filler2 */
    0  /* Filler3 */
};
#if _MSC_VER >= 1200
#pragma warning(pop)
#endif


#endif /* defined(_M_ARM64)*/

