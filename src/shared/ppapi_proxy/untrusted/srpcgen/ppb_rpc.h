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
class ObjectStubRpcClient {
 public:
  static NaClSrpcError HasProperty(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      int32_t* success,
      nacl_abi_size_t* exception_bytes, char* exception);
  static NaClSrpcError HasMethod(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      int32_t* success,
      nacl_abi_size_t* exception_bytes, char* exception);
  static NaClSrpcError GetProperty(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* value_bytes, char* value,
      nacl_abi_size_t* exception_bytes, char* exception);
  static NaClSrpcError GetAllPropertyNames(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      int32_t* property_count,
      nacl_abi_size_t* properties_bytes, char* properties,
      nacl_abi_size_t* exception_bytes, char* exception);
  static NaClSrpcError SetProperty(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t value_bytes, char* value,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* exception_bytes, char* exception);
  static NaClSrpcError RemoveProperty(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* exception_bytes, char* exception);
  static NaClSrpcError Call(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      int32_t argc,
      nacl_abi_size_t argv_bytes, char* argv,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* ret_bytes, char* ret,
      nacl_abi_size_t* exception_bytes, char* exception);
  static NaClSrpcError Construct(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      int32_t argc,
      nacl_abi_size_t argv_bytes, char* argv,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* ret_bytes, char* ret,
      nacl_abi_size_t* exception_bytes, char* exception);
  static NaClSrpcError Deallocate(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability);

 private:
  ObjectStubRpcClient();
  ObjectStubRpcClient(const ObjectStubRpcClient&);
  void operator=(const ObjectStubRpcClient);
};  // class ObjectStubRpcClient

class PpbRpcClient {
 public:
  static NaClSrpcError PPB_GetInterface(
      NaClSrpcChannel* channel,
      char* interface_name,
      int32_t* exports_interface_name);

 private:
  PpbRpcClient();
  PpbRpcClient(const PpbRpcClient&);
  void operator=(const PpbRpcClient);
};  // class PpbRpcClient

class PpbCoreRpcClient {
 public:
  static NaClSrpcError PPB_Core_AddRefResource(
      NaClSrpcChannel* channel,
      int64_t resource);
  static NaClSrpcError PPB_Core_ReleaseResource(
      NaClSrpcChannel* channel,
      int64_t resource);
  static NaClSrpcError PPB_Core_GetTime(
      NaClSrpcChannel* channel,
      double* time);

 private:
  PpbCoreRpcClient();
  PpbCoreRpcClient(const PpbCoreRpcClient&);
  void operator=(const PpbCoreRpcClient);
};  // class PpbCoreRpcClient

class PpbGraphics2DRpcClient {
 public:
  static NaClSrpcError PPB_Graphics2D_Create(
      NaClSrpcChannel* channel,
      int64_t module,
      nacl_abi_size_t size_bytes, int32_t* size,
      int32_t is_always_opaque,
      int64_t* resource);
  static NaClSrpcError PPB_Graphics2D_IsGraphics2D(
      NaClSrpcChannel* channel,
      int64_t resource,
      int32_t* success);
  static NaClSrpcError PPB_Graphics2D_Describe(
      NaClSrpcChannel* channel,
      int64_t graphics_2d,
      nacl_abi_size_t* size_bytes, int32_t* size,
      int32_t* is_always_opaque,
      int32_t* success);
  static NaClSrpcError PPB_Graphics2D_PaintImageData(
      NaClSrpcChannel* channel,
      int64_t graphics_2d,
      int64_t image,
      nacl_abi_size_t top_left_bytes, int32_t* top_left,
      nacl_abi_size_t src_rect_bytes, int32_t* src_rect);
  static NaClSrpcError PPB_Graphics2D_Scroll(
      NaClSrpcChannel* channel,
      int64_t graphics_2d,
      nacl_abi_size_t clip_rect_bytes, int32_t* clip_rect,
      nacl_abi_size_t amount_bytes, int32_t* amount);
  static NaClSrpcError PPB_Graphics2D_ReplaceContents(
      NaClSrpcChannel* channel,
      int64_t graphics_2d,
      int64_t image);

 private:
  PpbGraphics2DRpcClient();
  PpbGraphics2DRpcClient(const PpbGraphics2DRpcClient&);
  void operator=(const PpbGraphics2DRpcClient);
};  // class PpbGraphics2DRpcClient

class PpbImageDataRpcClient {
 public:
  static NaClSrpcError PPB_ImageData_GetNativeImageDataFormat(
      NaClSrpcChannel* channel,
      int32_t* format);
  static NaClSrpcError PPB_ImageData_IsImageDataFormatSupported(
      NaClSrpcChannel* channel,
      int32_t format,
      int32_t* success);
  static NaClSrpcError PPB_ImageData_Create(
      NaClSrpcChannel* channel,
      int64_t module,
      int32_t format,
      nacl_abi_size_t size_bytes, int32_t* size,
      int32_t init_to_zero,
      int64_t* resource);
  static NaClSrpcError PPB_ImageData_IsImageData(
      NaClSrpcChannel* channel,
      int64_t resource,
      int32_t* success);
  static NaClSrpcError PPB_ImageData_Describe(
      NaClSrpcChannel* channel,
      int64_t resource,
      nacl_abi_size_t* desc_bytes, int32_t* desc,
      int32_t* success);

 private:
  PpbImageDataRpcClient();
  PpbImageDataRpcClient(const PpbImageDataRpcClient&);
  void operator=(const PpbImageDataRpcClient);
};  // class PpbImageDataRpcClient



#endif  // GEN_PPAPI_PROXY_PPB_RPC_H_

