// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Automatically generated code.  See srpcgen.py
//
// NaCl Simple Remote Procedure Call interface abstractions.

#ifndef GEN_PPAPI_PROXY_PPB_RPC_H_
#define GEN_PPAPI_PROXY_PPB_RPC_H_
#ifndef __native_client__
#include "native_client/src/include/portability.h"
#endif  // __native_client__
#include "native_client/src/shared/srpc/nacl_srpc.h"
class ObjectStubRpcServer {
 public:
  static void HasProperty(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      int32_t* success,
      nacl_abi_size_t* exception_bytes, char* exception);
  static void HasMethod(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      int32_t* success,
      nacl_abi_size_t* exception_bytes, char* exception);
  static void GetProperty(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* value_bytes, char* value,
      nacl_abi_size_t* exception_bytes, char* exception);
  static void GetAllPropertyNames(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      int32_t* property_count,
      nacl_abi_size_t* properties_bytes, char* properties,
      nacl_abi_size_t* exception_bytes, char* exception);
  static void SetProperty(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t value_bytes, char* value,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* exception_bytes, char* exception);
  static void RemoveProperty(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* exception_bytes, char* exception);
  static void Call(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      int32_t argc,
      nacl_abi_size_t argv_bytes, char* argv,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* ret_bytes, char* ret,
      nacl_abi_size_t* exception_bytes, char* exception);
  static void Construct(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      int32_t argc,
      nacl_abi_size_t argv_bytes, char* argv,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* ret_bytes, char* ret,
      nacl_abi_size_t* exception_bytes, char* exception);
  static void Deallocate(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability);

 private:
  ObjectStubRpcServer();
  ObjectStubRpcServer(const ObjectStubRpcServer&);
  void operator=(const ObjectStubRpcServer);
};  // class ObjectStubRpcServer

class PpbCoreRpcServer {
 public:
  static void PPB_Core_AddRefResource(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int64_t resource);
  static void PPB_Core_ReleaseResource(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int64_t resource);
  static void PPB_Core_GetTime(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      double* time);

 private:
  PpbCoreRpcServer();
  PpbCoreRpcServer(const PpbCoreRpcServer&);
  void operator=(const PpbCoreRpcServer);
};  // class PpbCoreRpcServer

class PpbGraphics2DRpcServer {
 public:
  static void PPB_Graphics2D_Create(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int64_t module,
      nacl_abi_size_t size_bytes, int32_t* size,
      int32_t is_always_opaque,
      int64_t* resource);
  static void PPB_Graphics2D_IsGraphics2D(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int64_t resource,
      int32_t* success);
  static void PPB_Graphics2D_Describe(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int64_t graphics_2d,
      nacl_abi_size_t* size_bytes, int32_t* size,
      int32_t* is_always_opaque,
      int32_t* success);
  static void PPB_Graphics2D_PaintImageData(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int64_t graphics_2d,
      int64_t image,
      nacl_abi_size_t top_left_bytes, int32_t* top_left,
      nacl_abi_size_t src_rect_bytes, int32_t* src_rect);
  static void PPB_Graphics2D_Scroll(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int64_t graphics_2d,
      nacl_abi_size_t clip_rect_bytes, int32_t* clip_rect,
      nacl_abi_size_t amount_bytes, int32_t* amount);
  static void PPB_Graphics2D_ReplaceContents(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int64_t graphics_2d,
      int64_t image);

 private:
  PpbGraphics2DRpcServer();
  PpbGraphics2DRpcServer(const PpbGraphics2DRpcServer&);
  void operator=(const PpbGraphics2DRpcServer);
};  // class PpbGraphics2DRpcServer

class PpbImageDataRpcServer {
 public:
  static void PPB_ImageData_GetNativeImageDataFormat(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int32_t* format);
  static void PPB_ImageData_IsImageDataFormatSupported(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int32_t format,
      int32_t* success);
  static void PPB_ImageData_Create(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int64_t module,
      int32_t format,
      nacl_abi_size_t size_bytes, int32_t* size,
      int32_t init_to_zero,
      int64_t* resource);
  static void PPB_ImageData_IsImageData(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int64_t resource,
      int32_t* success);
  static void PPB_ImageData_Describe(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int64_t resource,
      nacl_abi_size_t* desc_bytes, int32_t* desc,
      int32_t* success);

 private:
  PpbImageDataRpcServer();
  PpbImageDataRpcServer(const PpbImageDataRpcServer&);
  void operator=(const PpbImageDataRpcServer);
};  // class PpbImageDataRpcServer

class PpbRpcs {
 public:
  static NACL_SRPC_METHOD_ARRAY(srpc_methods);
};  // class PpbRpcs

#endif  // GEN_PPAPI_PROXY_PPB_RPC_H_

