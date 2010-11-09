/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/plugin_image_data.h"

#include <stdio.h>
#include <string.h>
#include "gen/native_client/src/shared/ppapi_proxy/ppb_rpc.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/ppb_image_data.h"

namespace ppapi_proxy {

namespace {
PP_ImageDataFormat GetNativeImageDataFormat() {
  return PP_IMAGEDATAFORMAT_BGRA_PREMUL;
}

PP_Bool IsImageDataFormatSupported(PP_ImageDataFormat format) {
  UNREFERENCED_PARAMETER(format);
  return PP_FALSE;
}

PP_Resource Create(PP_Module module,
                   PP_ImageDataFormat format,
                   const struct PP_Size* size,
                   PP_Bool init_to_zero) {
  UNREFERENCED_PARAMETER(module);
  UNREFERENCED_PARAMETER(format);
  UNREFERENCED_PARAMETER(size);
  UNREFERENCED_PARAMETER(init_to_zero);
  return kInvalidResourceId;
}

PP_Bool IsImageData(PP_Resource resource) {
  UNREFERENCED_PARAMETER(resource);
  return PP_FALSE;
}

PP_Bool Describe(PP_Resource resource,
              struct PP_ImageDataDesc* desc) {
  UNREFERENCED_PARAMETER(resource);
  UNREFERENCED_PARAMETER(desc);
  return PP_FALSE;
}

void* Map(PP_Resource resource) {
  UNREFERENCED_PARAMETER(resource);
  return NULL;
}

void Unmap(PP_Resource resource) {
  UNREFERENCED_PARAMETER(resource);
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
