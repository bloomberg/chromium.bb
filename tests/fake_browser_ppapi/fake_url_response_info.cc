// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/tests/fake_browser_ppapi/fake_url_response_info.h"

#include <stdio.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_var_deprecated.h"

#include "native_client/tests/fake_browser_ppapi/fake_file_ref.h"
#include "native_client/tests/fake_browser_ppapi/fake_resource.h"
#include "native_client/tests/fake_browser_ppapi/utility.h"

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"

using fake_browser_ppapi::DebugPrintf;
using ppapi_proxy::PluginVarDeprecated;

namespace fake_browser_ppapi {

namespace {

PP_Bool IsURLResponseInfo(PP_Resource resource_id) {
  DebugPrintf("URLRequestInfo::IsURLResponseInfo: resource_id=%"NACL_PRId32"\n",
              resource_id);
  NACL_UNIMPLEMENTED();
  return PP_FALSE;
}

PP_Var GetProperty(PP_Resource response_id,
                   PP_URLResponseProperty property) {
  DebugPrintf("URLRequestInfo::GetProperty: response_id=%"NACL_PRId32"\n",
              response_id);
  URLResponseInfo* response = GetResource(response_id)->AsURLResponseInfo();
  if (response == NULL)
    return PP_MakeUndefined();

  switch (property) {
    case PP_URLRESPONSEPROPERTY_URL:
      return PluginVarDeprecated::StringToPPVar(response->module_id(),
                                                response->url());
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

PP_Resource GetBodyAsFileRef(PP_Resource response_id) {
  DebugPrintf("URLRequestInfo::GetBodyAsFileRef: response_id=%"NACL_PRId32"\n",
              response_id);
  URLResponseInfo* response = GetResource(response_id)->AsURLResponseInfo();
  if (response == NULL)
    return Resource::Invalid()->resource_id();
  FileRef* file_ref = response->body_as_file_ref();
  CHECK(file_ref != NULL);
  return file_ref->resource_id();
}

}  // namespace


const PPB_URLResponseInfo* URLResponseInfo::GetInterface() {
  static const PPB_URLResponseInfo url_response_info_interface = {
    IsURLResponseInfo,
    GetProperty,
    GetBodyAsFileRef
  };
  return &url_response_info_interface;
}

}  // namespace fake_browser_ppapi
