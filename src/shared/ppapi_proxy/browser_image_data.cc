// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/trusted/ppb_image_data_trusted.h"
#include "srpcgen/ppb_rpc.h"

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
  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbImageDataRpcServer::PPB_ImageData_Create(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Module module,
    int32_t format,
    nacl_abi_size_t size_bytes, char* size,
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
    nacl_abi_size_t* desc_bytes,
    char* desc,
    NaClSrpcImcDescType* shm,
    int32_t* shm_size,
    int32_t* success) {
  nacl::DescWrapperFactory factory;
  nacl::scoped_ptr<nacl::DescWrapper> desc_wrapper(factory.MakeInvalid());
  // IMPORTANT!
  // Make sure that the runner is destroyed before DescWrapper!
  // NaclDescs are refcounted. When the DescWrapper is destructed, it decreases
  // the refcount, and causes the Desc to be freed. The closure runner will send
  // the response in the destructor and segfault when trying to process a freed
  // Desc.
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (*desc_bytes != sizeof(struct PP_ImageDataDesc)) {
    return;
  }
  *shm = desc_wrapper->desc();
  *shm_size = -1;
  *success = PP_FALSE;
  PP_Bool pp_success =
      ppapi_proxy::PPBImageDataInterface()->Describe(
          resource, reinterpret_cast<struct PP_ImageDataDesc*>(desc));
  if (pp_success == PP_TRUE) {
    int native_handle = 0;
    uint32_t native_size = 0;
    if (ppapi_proxy::PPBImageDataTrustedInterface()->GetSharedMemory(
        static_cast<PP_Resource>(resource),
        &native_handle,
        &native_size) == PP_OK) {

#if NACL_LINUX
      desc_wrapper.reset(factory.ImportSysvShm(native_handle, native_size));
#else
      // Has to be a C-style cast because it's a reinterpret_cast on Windows
      // and a static_cast on Mac.
      desc_wrapper.reset(factory.ImportShmHandle((NaClHandle)native_handle,
                                                 native_size));
#endif
      *shm = desc_wrapper->desc();
      *shm_size = native_size;
      *success = PP_TRUE;
    }
  }
  rpc->result = NACL_SRPC_RESULT_OK;
}

// Map and Unmap are purely plugin-side methods, just using mmap/munmap.
