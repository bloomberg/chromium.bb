// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/url_loader.h"

#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/file_ref_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_URLLoader>() {
  return PPB_URLLOADER_INTERFACE;
}

}  // namespace

URLLoader::URLLoader(PP_Resource resource) : Resource(resource) {
}

// TODO(brettw) remove this when NaCl is updated.
URLLoader::URLLoader(const Instance& instance) {
  if (!has_interface<PPB_URLLoader>())
    return;
  PassRefFromConstructor(get_interface<PPB_URLLoader>()->Create(
      instance.pp_instance()));
}

URLLoader::URLLoader(Instance* instance) {
  if (!has_interface<PPB_URLLoader>())
    return;
  PassRefFromConstructor(get_interface<PPB_URLLoader>()->Create(
      instance->pp_instance()));
}

URLLoader::URLLoader(const URLLoader& other) : Resource(other) {
}

int32_t URLLoader::Open(const URLRequestInfo& request_info,
                        const CompletionCallback& cc) {
  if (!has_interface<PPB_URLLoader>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_URLLoader>()->Open(pp_resource(),
                                              request_info.pp_resource(),
                                              cc.pp_completion_callback());
}

int32_t URLLoader::FollowRedirect(const CompletionCallback& cc) {
  if (!has_interface<PPB_URLLoader>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_URLLoader>()->FollowRedirect(
      pp_resource(), cc.pp_completion_callback());
}

bool URLLoader::GetUploadProgress(int64_t* bytes_sent,
                                  int64_t* total_bytes_to_be_sent) const {
  if (!has_interface<PPB_URLLoader>())
    return false;
  return PP_ToBool(get_interface<PPB_URLLoader>()->GetUploadProgress(
      pp_resource(), bytes_sent, total_bytes_to_be_sent));
}

bool URLLoader::GetDownloadProgress(
    int64_t* bytes_received,
    int64_t* total_bytes_to_be_received) const {
  if (!has_interface<PPB_URLLoader>())
    return false;
  return PP_ToBool(get_interface<PPB_URLLoader>()->GetDownloadProgress(
      pp_resource(), bytes_received, total_bytes_to_be_received));
}

URLResponseInfo URLLoader::GetResponseInfo() const {
  if (!has_interface<PPB_URLLoader>())
    return URLResponseInfo();
  return URLResponseInfo(URLResponseInfo::PassRef(),
                         get_interface<PPB_URLLoader>()->GetResponseInfo(
                             pp_resource()));
}

int32_t URLLoader::ReadResponseBody(void* buffer,
                                    int32_t bytes_to_read,
                                    const CompletionCallback& cc) {
  if (!has_interface<PPB_URLLoader>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_URLLoader>()->ReadResponseBody(
      pp_resource(), buffer, bytes_to_read, cc.pp_completion_callback());
}

int32_t URLLoader::FinishStreamingToFile(const CompletionCallback& cc) {
  if (!has_interface<PPB_URLLoader>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_URLLoader>()->FinishStreamingToFile(
      pp_resource(), cc.pp_completion_callback());
}

void URLLoader::Close() {
  if (!has_interface<PPB_URLLoader>())
    return;
  get_interface<PPB_URLLoader>()->Close(pp_resource());
}

}  // namespace pp
