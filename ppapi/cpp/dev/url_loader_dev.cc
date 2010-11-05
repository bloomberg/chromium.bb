// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/url_loader_dev.h"

#include "ppapi/c/dev/ppb_url_loader_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/common.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/file_ref_dev.h"
#include "ppapi/cpp/dev/url_request_info_dev.h"
#include "ppapi/cpp/dev/url_response_info_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace {

DeviceFuncs<PPB_URLLoader_Dev> url_loader_f(PPB_URLLOADER_DEV_INTERFACE);

}  // namespace

namespace pp {

URLLoader_Dev::URLLoader_Dev(PP_Resource resource) : Resource(resource) {
}

URLLoader_Dev::URLLoader_Dev(const Instance& instance) {
  if (!url_loader_f)
    return;
  PassRefFromConstructor(url_loader_f->Create(instance.pp_instance()));
}

URLLoader_Dev::URLLoader_Dev(const URLLoader_Dev& other)
    : Resource(other) {
}

URLLoader_Dev& URLLoader_Dev::operator=(const URLLoader_Dev& other) {
  URLLoader_Dev copy(other);
  swap(copy);
  return *this;
}

void URLLoader_Dev::swap(URLLoader_Dev& other) {
  Resource::swap(other);
}

int32_t URLLoader_Dev::Open(const URLRequestInfo_Dev& request_info,
                            const CompletionCallback& cc) {
  if (!url_loader_f)
    return PP_ERROR_NOINTERFACE;
  return url_loader_f->Open(pp_resource(), request_info.pp_resource(),
                            cc.pp_completion_callback());
}

int32_t URLLoader_Dev::FollowRedirect(const CompletionCallback& cc) {
  if (!url_loader_f)
    return PP_ERROR_NOINTERFACE;
  return url_loader_f->FollowRedirect(pp_resource(),
                                      cc.pp_completion_callback());
}

bool URLLoader_Dev::GetUploadProgress(int64_t* bytes_sent,
                                      int64_t* total_bytes_to_be_sent) const {
  if (!url_loader_f)
    return false;
  return PPBoolToBool(url_loader_f->GetUploadProgress(pp_resource(),
                                                      bytes_sent,
                                                      total_bytes_to_be_sent));
}

bool URLLoader_Dev::GetDownloadProgress(
    int64_t* bytes_received,
    int64_t* total_bytes_to_be_received) const {
  if (!url_loader_f)
    return false;
  return PPBoolToBool(
      url_loader_f->GetDownloadProgress(pp_resource(),
                                        bytes_received,
                                        total_bytes_to_be_received));
}

URLResponseInfo_Dev URLLoader_Dev::GetResponseInfo() const {
  if (!url_loader_f)
    return URLResponseInfo_Dev();
  return URLResponseInfo_Dev(URLResponseInfo_Dev::PassRef(),
                             url_loader_f->GetResponseInfo(pp_resource()));
}

int32_t URLLoader_Dev::ReadResponseBody(char* buffer,
                                        int32_t bytes_to_read,
                                        const CompletionCallback& cc) {
  if (!url_loader_f)
    return PP_ERROR_NOINTERFACE;
  return url_loader_f->ReadResponseBody(pp_resource(),
                                        buffer,
                                        bytes_to_read,
                                        cc.pp_completion_callback());
}

int32_t URLLoader_Dev::FinishStreamingToFile(const CompletionCallback& cc) {
  if (!url_loader_f)
    return PP_ERROR_NOINTERFACE;
  return url_loader_f->FinishStreamingToFile(pp_resource(),
                                             cc.pp_completion_callback());
}

void URLLoader_Dev::Close() {
  if (!url_loader_f)
    return;
  url_loader_f->Close(pp_resource());
}

}  // namespace pp
