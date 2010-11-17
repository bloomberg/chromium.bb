/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/plugin_url_response_info.h"

#include <stdio.h>
#include <string.h>
#include "gen/native_client/src/shared/ppapi_proxy/ppb_rpc.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_url_response_info.h"

namespace ppapi_proxy {

namespace {
PP_Bool IsURLResponseInfo(PP_Resource resource) {
  UNREFERENCED_PARAMETER(resource);
  return PP_FALSE;
}

PP_Var GetProperty(PP_Resource response,
                   PP_URLResponseProperty property) {
  UNREFERENCED_PARAMETER(response);
  UNREFERENCED_PARAMETER(property);
  return PP_MakeUndefined();
}

PP_Resource GetBodyAsFileRef(PP_Resource response) {
  UNREFERENCED_PARAMETER(response);
  return kInvalidResourceId;
}
}  // namespace

const PPB_URLResponseInfo* PluginURLResponseInfo::GetInterface() {
  static const PPB_URLResponseInfo intf = {
    IsURLResponseInfo,
    GetProperty,
    GetBodyAsFileRef,
  };
  return &intf;
}

}  // namespace ppapi_proxy
