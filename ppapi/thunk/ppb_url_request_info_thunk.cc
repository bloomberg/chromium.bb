// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_url_request_info.idl modified Thu Mar 21 14:15:28 2013.

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/ppb_url_request_info_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateURLRequestInfo(instance);
}

PP_Bool IsURLRequestInfo(PP_Resource resource) {
  EnterResource<PPB_URLRequestInfo_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_Bool SetProperty(PP_Resource request,
                    PP_URLRequestProperty property,
                    struct PP_Var value) {
  EnterResource<PPB_URLRequestInfo_API> enter(request, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->SetProperty(property, value);
}

PP_Bool AppendDataToBody(PP_Resource request, const void* data, uint32_t len) {
  EnterResource<PPB_URLRequestInfo_API> enter(request, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->AppendDataToBody(data, len);
}

PP_Bool AppendFileToBody(PP_Resource request,
                         PP_Resource file_ref,
                         int64_t start_offset,
                         int64_t number_of_bytes,
                         PP_Time expected_last_modified_time) {
  EnterResource<PPB_URLRequestInfo_API> enter(request, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->AppendFileToBody(file_ref,
                                          start_offset,
                                          number_of_bytes,
                                          expected_last_modified_time);
}

const PPB_URLRequestInfo_1_0 g_ppb_urlrequestinfo_thunk_1_0 = {
  &Create,
  &IsURLRequestInfo,
  &SetProperty,
  &AppendDataToBody,
  &AppendFileToBody
};

}  // namespace

const PPB_URLRequestInfo_1_0* GetPPB_URLRequestInfo_1_0_Thunk() {
  return &g_ppb_urlrequestinfo_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
