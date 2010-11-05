// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/url_util_dev.h"

#include "ppapi/cpp/common.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"

namespace pp {

// static
const UrlUtil_Dev* UrlUtil_Dev::Get() {
  static bool tried_to_init = false;
  static UrlUtil_Dev util;

  if (!tried_to_init) {
    tried_to_init = true;
    util.interface_ = static_cast<const PPB_UrlUtil_Dev*>(
        Module::Get()->GetBrowserInterface(PPB_URLUTIL_DEV_INTERFACE));
  }

  if (!util.interface_)
    return NULL;
  return &util;
}

Var UrlUtil_Dev::Canonicalize(const Var& url,
                              PP_UrlComponents_Dev* components) const {
  return Var(Var::PassRef(),
             interface_->Canonicalize(url.pp_var(), components));
}

Var UrlUtil_Dev::ResolveRelativeToUrl(const Var& base_url,
                                      const Var& relative_string,
                                      PP_UrlComponents_Dev* components) const {
  return Var(Var::PassRef(),
             interface_->ResolveRelativeToUrl(base_url.pp_var(),
                                              relative_string.pp_var(),
                                              components));
}

Var UrlUtil_Dev::ResoveRelativeToDocument(
    const Instance& instance,
    const Var& relative_string,
    PP_UrlComponents_Dev* components) const {
  return Var(Var::PassRef(),
             interface_->ResolveRelativeToDocument(instance.pp_instance(),
                                                   relative_string.pp_var(),
                                                   components));
}

bool UrlUtil_Dev::IsSameSecurityOrigin(const Var& url_a,
                                       const Var& url_b) const {
  return PPBoolToBool(interface_->IsSameSecurityOrigin(url_a.pp_var(),
                                                       url_b.pp_var()));
}

bool UrlUtil_Dev::DocumentCanRequest(const Instance& instance,
                                     const Var& url) const {
  return PPBoolToBool(interface_->DocumentCanRequest(instance.pp_instance(),
                                                     url.pp_var()));
}

bool UrlUtil_Dev::DocumentCanAccessDocument(const Instance& active,
                                            const Instance& target) const {
  return PPBoolToBool(
      interface_->DocumentCanAccessDocument(active.pp_instance(),
                                            target.pp_instance()));
}

}  // namespace pp
