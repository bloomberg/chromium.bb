// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_url_response_info.h"

#include "base/logging.h"
#include "third_party/ppapi/c/pp_var.h"

namespace pepper {

namespace {

bool IsURLResponseInfo(PP_Resource resource) {
  return !!Resource::GetAs<URLResponseInfo>(resource).get();
}

PP_Var GetProperty(PP_Resource response_id,
                   PP_URLResponseProperty property) {
  scoped_refptr<URLResponseInfo> response(
      Resource::GetAs<URLResponseInfo>(response_id));
  if (!response.get())
    return PP_MakeVoid();

  return response->GetProperty(property);
}

PP_Resource GetBody(PP_Resource response_id) {
  NOTIMPLEMENTED();  // TODO(darin): Implement me!
  return 0;
}

const PPB_URLResponseInfo ppb_urlresponseinfo = {
  &IsURLResponseInfo,
  &GetProperty,
  &GetBody
};

}  // namespace

URLResponseInfo::URLResponseInfo(PluginModule* module)
    : Resource(module) {
}

URLResponseInfo::~URLResponseInfo() {
}

// static
const PPB_URLResponseInfo* URLResponseInfo::GetInterface() {
  return &ppb_urlresponseinfo;
}

PP_Var URLResponseInfo::GetProperty(PP_URLResponseProperty property) {
  NOTIMPLEMENTED();  // TODO(darin): Implement me!
  return PP_MakeVoid();
}

}  // namespace pepper
