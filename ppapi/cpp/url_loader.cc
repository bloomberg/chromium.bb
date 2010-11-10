// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/url_loader.h"

#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/common.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/file_ref_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"

namespace {

DeviceFuncs<PPB_URLLoader> url_loader_f(PPB_URLLOADER_INTERFACE);

}  // namespace

namespace pp {

URLLoader::URLLoader(PP_Resource resource) : Resource(resource) {
}

URLLoader::URLLoader(const Instance& instance) {
  if (!url_loader_f)
    return;
  PassRefFromConstructor(url_loader_f->Create(instance.pp_instance()));
}

URLLoader::URLLoader(const URLLoader& other) : Resource(other) {
}

int32_t URLLoader::Open(const URLRequestInfo& request_info,
                        const CompletionCallback& cc) {
  if (!url_loader_f)
    return PP_ERROR_NOINTERFACE;
  return url_loader_f->Open(pp_resource(), request_info.pp_resource(),
                            cc.pp_completion_callback());
}

int32_t URLLoader::FollowRedirect(const CompletionCallback& cc) {
  if (!url_loader_f)
    return PP_ERROR_NOINTERFACE;
  return url_loader_f->FollowRedirect(pp_resource(),
                                      cc.pp_completion_callback());
}

bool URLLoader::GetUploadProgress(int64_t* bytes_sent,
                                  int64_t* total_bytes_to_be_sent) const {
  if (!url_loader_f)
    return false;
  return PPBoolToBool(url_loader_f->GetUploadProgress(pp_resource(),
                                                      bytes_sent,
                                                      total_bytes_to_be_sent));
}

bool URLLoader::GetDownloadProgress(
    int64_t* bytes_received,
    int64_t* total_bytes_to_be_received) const {
  if (!url_loader_f)
    return false;
  return PPBoolToBool(
      url_loader_f->GetDownloadProgress(pp_resource(),
                                        bytes_received,
                                        total_bytes_to_be_received));
}

URLResponseInfo URLLoader::GetResponseInfo() const {
  if (!url_loader_f)
    return URLResponseInfo();
  return URLResponseInfo(URLResponseInfo::PassRef(),
                         url_loader_f->GetResponseInfo(pp_resource()));
}

int32_t URLLoader::ReadResponseBody(char* buffer,
                                    int32_t bytes_to_read,
                                    const CompletionCallback& cc) {
  if (!url_loader_f)
    return PP_ERROR_NOINTERFACE;
  return url_loader_f->ReadResponseBody(pp_resource(),
                                        buffer,
                                        bytes_to_read,
                                        cc.pp_completion_callback());
}

int32_t URLLoader::FinishStreamingToFile(const CompletionCallback& cc) {
  if (!url_loader_f)
    return PP_ERROR_NOINTERFACE;
  return url_loader_f->FinishStreamingToFile(pp_resource(),
                                             cc.pp_completion_callback());
}

void URLLoader::Close() {
  if (!url_loader_f)
    return;
  url_loader_f->Close(pp_resource());
}

}  // namespace pp
