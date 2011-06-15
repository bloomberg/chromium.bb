// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/tests/fake_browser_ppapi/fake_url_request_info.h"

#include <string>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_var_deprecated.h"

#include "native_client/tests/fake_browser_ppapi/fake_resource.h"
#include "native_client/tests/fake_browser_ppapi/utility.h"

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"

using ppapi_proxy::PluginVarDeprecated;
using fake_browser_ppapi::DebugPrintf;

namespace fake_browser_ppapi {

namespace {

PP_Resource Create(PP_Instance instance_id) {
  DebugPrintf("URLRequestInfo::Create: instance_id=%"NACL_PRId32"\n",
              instance_id);
  URLRequestInfo* request = new URLRequestInfo(instance_id);
  PP_Resource resource_id = TrackResource(request);
  DebugPrintf("URLRequestInfo::Create: resource_id=%"NACL_PRId32"\n",
              resource_id);
  return resource_id;
}

PP_Bool IsURLRequestInfo(PP_Resource resource_id) {
  DebugPrintf("URLRequestInfo::IsURLRequestInfo: resource_id=%"NACL_PRId32"\n",
              resource_id);
  UNREFERENCED_PARAMETER(resource_id);
  NACL_UNIMPLEMENTED();
  return PP_FALSE;
}

PP_Bool SetProperty(PP_Resource request_id,
                    PP_URLRequestProperty property,
                    struct PP_Var value) {
  DebugPrintf("URLRequestInfo::SetProperty: request_id=%"NACL_PRId32
              " property=%d value.type=%d\n", request_id, property, value.type);

  URLRequestInfo* request = GetResource(request_id)->AsURLRequestInfo();
  if (request == NULL)
    return PP_FALSE;

  if (value.type == PP_VARTYPE_BOOL) {
    switch (property) {
      case PP_URLREQUESTPROPERTY_STREAMTOFILE:
        request->set_stream_to_file(value.value.as_bool == PP_TRUE);
        return PP_TRUE;
      case PP_URLREQUESTPROPERTY_URL:
      case PP_URLREQUESTPROPERTY_METHOD:
      case PP_URLREQUESTPROPERTY_HEADERS:
        NACL_NOTREACHED();
        return PP_FALSE;
      case PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS:
      case PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS:
      case PP_URLREQUESTPROPERTY_CUSTOMCONTENTTRANSFERENCODING:
      case PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL:
      case PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS:
      case PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS:
      case PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGRESS:
      case PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD:
      case PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERTHRESHOLD:
        NACL_UNIMPLEMENTED();
        return PP_FALSE;
    }
  }

  if (value.type == PP_VARTYPE_STRING) {
    std::string str = PluginVarDeprecated::PPVarToString(value);
    CHECK(str != "");
    switch (property) {
      case PP_URLREQUESTPROPERTY_URL:
        request->set_url(str);
        return PP_TRUE;
      case PP_URLREQUESTPROPERTY_METHOD:
        request->set_method(str);
        return PP_TRUE;
      case PP_URLREQUESTPROPERTY_HEADERS:
        request->set_headers(str);
        return PP_TRUE;
      case PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS:
      case PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS:
      case PP_URLREQUESTPROPERTY_CUSTOMCONTENTTRANSFERENCODING:
      case PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL:
      case PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS:
      case PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS:
      case PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGRESS:
      case PP_URLREQUESTPROPERTY_STREAMTOFILE:
      case PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD:
      case PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERTHRESHOLD:
        NACL_NOTREACHED();
        return PP_FALSE;
    }
  }

  return PP_FALSE;
}

PP_Bool AppendDataToBody(PP_Resource request_id,
                         const void* data,
                         uint32_t len) {
  DebugPrintf("URLRequestInfo::AppendDataToBody: request_id=%"NACL_PRId32"\n",
              request_id);
  UNREFERENCED_PARAMETER(data);
  UNREFERENCED_PARAMETER(len);
  NACL_UNIMPLEMENTED();
  return PP_FALSE;
}

PP_Bool AppendFileToBody(PP_Resource request_id,
                         PP_Resource file_ref,
                         int64_t start_offset,
                         int64_t number_of_bytes,
                         PP_Time expected_last_modified_time) {
  DebugPrintf("URLRequestInfo::AppendFileToBody: request_id=%"NACL_PRId32"\n",
              request_id);
  UNREFERENCED_PARAMETER(file_ref);
  UNREFERENCED_PARAMETER(start_offset);
  UNREFERENCED_PARAMETER(number_of_bytes);
  UNREFERENCED_PARAMETER(expected_last_modified_time);
  NACL_UNIMPLEMENTED();
  return PP_FALSE;
}

}  // namespace


const PPB_URLRequestInfo* URLRequestInfo::GetInterface() {
  static const PPB_URLRequestInfo url_request_info_interface = {
    Create,
    IsURLRequestInfo,
    SetProperty,
    AppendDataToBody,
    AppendFileToBody
  };
  return &url_request_info_interface;
}

}  // namespace fake_browser_ppapi
