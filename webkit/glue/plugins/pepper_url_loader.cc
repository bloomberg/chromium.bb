// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_url_loader.h"

#include "base/logging.h"
#include "third_party/ppapi/c/pp_completion_callback.h"
#include "third_party/ppapi/c/pp_errors.h"
#include "third_party/ppapi/c/ppb_url_loader.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_resource_tracker.h"
#include "webkit/glue/plugins/pepper_url_request_info.h"
#include "webkit/glue/plugins/pepper_url_response_info.h"

namespace pepper {

namespace {

PP_Resource Create(PP_Instance instance_id) {
  PluginInstance* instance = PluginInstance::FromPPInstance(instance_id);
  if (!instance)
    return 0;

  URLLoader* loader = new URLLoader(instance);
  loader->AddRef();  // AddRef for the caller.

  return loader->GetResource();
}

bool IsURLLoader(PP_Resource resource) {
  return !!ResourceTracker::Get()->GetAsURLLoader(resource).get();
}

int32_t Open(PP_Resource loader_id,
             PP_Resource request_id,
             PP_CompletionCallback callback) {
  scoped_refptr<URLLoader> loader(
      ResourceTracker::Get()->GetAsURLLoader(loader_id));
  if (!loader.get())
    return PP_Error_BadResource;

  scoped_refptr<URLRequestInfo> request(
      ResourceTracker::Get()->GetAsURLRequestInfo(request_id));
  if (!request.get())
    return PP_Error_BadResource;

  return loader->Open(request, callback);
}

int32_t FollowRedirect(PP_Resource loader_id,
                       PP_CompletionCallback callback) {
  scoped_refptr<URLLoader> loader(
      ResourceTracker::Get()->GetAsURLLoader(loader_id));
  if (!loader.get())
    return PP_Error_BadResource;

  return loader->FollowRedirect(callback);
}

bool GetUploadProgress(PP_Resource loader_id,
                       int64_t* bytes_sent,
                       int64_t* total_bytes_to_be_sent) {
  scoped_refptr<URLLoader> loader(
      ResourceTracker::Get()->GetAsURLLoader(loader_id));
  if (!loader.get())
    return false;

  *bytes_sent = loader->bytes_sent();
  *total_bytes_to_be_sent = loader->total_bytes_to_be_sent();
  return true;
}

bool GetDownloadProgress(PP_Resource loader_id,
                         int64_t* bytes_received,
                         int64_t* total_bytes_to_be_received) {
  scoped_refptr<URLLoader> loader(
      ResourceTracker::Get()->GetAsURLLoader(loader_id));
  if (!loader.get())
    return false;

  *bytes_received = loader->bytes_received();
  *total_bytes_to_be_received = loader->total_bytes_to_be_received();
  return true;
}

PP_Resource GetResponseInfo(PP_Resource loader_id) {
  scoped_refptr<URLLoader> loader(
      ResourceTracker::Get()->GetAsURLLoader(loader_id));
  if (!loader.get())
    return 0;

  URLResponseInfo* response_info = loader->response_info();
  if (!response_info)
    return 0;
  response_info->AddRef();  // AddRef for the caller.

  return response_info->GetResource();
}

int32_t ReadResponseBody(PP_Resource loader_id,
                         char* buffer,
                         int32_t bytes_to_read,
                         PP_CompletionCallback callback) {
  scoped_refptr<URLLoader> loader(
      ResourceTracker::Get()->GetAsURLLoader(loader_id));
  if (!loader.get())
    return PP_Error_BadResource;

  return loader->ReadResponseBody(buffer, bytes_to_read, callback);
}

void Close(PP_Resource loader_id) {
  scoped_refptr<URLLoader> loader(
      ResourceTracker::Get()->GetAsURLLoader(loader_id));
  if (!loader.get())
    return;

  loader->Close();
}

const PPB_URLLoader ppb_urlloader = {
  &Create,
  &IsURLLoader,
  &Open,
  &FollowRedirect,
  &GetUploadProgress,
  &GetDownloadProgress,
  &GetResponseInfo,
  &ReadResponseBody,
  &Close
};

}  // namespace

URLLoader::URLLoader(PluginInstance* instance)
    : Resource(instance->module()),
      bytes_sent_(0),
      total_bytes_to_be_sent_(0),
      bytes_received_(0),
      total_bytes_to_be_received_(0) {
}

URLLoader::~URLLoader() {
}

int32_t URLLoader::Open(URLRequestInfo* request,
                        PP_CompletionCallback callback) {
  NOTIMPLEMENTED();  // TODO(darin): Implement me.
  return PP_Error_Failed;
}

int32_t URLLoader::FollowRedirect(PP_CompletionCallback callback) {
  NOTIMPLEMENTED();  // TODO(darin): Implement me.
  return PP_Error_Failed;
}

int32_t URLLoader::ReadResponseBody(char* buffer, int32_t bytes_to_read,
                                    PP_CompletionCallback callback) {
  NOTIMPLEMENTED();  // TODO(darin): Implement me.
  return PP_Error_Failed;
}

void URLLoader::Close() {
  NOTIMPLEMENTED();  // TODO(darin): Implement me.
}

// static
const PPB_URLLoader* URLLoader::GetInterface() {
  return &ppb_urlloader;
}

}  // namespace pepper
