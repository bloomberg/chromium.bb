// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/common.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_url_loader_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateURLLoader(instance);
}

PP_Bool IsURLLoader(PP_Resource resource) {
  EnterResource<PPB_URLLoader_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Open(PP_Resource loader,
             PP_Resource request_id,
             PP_CompletionCallback callback) {
  EnterResource<PPB_URLLoader_API> enter(loader, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Open(request_id, callback);
  return MayForceCallback(callback, result);
}

int32_t FollowRedirect(PP_Resource loader,
                       PP_CompletionCallback callback) {
  EnterResource<PPB_URLLoader_API> enter(loader, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->FollowRedirect(callback);
  return MayForceCallback(callback, result);
}

PP_Bool GetUploadProgress(PP_Resource loader,
                          int64_t* bytes_sent,
                          int64_t* total_bytes_to_be_sent) {
  EnterResource<PPB_URLLoader_API> enter(loader, true);
  if (enter.failed()) {
    *bytes_sent = 0;
    *total_bytes_to_be_sent = 0;
    return PP_FALSE;
  }
  return enter.object()->GetUploadProgress(bytes_sent,
                                           total_bytes_to_be_sent);
}

PP_Bool GetDownloadProgress(PP_Resource loader,
                            int64_t* bytes_received,
                            int64_t* total_bytes_to_be_received) {
  EnterResource<PPB_URLLoader_API> enter(loader, true);
  if (enter.failed()) {
    *bytes_received = 0;
    *total_bytes_to_be_received = 0;
    return PP_FALSE;
  }
  return enter.object()->GetDownloadProgress(bytes_received,
                                             total_bytes_to_be_received);
}

PP_Resource GetResponseInfo(PP_Resource loader) {
  EnterResource<PPB_URLLoader_API> enter(loader, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetResponseInfo();
}

int32_t ReadResponseBody(PP_Resource loader,
                         void* buffer,
                         int32_t bytes_to_read,
                         PP_CompletionCallback callback) {
  EnterResource<PPB_URLLoader_API> enter(loader, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->ReadResponseBody(buffer, bytes_to_read,
                                                    callback);
  return MayForceCallback(callback, result);
}

int32_t FinishStreamingToFile(PP_Resource loader,
                              PP_CompletionCallback callback) {
  EnterResource<PPB_URLLoader_API> enter(loader, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->FinishStreamingToFile(callback);
  return MayForceCallback(callback, result);
}

void Close(PP_Resource loader) {
  EnterResource<PPB_URLLoader_API> enter(loader, true);
  if (enter.succeeded())
    enter.object()->Close();
}

void GrantUniversalAccess(PP_Resource loader) {
  EnterResource<PPB_URLLoader_API> enter(loader, true);
  if (enter.succeeded())
    enter.object()->GrantUniversalAccess();
}

void SetStatusCallback(PP_Resource loader,
                       PP_URLLoaderTrusted_StatusCallback cb) {
  EnterResource<PPB_URLLoader_API> enter(loader, true);
  if (enter.succeeded())
    enter.object()->SetStatusCallback(cb);
}

const PPB_URLLoader g_ppb_urlloader_thunk = {
  &Create,
  &IsURLLoader,
  &Open,
  &FollowRedirect,
  &GetUploadProgress,
  &GetDownloadProgress,
  &GetResponseInfo,
  &ReadResponseBody,
  &FinishStreamingToFile,
  &Close
};

const PPB_URLLoaderTrusted g_ppb_urlloader_trusted_thunk = {
  &GrantUniversalAccess,
  &SetStatusCallback
};

}  // namespace

const PPB_URLLoader_1_0* GetPPB_URLLoader_1_0_Thunk() {
  return &g_ppb_urlloader_thunk;
}

const PPB_URLLoaderTrusted_0_3* GetPPB_URLLoaderTrusted_0_3_Thunk() {
  return &g_ppb_urlloader_trusted_thunk;
}

}  // namespace thunk
}  // namespace ppapi
