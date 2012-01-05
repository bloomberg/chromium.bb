// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/ppb_url_util_shared.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Var ResolveRelativeToDocument(PP_Instance instance,
                                 PP_Var relative,
                                 PP_URLComponents_Dev* components) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.functions()->ResolveRelativeToDocument(instance, relative,
                                                      components);
}

PP_Bool DocumentCanRequest(PP_Instance instance, PP_Var url) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->DocumentCanRequest(instance, url);
}

PP_Bool DocumentCanAccessDocument(PP_Instance active, PP_Instance target) {
  EnterInstance enter(active);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->DocumentCanAccessDocument(active, target);
}

PP_Var GetDocumentURL(PP_Instance instance,
                      PP_URLComponents_Dev* components) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.functions()->GetDocumentURL(instance, components);
}

PP_Var GetPluginInstanceURL(PP_Instance instance,
                            PP_URLComponents_Dev* components) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.functions()->GetPluginInstanceURL(instance, components);
}

const PPB_URLUtil_Dev g_ppb_url_util = {
  &PPB_URLUtil_Shared::Canonicalize,
  &PPB_URLUtil_Shared::ResolveRelativeToURL,
  &ResolveRelativeToDocument,
  &PPB_URLUtil_Shared::IsSameSecurityOrigin,
  &DocumentCanRequest,
  &DocumentCanAccessDocument,
  &GetDocumentURL,
  &GetPluginInstanceURL
};

}  // namespace

const PPB_URLUtil_Dev_0_6* GetPPB_URLUtil_Dev_0_6_Thunk() {
  return &g_ppb_url_util;
}

}  // namespace thunk
}  // namespace ppapi
