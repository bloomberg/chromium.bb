/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/plugin_image_data.h"

#include <stdio.h>
#include <string.h>
#include "srpcgen/ppb_rpc.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/ppb_image_data.h"

namespace ppapi_proxy {

namespace {

const nacl_abi_size_t kPpImageDataDescBytes =
      static_cast<nacl_abi_size_t>(sizeof(struct PP_ImageDataDesc));

PP_ImageDataFormat GetNativeImageDataFormat() {
  int32_t format;
  NaClSrpcError retval =
      PpbImageDataRpcClient::PPB_ImageData_GetNativeImageDataFormat(
          GetMainSrpcChannel(),
          &format);
  if (retval == NACL_SRPC_RESULT_OK) {
    return static_cast<PP_ImageDataFormat>(format);
  } else {
    return PP_IMAGEDATAFORMAT_BGRA_PREMUL;
  }
}

PP_Bool IsImageDataFormatSupported(PP_ImageDataFormat format) {
  int32_t result;
  NaClSrpcError retval =
      PpbImageDataRpcClient::PPB_ImageData_IsImageDataFormatSupported(
          GetMainSrpcChannel(),
          static_cast<int32_t>(format),
          &result);
  if (retval == NACL_SRPC_RESULT_OK) {
    return (result ? PP_TRUE : PP_FALSE);
  } else {
    return PP_FALSE;
  }
}

PP_Resource Create(PP_Module module,
                   PP_ImageDataFormat format,
                   const struct PP_Size* size,
                   PP_Bool init_to_zero) {
  int64_t resource;
  NaClSrpcError retval =
      PpbImageDataRpcClient::PPB_ImageData_Create(
          GetMainSrpcChannel(),
          static_cast<int64_t>(module),
          static_cast<int32_t>(format),
          static_cast<nacl_abi_size_t>(sizeof(struct PP_Size)),
          reinterpret_cast<int32_t*>(const_cast<struct PP_Size*>(size)),
          (init_to_zero == PP_TRUE),
          &resource);
  if (retval == NACL_SRPC_RESULT_OK) {
    return static_cast<PP_Resource>(resource);
  } else {
    return kInvalidResourceId;
  }
}

PP_Bool IsImageData(PP_Resource resource) {
  return PluginResource::GetAs<PluginImageData>(resource).get()
      ? PP_TRUE : PP_FALSE;
}

PP_Bool Describe(PP_Resource resource,
                 struct PP_ImageDataDesc* desc) {
  int32_t result;
  nacl_abi_size_t desc_size = kPpImageDataDescBytes;
  NaClSrpcError retval =
      PpbImageDataRpcClient::PPB_ImageData_Describe(
          GetMainSrpcChannel(),
          static_cast<int64_t>(resource),
          &desc_size,
          reinterpret_cast<int32_t*>(desc),
          &result);
  if (retval == NACL_SRPC_RESULT_OK) {
    return (result ? PP_TRUE : PP_FALSE);
  } else {
    return PP_FALSE;
  }
}

void* Map(PP_Resource resource) {
  UNREFERENCED_PARAMETER(resource);
  NACL_UNIMPLEMENTED();
  // TODO(sehr): add the implementation once we have a shared memory
  // descriptor for the resource.
  return NULL;
}

void Unmap(PP_Resource resource) {
  UNREFERENCED_PARAMETER(resource);
  NACL_UNIMPLEMENTED();
  // TODO(sehr): add the implementation once we have a shared memory
  // descriptor for the resource.
}

}  // namespace

const PPB_ImageData* PluginImageData::GetInterface() {
  static const PPB_ImageData intf = {
    GetNativeImageDataFormat,
    IsImageDataFormatSupported,
    Create,
    IsImageData,
    Describe,
    Map,
    Unmap,
  };
  return &intf;
}

}  // namespace ppapi_proxy
