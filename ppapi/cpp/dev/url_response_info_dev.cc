// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/url_response_info_dev.h"

#include "ppapi/cpp/dev/file_ref_dev.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace {

DeviceFuncs<PPB_URLResponseInfo_Dev> url_response_info_f(
    PPB_URLRESPONSEINFO_DEV_INTERFACE);

}  // namespace

namespace pp {

URLResponseInfo_Dev::URLResponseInfo_Dev(const URLResponseInfo_Dev& other)
    : Resource(other) {
}

URLResponseInfo_Dev::URLResponseInfo_Dev(PassRef, PP_Resource resource) {
  PassRefFromConstructor(resource);
}

URLResponseInfo_Dev& URLResponseInfo_Dev::operator=(
    const URLResponseInfo_Dev& other) {
  URLResponseInfo_Dev copy(other);
  swap(copy);
  return *this;
}

void URLResponseInfo_Dev::swap(URLResponseInfo_Dev& other) {
  Resource::swap(other);
}

Var URLResponseInfo_Dev::GetProperty(
    PP_URLResponseProperty_Dev property) const {
  if (!url_response_info_f)
    return Var();
  return Var(Var::PassRef(),
             url_response_info_f->GetProperty(pp_resource(), property));
}

FileRef_Dev URLResponseInfo_Dev::GetBody() const {
  if (!url_response_info_f)
    return FileRef_Dev();
  return FileRef_Dev(FileRef_Dev::PassRef(),
                     url_response_info_f->GetBody(pp_resource()));
}

}  // namespace pp
