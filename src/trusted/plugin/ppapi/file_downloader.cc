// Copyright 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "native_client/src/trusted/plugin/ppapi/file_downloader.h"

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/plugin/utility.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/dev/ppb_file_io_dev.h"
#include "ppapi/cpp/dev/file_ref_dev.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"

namespace plugin {

void FileDownloader::Initialize(const pp::Instance* instance) {
  PLUGIN_PRINTF(("FileDownloader::FileDownloader (this=%p)\n",
                 static_cast<void*>(this)));
  CHECK(instance != NULL);
  CHECK(instance_ == NULL);
  if (instance == NULL)
    return;
  if (instance_ != NULL)
    return;  // Can only initialize once.
  instance_ = instance;
  callback_factory_.Initialize(this);
}


bool FileDownloader::Open(const nacl::string& url,
                             const pp::CompletionCallback& callback) {
  CHECK(instance_ != NULL);
  if (instance_ == NULL)
    return false;  // Initialize() not called.
  url_ = url;
  file_open_notify_callback_ = callback;
  // Reset the url loader and file reader.
  // Note that we have the only refernce to the underlying objects, so
  // this will implicitly close any pending IO and destroy them.
  url_loader_ = pp::URLLoader(*instance_);
  file_reader_ = pp::FileIO_Dev();

  // Prepare the url request.
  pp::URLRequestInfo url_request;
  url_request.SetURL(url_);
  url_request.SetStreamToFile(true);

  // Request asynchronous download of the url providing an on-load callback.
  pp::CompletionCallback onload_callback =
      callback_factory_.NewCallback(&FileDownloader::URLLoadStartNotify);
  int32_t pp_error = url_loader_.Open(url_request, onload_callback);
  bool async_notify_ok = (pp_error == PP_ERROR_WOULDBLOCK);
  PLUGIN_PRINTF(("FileDownloader::RequestNaClModule (async_notify_ok=%d)\n",
                 async_notify_ok));
  if (!async_notify_ok) {
    // Call manually to free allocated memory and report errors.  This calls
    // |file_open_notify_callback_| with |pp_error| as the parameter.
    onload_callback.Run(pp_error);
  }
  return async_notify_ok;
}


void FileDownloader::URLLoadStartNotify(int32_t pp_error) {
  PLUGIN_PRINTF(("FileDownloader::URLLoadStartNotify (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {  // Url loading failed.
    file_open_notify_callback_.Run(pp_error);
    return;
  }

  // Process the response, validating the headers to confirm successful loading.
  pp::URLResponseInfo url_response(url_loader_.GetResponseInfo());
  if (url_response.is_null()) {
    PLUGIN_PRINTF((
        "FileDownloader::URLLoadStartNotify (url_response=NULL)\n"));
    file_open_notify_callback_.Run(pp_error);
    return;
  }
  int32_t status_code = url_response.GetStatusCode();
  PLUGIN_PRINTF(("FileDownloader::URLLoadStartNotify (status_code=%"
                 NACL_PRId32")\n", status_code));
  if (status_code != NACL_HTTP_STATUS_OK) {
    file_open_notify_callback_.Run(pp_error);
    return;
  }

  // Finish streaming the body asynchronously providing a callback.
  pp::CompletionCallback onload_callback =
      callback_factory_.NewCallback(&FileDownloader::URLLoadFinishNotify);
  pp_error = url_loader_.FinishStreamingToFile(onload_callback);
  bool async_notify_ok = (pp_error == PP_ERROR_WOULDBLOCK);
  PLUGIN_PRINTF(("FileDownloader::URLLoadStartNotify (async_notify_ok=%d)\n",
                 async_notify_ok));
  if (!async_notify_ok) {
    // Call manually to free allocated memory and report errors.  This calls
    // |file_open_notify_callback_| with |pp_error| as the parameter.
    onload_callback.Run(pp_error);
  }
}


void FileDownloader::URLLoadFinishNotify(int32_t pp_error) {
  PLUGIN_PRINTF(("FileDownloader::URLLoadFinishNotify (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {  // Streaming failed.
    file_open_notify_callback_.Run(pp_error);
    return;
  }

  pp::URLResponseInfo url_response(url_loader_.GetResponseInfo());
  // Validated on load.
  CHECK(url_response.GetStatusCode() == NACL_HTTP_STATUS_OK);

  // Record the full url from the response.
  pp::Var full_url = url_response.GetURL();
  PLUGIN_PRINTF(("FileDownloader::URLLoadFinishNotify (full_url=%s)\n",
                 full_url.DebugString().c_str()));
  if (!full_url.is_string()) {
    file_open_notify_callback_.Run(pp_error);
    return;
  }
  url_ = full_url.AsString();

  // The file is now fully downloaded.
  pp::FileRef_Dev file(url_response.GetBodyAsFileRef());
  if (file.is_null()) {
    PLUGIN_PRINTF(("FileDownloader::URLLoadFinishNotify (file=NULL)\n"));
    file_open_notify_callback_.Run(pp_error);
    return;
  }

  // Open the file asynchronously providing a callback.
  pp::CompletionCallback onopen_callback =
      callback_factory_.NewCallback(&FileDownloader::FileOpenNotify);
  pp_error = file_reader_.Open(file, PP_FILEOPENFLAG_READ, onopen_callback);
  bool async_notify_ok = (pp_error == PP_ERROR_WOULDBLOCK);
  PLUGIN_PRINTF(("FileDownloader::URLLoadFinishNotify (async_notify_ok=%d)\n",
                 async_notify_ok));
  if (!async_notify_ok) {
    // Call manually to free allocated memory and report errors.  This calls
    // |file_open_notify_callback_| with |pp_error| as the parameter.
    onopen_callback.Run(pp_error);
  }
}


void FileDownloader::FileOpenNotify(int32_t pp_error) {
  PLUGIN_PRINTF(("FileDownloader::FileOpenNotify (pp_error=%"NACL_PRId32")\n",
                 pp_error));
  file_open_notify_callback_.Run(pp_error);
}

}  // namespace plugin
