// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_ImageData functions.

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


using ppapi_proxy::DebugPrintf;

void PpbImageDataRpcServer::PPB_ImageData_GetNativeImageDataFormat(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t* format) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  PP_ImageDataFormat pp_format =
      ppapi_proxy::PPBImageDataInterface()->GetNativeImageDataFormat();
  *format = static_cast<int32_t>(pp_format);
  DebugPrintf("PPB_ImageData::GetNativeImageDataFormat: "
              "format=%"NACL_PRId32"\n", *format);
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
  DebugPrintf("PPB_ImageData::IsImageDataFormatSupported: "
              "format=%"NACL_PRId32", success=%"NACL_PRId32"\n",
              format, *success);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbImageDataRpcServer::PPB_ImageData_Create(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t format,
    nacl_abi_size_t size_bytes, char* size,
    int32_t init_to_zero,
    PP_Resource* resource) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (size_bytes != sizeof(struct PP_Size)) {
    return;
  }
  PP_Size pp_size = *(reinterpret_cast<struct PP_Size*>(size));
  *resource = ppapi_proxy::PPBImageDataInterface()->Create(
      instance,
      static_cast<PP_ImageDataFormat>(format),
      &pp_size,
      (init_to_zero ? PP_TRUE : PP_FALSE));
  DebugPrintf("PPB_ImageData::Create: format=%"NACL_PRId32", "
              "size=(%"NACL_PRId32", %"NACL_PRId32"), "
              "init_to_zero=%"NACL_PRId32", "
              "resource=%"NACL_PRIu32"\n",
              format, pp_size.width, pp_size.height,
              init_to_zero, *resource);
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
  DebugPrintf("PPB_ImageData::IsImageData: resource=%"NACL_PRIu32", "
              "success=%"NACL_PRId32"\n", resource, *success);
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
      *shm = desc_wrapper->desc();
      *shm_size = native_size;
      *success = PP_TRUE;
#elif NACL_WINDOWS
      HANDLE dup_handle;
      if (DuplicateHandle(GetCurrentProcess(),
                          reinterpret_cast<NaClHandle>(native_handle),
                          GetCurrentProcess(),
                          &dup_handle,
                          0,
                          FALSE,
                          DUPLICATE_SAME_ACCESS)) {
        desc_wrapper.reset(factory.ImportShmHandle(dup_handle, native_size));
        *shm = desc_wrapper->desc();
        *shm_size = native_size;
        *success = PP_TRUE;
      }
#else
      int dup_handle = dup(static_cast<int>(native_handle));
      if (dup_handle >= 0) {
        desc_wrapper.reset(factory.ImportShmHandle(dup_handle, native_size));
        *shm = desc_wrapper->desc();
        *shm_size = native_size;
        *success = PP_TRUE;
      }
#endif
    }
  }
  DebugPrintf("PPB_ImageData::Describe: resource=%"NACL_PRIu32", "
              "success=%"NACL_PRId32"\n", resource, *success);
  rpc->result = NACL_SRPC_RESULT_OK;
}

// Map and Unmap are purely plugin-side methods, just using mmap/munmap.
