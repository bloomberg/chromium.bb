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

namespace {

const PPB_ImageData* GetInterface() {
  static const PPB_ImageData* image_data_interface = NULL;

  if (image_data_interface == NULL) {
    image_data_interface = reinterpret_cast<const PPB_ImageData*>(
        ppapi_proxy::GetBrowserInterface(PPB_IMAGEDATA_INTERFACE));
  }
  if (image_data_interface == NULL) {
    ppapi_proxy::DebugPrintf("ERROR: attempt to get PPB_ImageData failed.\n");
  }
  CHECK(image_data_interface != NULL);
  return image_data_interface;
}

}  // namespace

void PpbImageDataRpcServer::PPB_ImageData_GetNativeImageDataFormat(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t* format) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  PP_ImageDataFormat pp_format = GetInterface()->GetNativeImageDataFormat();
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
      GetInterface()->IsImageDataFormatSupported(
          static_cast<PP_ImageDataFormat>(format));
  *success = static_cast<int32_t>(pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbImageDataRpcServer::PPB_ImageData_Create(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int64_t module,
    int32_t format,
    nacl_abi_size_t size_bytes, int32_t* size,
    int32_t init_to_zero,
    int64_t* resource) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (size_bytes != sizeof(struct PP_Size)) {
    return;
  }
  PP_Resource pp_resource =
      GetInterface()->Create(static_cast<PP_Module>(module),
                             static_cast<PP_ImageDataFormat>(format),
                             static_cast<const struct PP_Size*>(
                                 reinterpret_cast<struct PP_Size*>(size)),
                             (init_to_zero ? PP_TRUE : PP_FALSE));
  *resource = static_cast<int64_t>(pp_resource);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbImageDataRpcServer::PPB_ImageData_IsImageData(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int64_t resource,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  PP_Bool pp_success =
      GetInterface()->IsImageData(static_cast<PP_Resource>(resource));
  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbImageDataRpcServer::PPB_ImageData_Describe(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int64_t resource,
    nacl_abi_size_t* desc_bytes, int32_t* desc,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (*desc_bytes != sizeof(struct PP_ImageDataDesc)) {
    return;
  }
  PP_Bool pp_success =
      GetInterface()->Describe(
          static_cast<PP_Resource>(resource),
          reinterpret_cast<struct PP_ImageDataDesc*>(desc));
  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

// Map and Unmap are purely plugin-side methods, just using mmap/munmap.
