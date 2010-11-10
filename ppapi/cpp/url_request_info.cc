// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/url_request_info.h"

#include "ppapi/cpp/common.h"
#include "ppapi/cpp/dev/file_ref_dev.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace {

DeviceFuncs<PPB_URLRequestInfo> url_request_info_f(
    PPB_URLREQUESTINFO_INTERFACE);

}  // namespace

namespace pp {

URLRequestInfo::URLRequestInfo() {
  if (!url_request_info_f)
    return;
  PassRefFromConstructor(
      url_request_info_f->Create(Module::Get()->pp_module()));
}

URLRequestInfo::URLRequestInfo(const URLRequestInfo& other)
    : Resource(other) {
}

bool URLRequestInfo::SetProperty(PP_URLRequestProperty property,
                                 const Var& value) {
  if (!url_request_info_f)
    return false;
  return PPBoolToBool(url_request_info_f->SetProperty(pp_resource(),
                                                      property,
                                                      value.pp_var()));
}

bool URLRequestInfo::AppendDataToBody(const char* data, uint32_t len) {
  if (!url_request_info_f)
    return false;
  return PPBoolToBool(url_request_info_f->AppendDataToBody(pp_resource(),
                                                           data,
                                                           len));
}

bool URLRequestInfo::AppendFileToBody(const FileRef_Dev& file_ref,
                                      PP_Time expected_last_modified_time) {
  if (!url_request_info_f)
    return false;
  return PPBoolToBool(
      url_request_info_f->AppendFileToBody(pp_resource(),
                                           file_ref.pp_resource(),
                                           0,
                                           -1,
                                           expected_last_modified_time));
}

bool URLRequestInfo::AppendFileRangeToBody(
    const FileRef_Dev& file_ref,
    int64_t start_offset,
    int64_t length,
    PP_Time expected_last_modified_time) {
  return PPBoolToBool(
      url_request_info_f->AppendFileToBody(pp_resource(),
                                           file_ref.pp_resource(),
                                           start_offset,
                                           length,
                                           expected_last_modified_time));
}

}  // namespace pp
