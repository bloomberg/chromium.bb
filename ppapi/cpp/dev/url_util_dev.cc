// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/url_util_dev.h"

#include "ppapi/cpp/common.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"

namespace pp {

// static
const URLUtil_Dev* URLUtil_Dev::Get() {
  static bool tried_to_init = false;
  static URLUtil_Dev util;

  if (!tried_to_init) {
    tried_to_init = true;
    util.interface_ = static_cast<const PPB_URLUtil_Dev*>(
        Module::Get()->GetBrowserInterface(PPB_URLUTIL_DEV_INTERFACE));
  }

  if (!util.interface_)
    return NULL;
  return &util;
}

Var URLUtil_Dev::Canonicalize(const Var& url,
                              PP_URLComponents_Dev* components) const {
  return Var(Var::PassRef(),
             interface_->Canonicalize(url.pp_var(), components));
}

Var URLUtil_Dev::ResolveRelativeToURL(const Var& base_url,
                                      const Var& relative_string,
                                      PP_URLComponents_Dev* components) const {
  return Var(Var::PassRef(),
             interface_->ResolveRelativeToURL(base_url.pp_var(),
                                              relative_string.pp_var(),
                                              components));
}

Var URLUtil_Dev::ResolveRelativeToDocument(
    const Instance& instance,
    const Var& relative_string,
    PP_URLComponents_Dev* components) const {
  return Var(Var::PassRef(),
             interface_->ResolveRelativeToDocument(instance.pp_instance(),
                                                   relative_string.pp_var(),
                                                   components));
}

bool URLUtil_Dev::IsSameSecurityOrigin(const Var& url_a,
                                       const Var& url_b) const {
  return PPBoolToBool(interface_->IsSameSecurityOrigin(url_a.pp_var(),
                                                       url_b.pp_var()));
}

bool URLUtil_Dev::DocumentCanRequest(const Instance& instance,
                                     const Var& url) const {
  return PPBoolToBool(interface_->DocumentCanRequest(instance.pp_instance(),
                                                     url.pp_var()));
}

bool URLUtil_Dev::DocumentCanAccessDocument(const Instance& active,
                                            const Instance& target) const {
  return PPBoolToBool(
      interface_->DocumentCanAccessDocument(active.pp_instance(),
                                            target.pp_instance()));
}

Var URLUtil_Dev::GetDocumentURL(const Instance& instance,
                                PP_URLComponents_Dev* components) const {
  return Var(Var::PassRef(),
             interface_->GetDocumentURL(instance.pp_instance(), components));
}

}  // namespace pp
