// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_url_response_info_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool IsURLResponseInfo(PP_Resource resource) {
  EnterResource<PPB_URLResponseInfo_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_Var GetProperty(PP_Resource response, PP_URLResponseProperty property) {
  EnterResource<PPB_URLResponseInfo_API> enter(response, true);
  if (!enter.succeeded())
    return PP_MakeUndefined();
  return enter.object()->GetProperty(property);
}

PP_Resource GetBodyAsFileRef(PP_Resource response) {
  EnterResource<PPB_URLResponseInfo_API> enter(response, true);
  if (!enter.succeeded())
    return 0;
  return enter.object()->GetBodyAsFileRef();
}

const PPB_URLResponseInfo g_ppb_url_response_info_thunk = {
  &IsURLResponseInfo,
  &GetProperty,
  &GetBodyAsFileRef
};

}  // namespace

const PPB_URLResponseInfo_1_0* GetPPB_URLResponseInfo_1_0_Thunk() {
  return &g_ppb_url_response_info_thunk;
}

}  // namespace thunk
}  // namespace ppapi
