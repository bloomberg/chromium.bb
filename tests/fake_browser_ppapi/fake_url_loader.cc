// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/tests/fake_browser_ppapi/fake_url_loader.h"

#include <string>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"

#include "native_client/tests/fake_browser_ppapi/fake_file_ref.h"
#include "native_client/tests/fake_browser_ppapi/fake_resource.h"
#include "native_client/tests/fake_browser_ppapi/fake_url_request_info.h"
#include "native_client/tests/fake_browser_ppapi/fake_url_response_info.h"
#include "native_client/tests/fake_browser_ppapi/utility.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"

using fake_browser_ppapi::DebugPrintf;

namespace fake_browser_ppapi {

std::string g_nacl_ppapi_url_path = NACL_NO_URL;
std::string g_nacl_ppapi_local_path = NACL_NO_FILE_PATH;

namespace {

PP_Resource Create(PP_Instance instance_id) {
  DebugPrintf("URLLoader::Create: instance_id=%"NACL_PRId32"\n", instance_id);
  URLLoader* loader = new URLLoader(instance_id);
  PP_Resource resource_id = TrackResource(loader);
  DebugPrintf("URLLoader::Create: resource_id=%"NACL_PRId32"\n", resource_id);
  return resource_id;
}

PP_Bool IsURLLoader(PP_Resource resource_id) {
  DebugPrintf("URLLoader::IsURLLoader: resource_id=%"NACL_PRId32"\n",
              resource_id);
  NACL_UNIMPLEMENTED();
  return PP_FALSE;
}

int32_t Open(PP_Resource loader_id,
             PP_Resource request_id,
             struct PP_CompletionCallback callback) {
  DebugPrintf("URLLoader::Open: loader_id=%"NACL_PRIu64
              " request_id=%"NACL_PRId32"\n", loader_id, request_id);

  URLLoader* loader = GetResource(loader_id)->AsURLLoader();
  URLRequestInfo* request = GetResource(request_id)->AsURLRequestInfo();
  if (loader == NULL || request == NULL)
    return PP_ERROR_BADRESOURCE;
  loader->set_request(request);

  // We use stream-as-file mode only, so Open will be aimed at that.
  CHECK(request->stream_to_file());

  // We fake-open the url. main.cc must provide the full url for this to work.
  if (g_nacl_ppapi_url_path == NACL_NO_URL)
    return PP_ERROR_FAILED;

  // Set up the response.
  URLResponseInfo* response = new URLResponseInfo(request->instance_id());
  if (request->url().find(g_nacl_ppapi_url_path) == 0) {
    // It was already an absolute URL.
    response->set_url(request->url());
  } else {
    response->set_url(g_nacl_ppapi_url_path + request->url());
  }
  response->set_status_code(NACL_HTTP_STATUS_OK);
  loader->set_response(response);
  PP_Resource response_id = TrackResource(response);
  DebugPrintf("URLLoader::Open: response_id=%"NACL_PRId32"\n", response_id);

  // Invoke the callback right away to simplify mocking.
  if (callback.func == NULL)
    return PP_ERROR_BADARGUMENT;
  PP_RunCompletionCallback(&callback, PP_OK);
  return PP_OK_COMPLETIONPENDING;  // Fake successful async call.
}

int32_t FollowRedirect(PP_Resource loader_id,
                       struct PP_CompletionCallback callback) {
  DebugPrintf("URLLoader::FollowRedirect: loader_id=%"NACL_PRId32"\n",
              loader_id);
  UNREFERENCED_PARAMETER(callback);
  NACL_UNIMPLEMENTED();
  return PP_ERROR_BADRESOURCE;
}

PP_Bool GetUploadProgress(PP_Resource loader_id,
                          int64_t* bytes_sent,
                          int64_t* total_bytes_to_be_sent) {
  DebugPrintf("URLLoader::GetUploadProgress: loader_id=%"NACL_PRId32"\n",
              loader_id);
  UNREFERENCED_PARAMETER(bytes_sent);
  UNREFERENCED_PARAMETER(total_bytes_to_be_sent);
  NACL_UNIMPLEMENTED();
  return PP_FALSE;
}

PP_Bool GetDownloadProgress(PP_Resource loader_id,
                            int64_t* bytes_received,
                            int64_t* total_bytes_to_be_received) {
  DebugPrintf("URLLoader::GetDownloadProgress: loader_id=%"NACL_PRId32"\n",
              loader_id);
  UNREFERENCED_PARAMETER(bytes_received);
  UNREFERENCED_PARAMETER(total_bytes_to_be_received);
  NACL_UNIMPLEMENTED();
  return PP_FALSE;
}

PP_Resource GetResponseInfo(PP_Resource loader_id) {
  DebugPrintf("URLLoader::GetResponseInfo: loader_id=%"NACL_PRId32"\n",
              loader_id);
  URLLoader* loader = GetResource(loader_id)->AsURLLoader();
  if (loader == NULL)
    return PP_ERROR_BADRESOURCE;

  URLResponseInfo* response = loader->response();
  CHECK(response != NULL);
  return response->resource_id();
}

int32_t ReadResponseBody(PP_Resource loader_id,
                         void* buffer,
                         int32_t bytes_to_read,
                         struct PP_CompletionCallback callback) {
  DebugPrintf("URLLoader::ReadResponseBody: loader_id=%"NACL_PRId32"\n",
              loader_id);
  UNREFERENCED_PARAMETER(buffer);
  UNREFERENCED_PARAMETER(bytes_to_read);
  UNREFERENCED_PARAMETER(callback);
  NACL_UNIMPLEMENTED();
  return PP_ERROR_BADRESOURCE;
}

int32_t FinishStreamingToFile(PP_Resource loader_id,
                              struct PP_CompletionCallback callback) {
  DebugPrintf("URLLoader::FinishStreamingToFile: loader_id=%"NACL_PRId32"\n",
              loader_id);

  URLLoader* loader = GetResource(loader_id)->AsURLLoader();
  if (loader == NULL)
    return PP_ERROR_BADRESOURCE;
  URLRequestInfo* request = loader->request();
  URLResponseInfo* response = loader->response();
  CHECK(request != NULL && response != NULL);

  // We fake-stream the file. main.cc must provide the path for this to work.
  // Note that will only work if embed uses a relative nexe url.
  // TODO(polina): generalize this to work with full urls as well?
  if (g_nacl_ppapi_local_path == NACL_NO_FILE_PATH)
    return PP_ERROR_FAILED;
  std::string local_file =
      g_nacl_ppapi_local_path + "/" +
      request->url().substr(g_nacl_ppapi_url_path.size() + 1);

  // Set up the the file object corresponding to the response body.
  FileRef* file_ref = new FileRef(local_file);
  response->set_body_as_file_ref(file_ref);
  PP_Resource file_ref_id = TrackResource(file_ref);
  DebugPrintf("URLLoader::FinishStreamingToFile: file_ref_id=%"NACL_PRId32"\n",
              file_ref_id);

  // Call the callback right away to simplify mocking.
  if (callback.func == NULL)
    return PP_ERROR_BADARGUMENT;
  PP_RunCompletionCallback(&callback, PP_OK);
  return PP_OK_COMPLETIONPENDING;  // Fake successful async call.
}

void Close(PP_Resource loader_id) {
  DebugPrintf("URLLoader::ReadResponseBody: loader_id=%"NACL_PRId32"\n",
              loader_id);
  NACL_UNIMPLEMENTED();
}

}  // namespace


const PPB_URLLoader* URLLoader::GetInterface() {
  static const PPB_URLLoader url_loader_interface = {
    Create,
    IsURLLoader,
    Open,
    FollowRedirect,
    GetUploadProgress,
    GetDownloadProgress,
    GetResponseInfo,
    ReadResponseBody,
    FinishStreamingToFile,
    Close
  };
  return &url_loader_interface;
}

}  // namespace fake_browser_ppapi
