

/* this ALWAYS GENERATED file contains the proxy stub code */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for ../../third_party/iaccessible2/ia2_api_all.idl:
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


#include "ia2_api_all.h"

#define TYPE_FORMAT_STRING_SIZE   1519                              
#define PROC_FORMAT_STRING_SIZE   6071                              
#define EXPR_FORMAT_STRING_SIZE   1                                 
#define TRANSMIT_AS_TABLE_SIZE    0            
#define WIRE_MARSHAL_TABLE_SIZE   3            

typedef struct _ia2_api_all_MIDL_TYPE_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ TYPE_FORMAT_STRING_SIZE ];
    } ia2_api_all_MIDL_TYPE_FORMAT_STRING;

typedef struct _ia2_api_all_MIDL_PROC_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ PROC_FORMAT_STRING_SIZE ];
    } ia2_api_all_MIDL_PROC_FORMAT_STRING;

typedef struct _ia2_api_all_MIDL_EXPR_FORMAT_STRING
    {
    long          Pad;
    unsigned char  Format[ EXPR_FORMAT_STRING_SIZE ];
    } ia2_api_all_MIDL_EXPR_FORMAT_STRING;


static const RPC_SYNTAX_IDENTIFIER  _RpcTransferSyntax = 
{{0x8A885D04,0x1CEB,0x11C9,{0x9F,0xE8,0x08,0x00,0x2B,0x10,0x48,0x60}},{2,0}};


extern const ia2_api_all_MIDL_TYPE_FORMAT_STRING ia2_api_all__MIDL_TypeFormatString;
extern const ia2_api_all_MIDL_PROC_FORMAT_STRING ia2_api_all__MIDL_ProcFormatString;
extern const ia2_api_all_MIDL_EXPR_FORMAT_STRING ia2_api_all__MIDL_ExprFormatString;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAccessibleRelation_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleRelation_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAccessibleAction_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleAction_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAccessible2_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessible2_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAccessible2_2_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessible2_2_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAccessible2_3_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessible2_3_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAccessibleComponent_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleComponent_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAccessibleValue_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleValue_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAccessibleText_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleText_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAccessibleText2_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleText2_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAccessibleEditableText_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleEditableText_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAccessibleHyperlink_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleHyperlink_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAccessibleHypertext_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleHypertext_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAccessibleHypertext2_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleHypertext2_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAccessibleTable_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleTable_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAccessibleTable2_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleTable2_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAccessibleTableCell_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleTableCell_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAccessibleImage_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleImage_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAccessibleApplication_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleApplication_ProxyInfo;


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IAccessibleDocument_ServerInfo;
extern const MIDL_STUBLESS_PROXY_INFO IAccessibleDocument_ProxyInfo;


extern const USER_MARSHAL_ROUTINE_QUADRUPLE UserMarshalRoutines[ WIRE_MARSHAL_TABLE_SIZE ];

#if !defined(__RPC_ARM64__)
#error  Invalid build platform for this stub.
#endif

static const ia2_api_all_MIDL_PROC_FORMAT_STRING ia2_api_all__MIDL_ProcFormatString =
    {
        0,
        {

	/* Procedure get_appName */


	/* Procedure get_description */


	/* Procedure get_relationType */

			0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
/*  6 */	NdrFcShort( 0x3 ),	/* 3 */
/*  8 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 10 */	NdrFcShort( 0x0 ),	/* 0 */
/* 12 */	NdrFcShort( 0x8 ),	/* 8 */
/* 14 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 16 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 18 */	NdrFcShort( 0x1 ),	/* 1 */
/* 20 */	NdrFcShort( 0x0 ),	/* 0 */
/* 22 */	NdrFcShort( 0x0 ),	/* 0 */
/* 24 */	NdrFcShort( 0x2 ),	/* 2 */
/* 26 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 28 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter name */


	/* Parameter description */


	/* Parameter relationType */

/* 30 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 32 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 34 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */


	/* Return value */


	/* Return value */

/* 36 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 38 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 40 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_appVersion */


	/* Procedure get_localizedRelationType */

/* 42 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 44 */	NdrFcLong( 0x0 ),	/* 0 */
/* 48 */	NdrFcShort( 0x4 ),	/* 4 */
/* 50 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 52 */	NdrFcShort( 0x0 ),	/* 0 */
/* 54 */	NdrFcShort( 0x8 ),	/* 8 */
/* 56 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 58 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 60 */	NdrFcShort( 0x1 ),	/* 1 */
/* 62 */	NdrFcShort( 0x0 ),	/* 0 */
/* 64 */	NdrFcShort( 0x0 ),	/* 0 */
/* 66 */	NdrFcShort( 0x2 ),	/* 2 */
/* 68 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 70 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter version */


	/* Parameter localizedRelationType */

/* 72 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 74 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 76 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */


	/* Return value */

/* 78 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 80 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 82 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnIndex */


	/* Procedure get_caretOffset */


	/* Procedure get_background */


	/* Procedure get_nTargets */

/* 84 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 86 */	NdrFcLong( 0x0 ),	/* 0 */
/* 90 */	NdrFcShort( 0x5 ),	/* 5 */
/* 92 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 94 */	NdrFcShort( 0x0 ),	/* 0 */
/* 96 */	NdrFcShort( 0x24 ),	/* 36 */
/* 98 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 100 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 102 */	NdrFcShort( 0x0 ),	/* 0 */
/* 104 */	NdrFcShort( 0x0 ),	/* 0 */
/* 106 */	NdrFcShort( 0x0 ),	/* 0 */
/* 108 */	NdrFcShort( 0x2 ),	/* 2 */
/* 110 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 112 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter columnIndex */


	/* Parameter offset */


	/* Parameter background */


	/* Parameter nTargets */

/* 114 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 116 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 118 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */


	/* Return value */


	/* Return value */

/* 120 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 122 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 124 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_target */

/* 126 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 128 */	NdrFcLong( 0x0 ),	/* 0 */
/* 132 */	NdrFcShort( 0x6 ),	/* 6 */
/* 134 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 136 */	NdrFcShort( 0x8 ),	/* 8 */
/* 138 */	NdrFcShort( 0x8 ),	/* 8 */
/* 140 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 142 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 144 */	NdrFcShort( 0x0 ),	/* 0 */
/* 146 */	NdrFcShort( 0x0 ),	/* 0 */
/* 148 */	NdrFcShort( 0x0 ),	/* 0 */
/* 150 */	NdrFcShort( 0x3 ),	/* 3 */
/* 152 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 154 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter targetIndex */

/* 156 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 158 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 160 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter target */

/* 162 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 164 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 166 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */

/* 168 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 170 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 172 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_targets */

/* 174 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 176 */	NdrFcLong( 0x0 ),	/* 0 */
/* 180 */	NdrFcShort( 0x7 ),	/* 7 */
/* 182 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 184 */	NdrFcShort( 0x8 ),	/* 8 */
/* 186 */	NdrFcShort( 0x24 ),	/* 36 */
/* 188 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 190 */	0x10,		/* 16 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 192 */	NdrFcShort( 0x1 ),	/* 1 */
/* 194 */	NdrFcShort( 0x0 ),	/* 0 */
/* 196 */	NdrFcShort( 0x0 ),	/* 0 */
/* 198 */	NdrFcShort( 0x4 ),	/* 4 */
/* 200 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 202 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 204 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter maxTargets */

/* 206 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 208 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 210 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter targets */

/* 212 */	NdrFcShort( 0x113 ),	/* Flags:  must size, must free, out, simple ref, */
/* 214 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 216 */	NdrFcShort( 0x48 ),	/* Type Offset=72 */

	/* Parameter nTargets */

/* 218 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 220 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 222 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 224 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 226 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 228 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnExtent */


	/* Procedure nActions */

/* 230 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 232 */	NdrFcLong( 0x0 ),	/* 0 */
/* 236 */	NdrFcShort( 0x3 ),	/* 3 */
/* 238 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 240 */	NdrFcShort( 0x0 ),	/* 0 */
/* 242 */	NdrFcShort( 0x24 ),	/* 36 */
/* 244 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 246 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 248 */	NdrFcShort( 0x0 ),	/* 0 */
/* 250 */	NdrFcShort( 0x0 ),	/* 0 */
/* 252 */	NdrFcShort( 0x0 ),	/* 0 */
/* 254 */	NdrFcShort( 0x2 ),	/* 2 */
/* 256 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 258 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter nColumnsSpanned */


	/* Parameter nActions */

/* 260 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 262 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 264 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 266 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 268 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 270 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure doAction */

/* 272 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 274 */	NdrFcLong( 0x0 ),	/* 0 */
/* 278 */	NdrFcShort( 0x4 ),	/* 4 */
/* 280 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 282 */	NdrFcShort( 0x8 ),	/* 8 */
/* 284 */	NdrFcShort( 0x8 ),	/* 8 */
/* 286 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 288 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 290 */	NdrFcShort( 0x0 ),	/* 0 */
/* 292 */	NdrFcShort( 0x0 ),	/* 0 */
/* 294 */	NdrFcShort( 0x0 ),	/* 0 */
/* 296 */	NdrFcShort( 0x2 ),	/* 2 */
/* 298 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 300 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter actionIndex */

/* 302 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 304 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 306 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 308 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 310 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 312 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnDescription */


	/* Procedure get_description */

/* 314 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 316 */	NdrFcLong( 0x0 ),	/* 0 */
/* 320 */	NdrFcShort( 0x5 ),	/* 5 */
/* 322 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 324 */	NdrFcShort( 0x8 ),	/* 8 */
/* 326 */	NdrFcShort( 0x8 ),	/* 8 */
/* 328 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 330 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 332 */	NdrFcShort( 0x1 ),	/* 1 */
/* 334 */	NdrFcShort( 0x0 ),	/* 0 */
/* 336 */	NdrFcShort( 0x0 ),	/* 0 */
/* 338 */	NdrFcShort( 0x3 ),	/* 3 */
/* 340 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 342 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter column */


	/* Parameter actionIndex */

/* 344 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 346 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 348 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter description */


	/* Parameter description */

/* 350 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 352 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 354 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */


	/* Return value */

/* 356 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 358 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 360 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_keyBinding */

/* 362 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 364 */	NdrFcLong( 0x0 ),	/* 0 */
/* 368 */	NdrFcShort( 0x6 ),	/* 6 */
/* 370 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 372 */	NdrFcShort( 0x10 ),	/* 16 */
/* 374 */	NdrFcShort( 0x24 ),	/* 36 */
/* 376 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x5,		/* 5 */
/* 378 */	0x10,		/* 16 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 380 */	NdrFcShort( 0x1 ),	/* 1 */
/* 382 */	NdrFcShort( 0x0 ),	/* 0 */
/* 384 */	NdrFcShort( 0x0 ),	/* 0 */
/* 386 */	NdrFcShort( 0x5 ),	/* 5 */
/* 388 */	0x5,		/* 5 */
			0x80,		/* 128 */
/* 390 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 392 */	0x83,		/* 131 */
			0x84,		/* 132 */

	/* Parameter actionIndex */

/* 394 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 396 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 398 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter nMaxBindings */

/* 400 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 402 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 404 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter keyBindings */

/* 406 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 408 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 410 */	NdrFcShort( 0x5e ),	/* Type Offset=94 */

	/* Parameter nBindings */

/* 412 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 414 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 416 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 418 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 420 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 422 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_name */

/* 424 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 426 */	NdrFcLong( 0x0 ),	/* 0 */
/* 430 */	NdrFcShort( 0x7 ),	/* 7 */
/* 432 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 434 */	NdrFcShort( 0x8 ),	/* 8 */
/* 436 */	NdrFcShort( 0x8 ),	/* 8 */
/* 438 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 440 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 442 */	NdrFcShort( 0x1 ),	/* 1 */
/* 444 */	NdrFcShort( 0x0 ),	/* 0 */
/* 446 */	NdrFcShort( 0x0 ),	/* 0 */
/* 448 */	NdrFcShort( 0x3 ),	/* 3 */
/* 450 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 452 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter actionIndex */

/* 454 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 456 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 458 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter name */

/* 460 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 462 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 464 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 466 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 468 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 470 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_localizedName */

/* 472 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 474 */	NdrFcLong( 0x0 ),	/* 0 */
/* 478 */	NdrFcShort( 0x8 ),	/* 8 */
/* 480 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 482 */	NdrFcShort( 0x8 ),	/* 8 */
/* 484 */	NdrFcShort( 0x8 ),	/* 8 */
/* 486 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 488 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 490 */	NdrFcShort( 0x1 ),	/* 1 */
/* 492 */	NdrFcShort( 0x0 ),	/* 0 */
/* 494 */	NdrFcShort( 0x0 ),	/* 0 */
/* 496 */	NdrFcShort( 0x3 ),	/* 3 */
/* 498 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 500 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter actionIndex */

/* 502 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 504 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 506 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter localizedName */

/* 508 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 510 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 512 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 514 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 516 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 518 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nRelations */

/* 520 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 522 */	NdrFcLong( 0x0 ),	/* 0 */
/* 526 */	NdrFcShort( 0x1c ),	/* 28 */
/* 528 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 530 */	NdrFcShort( 0x0 ),	/* 0 */
/* 532 */	NdrFcShort( 0x24 ),	/* 36 */
/* 534 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 536 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 538 */	NdrFcShort( 0x0 ),	/* 0 */
/* 540 */	NdrFcShort( 0x0 ),	/* 0 */
/* 542 */	NdrFcShort( 0x0 ),	/* 0 */
/* 544 */	NdrFcShort( 0x2 ),	/* 2 */
/* 546 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 548 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter nRelations */

/* 550 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 552 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 554 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 556 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 558 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 560 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_relation */

/* 562 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 564 */	NdrFcLong( 0x0 ),	/* 0 */
/* 568 */	NdrFcShort( 0x1d ),	/* 29 */
/* 570 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 572 */	NdrFcShort( 0x8 ),	/* 8 */
/* 574 */	NdrFcShort( 0x8 ),	/* 8 */
/* 576 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 578 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 580 */	NdrFcShort( 0x0 ),	/* 0 */
/* 582 */	NdrFcShort( 0x0 ),	/* 0 */
/* 584 */	NdrFcShort( 0x0 ),	/* 0 */
/* 586 */	NdrFcShort( 0x3 ),	/* 3 */
/* 588 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 590 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter relationIndex */

/* 592 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 594 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 596 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter relation */

/* 598 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 600 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 602 */	NdrFcShort( 0x7c ),	/* Type Offset=124 */

	/* Return value */

/* 604 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 606 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 608 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_relations */

/* 610 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 612 */	NdrFcLong( 0x0 ),	/* 0 */
/* 616 */	NdrFcShort( 0x1e ),	/* 30 */
/* 618 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 620 */	NdrFcShort( 0x8 ),	/* 8 */
/* 622 */	NdrFcShort( 0x24 ),	/* 36 */
/* 624 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 626 */	0x10,		/* 16 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 628 */	NdrFcShort( 0x1 ),	/* 1 */
/* 630 */	NdrFcShort( 0x0 ),	/* 0 */
/* 632 */	NdrFcShort( 0x0 ),	/* 0 */
/* 634 */	NdrFcShort( 0x4 ),	/* 4 */
/* 636 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 638 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 640 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter maxRelations */

/* 642 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 644 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 646 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter relations */

/* 648 */	NdrFcShort( 0x113 ),	/* Flags:  must size, must free, out, simple ref, */
/* 650 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 652 */	NdrFcShort( 0x96 ),	/* Type Offset=150 */

	/* Parameter nRelations */

/* 654 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 656 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 658 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 660 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 662 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 664 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure role */

/* 666 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 668 */	NdrFcLong( 0x0 ),	/* 0 */
/* 672 */	NdrFcShort( 0x1f ),	/* 31 */
/* 674 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 676 */	NdrFcShort( 0x0 ),	/* 0 */
/* 678 */	NdrFcShort( 0x24 ),	/* 36 */
/* 680 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 682 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 684 */	NdrFcShort( 0x0 ),	/* 0 */
/* 686 */	NdrFcShort( 0x0 ),	/* 0 */
/* 688 */	NdrFcShort( 0x0 ),	/* 0 */
/* 690 */	NdrFcShort( 0x2 ),	/* 2 */
/* 692 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 694 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter role */

/* 696 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 698 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 700 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 702 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 704 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 706 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure scrollTo */

/* 708 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 710 */	NdrFcLong( 0x0 ),	/* 0 */
/* 714 */	NdrFcShort( 0x20 ),	/* 32 */
/* 716 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 718 */	NdrFcShort( 0x6 ),	/* 6 */
/* 720 */	NdrFcShort( 0x8 ),	/* 8 */
/* 722 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 724 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 726 */	NdrFcShort( 0x0 ),	/* 0 */
/* 728 */	NdrFcShort( 0x0 ),	/* 0 */
/* 730 */	NdrFcShort( 0x0 ),	/* 0 */
/* 732 */	NdrFcShort( 0x2 ),	/* 2 */
/* 734 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 736 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter scrollType */

/* 738 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 740 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 742 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Return value */

/* 744 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 746 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 748 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure scrollToPoint */

/* 750 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 752 */	NdrFcLong( 0x0 ),	/* 0 */
/* 756 */	NdrFcShort( 0x21 ),	/* 33 */
/* 758 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 760 */	NdrFcShort( 0x16 ),	/* 22 */
/* 762 */	NdrFcShort( 0x8 ),	/* 8 */
/* 764 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 766 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 768 */	NdrFcShort( 0x0 ),	/* 0 */
/* 770 */	NdrFcShort( 0x0 ),	/* 0 */
/* 772 */	NdrFcShort( 0x0 ),	/* 0 */
/* 774 */	NdrFcShort( 0x4 ),	/* 4 */
/* 776 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 778 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 780 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter coordinateType */

/* 782 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 784 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 786 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter x */

/* 788 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 790 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 792 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter y */

/* 794 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 796 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 798 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 800 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 802 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 804 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_groupPosition */

/* 806 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 808 */	NdrFcLong( 0x0 ),	/* 0 */
/* 812 */	NdrFcShort( 0x22 ),	/* 34 */
/* 814 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 816 */	NdrFcShort( 0x0 ),	/* 0 */
/* 818 */	NdrFcShort( 0x5c ),	/* 92 */
/* 820 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 822 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 824 */	NdrFcShort( 0x0 ),	/* 0 */
/* 826 */	NdrFcShort( 0x0 ),	/* 0 */
/* 828 */	NdrFcShort( 0x0 ),	/* 0 */
/* 830 */	NdrFcShort( 0x4 ),	/* 4 */
/* 832 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 834 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 836 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter groupLevel */

/* 838 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 840 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 842 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter similarItemsInGroup */

/* 844 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 846 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 848 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter positionInGroup */

/* 850 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 852 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 854 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 856 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 858 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 860 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_states */

/* 862 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 864 */	NdrFcLong( 0x0 ),	/* 0 */
/* 868 */	NdrFcShort( 0x23 ),	/* 35 */
/* 870 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 872 */	NdrFcShort( 0x0 ),	/* 0 */
/* 874 */	NdrFcShort( 0x24 ),	/* 36 */
/* 876 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 878 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 880 */	NdrFcShort( 0x0 ),	/* 0 */
/* 882 */	NdrFcShort( 0x0 ),	/* 0 */
/* 884 */	NdrFcShort( 0x0 ),	/* 0 */
/* 886 */	NdrFcShort( 0x2 ),	/* 2 */
/* 888 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 890 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter states */

/* 892 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 894 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 896 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 898 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 900 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 902 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_extendedRole */

/* 904 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 906 */	NdrFcLong( 0x0 ),	/* 0 */
/* 910 */	NdrFcShort( 0x24 ),	/* 36 */
/* 912 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 914 */	NdrFcShort( 0x0 ),	/* 0 */
/* 916 */	NdrFcShort( 0x8 ),	/* 8 */
/* 918 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 920 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 922 */	NdrFcShort( 0x1 ),	/* 1 */
/* 924 */	NdrFcShort( 0x0 ),	/* 0 */
/* 926 */	NdrFcShort( 0x0 ),	/* 0 */
/* 928 */	NdrFcShort( 0x2 ),	/* 2 */
/* 930 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 932 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter extendedRole */

/* 934 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 936 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 938 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 940 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 942 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 944 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_localizedExtendedRole */

/* 946 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 948 */	NdrFcLong( 0x0 ),	/* 0 */
/* 952 */	NdrFcShort( 0x25 ),	/* 37 */
/* 954 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 956 */	NdrFcShort( 0x0 ),	/* 0 */
/* 958 */	NdrFcShort( 0x8 ),	/* 8 */
/* 960 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 962 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 964 */	NdrFcShort( 0x1 ),	/* 1 */
/* 966 */	NdrFcShort( 0x0 ),	/* 0 */
/* 968 */	NdrFcShort( 0x0 ),	/* 0 */
/* 970 */	NdrFcShort( 0x2 ),	/* 2 */
/* 972 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 974 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter localizedExtendedRole */

/* 976 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 978 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 980 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 982 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 984 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 986 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nExtendedStates */

/* 988 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 990 */	NdrFcLong( 0x0 ),	/* 0 */
/* 994 */	NdrFcShort( 0x26 ),	/* 38 */
/* 996 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 998 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1000 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1002 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 1004 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1006 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1008 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1010 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1012 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1014 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1016 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter nExtendedStates */

/* 1018 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1020 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1022 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1024 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1026 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1028 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_extendedStates */

/* 1030 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1032 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1036 */	NdrFcShort( 0x27 ),	/* 39 */
/* 1038 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1040 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1042 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1044 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 1046 */	0x10,		/* 16 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1048 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1050 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1052 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1054 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1056 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 1058 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1060 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter maxExtendedStates */

/* 1062 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1064 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1066 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter extendedStates */

/* 1068 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 1070 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1072 */	NdrFcShort( 0xac ),	/* Type Offset=172 */

	/* Parameter nExtendedStates */

/* 1074 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1076 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1078 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1080 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1082 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1084 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_localizedExtendedStates */

/* 1086 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1088 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1092 */	NdrFcShort( 0x28 ),	/* 40 */
/* 1094 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1096 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1098 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1100 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 1102 */	0x10,		/* 16 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1104 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1106 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1108 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1110 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1112 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 1114 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1116 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter maxLocalizedExtendedStates */

/* 1118 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1120 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1122 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter localizedExtendedStates */

/* 1124 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 1126 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1128 */	NdrFcShort( 0xac ),	/* Type Offset=172 */

	/* Parameter nLocalizedExtendedStates */

/* 1130 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1132 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1134 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1136 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1138 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1140 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_uniqueID */

/* 1142 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1144 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1148 */	NdrFcShort( 0x29 ),	/* 41 */
/* 1150 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1152 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1154 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1156 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 1158 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1160 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1162 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1164 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1166 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1168 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1170 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter uniqueID */

/* 1172 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1174 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1176 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1178 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1180 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1182 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_windowHandle */

/* 1184 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1186 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1190 */	NdrFcShort( 0x2a ),	/* 42 */
/* 1192 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1194 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1196 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1198 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1200 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1202 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1204 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1206 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1208 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1210 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1212 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter windowHandle */

/* 1214 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 1216 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1218 */	NdrFcShort( 0xe6 ),	/* Type Offset=230 */

	/* Return value */

/* 1220 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1222 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1224 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_indexInParent */

/* 1226 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1228 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1232 */	NdrFcShort( 0x2b ),	/* 43 */
/* 1234 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1236 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1238 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1240 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 1242 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1244 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1246 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1248 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1250 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1252 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1254 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter indexInParent */

/* 1256 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1258 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1260 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1262 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1264 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1266 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_locale */

/* 1268 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1270 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1274 */	NdrFcShort( 0x2c ),	/* 44 */
/* 1276 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1278 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1280 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1282 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1284 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1286 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1288 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1290 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1292 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1294 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1296 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter locale */

/* 1298 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 1300 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1302 */	NdrFcShort( 0xf4 ),	/* Type Offset=244 */

	/* Return value */

/* 1304 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1306 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1308 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_attributes */

/* 1310 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1312 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1316 */	NdrFcShort( 0x2d ),	/* 45 */
/* 1318 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1320 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1322 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1324 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1326 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1328 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1330 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1332 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1334 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1336 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1338 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter attributes */

/* 1340 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 1342 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1344 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 1346 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1348 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1350 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_attribute */

/* 1352 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1354 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1358 */	NdrFcShort( 0x2e ),	/* 46 */
/* 1360 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1362 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1364 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1366 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 1368 */	0xe,		/* 14 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 1370 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1372 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1374 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1376 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1378 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 1380 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter name */

/* 1382 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1384 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1386 */	NdrFcShort( 0x10e ),	/* Type Offset=270 */

	/* Parameter attribute */

/* 1388 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 1390 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1392 */	NdrFcShort( 0x4bc ),	/* Type Offset=1212 */

	/* Return value */

/* 1394 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1396 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1398 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_accessibleWithCaret */

/* 1400 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1402 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1406 */	NdrFcShort( 0x2f ),	/* 47 */
/* 1408 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1410 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1412 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1414 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 1416 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1418 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1420 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1422 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1424 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1426 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 1428 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter accessible */

/* 1430 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 1432 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1434 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Parameter caretOffset */

/* 1436 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1438 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1440 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1442 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1444 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1446 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_relationTargetsOfType */

/* 1448 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1450 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1454 */	NdrFcShort( 0x30 ),	/* 48 */
/* 1456 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 1458 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1460 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1462 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 1464 */	0x10,		/* 16 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 1466 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1468 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1470 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1472 */	NdrFcShort( 0x5 ),	/* 5 */
/* 1474 */	0x5,		/* 5 */
			0x80,		/* 128 */
/* 1476 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1478 */	0x83,		/* 131 */
			0x84,		/* 132 */

	/* Parameter type */

/* 1480 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 1482 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1484 */	NdrFcShort( 0x10e ),	/* Type Offset=270 */

	/* Parameter maxTargets */

/* 1486 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1488 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1490 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter targets */

/* 1492 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 1494 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1496 */	NdrFcShort( 0x4c6 ),	/* Type Offset=1222 */

	/* Parameter nTargets */

/* 1498 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1500 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1502 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1504 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1506 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1508 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selectionRanges */

/* 1510 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1512 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1516 */	NdrFcShort( 0x31 ),	/* 49 */
/* 1518 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1520 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1522 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1524 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 1526 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1528 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1530 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1532 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1534 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1536 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 1538 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter ranges */

/* 1540 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 1542 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1544 */	NdrFcShort( 0x4e4 ),	/* Type Offset=1252 */

	/* Parameter nRanges */

/* 1546 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1548 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1550 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1552 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1554 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1556 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_locationInParent */

/* 1558 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1560 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1564 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1566 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1568 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1570 */	NdrFcShort( 0x40 ),	/* 64 */
/* 1572 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 1574 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1576 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1578 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1580 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1582 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1584 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 1586 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter x */

/* 1588 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1590 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1592 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter y */

/* 1594 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1596 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1598 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1600 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1602 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1604 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_foreground */

/* 1606 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1608 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1612 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1614 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1616 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1618 */	NdrFcShort( 0x24 ),	/* 36 */
/* 1620 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 1622 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1624 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1626 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1628 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1630 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1632 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1634 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter foreground */

/* 1636 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1638 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1640 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1642 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1644 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1646 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_currentValue */

/* 1648 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1650 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1654 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1656 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1658 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1660 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1662 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1664 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1666 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1668 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1670 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1672 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1674 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1676 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter currentValue */

/* 1678 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 1680 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1682 */	NdrFcShort( 0x4bc ),	/* Type Offset=1212 */

	/* Return value */

/* 1684 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1686 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1688 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure setCurrentValue */

/* 1690 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1692 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1696 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1698 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1700 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1702 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1704 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 1706 */	0xe,		/* 14 */
			0x85,		/* Ext Flags:  new corr desc, srv corr check, has big byval param */
/* 1708 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1710 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1712 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1714 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1716 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1718 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter value */

/* 1720 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 1722 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1724 */	NdrFcShort( 0x520 ),	/* Type Offset=1312 */

	/* Return value */

/* 1726 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1728 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1730 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_maximumValue */

/* 1732 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1734 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1738 */	NdrFcShort( 0x5 ),	/* 5 */
/* 1740 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1742 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1744 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1746 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1748 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1750 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1752 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1754 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1756 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1758 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1760 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter maximumValue */

/* 1762 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 1764 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1766 */	NdrFcShort( 0x4bc ),	/* Type Offset=1212 */

	/* Return value */

/* 1768 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1770 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1772 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_minimumValue */

/* 1774 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1776 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1780 */	NdrFcShort( 0x6 ),	/* 6 */
/* 1782 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1784 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1786 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1788 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 1790 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1792 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1794 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1796 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1798 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1800 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 1802 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter minimumValue */

/* 1804 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 1806 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1808 */	NdrFcShort( 0x4bc ),	/* Type Offset=1212 */

	/* Return value */

/* 1810 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1812 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1814 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure copyText */


	/* Procedure addSelection */

/* 1816 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1818 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1822 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1824 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1826 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1828 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1830 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 1832 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1834 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1836 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1838 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1840 */	NdrFcShort( 0x3 ),	/* 3 */
/* 1842 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 1844 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter startOffset */


	/* Parameter startOffset */

/* 1846 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1848 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1850 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */


	/* Parameter endOffset */

/* 1852 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1854 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1856 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 1858 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1860 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1862 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_attributes */

/* 1864 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1866 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1870 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1872 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 1874 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1876 */	NdrFcShort( 0x40 ),	/* 64 */
/* 1878 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x5,		/* 5 */
/* 1880 */	0x10,		/* 16 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 1882 */	NdrFcShort( 0x1 ),	/* 1 */
/* 1884 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1886 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1888 */	NdrFcShort( 0x5 ),	/* 5 */
/* 1890 */	0x5,		/* 5 */
			0x80,		/* 128 */
/* 1892 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1894 */	0x83,		/* 131 */
			0x84,		/* 132 */

	/* Parameter offset */

/* 1896 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1898 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1900 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 1902 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1904 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1906 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 1908 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1910 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1912 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter textAttributes */

/* 1914 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 1916 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1918 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 1920 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1922 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1924 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_characterExtents */

/* 1926 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 1928 */	NdrFcLong( 0x0 ),	/* 0 */
/* 1932 */	NdrFcShort( 0x6 ),	/* 6 */
/* 1934 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 1936 */	NdrFcShort( 0xe ),	/* 14 */
/* 1938 */	NdrFcShort( 0x78 ),	/* 120 */
/* 1940 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x7,		/* 7 */
/* 1942 */	0x12,		/* 18 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 1944 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1946 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1948 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1950 */	NdrFcShort( 0x7 ),	/* 7 */
/* 1952 */	0x7,		/* 7 */
			0x80,		/* 128 */
/* 1954 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 1956 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 1958 */	0x85,		/* 133 */
			0x86,		/* 134 */

	/* Parameter offset */

/* 1960 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1962 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1964 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter coordType */

/* 1966 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 1968 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1970 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter x */

/* 1972 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1974 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1976 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter y */

/* 1978 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1980 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1982 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter width */

/* 1984 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1986 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 1988 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter height */

/* 1990 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 1992 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 1994 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 1996 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 1998 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 2000 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nRows */


	/* Procedure get_nSelections */

/* 2002 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2004 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2008 */	NdrFcShort( 0x7 ),	/* 7 */
/* 2010 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2012 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2014 */	NdrFcShort( 0x24 ),	/* 36 */
/* 2016 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 2018 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2020 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2022 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2024 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2026 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2028 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2030 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter rowCount */


	/* Parameter nSelections */

/* 2032 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2034 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2036 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 2038 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2040 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2042 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_offsetAtPoint */

/* 2044 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2046 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2050 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2052 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 2054 */	NdrFcShort( 0x16 ),	/* 22 */
/* 2056 */	NdrFcShort( 0x24 ),	/* 36 */
/* 2058 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x5,		/* 5 */
/* 2060 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2062 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2064 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2066 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2068 */	NdrFcShort( 0x5 ),	/* 5 */
/* 2070 */	0x5,		/* 5 */
			0x80,		/* 128 */
/* 2072 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2074 */	0x83,		/* 131 */
			0x84,		/* 132 */

	/* Parameter x */

/* 2076 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2078 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2080 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter y */

/* 2082 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2084 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2086 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter coordType */

/* 2088 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2090 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2092 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter offset */

/* 2094 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2096 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2098 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2100 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2102 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2104 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selection */

/* 2106 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2108 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2112 */	NdrFcShort( 0x9 ),	/* 9 */
/* 2114 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2116 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2118 */	NdrFcShort( 0x40 ),	/* 64 */
/* 2120 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 2122 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2124 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2126 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2128 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2130 */	NdrFcShort( 0x4 ),	/* 4 */
/* 2132 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 2134 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2136 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter selectionIndex */

/* 2138 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2140 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2142 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 2144 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2146 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2148 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2150 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2152 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2154 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2156 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2158 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2160 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_text */

/* 2162 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2164 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2168 */	NdrFcShort( 0xa ),	/* 10 */
/* 2170 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2172 */	NdrFcShort( 0x10 ),	/* 16 */
/* 2174 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2176 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 2178 */	0x10,		/* 16 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2180 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2182 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2184 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2186 */	NdrFcShort( 0x4 ),	/* 4 */
/* 2188 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 2190 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2192 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 2194 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2196 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2198 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2200 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2202 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2204 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter text */

/* 2206 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2208 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2210 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 2212 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2214 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2216 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_textBeforeOffset */

/* 2218 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2220 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2224 */	NdrFcShort( 0xb ),	/* 11 */
/* 2226 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 2228 */	NdrFcShort( 0xe ),	/* 14 */
/* 2230 */	NdrFcShort( 0x40 ),	/* 64 */
/* 2232 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x6,		/* 6 */
/* 2234 */	0x12,		/* 18 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2236 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2238 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2240 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2242 */	NdrFcShort( 0x6 ),	/* 6 */
/* 2244 */	0x6,		/* 6 */
			0x80,		/* 128 */
/* 2246 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2248 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 2250 */	0x85,		/* 133 */
			0x0,		/* 0 */

	/* Parameter offset */

/* 2252 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2254 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2256 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter boundaryType */

/* 2258 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2260 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2262 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 2264 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2266 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2268 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2270 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2272 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2274 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter text */

/* 2276 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2278 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2280 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 2282 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2284 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 2286 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_textAfterOffset */

/* 2288 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2290 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2294 */	NdrFcShort( 0xc ),	/* 12 */
/* 2296 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 2298 */	NdrFcShort( 0xe ),	/* 14 */
/* 2300 */	NdrFcShort( 0x40 ),	/* 64 */
/* 2302 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x6,		/* 6 */
/* 2304 */	0x12,		/* 18 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2306 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2308 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2310 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2312 */	NdrFcShort( 0x6 ),	/* 6 */
/* 2314 */	0x6,		/* 6 */
			0x80,		/* 128 */
/* 2316 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2318 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 2320 */	0x85,		/* 133 */
			0x0,		/* 0 */

	/* Parameter offset */

/* 2322 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2324 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2326 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter boundaryType */

/* 2328 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2330 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2332 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 2334 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2336 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2338 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2340 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2342 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2344 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter text */

/* 2346 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2348 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2350 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 2352 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2354 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 2356 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_textAtOffset */

/* 2358 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2360 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2364 */	NdrFcShort( 0xd ),	/* 13 */
/* 2366 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 2368 */	NdrFcShort( 0xe ),	/* 14 */
/* 2370 */	NdrFcShort( 0x40 ),	/* 64 */
/* 2372 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x6,		/* 6 */
/* 2374 */	0x12,		/* 18 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2376 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2378 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2380 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2382 */	NdrFcShort( 0x6 ),	/* 6 */
/* 2384 */	0x6,		/* 6 */
			0x80,		/* 128 */
/* 2386 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2388 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 2390 */	0x85,		/* 133 */
			0x0,		/* 0 */

	/* Parameter offset */

/* 2392 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2394 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2396 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter boundaryType */

/* 2398 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2400 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2402 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 2404 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2406 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2408 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2410 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2412 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2414 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter text */

/* 2416 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2418 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2420 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 2422 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2424 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 2426 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure removeSelection */

/* 2428 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2430 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2434 */	NdrFcShort( 0xe ),	/* 14 */
/* 2436 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2438 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2440 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2442 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 2444 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2446 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2448 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2450 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2452 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2454 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2456 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter selectionIndex */

/* 2458 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2460 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2462 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2464 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2466 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2468 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure setCaretOffset */

/* 2470 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2472 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2476 */	NdrFcShort( 0xf ),	/* 15 */
/* 2478 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2480 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2482 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2484 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 2486 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2488 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2490 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2492 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2494 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2496 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2498 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter offset */

/* 2500 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2502 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2504 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2506 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2508 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2510 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure setSelection */

/* 2512 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2514 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2518 */	NdrFcShort( 0x10 ),	/* 16 */
/* 2520 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2522 */	NdrFcShort( 0x18 ),	/* 24 */
/* 2524 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2526 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 2528 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2530 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2532 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2534 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2536 */	NdrFcShort( 0x4 ),	/* 4 */
/* 2538 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 2540 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2542 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter selectionIndex */

/* 2544 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2546 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2548 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 2550 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2552 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2554 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2556 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2558 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2560 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2562 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2564 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2566 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nCharacters */

/* 2568 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2570 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2574 */	NdrFcShort( 0x11 ),	/* 17 */
/* 2576 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2578 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2580 */	NdrFcShort( 0x24 ),	/* 36 */
/* 2582 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 2584 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2586 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2588 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2590 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2592 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2594 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2596 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter nCharacters */

/* 2598 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2600 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2602 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2604 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2606 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2608 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure scrollSubstringTo */

/* 2610 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2612 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2616 */	NdrFcShort( 0x12 ),	/* 18 */
/* 2618 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2620 */	NdrFcShort( 0x16 ),	/* 22 */
/* 2622 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2624 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 2626 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2628 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2630 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2632 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2634 */	NdrFcShort( 0x4 ),	/* 4 */
/* 2636 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 2638 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2640 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter startIndex */

/* 2642 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2644 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2646 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endIndex */

/* 2648 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2650 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2652 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter scrollType */

/* 2654 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2656 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2658 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Return value */

/* 2660 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2662 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2664 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure scrollSubstringToPoint */

/* 2666 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2668 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2672 */	NdrFcShort( 0x13 ),	/* 19 */
/* 2674 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 2676 */	NdrFcShort( 0x26 ),	/* 38 */
/* 2678 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2680 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x6,		/* 6 */
/* 2682 */	0x12,		/* 18 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2684 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2686 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2688 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2690 */	NdrFcShort( 0x6 ),	/* 6 */
/* 2692 */	0x6,		/* 6 */
			0x80,		/* 128 */
/* 2694 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2696 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 2698 */	0x85,		/* 133 */
			0x0,		/* 0 */

	/* Parameter startIndex */

/* 2700 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2702 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2704 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endIndex */

/* 2706 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2708 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2710 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter coordinateType */

/* 2712 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2714 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2716 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter x */

/* 2718 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2720 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2722 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter y */

/* 2724 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2726 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2728 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2730 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2732 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 2734 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_newText */

/* 2736 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2738 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2742 */	NdrFcShort( 0x14 ),	/* 20 */
/* 2744 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2746 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2748 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2750 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 2752 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2754 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2756 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2758 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2760 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2762 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2764 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter newText */

/* 2766 */	NdrFcShort( 0x4113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=16 */
/* 2768 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2770 */	NdrFcShort( 0x52e ),	/* Type Offset=1326 */

	/* Return value */

/* 2772 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2774 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2776 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_oldText */

/* 2778 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2780 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2784 */	NdrFcShort( 0x15 ),	/* 21 */
/* 2786 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2788 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2790 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2792 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 2794 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 2796 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2798 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2800 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2802 */	NdrFcShort( 0x2 ),	/* 2 */
/* 2804 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 2806 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter oldText */

/* 2808 */	NdrFcShort( 0x4113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=16 */
/* 2810 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2812 */	NdrFcShort( 0x52e ),	/* Type Offset=1326 */

	/* Return value */

/* 2814 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2816 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2818 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_attributeRange */

/* 2820 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2822 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2826 */	NdrFcShort( 0x16 ),	/* 22 */
/* 2828 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 2830 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2832 */	NdrFcShort( 0x40 ),	/* 64 */
/* 2834 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x6,		/* 6 */
/* 2836 */	0x12,		/* 18 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 2838 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2840 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2842 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2844 */	NdrFcShort( 0x6 ),	/* 6 */
/* 2846 */	0x6,		/* 6 */
			0x80,		/* 128 */
/* 2848 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 2850 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 2852 */	0x85,		/* 133 */
			0x0,		/* 0 */

	/* Parameter offset */

/* 2854 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2856 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2858 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter filter */

/* 2860 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
/* 2862 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2864 */	NdrFcShort( 0x10e ),	/* Type Offset=270 */

	/* Parameter startOffset */

/* 2866 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2868 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2870 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2872 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 2874 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2876 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter attributeValues */

/* 2878 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 2880 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 2882 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 2884 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2886 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 2888 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure deleteText */

/* 2890 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2892 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2896 */	NdrFcShort( 0x4 ),	/* 4 */
/* 2898 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2900 */	NdrFcShort( 0x10 ),	/* 16 */
/* 2902 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2904 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 2906 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 2908 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2910 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2912 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2914 */	NdrFcShort( 0x3 ),	/* 3 */
/* 2916 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 2918 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter startOffset */

/* 2920 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2922 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2924 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 2926 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2928 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2930 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 2932 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2934 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2936 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure insertText */

/* 2938 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2940 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2944 */	NdrFcShort( 0x5 ),	/* 5 */
/* 2946 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2948 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2950 */	NdrFcShort( 0x8 ),	/* 8 */
/* 2952 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x3,		/* 3 */
/* 2954 */	0xe,		/* 14 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 2956 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2958 */	NdrFcShort( 0x1 ),	/* 1 */
/* 2960 */	NdrFcShort( 0x0 ),	/* 0 */
/* 2962 */	NdrFcShort( 0x3 ),	/* 3 */
/* 2964 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 2966 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter offset */

/* 2968 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 2970 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 2972 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter text */

/* 2974 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 2976 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 2978 */	NdrFcShort( 0x10e ),	/* Type Offset=270 */

	/* Return value */

/* 2980 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 2982 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 2984 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure cutText */

/* 2986 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 2988 */	NdrFcLong( 0x0 ),	/* 0 */
/* 2992 */	NdrFcShort( 0x6 ),	/* 6 */
/* 2994 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 2996 */	NdrFcShort( 0x10 ),	/* 16 */
/* 2998 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3000 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 3002 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3004 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3006 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3008 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3010 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3012 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3014 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter startOffset */

/* 3016 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3018 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3020 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 3022 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3024 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3026 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3028 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3030 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3032 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure pasteText */

/* 3034 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3036 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3040 */	NdrFcShort( 0x7 ),	/* 7 */
/* 3042 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3044 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3046 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3048 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3050 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3052 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3054 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3056 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3058 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3060 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3062 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter offset */

/* 3064 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3066 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3068 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3070 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3072 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3074 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure replaceText */

/* 3076 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3078 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3082 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3084 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 3086 */	NdrFcShort( 0x10 ),	/* 16 */
/* 3088 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3090 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x4,		/* 4 */
/* 3092 */	0x10,		/* 16 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 3094 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3096 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3098 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3100 */	NdrFcShort( 0x4 ),	/* 4 */
/* 3102 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 3104 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 3106 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 3108 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3110 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3112 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 3114 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3116 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3118 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter text */

/* 3120 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 3122 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3124 */	NdrFcShort( 0x10e ),	/* Type Offset=270 */

	/* Return value */

/* 3126 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3128 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3130 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure setAttributes */

/* 3132 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3134 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3138 */	NdrFcShort( 0x9 ),	/* 9 */
/* 3140 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 3142 */	NdrFcShort( 0x10 ),	/* 16 */
/* 3144 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3146 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x4,		/* 4 */
/* 3148 */	0x10,		/* 16 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 3150 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3152 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3154 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3156 */	NdrFcShort( 0x4 ),	/* 4 */
/* 3158 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 3160 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 3162 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter startOffset */

/* 3164 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3166 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3168 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter endOffset */

/* 3170 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3172 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3174 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter attributes */

/* 3176 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 3178 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3180 */	NdrFcShort( 0x10e ),	/* Type Offset=270 */

	/* Return value */

/* 3182 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3184 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3186 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_anchor */

/* 3188 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3190 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3194 */	NdrFcShort( 0x9 ),	/* 9 */
/* 3196 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3198 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3200 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3202 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 3204 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 3206 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3208 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3210 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3212 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3214 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3216 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter index */

/* 3218 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3220 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3222 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter anchor */

/* 3224 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 3226 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3228 */	NdrFcShort( 0x4bc ),	/* Type Offset=1212 */

	/* Return value */

/* 3230 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3232 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3234 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_anchorTarget */

/* 3236 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3238 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3242 */	NdrFcShort( 0xa ),	/* 10 */
/* 3244 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3246 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3248 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3250 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 3252 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 3254 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3256 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3258 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3260 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3262 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3264 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter index */

/* 3266 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3268 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3270 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter anchorTarget */

/* 3272 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 3274 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3276 */	NdrFcShort( 0x4bc ),	/* Type Offset=1212 */

	/* Return value */

/* 3278 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3280 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3282 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nRows */


	/* Procedure get_startIndex */

/* 3284 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3286 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3290 */	NdrFcShort( 0xb ),	/* 11 */
/* 3292 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3294 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3296 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3298 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3300 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3302 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3304 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3306 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3308 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3310 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3312 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter rowCount */


	/* Parameter index */

/* 3314 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3316 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3318 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 3320 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3322 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3324 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nSelectedChildren */


	/* Procedure get_endIndex */

/* 3326 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3328 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3332 */	NdrFcShort( 0xc ),	/* 12 */
/* 3334 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3336 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3338 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3340 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3342 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3344 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3346 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3348 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3350 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3352 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3354 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter cellCount */


	/* Parameter index */

/* 3356 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3358 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3360 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 3362 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3364 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3366 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_valid */

/* 3368 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3370 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3374 */	NdrFcShort( 0xd ),	/* 13 */
/* 3376 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3378 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3380 */	NdrFcShort( 0x21 ),	/* 33 */
/* 3382 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3384 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3386 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3388 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3390 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3392 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3394 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3396 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter valid */

/* 3398 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3400 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3402 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 3404 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3406 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3408 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nHyperlinks */

/* 3410 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3412 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3416 */	NdrFcShort( 0x16 ),	/* 22 */
/* 3418 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3420 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3422 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3424 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3426 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3428 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3430 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3432 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3434 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3436 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3438 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter hyperlinkCount */

/* 3440 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3442 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3444 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3446 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3448 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3450 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_hyperlink */

/* 3452 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3454 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3458 */	NdrFcShort( 0x17 ),	/* 23 */
/* 3460 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3462 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3464 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3466 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 3468 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3470 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3472 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3474 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3476 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3478 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3480 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter index */

/* 3482 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3484 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3486 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter hyperlink */

/* 3488 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3490 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3492 */	NdrFcShort( 0x546 ),	/* Type Offset=1350 */

	/* Return value */

/* 3494 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3496 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3498 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_hyperlinkIndex */

/* 3500 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3502 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3506 */	NdrFcShort( 0x18 ),	/* 24 */
/* 3508 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3510 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3512 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3514 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 3516 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3518 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3520 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3522 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3524 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3526 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3528 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter charIndex */

/* 3530 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3532 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3534 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter hyperlinkIndex */

/* 3536 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3538 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3540 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3542 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3544 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3546 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_hyperlinks */

/* 3548 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3550 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3554 */	NdrFcShort( 0x19 ),	/* 25 */
/* 3556 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3558 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3560 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3562 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 3564 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 3566 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3568 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3570 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3572 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3574 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3576 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter hyperlinks */

/* 3578 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 3580 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3582 */	NdrFcShort( 0x55c ),	/* Type Offset=1372 */

	/* Parameter nHyperlinks */

/* 3584 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3586 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3588 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3590 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3592 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3594 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_cellAt */


	/* Procedure get_accessibleAt */

/* 3596 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3598 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3602 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3604 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 3606 */	NdrFcShort( 0x10 ),	/* 16 */
/* 3608 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3610 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 3612 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3614 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3616 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3618 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3620 */	NdrFcShort( 0x4 ),	/* 4 */
/* 3622 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 3624 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 3626 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter row */


	/* Parameter row */

/* 3628 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3630 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3632 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter column */


	/* Parameter column */

/* 3634 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3636 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3638 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter cell */


	/* Parameter accessible */

/* 3640 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3642 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3644 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */


	/* Return value */

/* 3646 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3648 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3650 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_caption */


	/* Procedure get_caption */

/* 3652 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3654 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3658 */	NdrFcShort( 0x4 ),	/* 4 */
/* 3660 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3662 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3664 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3666 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 3668 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3670 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3672 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3674 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3676 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3678 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3680 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter accessible */


	/* Parameter accessible */

/* 3682 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3684 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3686 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */


	/* Return value */

/* 3688 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3690 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3692 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_childIndex */

/* 3694 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3696 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3700 */	NdrFcShort( 0x5 ),	/* 5 */
/* 3702 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 3704 */	NdrFcShort( 0x10 ),	/* 16 */
/* 3706 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3708 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 3710 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3712 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3714 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3716 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3718 */	NdrFcShort( 0x4 ),	/* 4 */
/* 3720 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 3722 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 3724 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter rowIndex */

/* 3726 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3728 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3730 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter columnIndex */

/* 3732 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3734 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3736 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter cellIndex */

/* 3738 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3740 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3742 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3744 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3746 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3748 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnDescription */

/* 3750 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3752 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3756 */	NdrFcShort( 0x6 ),	/* 6 */
/* 3758 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3760 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3762 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3764 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 3766 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 3768 */	NdrFcShort( 0x1 ),	/* 1 */
/* 3770 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3772 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3774 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3776 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3778 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter column */

/* 3780 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3782 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3784 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter description */

/* 3786 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 3788 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3790 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 3792 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3794 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3796 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnExtentAt */

/* 3798 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3800 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3804 */	NdrFcShort( 0x7 ),	/* 7 */
/* 3806 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 3808 */	NdrFcShort( 0x10 ),	/* 16 */
/* 3810 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3812 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 3814 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3816 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3818 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3820 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3822 */	NdrFcShort( 0x4 ),	/* 4 */
/* 3824 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 3826 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 3828 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter row */

/* 3830 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3832 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3834 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter column */

/* 3836 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3838 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3840 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter nColumnsSpanned */

/* 3842 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3844 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3846 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3848 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3850 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3852 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnHeader */

/* 3854 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3856 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3860 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3862 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3864 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3866 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3868 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 3870 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3872 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3874 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3876 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3878 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3880 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3882 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter accessibleTable */

/* 3884 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 3886 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3888 */	NdrFcShort( 0x57a ),	/* Type Offset=1402 */

	/* Parameter startingRowIndex */

/* 3890 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3892 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3894 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3896 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3898 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3900 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnIndex */

/* 3902 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3904 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3908 */	NdrFcShort( 0x9 ),	/* 9 */
/* 3910 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 3912 */	NdrFcShort( 0x8 ),	/* 8 */
/* 3914 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3916 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 3918 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3920 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3922 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3924 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3926 */	NdrFcShort( 0x3 ),	/* 3 */
/* 3928 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 3930 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter cellIndex */

/* 3932 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 3934 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3936 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter columnIndex */

/* 3938 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3940 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3942 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 3944 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3946 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3948 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nSelectedRows */


	/* Procedure get_nColumns */

/* 3950 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3952 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3956 */	NdrFcShort( 0xa ),	/* 10 */
/* 3958 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 3960 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3962 */	NdrFcShort( 0x24 ),	/* 36 */
/* 3964 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 3966 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 3968 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3970 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3972 */	NdrFcShort( 0x0 ),	/* 0 */
/* 3974 */	NdrFcShort( 0x2 ),	/* 2 */
/* 3976 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 3978 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter rowCount */


	/* Parameter columnCount */

/* 3980 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 3982 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 3984 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 3986 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 3988 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 3990 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nSelectedColumns */

/* 3992 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 3994 */	NdrFcLong( 0x0 ),	/* 0 */
/* 3998 */	NdrFcShort( 0xd ),	/* 13 */
/* 4000 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4002 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4004 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4006 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4008 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4010 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4012 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4014 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4016 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4018 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4020 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter columnCount */

/* 4022 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4024 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4026 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4028 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4030 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4032 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nSelectedRows */

/* 4034 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4036 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4040 */	NdrFcShort( 0xe ),	/* 14 */
/* 4042 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4044 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4046 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4048 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4050 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4052 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4054 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4056 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4058 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4060 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4062 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter rowCount */

/* 4064 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4066 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4068 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4070 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4072 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4074 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowDescription */

/* 4076 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4078 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4082 */	NdrFcShort( 0xf ),	/* 15 */
/* 4084 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4086 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4088 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4090 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 4092 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 4094 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4096 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4098 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4100 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4102 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4104 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter row */

/* 4106 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4108 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4110 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter description */

/* 4112 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 4114 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4116 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 4118 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4120 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4122 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowExtentAt */

/* 4124 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4126 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4130 */	NdrFcShort( 0x10 ),	/* 16 */
/* 4132 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 4134 */	NdrFcShort( 0x10 ),	/* 16 */
/* 4136 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4138 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 4140 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4142 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4144 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4146 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4148 */	NdrFcShort( 0x4 ),	/* 4 */
/* 4150 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 4152 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 4154 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter row */

/* 4156 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4158 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4160 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter column */

/* 4162 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4164 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4166 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter nRowsSpanned */

/* 4168 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4170 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4172 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4174 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4176 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4178 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowHeader */

/* 4180 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4182 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4186 */	NdrFcShort( 0x11 ),	/* 17 */
/* 4188 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4190 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4192 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4194 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 4196 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4198 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4200 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4202 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4204 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4206 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4208 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter accessibleTable */

/* 4210 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 4212 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4214 */	NdrFcShort( 0x57a ),	/* Type Offset=1402 */

	/* Parameter startingColumnIndex */

/* 4216 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4218 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4220 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4222 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4224 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4226 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowIndex */

/* 4228 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4230 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4234 */	NdrFcShort( 0x12 ),	/* 18 */
/* 4236 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4238 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4240 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4242 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 4244 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4246 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4248 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4250 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4252 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4254 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4256 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter cellIndex */

/* 4258 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4260 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4262 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter rowIndex */

/* 4264 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4266 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4268 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4270 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4272 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4274 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selectedChildren */

/* 4276 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4278 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4282 */	NdrFcShort( 0x13 ),	/* 19 */
/* 4284 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 4286 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4288 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4290 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 4292 */	0x10,		/* 16 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 4294 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4296 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4298 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4300 */	NdrFcShort( 0x4 ),	/* 4 */
/* 4302 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 4304 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 4306 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter maxChildren */

/* 4308 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4310 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4312 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter children */

/* 4314 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 4316 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4318 */	NdrFcShort( 0x590 ),	/* Type Offset=1424 */

	/* Parameter nChildren */

/* 4320 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4322 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4324 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4326 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4328 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4330 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selectedColumns */

/* 4332 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4334 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4338 */	NdrFcShort( 0x14 ),	/* 20 */
/* 4340 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 4342 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4344 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4346 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 4348 */	0x10,		/* 16 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 4350 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4352 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4354 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4356 */	NdrFcShort( 0x4 ),	/* 4 */
/* 4358 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 4360 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 4362 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter maxColumns */

/* 4364 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4366 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4368 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter columns */

/* 4370 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 4372 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4374 */	NdrFcShort( 0x590 ),	/* Type Offset=1424 */

	/* Parameter nColumns */

/* 4376 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4378 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4380 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4382 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4384 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4386 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selectedRows */

/* 4388 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4390 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4394 */	NdrFcShort( 0x15 ),	/* 21 */
/* 4396 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 4398 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4400 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4402 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x4,		/* 4 */
/* 4404 */	0x10,		/* 16 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 4406 */	NdrFcShort( 0x1 ),	/* 1 */
/* 4408 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4410 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4412 */	NdrFcShort( 0x4 ),	/* 4 */
/* 4414 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 4416 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 4418 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter maxRows */

/* 4420 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4422 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4424 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter rows */

/* 4426 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 4428 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4430 */	NdrFcShort( 0x590 ),	/* Type Offset=1424 */

	/* Parameter nRows */

/* 4432 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4434 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4436 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4438 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4440 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4442 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_summary */

/* 4444 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4446 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4450 */	NdrFcShort( 0x16 ),	/* 22 */
/* 4452 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4454 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4456 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4458 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 4460 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4462 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4464 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4466 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4468 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4470 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4472 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter accessible */

/* 4474 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 4476 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4478 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */

/* 4480 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4482 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4484 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isColumnSelected */

/* 4486 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4488 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4492 */	NdrFcShort( 0x17 ),	/* 23 */
/* 4494 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4496 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4498 */	NdrFcShort( 0x21 ),	/* 33 */
/* 4500 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 4502 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4504 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4506 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4508 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4510 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4512 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4514 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter column */

/* 4516 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4518 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4520 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 4522 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4524 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4526 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 4528 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4530 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4532 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isRowSelected */

/* 4534 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4536 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4540 */	NdrFcShort( 0x18 ),	/* 24 */
/* 4542 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4544 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4546 */	NdrFcShort( 0x21 ),	/* 33 */
/* 4548 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 4550 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4552 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4554 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4556 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4558 */	NdrFcShort( 0x3 ),	/* 3 */
/* 4560 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 4562 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter row */

/* 4564 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4566 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4568 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 4570 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4572 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4574 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 4576 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4578 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4580 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isSelected */

/* 4582 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4584 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4588 */	NdrFcShort( 0x19 ),	/* 25 */
/* 4590 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 4592 */	NdrFcShort( 0x10 ),	/* 16 */
/* 4594 */	NdrFcShort( 0x21 ),	/* 33 */
/* 4596 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 4598 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4600 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4602 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4604 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4606 */	NdrFcShort( 0x4 ),	/* 4 */
/* 4608 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 4610 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 4612 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter row */

/* 4614 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4616 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4618 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter column */

/* 4620 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4622 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4624 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 4626 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4628 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4630 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 4632 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4634 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4636 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure selectRow */

/* 4638 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4640 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4644 */	NdrFcShort( 0x1a ),	/* 26 */
/* 4646 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4648 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4650 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4652 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4654 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4656 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4658 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4660 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4662 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4664 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4666 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter row */

/* 4668 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4670 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4672 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4674 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4676 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4678 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure selectColumn */

/* 4680 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4682 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4686 */	NdrFcShort( 0x1b ),	/* 27 */
/* 4688 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4690 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4692 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4694 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4696 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4698 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4700 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4702 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4704 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4706 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4708 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter column */

/* 4710 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4712 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4714 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4716 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4718 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4720 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure unselectRow */

/* 4722 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4724 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4728 */	NdrFcShort( 0x1c ),	/* 28 */
/* 4730 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4732 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4734 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4736 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4738 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4740 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4742 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4744 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4746 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4748 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4750 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter row */

/* 4752 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4754 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4756 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4758 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4760 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4762 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure unselectColumn */

/* 4764 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4766 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4770 */	NdrFcShort( 0x1d ),	/* 29 */
/* 4772 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4774 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4776 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4778 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4780 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4782 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4784 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4786 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4788 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4790 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4792 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter column */

/* 4794 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4796 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4798 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 4800 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4802 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4804 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowColumnExtentsAtIndex */

/* 4806 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4808 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4812 */	NdrFcShort( 0x1e ),	/* 30 */
/* 4814 */	NdrFcShort( 0x40 ),	/* ARM64 Stack size/offset = 64 */
/* 4816 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4818 */	NdrFcShort( 0x91 ),	/* 145 */
/* 4820 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x7,		/* 7 */
/* 4822 */	0x12,		/* 18 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4824 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4826 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4828 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4830 */	NdrFcShort( 0x7 ),	/* 7 */
/* 4832 */	0x7,		/* 7 */
			0x80,		/* 128 */
/* 4834 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 4836 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 4838 */	0x85,		/* 133 */
			0x86,		/* 134 */

	/* Parameter index */

/* 4840 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 4842 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4844 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter row */

/* 4846 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4848 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4850 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter column */

/* 4852 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4854 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4856 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter rowExtents */

/* 4858 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4860 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 4862 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter columnExtents */

/* 4864 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4866 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 4868 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 4870 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4872 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 4874 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 4876 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4878 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 4880 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_modelChange */

/* 4882 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4884 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4888 */	NdrFcShort( 0x1f ),	/* 31 */
/* 4890 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4892 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4894 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4896 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 4898 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4900 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4902 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4904 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4906 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4908 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4910 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter modelChange */

/* 4912 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 4914 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4916 */	NdrFcShort( 0x5ae ),	/* Type Offset=1454 */

	/* Return value */

/* 4918 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4920 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4922 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowExtent */


	/* Procedure get_nColumns */

/* 4924 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4926 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4930 */	NdrFcShort( 0x6 ),	/* 6 */
/* 4932 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4934 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4936 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4938 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4940 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4942 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4944 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4946 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4948 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4950 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4952 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter nRowsSpanned */


	/* Parameter columnCount */

/* 4954 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4956 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 4958 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 4960 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 4962 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 4964 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowIndex */


	/* Procedure get_nSelectedCells */

/* 4966 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 4968 */	NdrFcLong( 0x0 ),	/* 0 */
/* 4972 */	NdrFcShort( 0x8 ),	/* 8 */
/* 4974 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 4976 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4978 */	NdrFcShort( 0x24 ),	/* 36 */
/* 4980 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 4982 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 4984 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4986 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4988 */	NdrFcShort( 0x0 ),	/* 0 */
/* 4990 */	NdrFcShort( 0x2 ),	/* 2 */
/* 4992 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 4994 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter rowIndex */


	/* Parameter cellCount */

/* 4996 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 4998 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5000 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */


	/* Return value */

/* 5002 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5004 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5006 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_nSelectedColumns */

/* 5008 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5010 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5014 */	NdrFcShort( 0x9 ),	/* 9 */
/* 5016 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5018 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5020 */	NdrFcShort( 0x24 ),	/* 36 */
/* 5022 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 5024 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5026 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5028 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5030 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5032 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5034 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5036 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter columnCount */

/* 5038 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5040 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5042 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5044 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5046 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5048 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowDescription */

/* 5050 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5052 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5056 */	NdrFcShort( 0xb ),	/* 11 */
/* 5058 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5060 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5062 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5064 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 5066 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 5068 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5070 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5072 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5074 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5076 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5078 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter row */

/* 5080 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5082 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5084 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter description */

/* 5086 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 5088 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5090 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 5092 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5094 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5096 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selectedCells */

/* 5098 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5100 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5104 */	NdrFcShort( 0xc ),	/* 12 */
/* 5106 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5108 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5110 */	NdrFcShort( 0x24 ),	/* 36 */
/* 5112 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 5114 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 5116 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5118 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5120 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5122 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5124 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5126 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter cells */

/* 5128 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 5130 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5132 */	NdrFcShort( 0x5bc ),	/* Type Offset=1468 */

	/* Parameter nSelectedCells */

/* 5134 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5136 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5138 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5140 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5142 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5144 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selectedColumns */

/* 5146 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5148 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5152 */	NdrFcShort( 0xd ),	/* 13 */
/* 5154 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5156 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5158 */	NdrFcShort( 0x24 ),	/* 36 */
/* 5160 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 5162 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 5164 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5166 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5168 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5170 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5172 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5174 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter selectedColumns */

/* 5176 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 5178 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5180 */	NdrFcShort( 0x5da ),	/* Type Offset=1498 */

	/* Parameter nColumns */

/* 5182 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5184 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5186 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5188 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5190 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5192 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_selectedRows */

/* 5194 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5196 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5200 */	NdrFcShort( 0xe ),	/* 14 */
/* 5202 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5204 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5206 */	NdrFcShort( 0x24 ),	/* 36 */
/* 5208 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 5210 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 5212 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5214 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5216 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5218 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5220 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5222 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter selectedRows */

/* 5224 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 5226 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5228 */	NdrFcShort( 0x5da ),	/* Type Offset=1498 */

	/* Parameter nRows */

/* 5230 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5232 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5234 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5236 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5238 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5240 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_summary */

/* 5242 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5244 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5248 */	NdrFcShort( 0xf ),	/* 15 */
/* 5250 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5252 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5254 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5256 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 5258 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5260 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5262 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5264 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5266 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5268 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5270 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter accessible */

/* 5272 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 5274 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5276 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */

/* 5278 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5280 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5282 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isColumnSelected */

/* 5284 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5286 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5290 */	NdrFcShort( 0x10 ),	/* 16 */
/* 5292 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5294 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5296 */	NdrFcShort( 0x21 ),	/* 33 */
/* 5298 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 5300 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5302 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5304 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5306 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5308 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5310 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5312 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter column */

/* 5314 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5316 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5318 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 5320 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5322 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5324 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 5326 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5328 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5330 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isRowSelected */

/* 5332 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5334 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5338 */	NdrFcShort( 0x11 ),	/* 17 */
/* 5340 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5342 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5344 */	NdrFcShort( 0x21 ),	/* 33 */
/* 5346 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 5348 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5350 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5352 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5354 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5356 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5358 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5360 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter row */

/* 5362 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5364 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5366 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 5368 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5370 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5372 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 5374 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5376 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5378 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure selectRow */

/* 5380 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5382 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5386 */	NdrFcShort( 0x12 ),	/* 18 */
/* 5388 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5390 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5392 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5394 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 5396 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5398 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5400 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5402 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5404 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5406 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5408 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter row */

/* 5410 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5412 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5414 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5416 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5418 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5420 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure selectColumn */

/* 5422 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5424 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5428 */	NdrFcShort( 0x13 ),	/* 19 */
/* 5430 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5432 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5434 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5436 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 5438 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5440 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5442 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5444 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5446 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5448 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5450 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter column */

/* 5452 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5454 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5456 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5458 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5460 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5462 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure unselectRow */

/* 5464 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5466 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5470 */	NdrFcShort( 0x14 ),	/* 20 */
/* 5472 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5474 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5476 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5478 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 5480 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5482 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5484 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5486 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5488 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5490 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5492 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter row */

/* 5494 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5496 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5498 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5500 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5502 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5504 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure unselectColumn */

/* 5506 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5508 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5512 */	NdrFcShort( 0x15 ),	/* 21 */
/* 5514 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5516 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5518 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5520 */	0x44,		/* Oi2 Flags:  has return, has ext, */
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

	/* Parameter column */

/* 5536 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5538 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5540 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5542 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5544 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5546 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_modelChange */

/* 5548 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5550 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5554 */	NdrFcShort( 0x16 ),	/* 22 */
/* 5556 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5558 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5560 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5562 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
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

	/* Parameter modelChange */

/* 5578 */	NdrFcShort( 0x6113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=24 */
/* 5580 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5582 */	NdrFcShort( 0x5ae ),	/* Type Offset=1454 */

	/* Return value */

/* 5584 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5586 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5588 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_columnHeaderCells */

/* 5590 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5592 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5596 */	NdrFcShort( 0x4 ),	/* 4 */
/* 5598 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5600 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5602 */	NdrFcShort( 0x24 ),	/* 36 */
/* 5604 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 5606 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 5608 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5610 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5612 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5614 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5616 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5618 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter cellAccessibles */

/* 5620 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 5622 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5624 */	NdrFcShort( 0x5bc ),	/* Type Offset=1468 */

	/* Parameter nColumnHeaderCells */

/* 5626 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5628 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5630 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5632 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5634 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5636 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowHeaderCells */

/* 5638 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5640 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5644 */	NdrFcShort( 0x7 ),	/* 7 */
/* 5646 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5648 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5650 */	NdrFcShort( 0x24 ),	/* 36 */
/* 5652 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x3,		/* 3 */
/* 5654 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 5656 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5658 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5660 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5662 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5664 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5666 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter cellAccessibles */

/* 5668 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 5670 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5672 */	NdrFcShort( 0x5bc ),	/* Type Offset=1468 */

	/* Parameter nRowHeaderCells */

/* 5674 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5676 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5678 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5680 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5682 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5684 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_isSelected */

/* 5686 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5688 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5692 */	NdrFcShort( 0x9 ),	/* 9 */
/* 5694 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5696 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5698 */	NdrFcShort( 0x21 ),	/* 33 */
/* 5700 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 5702 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5704 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5706 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5708 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5710 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5712 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5714 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 5716 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5718 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5720 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 5722 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5724 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5726 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_rowColumnExtents */

/* 5728 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5730 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5734 */	NdrFcShort( 0xa ),	/* 10 */
/* 5736 */	NdrFcShort( 0x38 ),	/* ARM64 Stack size/offset = 56 */
/* 5738 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5740 */	NdrFcShort( 0x91 ),	/* 145 */
/* 5742 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x6,		/* 6 */
/* 5744 */	0x12,		/* 18 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5746 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5748 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5750 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5752 */	NdrFcShort( 0x6 ),	/* 6 */
/* 5754 */	0x6,		/* 6 */
			0x80,		/* 128 */
/* 5756 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 5758 */	0x83,		/* 131 */
			0x84,		/* 132 */
/* 5760 */	0x85,		/* 133 */
			0x0,		/* 0 */

	/* Parameter row */

/* 5762 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5764 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5766 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter column */

/* 5768 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5770 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5772 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter rowExtents */

/* 5774 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5776 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5778 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter columnExtents */

/* 5780 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5782 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5784 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter isSelected */

/* 5786 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5788 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 5790 */	0x3,		/* FC_SMALL */
			0x0,		/* 0 */

	/* Return value */

/* 5792 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5794 */	NdrFcShort( 0x30 ),	/* ARM64 Stack size/offset = 48 */
/* 5796 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_table */

/* 5798 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5800 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5804 */	NdrFcShort( 0xb ),	/* 11 */
/* 5806 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5808 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5810 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5812 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 5814 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5816 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5818 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5820 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5822 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5824 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5826 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter table */

/* 5828 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 5830 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5832 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */

/* 5834 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5836 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5838 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_imagePosition */

/* 5840 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5842 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5846 */	NdrFcShort( 0x4 ),	/* 4 */
/* 5848 */	NdrFcShort( 0x28 ),	/* ARM64 Stack size/offset = 40 */
/* 5850 */	NdrFcShort( 0x6 ),	/* 6 */
/* 5852 */	NdrFcShort( 0x40 ),	/* 64 */
/* 5854 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x4,		/* 4 */
/* 5856 */	0x10,		/* 16 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5858 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5860 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5862 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5864 */	NdrFcShort( 0x4 ),	/* 4 */
/* 5866 */	0x4,		/* 4 */
			0x80,		/* 128 */
/* 5868 */	0x81,		/* 129 */
			0x82,		/* 130 */
/* 5870 */	0x83,		/* 131 */
			0x0,		/* 0 */

	/* Parameter coordinateType */

/* 5872 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 5874 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5876 */	0xd,		/* FC_ENUM16 */
			0x0,		/* 0 */

	/* Parameter x */

/* 5878 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5880 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5882 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter y */

/* 5884 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5886 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5888 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5890 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5892 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5894 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_imageSize */

/* 5896 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5898 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5902 */	NdrFcShort( 0x5 ),	/* 5 */
/* 5904 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 5906 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5908 */	NdrFcShort( 0x40 ),	/* 64 */
/* 5910 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x3,		/* 3 */
/* 5912 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 5914 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5916 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5918 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5920 */	NdrFcShort( 0x3 ),	/* 3 */
/* 5922 */	0x3,		/* 3 */
			0x80,		/* 128 */
/* 5924 */	0x81,		/* 129 */
			0x82,		/* 130 */

	/* Parameter height */

/* 5926 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5928 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5930 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter width */

/* 5932 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 5934 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5936 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 5938 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5940 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5942 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_toolkitName */

/* 5944 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5946 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5950 */	NdrFcShort( 0x5 ),	/* 5 */
/* 5952 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5954 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5956 */	NdrFcShort( 0x8 ),	/* 8 */
/* 5958 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 5960 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 5962 */	NdrFcShort( 0x1 ),	/* 1 */
/* 5964 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5966 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5968 */	NdrFcShort( 0x2 ),	/* 2 */
/* 5970 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 5972 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter name */

/* 5974 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 5976 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 5978 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 5980 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 5982 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 5984 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_toolkitVersion */

/* 5986 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 5988 */	NdrFcLong( 0x0 ),	/* 0 */
/* 5992 */	NdrFcShort( 0x6 ),	/* 6 */
/* 5994 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 5996 */	NdrFcShort( 0x0 ),	/* 0 */
/* 5998 */	NdrFcShort( 0x8 ),	/* 8 */
/* 6000 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 6002 */	0xe,		/* 14 */
			0x3,		/* Ext Flags:  new corr desc, clt corr check, */
/* 6004 */	NdrFcShort( 0x1 ),	/* 1 */
/* 6006 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6008 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6010 */	NdrFcShort( 0x2 ),	/* 2 */
/* 6012 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 6014 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter version */

/* 6016 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
/* 6018 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 6020 */	NdrFcShort( 0x20 ),	/* Type Offset=32 */

	/* Return value */

/* 6022 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 6024 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 6026 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure get_anchorTarget */

/* 6028 */	0x33,		/* FC_AUTO_HANDLE */
			0x6c,		/* Old Flags:  object, Oi2 */
/* 6030 */	NdrFcLong( 0x0 ),	/* 0 */
/* 6034 */	NdrFcShort( 0x3 ),	/* 3 */
/* 6036 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 6038 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6040 */	NdrFcShort( 0x8 ),	/* 8 */
/* 6042 */	0x45,		/* Oi2 Flags:  srv must size, has return, has ext, */
			0x2,		/* 2 */
/* 6044 */	0xe,		/* 14 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 6046 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6048 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6050 */	NdrFcShort( 0x0 ),	/* 0 */
/* 6052 */	NdrFcShort( 0x2 ),	/* 2 */
/* 6054 */	0x2,		/* 2 */
			0x80,		/* 128 */
/* 6056 */	0x81,		/* 129 */
			0x0,		/* 0 */

	/* Parameter accessible */

/* 6058 */	NdrFcShort( 0x13 ),	/* Flags:  must size, must free, out, */
/* 6060 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 6062 */	NdrFcShort( 0x2e ),	/* Type Offset=46 */

	/* Return value */

/* 6064 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 6066 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 6068 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

			0x0
        }
    };

static const ia2_api_all_MIDL_TYPE_FORMAT_STRING ia2_api_all__MIDL_TypeFormatString =
    {
        0,
        {
			NdrFcShort( 0x0 ),	/* 0 */
/*  2 */	
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/*  4 */	NdrFcShort( 0x1c ),	/* Offset= 28 (32) */
/*  6 */	
			0x13, 0x0,	/* FC_OP */
/*  8 */	NdrFcShort( 0xe ),	/* Offset= 14 (22) */
/* 10 */	
			0x1b,		/* FC_CARRAY */
			0x1,		/* 1 */
/* 12 */	NdrFcShort( 0x2 ),	/* 2 */
/* 14 */	0x9,		/* Corr desc: FC_ULONG */
			0x0,		/*  */
/* 16 */	NdrFcShort( 0xfffc ),	/* -4 */
/* 18 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 20 */	0x6,		/* FC_SHORT */
			0x5b,		/* FC_END */
/* 22 */	
			0x17,		/* FC_CSTRUCT */
			0x3,		/* 3 */
/* 24 */	NdrFcShort( 0x8 ),	/* 8 */
/* 26 */	NdrFcShort( 0xfff0 ),	/* Offset= -16 (10) */
/* 28 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 30 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 32 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 34 */	NdrFcShort( 0x0 ),	/* 0 */
/* 36 */	NdrFcShort( 0x8 ),	/* 8 */
/* 38 */	NdrFcShort( 0x0 ),	/* 0 */
/* 40 */	NdrFcShort( 0xffde ),	/* Offset= -34 (6) */
/* 42 */	
			0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
/* 44 */	0x8,		/* FC_LONG */
			0x5c,		/* FC_PAD */
/* 46 */	
			0x11, 0x10,	/* FC_RP [pointer_deref] */
/* 48 */	NdrFcShort( 0x2 ),	/* Offset= 2 (50) */
/* 50 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 52 */	NdrFcLong( 0x0 ),	/* 0 */
/* 56 */	NdrFcShort( 0x0 ),	/* 0 */
/* 58 */	NdrFcShort( 0x0 ),	/* 0 */
/* 60 */	0xc0,		/* 192 */
			0x0,		/* 0 */
/* 62 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 64 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 66 */	0x0,		/* 0 */
			0x46,		/* 70 */
/* 68 */	
			0x11, 0x0,	/* FC_RP */
/* 70 */	NdrFcShort( 0x2 ),	/* Offset= 2 (72) */
/* 72 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 74 */	NdrFcShort( 0x0 ),	/* 0 */
/* 76 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x0,		/*  */
/* 78 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 80 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 82 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 84 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 86 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 88 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 90 */	NdrFcShort( 0xffd8 ),	/* Offset= -40 (50) */
/* 92 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 94 */	
			0x11, 0x14,	/* FC_RP [alloced_on_stack] [pointer_deref] */
/* 96 */	NdrFcShort( 0x2 ),	/* Offset= 2 (98) */
/* 98 */	
			0x13, 0x0,	/* FC_OP */
/* 100 */	NdrFcShort( 0x2 ),	/* Offset= 2 (102) */
/* 102 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 104 */	NdrFcShort( 0x0 ),	/* 0 */
/* 106 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x0,		/*  */
/* 108 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 110 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 112 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 114 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 116 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 118 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 120 */	NdrFcShort( 0xffa8 ),	/* Offset= -88 (32) */
/* 122 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 124 */	
			0x11, 0x10,	/* FC_RP [pointer_deref] */
/* 126 */	NdrFcShort( 0x2 ),	/* Offset= 2 (128) */
/* 128 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 130 */	NdrFcLong( 0x7cdf86ee ),	/* 2095023854 */
/* 134 */	NdrFcShort( 0xc3da ),	/* -15398 */
/* 136 */	NdrFcShort( 0x496a ),	/* 18794 */
/* 138 */	0xbd,		/* 189 */
			0xa4,		/* 164 */
/* 140 */	0x28,		/* 40 */
			0x1b,		/* 27 */
/* 142 */	0x33,		/* 51 */
			0x6e,		/* 110 */
/* 144 */	0x1f,		/* 31 */
			0xdc,		/* 220 */
/* 146 */	
			0x11, 0x0,	/* FC_RP */
/* 148 */	NdrFcShort( 0x2 ),	/* Offset= 2 (150) */
/* 150 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 152 */	NdrFcShort( 0x0 ),	/* 0 */
/* 154 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x0,		/*  */
/* 156 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 158 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 160 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 162 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 164 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 166 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 168 */	NdrFcShort( 0xffd8 ),	/* Offset= -40 (128) */
/* 170 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 172 */	
			0x11, 0x14,	/* FC_RP [alloced_on_stack] [pointer_deref] */
/* 174 */	NdrFcShort( 0x2 ),	/* Offset= 2 (176) */
/* 176 */	
			0x13, 0x0,	/* FC_OP */
/* 178 */	NdrFcShort( 0x2 ),	/* Offset= 2 (180) */
/* 180 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 182 */	NdrFcShort( 0x0 ),	/* 0 */
/* 184 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x0,		/*  */
/* 186 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 188 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 190 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 192 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 194 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 196 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 198 */	NdrFcShort( 0xff5a ),	/* Offset= -166 (32) */
/* 200 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 202 */	
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/* 204 */	NdrFcShort( 0x1a ),	/* Offset= 26 (230) */
/* 206 */	
			0x13, 0x0,	/* FC_OP */
/* 208 */	NdrFcShort( 0x2 ),	/* Offset= 2 (210) */
/* 210 */	
			0x2a,		/* FC_ENCAPSULATED_UNION */
			0x48,		/* 72 */
/* 212 */	NdrFcShort( 0x4 ),	/* 4 */
/* 214 */	NdrFcShort( 0x2 ),	/* 2 */
/* 216 */	NdrFcLong( 0x48746457 ),	/* 1215587415 */
/* 220 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 222 */	NdrFcLong( 0x52746457 ),	/* 1383359575 */
/* 226 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 228 */	NdrFcShort( 0xffff ),	/* Offset= -1 (227) */
/* 230 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 232 */	NdrFcShort( 0x1 ),	/* 1 */
/* 234 */	NdrFcShort( 0x8 ),	/* 8 */
/* 236 */	NdrFcShort( 0x0 ),	/* 0 */
/* 238 */	NdrFcShort( 0xffe0 ),	/* Offset= -32 (206) */
/* 240 */	
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/* 242 */	NdrFcShort( 0x2 ),	/* Offset= 2 (244) */
/* 244 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 246 */	NdrFcShort( 0x18 ),	/* 24 */
/* 248 */	NdrFcShort( 0x0 ),	/* 0 */
/* 250 */	NdrFcShort( 0x0 ),	/* Offset= 0 (250) */
/* 252 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 254 */	NdrFcShort( 0xff22 ),	/* Offset= -222 (32) */
/* 256 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 258 */	NdrFcShort( 0xff1e ),	/* Offset= -226 (32) */
/* 260 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 262 */	NdrFcShort( 0xff1a ),	/* Offset= -230 (32) */
/* 264 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 266 */	
			0x12, 0x0,	/* FC_UP */
/* 268 */	NdrFcShort( 0xff0a ),	/* Offset= -246 (22) */
/* 270 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 272 */	NdrFcShort( 0x0 ),	/* 0 */
/* 274 */	NdrFcShort( 0x8 ),	/* 8 */
/* 276 */	NdrFcShort( 0x0 ),	/* 0 */
/* 278 */	NdrFcShort( 0xfff4 ),	/* Offset= -12 (266) */
/* 280 */	
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/* 282 */	NdrFcShort( 0x3a2 ),	/* Offset= 930 (1212) */
/* 284 */	
			0x13, 0x0,	/* FC_OP */
/* 286 */	NdrFcShort( 0x38a ),	/* Offset= 906 (1192) */
/* 288 */	
			0x2b,		/* FC_NON_ENCAPSULATED_UNION */
			0x9,		/* FC_ULONG */
/* 290 */	0x7,		/* Corr desc: FC_USHORT */
			0x0,		/*  */
/* 292 */	NdrFcShort( 0xfff8 ),	/* -8 */
/* 294 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 296 */	NdrFcShort( 0x2 ),	/* Offset= 2 (298) */
/* 298 */	NdrFcShort( 0x10 ),	/* 16 */
/* 300 */	NdrFcShort( 0x2f ),	/* 47 */
/* 302 */	NdrFcLong( 0x14 ),	/* 20 */
/* 306 */	NdrFcShort( 0x800b ),	/* Simple arm type: FC_HYPER */
/* 308 */	NdrFcLong( 0x3 ),	/* 3 */
/* 312 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 314 */	NdrFcLong( 0x11 ),	/* 17 */
/* 318 */	NdrFcShort( 0x8001 ),	/* Simple arm type: FC_BYTE */
/* 320 */	NdrFcLong( 0x2 ),	/* 2 */
/* 324 */	NdrFcShort( 0x8006 ),	/* Simple arm type: FC_SHORT */
/* 326 */	NdrFcLong( 0x4 ),	/* 4 */
/* 330 */	NdrFcShort( 0x800a ),	/* Simple arm type: FC_FLOAT */
/* 332 */	NdrFcLong( 0x5 ),	/* 5 */
/* 336 */	NdrFcShort( 0x800c ),	/* Simple arm type: FC_DOUBLE */
/* 338 */	NdrFcLong( 0xb ),	/* 11 */
/* 342 */	NdrFcShort( 0x8006 ),	/* Simple arm type: FC_SHORT */
/* 344 */	NdrFcLong( 0xa ),	/* 10 */
/* 348 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 350 */	NdrFcLong( 0x6 ),	/* 6 */
/* 354 */	NdrFcShort( 0xe8 ),	/* Offset= 232 (586) */
/* 356 */	NdrFcLong( 0x7 ),	/* 7 */
/* 360 */	NdrFcShort( 0x800c ),	/* Simple arm type: FC_DOUBLE */
/* 362 */	NdrFcLong( 0x8 ),	/* 8 */
/* 366 */	NdrFcShort( 0xfe98 ),	/* Offset= -360 (6) */
/* 368 */	NdrFcLong( 0xd ),	/* 13 */
/* 372 */	NdrFcShort( 0xfebe ),	/* Offset= -322 (50) */
/* 374 */	NdrFcLong( 0x9 ),	/* 9 */
/* 378 */	NdrFcShort( 0xd6 ),	/* Offset= 214 (592) */
/* 380 */	NdrFcLong( 0x2000 ),	/* 8192 */
/* 384 */	NdrFcShort( 0xe2 ),	/* Offset= 226 (610) */
/* 386 */	NdrFcLong( 0x24 ),	/* 36 */
/* 390 */	NdrFcShort( 0x2d8 ),	/* Offset= 728 (1118) */
/* 392 */	NdrFcLong( 0x4024 ),	/* 16420 */
/* 396 */	NdrFcShort( 0x2d2 ),	/* Offset= 722 (1118) */
/* 398 */	NdrFcLong( 0x4011 ),	/* 16401 */
/* 402 */	NdrFcShort( 0x2d0 ),	/* Offset= 720 (1122) */
/* 404 */	NdrFcLong( 0x4002 ),	/* 16386 */
/* 408 */	NdrFcShort( 0x2ce ),	/* Offset= 718 (1126) */
/* 410 */	NdrFcLong( 0x4003 ),	/* 16387 */
/* 414 */	NdrFcShort( 0x2cc ),	/* Offset= 716 (1130) */
/* 416 */	NdrFcLong( 0x4014 ),	/* 16404 */
/* 420 */	NdrFcShort( 0x2ca ),	/* Offset= 714 (1134) */
/* 422 */	NdrFcLong( 0x4004 ),	/* 16388 */
/* 426 */	NdrFcShort( 0x2c8 ),	/* Offset= 712 (1138) */
/* 428 */	NdrFcLong( 0x4005 ),	/* 16389 */
/* 432 */	NdrFcShort( 0x2c6 ),	/* Offset= 710 (1142) */
/* 434 */	NdrFcLong( 0x400b ),	/* 16395 */
/* 438 */	NdrFcShort( 0x2b0 ),	/* Offset= 688 (1126) */
/* 440 */	NdrFcLong( 0x400a ),	/* 16394 */
/* 444 */	NdrFcShort( 0x2ae ),	/* Offset= 686 (1130) */
/* 446 */	NdrFcLong( 0x4006 ),	/* 16390 */
/* 450 */	NdrFcShort( 0x2b8 ),	/* Offset= 696 (1146) */
/* 452 */	NdrFcLong( 0x4007 ),	/* 16391 */
/* 456 */	NdrFcShort( 0x2ae ),	/* Offset= 686 (1142) */
/* 458 */	NdrFcLong( 0x4008 ),	/* 16392 */
/* 462 */	NdrFcShort( 0x2b0 ),	/* Offset= 688 (1150) */
/* 464 */	NdrFcLong( 0x400d ),	/* 16397 */
/* 468 */	NdrFcShort( 0x2ae ),	/* Offset= 686 (1154) */
/* 470 */	NdrFcLong( 0x4009 ),	/* 16393 */
/* 474 */	NdrFcShort( 0x2ac ),	/* Offset= 684 (1158) */
/* 476 */	NdrFcLong( 0x6000 ),	/* 24576 */
/* 480 */	NdrFcShort( 0x2aa ),	/* Offset= 682 (1162) */
/* 482 */	NdrFcLong( 0x400c ),	/* 16396 */
/* 486 */	NdrFcShort( 0x2a8 ),	/* Offset= 680 (1166) */
/* 488 */	NdrFcLong( 0x10 ),	/* 16 */
/* 492 */	NdrFcShort( 0x8002 ),	/* Simple arm type: FC_CHAR */
/* 494 */	NdrFcLong( 0x12 ),	/* 18 */
/* 498 */	NdrFcShort( 0x8006 ),	/* Simple arm type: FC_SHORT */
/* 500 */	NdrFcLong( 0x13 ),	/* 19 */
/* 504 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 506 */	NdrFcLong( 0x15 ),	/* 21 */
/* 510 */	NdrFcShort( 0x800b ),	/* Simple arm type: FC_HYPER */
/* 512 */	NdrFcLong( 0x16 ),	/* 22 */
/* 516 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 518 */	NdrFcLong( 0x17 ),	/* 23 */
/* 522 */	NdrFcShort( 0x8008 ),	/* Simple arm type: FC_LONG */
/* 524 */	NdrFcLong( 0xe ),	/* 14 */
/* 528 */	NdrFcShort( 0x286 ),	/* Offset= 646 (1174) */
/* 530 */	NdrFcLong( 0x400e ),	/* 16398 */
/* 534 */	NdrFcShort( 0x28a ),	/* Offset= 650 (1184) */
/* 536 */	NdrFcLong( 0x4010 ),	/* 16400 */
/* 540 */	NdrFcShort( 0x288 ),	/* Offset= 648 (1188) */
/* 542 */	NdrFcLong( 0x4012 ),	/* 16402 */
/* 546 */	NdrFcShort( 0x244 ),	/* Offset= 580 (1126) */
/* 548 */	NdrFcLong( 0x4013 ),	/* 16403 */
/* 552 */	NdrFcShort( 0x242 ),	/* Offset= 578 (1130) */
/* 554 */	NdrFcLong( 0x4015 ),	/* 16405 */
/* 558 */	NdrFcShort( 0x240 ),	/* Offset= 576 (1134) */
/* 560 */	NdrFcLong( 0x4016 ),	/* 16406 */
/* 564 */	NdrFcShort( 0x236 ),	/* Offset= 566 (1130) */
/* 566 */	NdrFcLong( 0x4017 ),	/* 16407 */
/* 570 */	NdrFcShort( 0x230 ),	/* Offset= 560 (1130) */
/* 572 */	NdrFcLong( 0x0 ),	/* 0 */
/* 576 */	NdrFcShort( 0x0 ),	/* Offset= 0 (576) */
/* 578 */	NdrFcLong( 0x1 ),	/* 1 */
/* 582 */	NdrFcShort( 0x0 ),	/* Offset= 0 (582) */
/* 584 */	NdrFcShort( 0xffff ),	/* Offset= -1 (583) */
/* 586 */	
			0x15,		/* FC_STRUCT */
			0x7,		/* 7 */
/* 588 */	NdrFcShort( 0x8 ),	/* 8 */
/* 590 */	0xb,		/* FC_HYPER */
			0x5b,		/* FC_END */
/* 592 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 594 */	NdrFcLong( 0x20400 ),	/* 132096 */
/* 598 */	NdrFcShort( 0x0 ),	/* 0 */
/* 600 */	NdrFcShort( 0x0 ),	/* 0 */
/* 602 */	0xc0,		/* 192 */
			0x0,		/* 0 */
/* 604 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 606 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 608 */	0x0,		/* 0 */
			0x46,		/* 70 */
/* 610 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 612 */	NdrFcShort( 0x2 ),	/* Offset= 2 (614) */
/* 614 */	
			0x13, 0x0,	/* FC_OP */
/* 616 */	NdrFcShort( 0x1e4 ),	/* Offset= 484 (1100) */
/* 618 */	
			0x2a,		/* FC_ENCAPSULATED_UNION */
			0x89,		/* 137 */
/* 620 */	NdrFcShort( 0x20 ),	/* 32 */
/* 622 */	NdrFcShort( 0xa ),	/* 10 */
/* 624 */	NdrFcLong( 0x8 ),	/* 8 */
/* 628 */	NdrFcShort( 0x50 ),	/* Offset= 80 (708) */
/* 630 */	NdrFcLong( 0xd ),	/* 13 */
/* 634 */	NdrFcShort( 0x70 ),	/* Offset= 112 (746) */
/* 636 */	NdrFcLong( 0x9 ),	/* 9 */
/* 640 */	NdrFcShort( 0x90 ),	/* Offset= 144 (784) */
/* 642 */	NdrFcLong( 0xc ),	/* 12 */
/* 646 */	NdrFcShort( 0xb0 ),	/* Offset= 176 (822) */
/* 648 */	NdrFcLong( 0x24 ),	/* 36 */
/* 652 */	NdrFcShort( 0x102 ),	/* Offset= 258 (910) */
/* 654 */	NdrFcLong( 0x800d ),	/* 32781 */
/* 658 */	NdrFcShort( 0x11e ),	/* Offset= 286 (944) */
/* 660 */	NdrFcLong( 0x10 ),	/* 16 */
/* 664 */	NdrFcShort( 0x138 ),	/* Offset= 312 (976) */
/* 666 */	NdrFcLong( 0x2 ),	/* 2 */
/* 670 */	NdrFcShort( 0x14e ),	/* Offset= 334 (1004) */
/* 672 */	NdrFcLong( 0x3 ),	/* 3 */
/* 676 */	NdrFcShort( 0x164 ),	/* Offset= 356 (1032) */
/* 678 */	NdrFcLong( 0x14 ),	/* 20 */
/* 682 */	NdrFcShort( 0x17a ),	/* Offset= 378 (1060) */
/* 684 */	NdrFcShort( 0xffff ),	/* Offset= -1 (683) */
/* 686 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 688 */	NdrFcShort( 0x0 ),	/* 0 */
/* 690 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 692 */	NdrFcShort( 0x0 ),	/* 0 */
/* 694 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 696 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 700 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 702 */	
			0x13, 0x0,	/* FC_OP */
/* 704 */	NdrFcShort( 0xfd56 ),	/* Offset= -682 (22) */
/* 706 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 708 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 710 */	NdrFcShort( 0x10 ),	/* 16 */
/* 712 */	NdrFcShort( 0x0 ),	/* 0 */
/* 714 */	NdrFcShort( 0x6 ),	/* Offset= 6 (720) */
/* 716 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 718 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 720 */	
			0x11, 0x0,	/* FC_RP */
/* 722 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (686) */
/* 724 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 726 */	NdrFcShort( 0x0 ),	/* 0 */
/* 728 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 730 */	NdrFcShort( 0x0 ),	/* 0 */
/* 732 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 734 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 738 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 740 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 742 */	NdrFcShort( 0xfd4c ),	/* Offset= -692 (50) */
/* 744 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 746 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 748 */	NdrFcShort( 0x10 ),	/* 16 */
/* 750 */	NdrFcShort( 0x0 ),	/* 0 */
/* 752 */	NdrFcShort( 0x6 ),	/* Offset= 6 (758) */
/* 754 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 756 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 758 */	
			0x11, 0x0,	/* FC_RP */
/* 760 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (724) */
/* 762 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 764 */	NdrFcShort( 0x0 ),	/* 0 */
/* 766 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 768 */	NdrFcShort( 0x0 ),	/* 0 */
/* 770 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 772 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 776 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 778 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 780 */	NdrFcShort( 0xff44 ),	/* Offset= -188 (592) */
/* 782 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 784 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 786 */	NdrFcShort( 0x10 ),	/* 16 */
/* 788 */	NdrFcShort( 0x0 ),	/* 0 */
/* 790 */	NdrFcShort( 0x6 ),	/* Offset= 6 (796) */
/* 792 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 794 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 796 */	
			0x11, 0x0,	/* FC_RP */
/* 798 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (762) */
/* 800 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 802 */	NdrFcShort( 0x0 ),	/* 0 */
/* 804 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 806 */	NdrFcShort( 0x0 ),	/* 0 */
/* 808 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 810 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 814 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 816 */	
			0x13, 0x0,	/* FC_OP */
/* 818 */	NdrFcShort( 0x176 ),	/* Offset= 374 (1192) */
/* 820 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 822 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 824 */	NdrFcShort( 0x10 ),	/* 16 */
/* 826 */	NdrFcShort( 0x0 ),	/* 0 */
/* 828 */	NdrFcShort( 0x6 ),	/* Offset= 6 (834) */
/* 830 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 832 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 834 */	
			0x11, 0x0,	/* FC_RP */
/* 836 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (800) */
/* 838 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 840 */	NdrFcLong( 0x2f ),	/* 47 */
/* 844 */	NdrFcShort( 0x0 ),	/* 0 */
/* 846 */	NdrFcShort( 0x0 ),	/* 0 */
/* 848 */	0xc0,		/* 192 */
			0x0,		/* 0 */
/* 850 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 852 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 854 */	0x0,		/* 0 */
			0x46,		/* 70 */
/* 856 */	
			0x1b,		/* FC_CARRAY */
			0x0,		/* 0 */
/* 858 */	NdrFcShort( 0x1 ),	/* 1 */
/* 860 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 862 */	NdrFcShort( 0x4 ),	/* 4 */
/* 864 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 866 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */
/* 868 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 870 */	NdrFcShort( 0x18 ),	/* 24 */
/* 872 */	NdrFcShort( 0x0 ),	/* 0 */
/* 874 */	NdrFcShort( 0xa ),	/* Offset= 10 (884) */
/* 876 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 878 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 880 */	NdrFcShort( 0xffd6 ),	/* Offset= -42 (838) */
/* 882 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 884 */	
			0x13, 0x0,	/* FC_OP */
/* 886 */	NdrFcShort( 0xffe2 ),	/* Offset= -30 (856) */
/* 888 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 890 */	NdrFcShort( 0x0 ),	/* 0 */
/* 892 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 894 */	NdrFcShort( 0x0 ),	/* 0 */
/* 896 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 898 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 902 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 904 */	
			0x13, 0x0,	/* FC_OP */
/* 906 */	NdrFcShort( 0xffda ),	/* Offset= -38 (868) */
/* 908 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 910 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 912 */	NdrFcShort( 0x10 ),	/* 16 */
/* 914 */	NdrFcShort( 0x0 ),	/* 0 */
/* 916 */	NdrFcShort( 0x6 ),	/* Offset= 6 (922) */
/* 918 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 920 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 922 */	
			0x11, 0x0,	/* FC_RP */
/* 924 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (888) */
/* 926 */	
			0x1d,		/* FC_SMFARRAY */
			0x0,		/* 0 */
/* 928 */	NdrFcShort( 0x8 ),	/* 8 */
/* 930 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */
/* 932 */	
			0x15,		/* FC_STRUCT */
			0x3,		/* 3 */
/* 934 */	NdrFcShort( 0x10 ),	/* 16 */
/* 936 */	0x8,		/* FC_LONG */
			0x6,		/* FC_SHORT */
/* 938 */	0x6,		/* FC_SHORT */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 940 */	0x0,		/* 0 */
			NdrFcShort( 0xfff1 ),	/* Offset= -15 (926) */
			0x5b,		/* FC_END */
/* 944 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 946 */	NdrFcShort( 0x20 ),	/* 32 */
/* 948 */	NdrFcShort( 0x0 ),	/* 0 */
/* 950 */	NdrFcShort( 0xa ),	/* Offset= 10 (960) */
/* 952 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 954 */	0x36,		/* FC_POINTER */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 956 */	0x0,		/* 0 */
			NdrFcShort( 0xffe7 ),	/* Offset= -25 (932) */
			0x5b,		/* FC_END */
/* 960 */	
			0x11, 0x0,	/* FC_RP */
/* 962 */	NdrFcShort( 0xff12 ),	/* Offset= -238 (724) */
/* 964 */	
			0x1b,		/* FC_CARRAY */
			0x0,		/* 0 */
/* 966 */	NdrFcShort( 0x1 ),	/* 1 */
/* 968 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 970 */	NdrFcShort( 0x0 ),	/* 0 */
/* 972 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 974 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */
/* 976 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 978 */	NdrFcShort( 0x10 ),	/* 16 */
/* 980 */	NdrFcShort( 0x0 ),	/* 0 */
/* 982 */	NdrFcShort( 0x6 ),	/* Offset= 6 (988) */
/* 984 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 986 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 988 */	
			0x13, 0x0,	/* FC_OP */
/* 990 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (964) */
/* 992 */	
			0x1b,		/* FC_CARRAY */
			0x1,		/* 1 */
/* 994 */	NdrFcShort( 0x2 ),	/* 2 */
/* 996 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 998 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1000 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 1002 */	0x6,		/* FC_SHORT */
			0x5b,		/* FC_END */
/* 1004 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 1006 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1008 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1010 */	NdrFcShort( 0x6 ),	/* Offset= 6 (1016) */
/* 1012 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 1014 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 1016 */	
			0x13, 0x0,	/* FC_OP */
/* 1018 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (992) */
/* 1020 */	
			0x1b,		/* FC_CARRAY */
			0x3,		/* 3 */
/* 1022 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1024 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 1026 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1028 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 1030 */	0x8,		/* FC_LONG */
			0x5b,		/* FC_END */
/* 1032 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 1034 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1036 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1038 */	NdrFcShort( 0x6 ),	/* Offset= 6 (1044) */
/* 1040 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 1042 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 1044 */	
			0x13, 0x0,	/* FC_OP */
/* 1046 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (1020) */
/* 1048 */	
			0x1b,		/* FC_CARRAY */
			0x7,		/* 7 */
/* 1050 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1052 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 1054 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1056 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 1058 */	0xb,		/* FC_HYPER */
			0x5b,		/* FC_END */
/* 1060 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 1062 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1064 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1066 */	NdrFcShort( 0x6 ),	/* Offset= 6 (1072) */
/* 1068 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 1070 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 1072 */	
			0x13, 0x0,	/* FC_OP */
/* 1074 */	NdrFcShort( 0xffe6 ),	/* Offset= -26 (1048) */
/* 1076 */	
			0x15,		/* FC_STRUCT */
			0x3,		/* 3 */
/* 1078 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1080 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 1082 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1084 */	
			0x1b,		/* FC_CARRAY */
			0x3,		/* 3 */
/* 1086 */	NdrFcShort( 0x8 ),	/* 8 */
/* 1088 */	0x7,		/* Corr desc: FC_USHORT */
			0x0,		/*  */
/* 1090 */	NdrFcShort( 0xffc8 ),	/* -56 */
/* 1092 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 1094 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1096 */	NdrFcShort( 0xffec ),	/* Offset= -20 (1076) */
/* 1098 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1100 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 1102 */	NdrFcShort( 0x38 ),	/* 56 */
/* 1104 */	NdrFcShort( 0xffec ),	/* Offset= -20 (1084) */
/* 1106 */	NdrFcShort( 0x0 ),	/* Offset= 0 (1106) */
/* 1108 */	0x6,		/* FC_SHORT */
			0x6,		/* FC_SHORT */
/* 1110 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 1112 */	0x40,		/* FC_STRUCTPAD4 */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 1114 */	0x0,		/* 0 */
			NdrFcShort( 0xfe0f ),	/* Offset= -497 (618) */
			0x5b,		/* FC_END */
/* 1118 */	
			0x13, 0x0,	/* FC_OP */
/* 1120 */	NdrFcShort( 0xff04 ),	/* Offset= -252 (868) */
/* 1122 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 1124 */	0x1,		/* FC_BYTE */
			0x5c,		/* FC_PAD */
/* 1126 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 1128 */	0x6,		/* FC_SHORT */
			0x5c,		/* FC_PAD */
/* 1130 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 1132 */	0x8,		/* FC_LONG */
			0x5c,		/* FC_PAD */
/* 1134 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 1136 */	0xb,		/* FC_HYPER */
			0x5c,		/* FC_PAD */
/* 1138 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 1140 */	0xa,		/* FC_FLOAT */
			0x5c,		/* FC_PAD */
/* 1142 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 1144 */	0xc,		/* FC_DOUBLE */
			0x5c,		/* FC_PAD */
/* 1146 */	
			0x13, 0x0,	/* FC_OP */
/* 1148 */	NdrFcShort( 0xfdce ),	/* Offset= -562 (586) */
/* 1150 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 1152 */	NdrFcShort( 0xfb86 ),	/* Offset= -1146 (6) */
/* 1154 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 1156 */	NdrFcShort( 0xfbae ),	/* Offset= -1106 (50) */
/* 1158 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 1160 */	NdrFcShort( 0xfdc8 ),	/* Offset= -568 (592) */
/* 1162 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 1164 */	NdrFcShort( 0xfdd6 ),	/* Offset= -554 (610) */
/* 1166 */	
			0x13, 0x10,	/* FC_OP [pointer_deref] */
/* 1168 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1170) */
/* 1170 */	
			0x13, 0x0,	/* FC_OP */
/* 1172 */	NdrFcShort( 0x14 ),	/* Offset= 20 (1192) */
/* 1174 */	
			0x15,		/* FC_STRUCT */
			0x7,		/* 7 */
/* 1176 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1178 */	0x6,		/* FC_SHORT */
			0x1,		/* FC_BYTE */
/* 1180 */	0x1,		/* FC_BYTE */
			0x8,		/* FC_LONG */
/* 1182 */	0xb,		/* FC_HYPER */
			0x5b,		/* FC_END */
/* 1184 */	
			0x13, 0x0,	/* FC_OP */
/* 1186 */	NdrFcShort( 0xfff4 ),	/* Offset= -12 (1174) */
/* 1188 */	
			0x13, 0x8,	/* FC_OP [simple_pointer] */
/* 1190 */	0x2,		/* FC_CHAR */
			0x5c,		/* FC_PAD */
/* 1192 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x7,		/* 7 */
/* 1194 */	NdrFcShort( 0x20 ),	/* 32 */
/* 1196 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1198 */	NdrFcShort( 0x0 ),	/* Offset= 0 (1198) */
/* 1200 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 1202 */	0x6,		/* FC_SHORT */
			0x6,		/* FC_SHORT */
/* 1204 */	0x6,		/* FC_SHORT */
			0x6,		/* FC_SHORT */
/* 1206 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1208 */	NdrFcShort( 0xfc68 ),	/* Offset= -920 (288) */
/* 1210 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1212 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 1214 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1216 */	NdrFcShort( 0x18 ),	/* 24 */
/* 1218 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1220 */	NdrFcShort( 0xfc58 ),	/* Offset= -936 (284) */
/* 1222 */	
			0x11, 0x14,	/* FC_RP [alloced_on_stack] [pointer_deref] */
/* 1224 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1226) */
/* 1226 */	
			0x13, 0x0,	/* FC_OP */
/* 1228 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1230) */
/* 1230 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 1232 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1234 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 1236 */	NdrFcShort( 0x20 ),	/* ARM64 Stack size/offset = 32 */
/* 1238 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1240 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 1244 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1246 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1248 */	NdrFcShort( 0xfb52 ),	/* Offset= -1198 (50) */
/* 1250 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1252 */	
			0x11, 0x14,	/* FC_RP [alloced_on_stack] [pointer_deref] */
/* 1254 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1256) */
/* 1256 */	
			0x13, 0x0,	/* FC_OP */
/* 1258 */	NdrFcShort( 0x18 ),	/* Offset= 24 (1282) */
/* 1260 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 1262 */	NdrFcShort( 0x20 ),	/* 32 */
/* 1264 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1266 */	NdrFcShort( 0x10 ),	/* Offset= 16 (1282) */
/* 1268 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1270 */	NdrFcShort( 0xfb3c ),	/* Offset= -1220 (50) */
/* 1272 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 1274 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1276 */	NdrFcShort( 0xfb36 ),	/* Offset= -1226 (50) */
/* 1278 */	0x8,		/* FC_LONG */
			0x40,		/* FC_STRUCTPAD4 */
/* 1280 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1282 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 1284 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1286 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 1288 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1290 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1292 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 1296 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1298 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1300 */	NdrFcShort( 0xffd8 ),	/* Offset= -40 (1260) */
/* 1302 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1304 */	
			0x11, 0x0,	/* FC_RP */
/* 1306 */	NdrFcShort( 0x6 ),	/* Offset= 6 (1312) */
/* 1308 */	
			0x12, 0x0,	/* FC_UP */
/* 1310 */	NdrFcShort( 0xff8a ),	/* Offset= -118 (1192) */
/* 1312 */	0xb4,		/* FC_USER_MARSHAL */
			0x83,		/* 131 */
/* 1314 */	NdrFcShort( 0x2 ),	/* 2 */
/* 1316 */	NdrFcShort( 0x18 ),	/* 24 */
/* 1318 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1320 */	NdrFcShort( 0xfff4 ),	/* Offset= -12 (1308) */
/* 1322 */	
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/* 1324 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1326) */
/* 1326 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 1328 */	NdrFcShort( 0x10 ),	/* 16 */
/* 1330 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1332 */	NdrFcShort( 0x0 ),	/* Offset= 0 (1332) */
/* 1334 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1336 */	NdrFcShort( 0xfae8 ),	/* Offset= -1304 (32) */
/* 1338 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 1340 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1342 */	
			0x11, 0x0,	/* FC_RP */
/* 1344 */	NdrFcShort( 0xfbce ),	/* Offset= -1074 (270) */
/* 1346 */	
			0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
/* 1348 */	0x3,		/* FC_SMALL */
			0x5c,		/* FC_PAD */
/* 1350 */	
			0x11, 0x10,	/* FC_RP [pointer_deref] */
/* 1352 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1354) */
/* 1354 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1356 */	NdrFcLong( 0x1c20f2b ),	/* 29495083 */
/* 1360 */	NdrFcShort( 0x3dd2 ),	/* 15826 */
/* 1362 */	NdrFcShort( 0x400f ),	/* 16399 */
/* 1364 */	0x94,		/* 148 */
			0x9f,		/* 159 */
/* 1366 */	0xad,		/* 173 */
			0x0,		/* 0 */
/* 1368 */	0xbd,		/* 189 */
			0xab,		/* 171 */
/* 1370 */	0x1d,		/* 29 */
			0x41,		/* 65 */
/* 1372 */	
			0x11, 0x14,	/* FC_RP [alloced_on_stack] [pointer_deref] */
/* 1374 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1376) */
/* 1376 */	
			0x13, 0x0,	/* FC_OP */
/* 1378 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1380) */
/* 1380 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 1382 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1384 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 1386 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1388 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1390 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 1394 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1396 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1398 */	NdrFcShort( 0xffd4 ),	/* Offset= -44 (1354) */
/* 1400 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1402 */	
			0x11, 0x10,	/* FC_RP [pointer_deref] */
/* 1404 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1406) */
/* 1406 */	
			0x2f,		/* FC_IP */
			0x5a,		/* FC_CONSTANT_IID */
/* 1408 */	NdrFcLong( 0x35ad8070 ),	/* 900563056 */
/* 1412 */	NdrFcShort( 0xc20c ),	/* -15860 */
/* 1414 */	NdrFcShort( 0x4fb4 ),	/* 20404 */
/* 1416 */	0xb0,		/* 176 */
			0x94,		/* 148 */
/* 1418 */	0xf4,		/* 244 */
			0xf7,		/* 247 */
/* 1420 */	0x27,		/* 39 */
			0x5d,		/* 93 */
/* 1422 */	0xd4,		/* 212 */
			0x69,		/* 105 */
/* 1424 */	
			0x11, 0x14,	/* FC_RP [alloced_on_stack] [pointer_deref] */
/* 1426 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1428) */
/* 1428 */	
			0x13, 0x0,	/* FC_OP */
/* 1430 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1432) */
/* 1432 */	
			0x1c,		/* FC_CVARRAY */
			0x3,		/* 3 */
/* 1434 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1436 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x0,		/*  */
/* 1438 */	NdrFcShort( 0x8 ),	/* ARM64 Stack size/offset = 8 */
/* 1440 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 1442 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 1444 */	NdrFcShort( 0x18 ),	/* ARM64 Stack size/offset = 24 */
/* 1446 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1448 */	0x8,		/* FC_LONG */
			0x5b,		/* FC_END */
/* 1450 */	
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/* 1452 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1454) */
/* 1454 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 1456 */	NdrFcShort( 0x14 ),	/* 20 */
/* 1458 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1460 */	NdrFcShort( 0x0 ),	/* Offset= 0 (1460) */
/* 1462 */	0xd,		/* FC_ENUM16 */
			0x8,		/* FC_LONG */
/* 1464 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 1466 */	0x8,		/* FC_LONG */
			0x5b,		/* FC_END */
/* 1468 */	
			0x11, 0x14,	/* FC_RP [alloced_on_stack] [pointer_deref] */
/* 1470 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1472) */
/* 1472 */	
			0x13, 0x0,	/* FC_OP */
/* 1474 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1476) */
/* 1476 */	
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 1478 */	NdrFcShort( 0x0 ),	/* 0 */
/* 1480 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 1482 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1484 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1486 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 1490 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1492 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 1494 */	NdrFcShort( 0xfa5c ),	/* Offset= -1444 (50) */
/* 1496 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 1498 */	
			0x11, 0x14,	/* FC_RP [alloced_on_stack] [pointer_deref] */
/* 1500 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1502) */
/* 1502 */	
			0x13, 0x0,	/* FC_OP */
/* 1504 */	NdrFcShort( 0x2 ),	/* Offset= 2 (1506) */
/* 1506 */	
			0x1b,		/* FC_CARRAY */
			0x3,		/* 3 */
/* 1508 */	NdrFcShort( 0x4 ),	/* 4 */
/* 1510 */	0x28,		/* Corr desc:  parameter, FC_LONG */
			0x54,		/* FC_DEREFERENCE */
/* 1512 */	NdrFcShort( 0x10 ),	/* ARM64 Stack size/offset = 16 */
/* 1514 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 1516 */	0x8,		/* FC_LONG */
			0x5b,		/* FC_END */

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
            HWND_UserSize
            ,HWND_UserMarshal
            ,HWND_UserUnmarshal
            ,HWND_UserFree
            },
            {
            VARIANT_UserSize
            ,VARIANT_UserMarshal
            ,VARIANT_UserUnmarshal
            ,VARIANT_UserFree
            }

        };



/* Standard interface: __MIDL_itf_ia2_api_all_0000_0000, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IUnknown, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}} */


/* Object interface: IAccessibleRelation, ver. 0.0,
   GUID={0x7CDF86EE,0xC3DA,0x496a,{0xBD,0xA4,0x28,0x1B,0x33,0x6E,0x1F,0xDC}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleRelation_FormatStringOffsetTable[] =
    {
    0,
    42,
    84,
    126,
    174
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleRelation_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleRelation_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleRelation_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleRelation_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(8) _IAccessibleRelationProxyVtbl = 
{
    &IAccessibleRelation_ProxyInfo,
    &IID_IAccessibleRelation,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleRelation::get_relationType */ ,
    (void *) (INT_PTR) -1 /* IAccessibleRelation::get_localizedRelationType */ ,
    (void *) (INT_PTR) -1 /* IAccessibleRelation::get_nTargets */ ,
    (void *) (INT_PTR) -1 /* IAccessibleRelation::get_target */ ,
    (void *) (INT_PTR) -1 /* IAccessibleRelation::get_targets */
};

const CInterfaceStubVtbl _IAccessibleRelationStubVtbl =
{
    &IID_IAccessibleRelation,
    &IAccessibleRelation_ServerInfo,
    8,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Standard interface: __MIDL_itf_ia2_api_all_0000_0001, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IAccessibleAction, ver. 0.0,
   GUID={0xB70D9F59,0x3B5A,0x4dba,{0xAB,0x9E,0x22,0x01,0x2F,0x60,0x7D,0xF5}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleAction_FormatStringOffsetTable[] =
    {
    230,
    272,
    314,
    362,
    424,
    472
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleAction_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleAction_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleAction_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleAction_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(9) _IAccessibleActionProxyVtbl = 
{
    &IAccessibleAction_ProxyInfo,
    &IID_IAccessibleAction,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::nActions */ ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::doAction */ ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::get_description */ ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::get_keyBinding */ ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::get_name */ ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::get_localizedName */
};

const CInterfaceStubVtbl _IAccessibleActionStubVtbl =
{
    &IID_IAccessibleAction,
    &IAccessibleAction_ServerInfo,
    9,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Standard interface: __MIDL_itf_ia2_api_all_0000_0002, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IDispatch, ver. 0.0,
   GUID={0x00020400,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}} */


/* Object interface: IAccessible, ver. 0.0,
   GUID={0x618736e0,0x3c3d,0x11cf,{0x81,0x0c,0x00,0xaa,0x00,0x38,0x9b,0x71}} */


/* Object interface: IAccessible2, ver. 0.0,
   GUID={0xE89F726E,0xC4F4,0x4c19,{0xBB,0x19,0xB6,0x47,0xD7,0xFA,0x84,0x78}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessible2_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    520,
    562,
    610,
    666,
    708,
    750,
    806,
    862,
    904,
    946,
    988,
    1030,
    1086,
    1142,
    1184,
    1226,
    1268,
    1310
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessible2_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessible2_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessible2_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessible2_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(46) _IAccessible2ProxyVtbl = 
{
    &IAccessible2_ProxyInfo,
    &IID_IAccessible2,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    0 /* IAccessible::get_accParent */ ,
    0 /* IAccessible::get_accChildCount */ ,
    0 /* IAccessible::get_accChild */ ,
    0 /* IAccessible::get_accName */ ,
    0 /* IAccessible::get_accValue */ ,
    0 /* IAccessible::get_accDescription */ ,
    0 /* IAccessible::get_accRole */ ,
    0 /* IAccessible::get_accState */ ,
    0 /* IAccessible::get_accHelp */ ,
    0 /* IAccessible::get_accHelpTopic */ ,
    0 /* IAccessible::get_accKeyboardShortcut */ ,
    0 /* IAccessible::get_accFocus */ ,
    0 /* IAccessible::get_accSelection */ ,
    0 /* IAccessible::get_accDefaultAction */ ,
    0 /* IAccessible::accSelect */ ,
    0 /* IAccessible::accLocation */ ,
    0 /* IAccessible::accNavigate */ ,
    0 /* IAccessible::accHitTest */ ,
    0 /* IAccessible::accDoDefaultAction */ ,
    0 /* IAccessible::put_accName */ ,
    0 /* IAccessible::put_accValue */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_nRelations */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_relation */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_relations */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::role */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::scrollTo */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::scrollToPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_groupPosition */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_states */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_extendedRole */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_localizedExtendedRole */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_nExtendedStates */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_extendedStates */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_localizedExtendedStates */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_uniqueID */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_windowHandle */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_indexInParent */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_locale */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_attributes */
};


static const PRPC_STUB_FUNCTION IAccessible2_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
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
    NdrStubCall2
};

CInterfaceStubVtbl _IAccessible2StubVtbl =
{
    &IID_IAccessible2,
    &IAccessible2_ServerInfo,
    46,
    &IAccessible2_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Object interface: IAccessible2_2, ver. 0.0,
   GUID={0x6C9430E9,0x299D,0x4E6F,{0xBD,0x01,0xA8,0x2A,0x1E,0x88,0xD3,0xFF}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessible2_2_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    520,
    562,
    610,
    666,
    708,
    750,
    806,
    862,
    904,
    946,
    988,
    1030,
    1086,
    1142,
    1184,
    1226,
    1268,
    1310,
    1352,
    1400,
    1448
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessible2_2_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessible2_2_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessible2_2_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessible2_2_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(49) _IAccessible2_2ProxyVtbl = 
{
    &IAccessible2_2_ProxyInfo,
    &IID_IAccessible2_2,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    0 /* IAccessible::get_accParent */ ,
    0 /* IAccessible::get_accChildCount */ ,
    0 /* IAccessible::get_accChild */ ,
    0 /* IAccessible::get_accName */ ,
    0 /* IAccessible::get_accValue */ ,
    0 /* IAccessible::get_accDescription */ ,
    0 /* IAccessible::get_accRole */ ,
    0 /* IAccessible::get_accState */ ,
    0 /* IAccessible::get_accHelp */ ,
    0 /* IAccessible::get_accHelpTopic */ ,
    0 /* IAccessible::get_accKeyboardShortcut */ ,
    0 /* IAccessible::get_accFocus */ ,
    0 /* IAccessible::get_accSelection */ ,
    0 /* IAccessible::get_accDefaultAction */ ,
    0 /* IAccessible::accSelect */ ,
    0 /* IAccessible::accLocation */ ,
    0 /* IAccessible::accNavigate */ ,
    0 /* IAccessible::accHitTest */ ,
    0 /* IAccessible::accDoDefaultAction */ ,
    0 /* IAccessible::put_accName */ ,
    0 /* IAccessible::put_accValue */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_nRelations */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_relation */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_relations */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::role */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::scrollTo */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::scrollToPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_groupPosition */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_states */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_extendedRole */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_localizedExtendedRole */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_nExtendedStates */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_extendedStates */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_localizedExtendedStates */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_uniqueID */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_windowHandle */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_indexInParent */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_locale */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_attributes */ ,
    (void *) (INT_PTR) -1 /* IAccessible2_2::get_attribute */ ,
    (void *) (INT_PTR) -1 /* IAccessible2_2::get_accessibleWithCaret */ ,
    (void *) (INT_PTR) -1 /* IAccessible2_2::get_relationTargetsOfType */
};


static const PRPC_STUB_FUNCTION IAccessible2_2_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
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
    NdrStubCall2
};

CInterfaceStubVtbl _IAccessible2_2StubVtbl =
{
    &IID_IAccessible2_2,
    &IAccessible2_2_ServerInfo,
    49,
    &IAccessible2_2_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Standard interface: __MIDL_itf_ia2_api_all_0000_0004, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IAccessible2_3, ver. 0.0,
   GUID={0x5BE18059,0x762E,0x4E73,{0x94,0x76,0xAB,0xA2,0x94,0xFE,0xD4,0x11}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessible2_3_FormatStringOffsetTable[] =
    {
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    (unsigned short) -1,
    520,
    562,
    610,
    666,
    708,
    750,
    806,
    862,
    904,
    946,
    988,
    1030,
    1086,
    1142,
    1184,
    1226,
    1268,
    1310,
    1352,
    1400,
    1448,
    1510
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessible2_3_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessible2_3_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessible2_3_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessible2_3_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(50) _IAccessible2_3ProxyVtbl = 
{
    &IAccessible2_3_ProxyInfo,
    &IID_IAccessible2_3,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    0 /* IDispatch::GetTypeInfoCount */ ,
    0 /* IDispatch::GetTypeInfo */ ,
    0 /* IDispatch::GetIDsOfNames */ ,
    0 /* IDispatch_Invoke_Proxy */ ,
    0 /* IAccessible::get_accParent */ ,
    0 /* IAccessible::get_accChildCount */ ,
    0 /* IAccessible::get_accChild */ ,
    0 /* IAccessible::get_accName */ ,
    0 /* IAccessible::get_accValue */ ,
    0 /* IAccessible::get_accDescription */ ,
    0 /* IAccessible::get_accRole */ ,
    0 /* IAccessible::get_accState */ ,
    0 /* IAccessible::get_accHelp */ ,
    0 /* IAccessible::get_accHelpTopic */ ,
    0 /* IAccessible::get_accKeyboardShortcut */ ,
    0 /* IAccessible::get_accFocus */ ,
    0 /* IAccessible::get_accSelection */ ,
    0 /* IAccessible::get_accDefaultAction */ ,
    0 /* IAccessible::accSelect */ ,
    0 /* IAccessible::accLocation */ ,
    0 /* IAccessible::accNavigate */ ,
    0 /* IAccessible::accHitTest */ ,
    0 /* IAccessible::accDoDefaultAction */ ,
    0 /* IAccessible::put_accName */ ,
    0 /* IAccessible::put_accValue */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_nRelations */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_relation */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_relations */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::role */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::scrollTo */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::scrollToPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_groupPosition */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_states */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_extendedRole */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_localizedExtendedRole */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_nExtendedStates */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_extendedStates */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_localizedExtendedStates */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_uniqueID */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_windowHandle */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_indexInParent */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_locale */ ,
    (void *) (INT_PTR) -1 /* IAccessible2::get_attributes */ ,
    (void *) (INT_PTR) -1 /* IAccessible2_2::get_attribute */ ,
    (void *) (INT_PTR) -1 /* IAccessible2_2::get_accessibleWithCaret */ ,
    (void *) (INT_PTR) -1 /* IAccessible2_2::get_relationTargetsOfType */ ,
    (void *) (INT_PTR) -1 /* IAccessible2_3::get_selectionRanges */
};


static const PRPC_STUB_FUNCTION IAccessible2_3_table[] =
{
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
    STUB_FORWARDING_FUNCTION,
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
    NdrStubCall2
};

CInterfaceStubVtbl _IAccessible2_3StubVtbl =
{
    &IID_IAccessible2_3,
    &IAccessible2_3_ServerInfo,
    50,
    &IAccessible2_3_table[-3],
    CStdStubBuffer_DELEGATING_METHODS
};


/* Standard interface: __MIDL_itf_ia2_api_all_0000_0005, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IAccessibleComponent, ver. 0.0,
   GUID={0x1546D4B0,0x4C98,0x4bda,{0x89,0xAE,0x9A,0x64,0x74,0x8B,0xDD,0xE4}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleComponent_FormatStringOffsetTable[] =
    {
    1558,
    1606,
    84
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleComponent_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleComponent_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleComponent_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleComponent_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(6) _IAccessibleComponentProxyVtbl = 
{
    &IAccessibleComponent_ProxyInfo,
    &IID_IAccessibleComponent,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleComponent::get_locationInParent */ ,
    (void *) (INT_PTR) -1 /* IAccessibleComponent::get_foreground */ ,
    (void *) (INT_PTR) -1 /* IAccessibleComponent::get_background */
};

const CInterfaceStubVtbl _IAccessibleComponentStubVtbl =
{
    &IID_IAccessibleComponent,
    &IAccessibleComponent_ServerInfo,
    6,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAccessibleValue, ver. 0.0,
   GUID={0x35855B5B,0xC566,0x4fd0,{0xA7,0xB1,0xE6,0x54,0x65,0x60,0x03,0x94}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleValue_FormatStringOffsetTable[] =
    {
    1648,
    1690,
    1732,
    1774
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleValue_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleValue_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleValue_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleValue_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(7) _IAccessibleValueProxyVtbl = 
{
    &IAccessibleValue_ProxyInfo,
    &IID_IAccessibleValue,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleValue::get_currentValue */ ,
    (void *) (INT_PTR) -1 /* IAccessibleValue::setCurrentValue */ ,
    (void *) (INT_PTR) -1 /* IAccessibleValue::get_maximumValue */ ,
    (void *) (INT_PTR) -1 /* IAccessibleValue::get_minimumValue */
};

const CInterfaceStubVtbl _IAccessibleValueStubVtbl =
{
    &IID_IAccessibleValue,
    &IAccessibleValue_ServerInfo,
    7,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Standard interface: __MIDL_itf_ia2_api_all_0000_0007, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IAccessibleText, ver. 0.0,
   GUID={0x24FD2FFB,0x3AAD,0x4a08,{0x83,0x35,0xA3,0xAD,0x89,0xC0,0xFB,0x4B}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleText_FormatStringOffsetTable[] =
    {
    1816,
    1864,
    84,
    1926,
    2002,
    2044,
    2106,
    2162,
    2218,
    2288,
    2358,
    2428,
    2470,
    2512,
    2568,
    2610,
    2666,
    2736,
    2778
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleText_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleText_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleText_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleText_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(22) _IAccessibleTextProxyVtbl = 
{
    &IAccessibleText_ProxyInfo,
    &IID_IAccessibleText,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleText::addSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_attributes */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_caretOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_characterExtents */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_nSelections */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_offsetAtPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_selection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_text */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textBeforeOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textAfterOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textAtOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::removeSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::setCaretOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::setSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_nCharacters */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::scrollSubstringTo */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::scrollSubstringToPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_newText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_oldText */
};

const CInterfaceStubVtbl _IAccessibleTextStubVtbl =
{
    &IID_IAccessibleText,
    &IAccessibleText_ServerInfo,
    22,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAccessibleText2, ver. 0.0,
   GUID={0x9690A9CC,0x5C80,0x4DF5,{0x85,0x2E,0x2D,0x5A,0xE4,0x18,0x9A,0x54}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleText2_FormatStringOffsetTable[] =
    {
    1816,
    1864,
    84,
    1926,
    2002,
    2044,
    2106,
    2162,
    2218,
    2288,
    2358,
    2428,
    2470,
    2512,
    2568,
    2610,
    2666,
    2736,
    2778,
    2820
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleText2_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleText2_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleText2_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleText2_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(23) _IAccessibleText2ProxyVtbl = 
{
    &IAccessibleText2_ProxyInfo,
    &IID_IAccessibleText2,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleText::addSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_attributes */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_caretOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_characterExtents */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_nSelections */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_offsetAtPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_selection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_text */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textBeforeOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textAfterOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textAtOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::removeSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::setCaretOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::setSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_nCharacters */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::scrollSubstringTo */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::scrollSubstringToPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_newText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_oldText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText2::get_attributeRange */
};

const CInterfaceStubVtbl _IAccessibleText2StubVtbl =
{
    &IID_IAccessibleText2,
    &IAccessibleText2_ServerInfo,
    23,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAccessibleEditableText, ver. 0.0,
   GUID={0xA59AA09A,0x7011,0x4b65,{0x93,0x9D,0x32,0xB1,0xFB,0x55,0x47,0xE3}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleEditableText_FormatStringOffsetTable[] =
    {
    1816,
    2890,
    2938,
    2986,
    3034,
    3076,
    3132
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleEditableText_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleEditableText_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleEditableText_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleEditableText_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(10) _IAccessibleEditableTextProxyVtbl = 
{
    &IAccessibleEditableText_ProxyInfo,
    &IID_IAccessibleEditableText,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleEditableText::copyText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleEditableText::deleteText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleEditableText::insertText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleEditableText::cutText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleEditableText::pasteText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleEditableText::replaceText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleEditableText::setAttributes */
};

const CInterfaceStubVtbl _IAccessibleEditableTextStubVtbl =
{
    &IID_IAccessibleEditableText,
    &IAccessibleEditableText_ServerInfo,
    10,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAccessibleHyperlink, ver. 0.0,
   GUID={0x01C20F2B,0x3DD2,0x400f,{0x94,0x9F,0xAD,0x00,0xBD,0xAB,0x1D,0x41}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleHyperlink_FormatStringOffsetTable[] =
    {
    230,
    272,
    314,
    362,
    424,
    472,
    3188,
    3236,
    3284,
    3326,
    3368
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleHyperlink_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleHyperlink_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleHyperlink_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleHyperlink_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(14) _IAccessibleHyperlinkProxyVtbl = 
{
    &IAccessibleHyperlink_ProxyInfo,
    &IID_IAccessibleHyperlink,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::nActions */ ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::doAction */ ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::get_description */ ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::get_keyBinding */ ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::get_name */ ,
    (void *) (INT_PTR) -1 /* IAccessibleAction::get_localizedName */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHyperlink::get_anchor */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHyperlink::get_anchorTarget */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHyperlink::get_startIndex */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHyperlink::get_endIndex */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHyperlink::get_valid */
};

const CInterfaceStubVtbl _IAccessibleHyperlinkStubVtbl =
{
    &IID_IAccessibleHyperlink,
    &IAccessibleHyperlink_ServerInfo,
    14,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAccessibleHypertext, ver. 0.0,
   GUID={0x6B4F8BBF,0xF1F2,0x418a,{0xB3,0x5E,0xA1,0x95,0xBC,0x41,0x03,0xB9}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleHypertext_FormatStringOffsetTable[] =
    {
    1816,
    1864,
    84,
    1926,
    2002,
    2044,
    2106,
    2162,
    2218,
    2288,
    2358,
    2428,
    2470,
    2512,
    2568,
    2610,
    2666,
    2736,
    2778,
    3410,
    3452,
    3500
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleHypertext_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleHypertext_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleHypertext_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleHypertext_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(25) _IAccessibleHypertextProxyVtbl = 
{
    &IAccessibleHypertext_ProxyInfo,
    &IID_IAccessibleHypertext,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleText::addSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_attributes */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_caretOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_characterExtents */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_nSelections */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_offsetAtPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_selection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_text */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textBeforeOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textAfterOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textAtOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::removeSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::setCaretOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::setSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_nCharacters */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::scrollSubstringTo */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::scrollSubstringToPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_newText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_oldText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHypertext::get_nHyperlinks */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHypertext::get_hyperlink */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHypertext::get_hyperlinkIndex */
};

const CInterfaceStubVtbl _IAccessibleHypertextStubVtbl =
{
    &IID_IAccessibleHypertext,
    &IAccessibleHypertext_ServerInfo,
    25,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAccessibleHypertext2, ver. 0.0,
   GUID={0xCF64D89F,0x8287,0x4B44,{0x85,0x01,0xA8,0x27,0x45,0x3A,0x60,0x77}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleHypertext2_FormatStringOffsetTable[] =
    {
    1816,
    1864,
    84,
    1926,
    2002,
    2044,
    2106,
    2162,
    2218,
    2288,
    2358,
    2428,
    2470,
    2512,
    2568,
    2610,
    2666,
    2736,
    2778,
    3410,
    3452,
    3500,
    3548
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleHypertext2_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleHypertext2_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleHypertext2_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleHypertext2_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(26) _IAccessibleHypertext2ProxyVtbl = 
{
    &IAccessibleHypertext2_ProxyInfo,
    &IID_IAccessibleHypertext2,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleText::addSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_attributes */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_caretOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_characterExtents */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_nSelections */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_offsetAtPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_selection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_text */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textBeforeOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textAfterOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_textAtOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::removeSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::setCaretOffset */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::setSelection */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_nCharacters */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::scrollSubstringTo */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::scrollSubstringToPoint */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_newText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleText::get_oldText */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHypertext::get_nHyperlinks */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHypertext::get_hyperlink */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHypertext::get_hyperlinkIndex */ ,
    (void *) (INT_PTR) -1 /* IAccessibleHypertext2::get_hyperlinks */
};

const CInterfaceStubVtbl _IAccessibleHypertext2StubVtbl =
{
    &IID_IAccessibleHypertext2,
    &IAccessibleHypertext2_ServerInfo,
    26,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAccessibleTable, ver. 0.0,
   GUID={0x35AD8070,0xC20C,0x4fb4,{0xB0,0x94,0xF4,0xF7,0x27,0x5D,0xD4,0x69}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleTable_FormatStringOffsetTable[] =
    {
    3596,
    3652,
    3694,
    3750,
    3798,
    3854,
    3902,
    3950,
    3284,
    3326,
    3992,
    4034,
    4076,
    4124,
    4180,
    4228,
    4276,
    4332,
    4388,
    4444,
    4486,
    4534,
    4582,
    4638,
    4680,
    4722,
    4764,
    4806,
    4882
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleTable_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleTable_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleTable_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleTable_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(32) _IAccessibleTableProxyVtbl = 
{
    &IAccessibleTable_ProxyInfo,
    &IID_IAccessibleTable,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_accessibleAt */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_caption */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_childIndex */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_columnDescription */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_columnExtentAt */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_columnHeader */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_columnIndex */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_nColumns */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_nRows */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_nSelectedChildren */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_nSelectedColumns */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_nSelectedRows */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_rowDescription */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_rowExtentAt */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_rowHeader */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_rowIndex */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_selectedChildren */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_selectedColumns */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_selectedRows */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_summary */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_isColumnSelected */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_isRowSelected */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_isSelected */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::selectRow */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::selectColumn */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::unselectRow */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::unselectColumn */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_rowColumnExtentsAtIndex */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable::get_modelChange */
};

const CInterfaceStubVtbl _IAccessibleTableStubVtbl =
{
    &IID_IAccessibleTable,
    &IAccessibleTable_ServerInfo,
    32,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAccessibleTable2, ver. 0.0,
   GUID={0x6167f295,0x06f0,0x4cdd,{0xa1,0xfa,0x02,0xe2,0x51,0x53,0xd8,0x69}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleTable2_FormatStringOffsetTable[] =
    {
    3596,
    3652,
    314,
    4924,
    2002,
    4966,
    5008,
    3950,
    5050,
    5098,
    5146,
    5194,
    5242,
    5284,
    5332,
    5380,
    5422,
    5464,
    5506,
    5548
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleTable2_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleTable2_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleTable2_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleTable2_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(23) _IAccessibleTable2ProxyVtbl = 
{
    &IAccessibleTable2_ProxyInfo,
    &IID_IAccessibleTable2,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_cellAt */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_caption */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_columnDescription */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_nColumns */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_nRows */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_nSelectedCells */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_nSelectedColumns */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_nSelectedRows */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_rowDescription */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_selectedCells */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_selectedColumns */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_selectedRows */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_summary */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_isColumnSelected */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_isRowSelected */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::selectRow */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::selectColumn */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::unselectRow */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::unselectColumn */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTable2::get_modelChange */
};

const CInterfaceStubVtbl _IAccessibleTable2StubVtbl =
{
    &IID_IAccessibleTable2,
    &IAccessibleTable2_ServerInfo,
    23,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAccessibleTableCell, ver. 0.0,
   GUID={0x594116B1,0xC99F,0x4847,{0xAD,0x06,0x0A,0x7A,0x86,0xEC,0xE6,0x45}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleTableCell_FormatStringOffsetTable[] =
    {
    230,
    5590,
    84,
    4924,
    5638,
    4966,
    5686,
    5728,
    5798
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleTableCell_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleTableCell_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleTableCell_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleTableCell_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(12) _IAccessibleTableCellProxyVtbl = 
{
    &IAccessibleTableCell_ProxyInfo,
    &IID_IAccessibleTableCell,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleTableCell::get_columnExtent */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTableCell::get_columnHeaderCells */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTableCell::get_columnIndex */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTableCell::get_rowExtent */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTableCell::get_rowHeaderCells */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTableCell::get_rowIndex */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTableCell::get_isSelected */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTableCell::get_rowColumnExtents */ ,
    (void *) (INT_PTR) -1 /* IAccessibleTableCell::get_table */
};

const CInterfaceStubVtbl _IAccessibleTableCellStubVtbl =
{
    &IID_IAccessibleTableCell,
    &IAccessibleTableCell_ServerInfo,
    12,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAccessibleImage, ver. 0.0,
   GUID={0xFE5ABB3D,0x615E,0x4f7b,{0x90,0x9F,0x5F,0x0E,0xDA,0x9E,0x8D,0xDE}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleImage_FormatStringOffsetTable[] =
    {
    0,
    5840,
    5896
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleImage_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleImage_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleImage_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleImage_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(6) _IAccessibleImageProxyVtbl = 
{
    &IAccessibleImage_ProxyInfo,
    &IID_IAccessibleImage,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleImage::get_description */ ,
    (void *) (INT_PTR) -1 /* IAccessibleImage::get_imagePosition */ ,
    (void *) (INT_PTR) -1 /* IAccessibleImage::get_imageSize */
};

const CInterfaceStubVtbl _IAccessibleImageStubVtbl =
{
    &IID_IAccessibleImage,
    &IAccessibleImage_ServerInfo,
    6,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Standard interface: __MIDL_itf_ia2_api_all_0000_0017, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IAccessibleApplication, ver. 0.0,
   GUID={0xD49DED83,0x5B25,0x43F4,{0x9B,0x95,0x93,0xB4,0x45,0x95,0x97,0x9E}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleApplication_FormatStringOffsetTable[] =
    {
    0,
    42,
    5944,
    5986
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleApplication_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleApplication_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleApplication_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleApplication_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(7) _IAccessibleApplicationProxyVtbl = 
{
    &IAccessibleApplication_ProxyInfo,
    &IID_IAccessibleApplication,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleApplication::get_appName */ ,
    (void *) (INT_PTR) -1 /* IAccessibleApplication::get_appVersion */ ,
    (void *) (INT_PTR) -1 /* IAccessibleApplication::get_toolkitName */ ,
    (void *) (INT_PTR) -1 /* IAccessibleApplication::get_toolkitVersion */
};

const CInterfaceStubVtbl _IAccessibleApplicationStubVtbl =
{
    &IID_IAccessibleApplication,
    &IAccessibleApplication_ServerInfo,
    7,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Object interface: IAccessibleDocument, ver. 0.0,
   GUID={0xC48C7FCF,0x4AB5,0x4056,{0xAF,0xA6,0x90,0x2D,0x6E,0x1D,0x11,0x49}} */

#pragma code_seg(".orpc")
static const unsigned short IAccessibleDocument_FormatStringOffsetTable[] =
    {
    6028
    };

static const MIDL_STUBLESS_PROXY_INFO IAccessibleDocument_ProxyInfo =
    {
    &Object_StubDesc,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleDocument_FormatStringOffsetTable[-3],
    0,
    0,
    0
    };


static const MIDL_SERVER_INFO IAccessibleDocument_ServerInfo = 
    {
    &Object_StubDesc,
    0,
    ia2_api_all__MIDL_ProcFormatString.Format,
    &IAccessibleDocument_FormatStringOffsetTable[-3],
    0,
    0,
    0,
    0};
CINTERFACE_PROXY_VTABLE(4) _IAccessibleDocumentProxyVtbl = 
{
    &IAccessibleDocument_ProxyInfo,
    &IID_IAccessibleDocument,
    IUnknown_QueryInterface_Proxy,
    IUnknown_AddRef_Proxy,
    IUnknown_Release_Proxy ,
    (void *) (INT_PTR) -1 /* IAccessibleDocument::get_anchorTarget */
};

const CInterfaceStubVtbl _IAccessibleDocumentStubVtbl =
{
    &IID_IAccessibleDocument,
    &IAccessibleDocument_ServerInfo,
    4,
    0, /* pure interpreted */
    CStdStubBuffer_METHODS
};


/* Standard interface: __MIDL_itf_ia2_api_all_0000_0019, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */

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
    ia2_api_all__MIDL_TypeFormatString.Format,
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

const CInterfaceProxyVtbl * const _ia2_api_all_ProxyVtblList[] = 
{
    ( CInterfaceProxyVtbl *) &_IAccessibleHyperlinkProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleImageProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessible2_3ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleActionProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleValueProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessible2ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleTableProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleApplicationProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleTable2ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleEditableTextProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleHypertext2ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleComponentProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleTableCellProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleHypertextProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleText2ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleDocumentProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessible2_2ProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleRelationProxyVtbl,
    ( CInterfaceProxyVtbl *) &_IAccessibleTextProxyVtbl,
    0
};

const CInterfaceStubVtbl * const _ia2_api_all_StubVtblList[] = 
{
    ( CInterfaceStubVtbl *) &_IAccessibleHyperlinkStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleImageStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessible2_3StubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleActionStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleValueStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessible2StubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleTableStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleApplicationStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleTable2StubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleEditableTextStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleHypertext2StubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleComponentStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleTableCellStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleHypertextStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleText2StubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleDocumentStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessible2_2StubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleRelationStubVtbl,
    ( CInterfaceStubVtbl *) &_IAccessibleTextStubVtbl,
    0
};

PCInterfaceName const _ia2_api_all_InterfaceNamesList[] = 
{
    "IAccessibleHyperlink",
    "IAccessibleImage",
    "IAccessible2_3",
    "IAccessibleAction",
    "IAccessibleValue",
    "IAccessible2",
    "IAccessibleTable",
    "IAccessibleApplication",
    "IAccessibleTable2",
    "IAccessibleEditableText",
    "IAccessibleHypertext2",
    "IAccessibleComponent",
    "IAccessibleTableCell",
    "IAccessibleHypertext",
    "IAccessibleText2",
    "IAccessibleDocument",
    "IAccessible2_2",
    "IAccessibleRelation",
    "IAccessibleText",
    0
};

const IID *  const _ia2_api_all_BaseIIDList[] = 
{
    0,
    0,
    &IID_IAccessible,
    0,
    0,
    &IID_IAccessible,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    &IID_IAccessible,
    0,
    0,
    0
};


#define _ia2_api_all_CHECK_IID(n)	IID_GENERIC_CHECK_IID( _ia2_api_all, pIID, n)

int __stdcall _ia2_api_all_IID_Lookup( const IID * pIID, int * pIndex )
{
    IID_BS_LOOKUP_SETUP

    IID_BS_LOOKUP_INITIAL_TEST( _ia2_api_all, 19, 16 )
    IID_BS_LOOKUP_NEXT_TEST( _ia2_api_all, 8 )
    IID_BS_LOOKUP_NEXT_TEST( _ia2_api_all, 4 )
    IID_BS_LOOKUP_NEXT_TEST( _ia2_api_all, 2 )
    IID_BS_LOOKUP_NEXT_TEST( _ia2_api_all, 1 )
    IID_BS_LOOKUP_RETURN_RESULT( _ia2_api_all, 19, *pIndex )
    
}

const ExtendedProxyFileInfo ia2_api_all_ProxyFileInfo = 
{
    (PCInterfaceProxyVtblList *) & _ia2_api_all_ProxyVtblList,
    (PCInterfaceStubVtblList *) & _ia2_api_all_StubVtblList,
    (const PCInterfaceName * ) & _ia2_api_all_InterfaceNamesList,
    (const IID ** ) & _ia2_api_all_BaseIIDList,
    & _ia2_api_all_IID_Lookup, 
    19,
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

