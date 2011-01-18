/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/plugin_image_data.h"

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include "srpcgen/ppb_rpc.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/ppb_image_data.h"

namespace ppapi_proxy {

namespace {

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

PP_Resource Create(PP_Instance instance,
                   PP_ImageDataFormat format,
                   const struct PP_Size* size,
                   PP_Bool init_to_zero) {
  PP_Resource resource;
  NaClSrpcError retval =
      PpbImageDataRpcClient::PPB_ImageData_Create(
          GetMainSrpcChannel(),
          instance,
          static_cast<int32_t>(format),
          static_cast<nacl_abi_size_t>(sizeof(struct PP_Size)),
          reinterpret_cast<char*>(const_cast<struct PP_Size*>(size)),
          (init_to_zero == PP_TRUE),
          &resource);
  if (retval == NACL_SRPC_RESULT_OK) {
    scoped_refptr<PluginImageData> image_data =
        PluginResource::AdoptAs<PluginImageData>(resource);
    if (image_data.get()) {
      return resource;
    }
  }
  return kInvalidResourceId;
}

PP_Bool IsImageData(PP_Resource resource) {
  return PluginResource::GetAs<PluginImageData>(resource).get()
      ? PP_TRUE : PP_FALSE;
}

PP_Bool Describe(PP_Resource resource,
                 struct PP_ImageDataDesc* desc) {
  scoped_refptr<PluginImageData> imagedata =
      PluginResource::GetAs<PluginImageData>(resource);
  if (!imagedata.get()) {
    return PP_FALSE;
  }

  *desc = imagedata->desc();
  return PP_TRUE;
}

void* DoMap(PP_Resource resource) {
  scoped_refptr<PluginImageData> imagedata =
      PluginResource::GetAs<PluginImageData>(resource);

  return imagedata.get() ? imagedata->Map() : NULL;
}

void DoUnmap(PP_Resource resource) {
  scoped_refptr<PluginImageData> imagedata =
      PluginResource::GetAs<PluginImageData>(resource);
  if (imagedata.get())
    imagedata->Unmap();
}

}  // namespace

const PPB_ImageData* PluginImageData::GetInterface() {
  static const PPB_ImageData intf = {
    &GetNativeImageDataFormat,
    &IsImageDataFormatSupported,
    &Create,
    &IsImageData,
    &Describe,
    &DoMap,
    &DoUnmap,
  };
  return &intf;
}

PluginImageData::PluginImageData()
    : shm_size_(0),
      addr_(NULL) {
}

bool PluginImageData::InitFromBrowserResource(PP_Resource resource) {
  nacl_abi_size_t desc_size = static_cast<nacl_abi_size_t>(sizeof(desc_));
  int32_t success = PP_FALSE;

  NaClSrpcError result =
      PpbImageDataRpcClient::PPB_ImageData_Describe(
          GetMainSrpcChannel(),
          resource,
          &desc_size,
          reinterpret_cast<char*>(&desc_),
          &shm_fd_,
          &shm_size_,
          &success);
  return (NACL_SRPC_RESULT_OK == result) && (PP_TRUE == success);
}

void* PluginImageData::Map() {
  if (NULL != addr_) {
    return addr_;
  }
  if (!shm_size_) {
    return NULL;
  }

  addr_ = mmap(0, shm_size_, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
  return addr_;
}

void PluginImageData::Unmap() {
  if (addr_) {
    munmap(addr_, shm_size_);
    addr_ = NULL;
  }
}

}  // namespace ppapi_proxy
