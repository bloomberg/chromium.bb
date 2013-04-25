// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_url_response_info.idl modified Thu Apr 25 13:21:08 2013.

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_url_response_info.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/ppb_url_response_info_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool IsURLResponseInfo(PP_Resource resource) {
  VLOG(4) << "PPB_URLResponseInfo::IsURLResponseInfo()";
  EnterResource<PPB_URLResponseInfo_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

struct PP_Var GetProperty(PP_Resource response,
                          PP_URLResponseProperty property) {
  VLOG(4) << "PPB_URLResponseInfo::GetProperty()";
  EnterResource<PPB_URLResponseInfo_API> enter(response, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetProperty(property);
}

PP_Resource GetBodyAsFileRef(PP_Resource response) {
  VLOG(4) << "PPB_URLResponseInfo::GetBodyAsFileRef()";
  EnterResource<PPB_URLResponseInfo_API> enter(response, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetBodyAsFileRef();
}

const PPB_URLResponseInfo_1_0 g_ppb_urlresponseinfo_thunk_1_0 = {
  &IsURLResponseInfo,
  &GetProperty,
  &GetBodyAsFileRef
};

}  // namespace

const PPB_URLResponseInfo_1_0* GetPPB_URLResponseInfo_1_0_Thunk() {
  return &g_ppb_urlresponseinfo_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
