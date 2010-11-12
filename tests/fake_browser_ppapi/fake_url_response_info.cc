/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/tests/fake_browser_ppapi/fake_url_response_info.h"

#include <stdio.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_var.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"

#include "native_client/tests/fake_browser_ppapi/fake_file_ref.h"
#include "native_client/tests/fake_browser_ppapi/fake_resource.h"

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PluginVar;

namespace fake_browser_ppapi {

namespace {

PP_Bool IsURLResponseInfo(PP_Resource resource_id) {
  DebugPrintf("URLRequestInfo::IsURLResponseInfo: resource_id=%"NACL_PRId64"\n",
              resource_id);
  NACL_UNIMPLEMENTED();
  return PP_FALSE;
}

PP_Var GetProperty(PP_Resource response_id,
                   PP_URLResponseProperty_Dev property) {
  DebugPrintf("URLRequestInfo::GetProperty: response_id=%"NACL_PRId64"\n",
              response_id);
  URLResponseInfo* response = GetResource(response_id)->AsURLResponseInfo();
  if (response == NULL)
    return PP_MakeUndefined();

  switch (property) {
    case PP_URLRESPONSEPROPERTY_URL:
      return PluginVar::StringToPPVar(response->module_id(), response->url());
    case PP_URLRESPONSEPROPERTY_STATUSCODE:
      return PP_MakeInt32(response->status_code());
    case PP_URLRESPONSEPROPERTY_HEADERS:
    case PP_URLRESPONSEPROPERTY_REDIRECTURL:
    case PP_URLRESPONSEPROPERTY_REDIRECTMETHOD:
    case PP_URLRESPONSEPROPERTY_STATUSLINE:
      NACL_UNIMPLEMENTED();
  }
  return PP_MakeUndefined();
}

PP_Resource GetBody(PP_Resource response_id) {
  DebugPrintf("URLRequestInfo::GetBody: response_id=%"NACL_PRId64"\n",
              response_id);
  URLResponseInfo* response = GetResource(response_id)->AsURLResponseInfo();
  if (response == NULL)
    return Resource::Invalid()->resource_id();
  FileRef* file_ref = response->body_as_file_ref();
  CHECK(file_ref != NULL);
  return file_ref->resource_id();
}

}  // namespace


const PPB_URLResponseInfo_Dev* URLResponseInfo::GetInterface() {
  static const PPB_URLResponseInfo_Dev url_response_info_interface = {
    IsURLResponseInfo,
    GetProperty,
    GetBody
  };
  return &url_response_info_interface;
}

}  // namespace fake_browser_ppapi
