// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "srpcgen/ppb_rpc.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"

//
// The following methods are the SRPC dispatchers for ppapi/c/ppb_image_data.h.
//

void PpbImageDataRpcServer::PPB_ImageData_GetNativeImageDataFormat(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t* format) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  PP_ImageDataFormat pp_format =
      ppapi_proxy::PPBImageDataInterface()->GetNativeImageDataFormat();
  *format = static_cast<int32_t>(pp_format);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbImageDataRpcServer::PPB_ImageData_IsImageDataFormatSupported(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t format,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  PP_Bool pp_success =
      ppapi_proxy::PPBImageDataInterface()->IsImageDataFormatSupported(
          static_cast<PP_ImageDataFormat>(format));
  *success = static_cast<int32_t>(pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbImageDataRpcServer::PPB_ImageData_Create(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Module module,
    int32_t format,
    nacl_abi_size_t size_bytes, int32_t* size,
    int32_t init_to_zero,
    PP_Resource* resource) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (size_bytes != sizeof(struct PP_Size)) {
    return;
  }
  *resource = ppapi_proxy::PPBImageDataInterface()->Create(
      module,
      static_cast<PP_ImageDataFormat>(format),
      static_cast<const struct PP_Size*>(
          reinterpret_cast<struct PP_Size*>(size)),
      (init_to_zero ? PP_TRUE : PP_FALSE));
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbImageDataRpcServer::PPB_ImageData_IsImageData(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  PP_Bool pp_success =
      ppapi_proxy::PPBImageDataInterface()->IsImageData(resource);
  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbImageDataRpcServer::PPB_ImageData_Describe(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    nacl_abi_size_t* desc_bytes, int32_t* desc,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (*desc_bytes != sizeof(struct PP_ImageDataDesc)) {
    return;
  }
  PP_Bool pp_success =
      ppapi_proxy::PPBImageDataInterface()->Describe(
          resource, reinterpret_cast<struct PP_ImageDataDesc*>(desc));
  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

// Map and Unmap are purely plugin-side methods, just using mmap/munmap.
