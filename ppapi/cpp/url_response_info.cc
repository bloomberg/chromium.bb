// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/url_response_info.h"

#include "ppapi/cpp/dev/file_ref_dev.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_URLResponseInfo>() {
  return PPB_URLRESPONSEINFO_INTERFACE;
}

}  // namespace

URLResponseInfo::URLResponseInfo(const URLResponseInfo& other)
    : Resource(other) {
}

URLResponseInfo::URLResponseInfo(PassRef, PP_Resource resource) {
  PassRefFromConstructor(resource);
}

Var URLResponseInfo::GetProperty(PP_URLResponseProperty property) const {
  if (!has_interface<PPB_URLResponseInfo>())
    return Var();
  return Var(Var::PassRef(),
             get_interface<PPB_URLResponseInfo>()->GetProperty(pp_resource(),
                                                               property));
}

FileRef_Dev URLResponseInfo::GetBodyAsFileRef() const {
  if (!has_interface<PPB_URLResponseInfo>())
    return FileRef_Dev();
  return FileRef_Dev(FileRef_Dev::PassRef(),
                     get_interface<PPB_URLResponseInfo>()->GetBodyAsFileRef(
                         pp_resource()));
}

}  // namespace pp
