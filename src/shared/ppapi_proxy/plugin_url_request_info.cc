/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/plugin_url_request_info.h"

#include <stdio.h>
#include <string.h>
#include "gen/native_client/src/shared/ppapi_proxy/ppb_rpc.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/dev/ppb_url_request_info_dev.h"

namespace ppapi_proxy {

namespace {
PP_Resource Create(PP_Module module) {
  UNREFERENCED_PARAMETER(module);
  return kInvalidResourceId;
}

PP_Bool IsURLRequestInfo(PP_Resource resource) {
  UNREFERENCED_PARAMETER(resource);
  return PP_FALSE;
}

PP_Bool SetProperty(PP_Resource request,
                 PP_URLRequestProperty_Dev property,
                 struct PP_Var value) {
  UNREFERENCED_PARAMETER(request);
  UNREFERENCED_PARAMETER(property);
  UNREFERENCED_PARAMETER(value);
  return PP_FALSE;
}

PP_Bool AppendDataToBody(PP_Resource request, const char* data, uint32_t len) {
  UNREFERENCED_PARAMETER(request);
  UNREFERENCED_PARAMETER(data);
  UNREFERENCED_PARAMETER(len);
  return PP_FALSE;
}

PP_Bool AppendFileToBody(PP_Resource request,
                      PP_Resource file_ref,
                      int64_t start_offset,
                      int64_t number_of_bytes,
                      PP_Time expected_last_modified_time) {
  UNREFERENCED_PARAMETER(request);
  UNREFERENCED_PARAMETER(file_ref);
  UNREFERENCED_PARAMETER(start_offset);
  UNREFERENCED_PARAMETER(number_of_bytes);
  UNREFERENCED_PARAMETER(expected_last_modified_time);
  return PP_FALSE;
}
}  // namespace

const PPB_URLRequestInfo_Dev* PluginURLRequestInfo::GetInterface() {
  static const PPB_URLRequestInfo_Dev intf = {
    Create,
    IsURLRequestInfo,
    SetProperty,
    AppendDataToBody,
    AppendFileToBody,
  };
  return &intf;
}

}  // namespace ppapi_proxy
