// Copyright 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_buffer.h"

#include <stdio.h>
#include <string.h>
#include "srpcgen/ppb_rpc.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/dev/ppb_buffer_dev.h"

namespace ppapi_proxy {

namespace {
PP_Resource Create(PP_Module module, uint32_t size_in_bytes) {
  UNREFERENCED_PARAMETER(module);
  UNREFERENCED_PARAMETER(size_in_bytes);
  return kInvalidResourceId;
}

PP_Bool IsBuffer(PP_Resource resource) {
  return PluginResource::GetAs<PluginBuffer>(resource).get()
      ? PP_TRUE : PP_FALSE;
}

PP_Bool Describe(PP_Resource resource, uint32_t* size_in_bytes) {
  UNREFERENCED_PARAMETER(resource);
  UNREFERENCED_PARAMETER(size_in_bytes);
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

const PPB_Buffer_Dev* PluginBuffer::GetInterface() {
  static const PPB_Buffer_Dev buffer_interface = {
    Create,
    IsBuffer,
    Describe,
    Map,
    Unmap,
  };
  return &buffer_interface;
}

}  // namespace ppapi_proxy
