// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/file_downloader.h"

#include <stdio.h>
#include <string>

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_time.h"
#include "native_client/src/trusted/plugin/callback_source.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/utility.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"

namespace {
const int32_t kExtensionUrlRequestStatusOk = 200;
const int32_t kDataUriRequestStatusOk = 0;
}

namespace plugin {

void FileDownloader::Initialize(Plugin* instance) {
  PLUGIN_PRINTF(("FileDownloader::FileDownloader (this=%p)\n",
                 static_cast<void*>(this)));
  CHECK(instance != NULL);
  CHECK(instance_ == NULL);  // Can only initialize once.
  instance_ = instance;
  callback_factory_.Initialize(this);
  file_io_trusted_interface_ = static_cast<const PPB_FileIOTrusted*>(
      pp::Module::Get()->GetBrowserInterface(PPB_FILEIOTRUSTED_INTERFACE));
  url_loader_trusted_interface_ = static_cast<const PPB_URLLoaderTrusted*>(
      pp::Module::Get()->GetBrowserInterface(PPB_URLLOADERTRUSTED_INTERFACE));
  temp_buffer_.resize(kTempBufferSize);
}

bool FileDownloader::OpenStream(
    const nacl::string& url,
    const pp::CompletionCallback& callback,
    StreamCallbackSource* stream_callback_source) {
  data_stream_callback_source_ = stream_callback_source;
  return Open(url, DOWNLOAD_STREAM, callback, true, NULL);
}

bool FileDownloader::Open(
    const nacl::string& url,
    DownloadMode mode,
    const pp::CompletionCallback& callback,
    bool record_progress,
    PP_URLLoaderTrusted_StatusCallback progress_callback) {
  PLUGIN_PRINTF(("FileDownloader::Open (url=%s)\n", url.c_str()));
  if (callback.pp_completion_callback().func == NULL ||
      instance_ == NULL ||
      file_io_trusted_interface_ == NULL)
    return false;

  CHECK(instance_ != NULL);
  open_time_ = NaClGetTimeOfDayMicroseconds();
  status_code_ = -1;
  url_to_open_ = url;
  url_ = url;
  file_open_notify_callback_ = callback;
  mode_ = mode;
  buffer_.clear();
  pp::URLRequestInfo url_request(instance_);

  // Allow CORS.
  // Note that "SetAllowCrossOriginRequests" (currently) has the side effect of
  // preventing credentials from being sent on same-origin requests.  We
  // therefore avoid setting this flag unless we know for sure it is a
  // cross-origin request, resulting in behavior similar to XMLHttpRequest.
  if (!instance_->DocumentCanRequest(url))
    url_request.SetAllowCrossOriginRequests(true);

  do {
    // Reset the url loader and file reader.
    // Note that we have the only reference to the underlying objects, so
    // this will implicitly close any pending IO and destroy them.
    url_loader_ = pp::URLLoader(instance_);
    url_scheme_ = instance_->GetUrlScheme(url);
    bool grant_universal_access = false;
    if (url_scheme_ == SCHEME_DATA) {
      // TODO(elijahtaylor) Remove this when data URIs can be read without
      // universal access.
      // https://bugs.webkit.org/show_bug.cgi?id=17352
      if (streaming_to_buffer()) {
        grant_universal_access = true;
      } else {
        // Open is to invoke a callback on success or failure. Schedule
        // it asynchronously to follow PPAPI's convention and avoid reentrancy.
        pp::Core* core = pp::Module::Get()->core();
        core->CallOnMainThread(0, callback, PP_ERROR_NOACCESS);
        PLUGIN_PRINTF(("FileDownloader::Open (pp_error=PP_ERROR_NOACCESS)\n"));
        return true;
      }
    }

    url_request.SetRecordDownloadProgress(record_progress);

    if (url_loader_trusted_interface_ != NULL) {
      if (grant_universal_access) {
        // TODO(sehr,jvoung): See if we can remove this -- currently
        // only used for data URIs.
        url_loader_trusted_interface_->GrantUniversalAccess(
            url_loader_.pp_resource());
      }
      if (progress_callback != NULL) {
        url_loader_trusted_interface_->RegisterStatusCallback(
            url_loader_.pp_resource(), progress_callback);
      }
    }

    // Prepare the url request.
    url_request.SetURL(url_);

    if (streaming_to_file()) {
      file_reader_ = pp::FileIO(instance_);
      url_request.SetStreamToFile(true);
    }
  } while (0);

  void (FileDownloader::*start_notify)(int32_t);
  if (streaming_to_file())
    start_notify = &FileDownloader::URLLoadStartNotify;
  else
    start_notify = &FileDownloader::URLBufferStartNotify;

  // Request asynchronous download of the url providing an on-load callback.
  // As long as this step is guaranteed to be asynchronous, we can call
  // synchronously all other internal callbacks that eventually result in the
  // invocation of the user callback. The user code will not be reentered.
  pp::CompletionCallback onload_callback =
      callback_factory_.NewCallback(start_notify);
  int32_t pp_error = url_loader_.Open(url_request, onload_callback);
  PLUGIN_PRINTF(("FileDownloader::Open (pp_error=%"NACL_PRId32")\n", pp_error));
  CHECK(pp_error == PP_OK_COMPLETIONPENDING);
  return true;
}

int32_t FileDownloader::GetPOSIXFileDescriptor() {
  if (!streaming_to_file()) {
    return NACL_NO_FILE_DESC;
  }
  // Use the trusted interface to get the file descriptor.
  if (file_io_trusted_interface_ == NULL) {
    return NACL_NO_FILE_DESC;
  }
  int32_t file_desc = file_io_trusted_interface_->GetOSFileDescriptor(
      file_reader_.pp_resource());


#if NACL_WINDOWS
  // Convert the Windows HANDLE from Pepper to a POSIX file descriptor.
  int32_t posix_desc = _open_osfhandle(file_desc, _O_RDWR | _O_BINARY);
  if (posix_desc == -1) {
    // Close the Windows HANDLE if it can't be converted.
    CloseHandle(reinterpret_cast<HANDLE>(file_desc));
    return NACL_NO_FILE_DESC;
  }
  file_desc = posix_desc;
#endif

  return file_desc;
}

int64_t FileDownloader::TimeSinceOpenMilliseconds() const {
  int64_t now = NaClGetTimeOfDayMicroseconds();
  // If Open() wasn't called or we somehow return an earlier time now, just
  // return the 0 rather than worse nonsense values.
  if (open_time_ < 0 || now < open_time_)
    return 0;
  return (now - open_time_) / NACL_MICROS_PER_MILLI;
}

bool FileDownloader::InitialResponseIsValid(int32_t pp_error) {
  if (pp_error != PP_OK) {  // Url loading failed.
    file_open_notify_callback_.Run(pp_error);
    return false;
  }

  // Process the response, validating the headers to confirm successful loading.
  pp::URLResponseInfo url_response(url_loader_.GetResponseInfo());
  if (url_response.is_null()) {
    PLUGIN_PRINTF((
        "FileDownloader::InitialResponseIsValid (url_response=NULL)\n"));
    file_open_notify_callback_.Run(PP_ERROR_FAILED);
    return false;
  }
  // Note that URLs in the chrome-extension scheme produce different error
  // codes than other schemes.  This is because chrome-extension URLs are
  // really a special kind of file scheme, and therefore do not produce HTTP
  // status codes.
  pp::Var full_url = url_response.GetURL();
  if (!full_url.is_string()) {
    PLUGIN_PRINTF((
        "FileDownloader::InitialResponseIsValid (url is not a string)\n"));
    file_open_notify_callback_.Run(PP_ERROR_FAILED);
    return false;
  }
  bool status_ok = false;
  status_code_ = url_response.GetStatusCode();
  switch (url_scheme_) {
    case SCHEME_CHROME_EXTENSION:
      PLUGIN_PRINTF(("FileDownloader::InitialResponseIsValid (chrome-extension "
                     "response status_code=%"NACL_PRId32")\n", status_code_));
      status_ok = (status_code_ == kExtensionUrlRequestStatusOk);
      break;
    case SCHEME_DATA:
      PLUGIN_PRINTF(("FileDownloader::InitialResponseIsValid (data URI "
                     "response status_code=%"NACL_PRId32")\n", status_code_));
      status_ok = (status_code_ == kDataUriRequestStatusOk);
      break;
    case SCHEME_OTHER:
      PLUGIN_PRINTF(("FileDownloader::InitialResponseIsValid (HTTP response "
                     "status_code=%"NACL_PRId32")\n", status_code_));
      status_ok = (status_code_ == NACL_HTTP_STATUS_OK);
      break;
  }

  if (!status_ok) {
    file_open_notify_callback_.Run(PP_ERROR_FAILED);
    return false;
  }

  return true;
}

void FileDownloader::URLLoadStartNotify(int32_t pp_error) {
  PLUGIN_PRINTF(("FileDownloader::URLLoadStartNotify (pp_error=%"
                 NACL_PRId32")\n", pp_error));

  if (!InitialResponseIsValid(pp_error))
    return;
  // Finish streaming the body providing an optional callback.
  pp::CompletionCallback onload_callback =
      callback_factory_.NewOptionalCallback(
          &FileDownloader::URLLoadFinishNotify);
  pp_error = url_loader_.FinishStreamingToFile(onload_callback);
  bool async_notify_ok = (pp_error == PP_OK_COMPLETIONPENDING);
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
  CHECK(url_response.GetStatusCode() == NACL_HTTP_STATUS_OK ||
        url_response.GetStatusCode() == kExtensionUrlRequestStatusOk);

  // Record the full url from the response.
  pp::Var full_url = url_response.GetURL();
  PLUGIN_PRINTF(("FileDownloader::URLLoadFinishNotify (full_url=%s)\n",
                 full_url.DebugString().c_str()));
  if (!full_url.is_string()) {
    file_open_notify_callback_.Run(PP_ERROR_FAILED);
    return;
  }
  url_ = full_url.AsString();

  // The file is now fully downloaded.
  pp::FileRef file(url_response.GetBodyAsFileRef());
  if (file.is_null()) {
    PLUGIN_PRINTF(("FileDownloader::URLLoadFinishNotify (file=NULL)\n"));
    file_open_notify_callback_.Run(PP_ERROR_FAILED);
    return;
  }

  // Open the file providing an optional callback.
  pp::CompletionCallback onopen_callback =
      callback_factory_.NewOptionalCallback(&FileDownloader::FileOpenNotify);
  pp_error = file_reader_.Open(file, PP_FILEOPENFLAG_READ, onopen_callback);
  bool async_notify_ok = (pp_error == PP_OK_COMPLETIONPENDING);
  PLUGIN_PRINTF(("FileDownloader::URLLoadFinishNotify (async_notify_ok=%d)\n",
                 async_notify_ok));
  if (!async_notify_ok) {
    // Call manually to free allocated memory and report errors.  This calls
    // |file_open_notify_callback_| with |pp_error| as the parameter.
    onopen_callback.Run(pp_error);
  }
}

void FileDownloader::URLBufferStartNotify(int32_t pp_error) {
  PLUGIN_PRINTF(("FileDownloader::URLBufferStartNotify (pp_error=%"
                 NACL_PRId32")\n", pp_error));

  if (!InitialResponseIsValid(pp_error))
    return;
  // Finish streaming the body asynchronously providing a callback.
  pp::CompletionCallback onread_callback =
      callback_factory_.NewOptionalCallback(&FileDownloader::URLReadBodyNotify);

  int32_t temp_size = static_cast<int32_t>(temp_buffer_.size());
  pp_error = url_loader_.ReadResponseBody(&temp_buffer_[0],
                                          temp_size,
                                          onread_callback);
  bool async_notify_ok = (pp_error == PP_OK_COMPLETIONPENDING);
  PLUGIN_PRINTF(("FileDownloader::URLBufferStartNotify (async_notify_ok=%d)\n",
                 async_notify_ok));
  if (!async_notify_ok) {
    onread_callback.Run(pp_error);
  }
}

void FileDownloader::URLReadBodyNotify(int32_t pp_error) {
  PLUGIN_PRINTF(("FileDownloader::URLReadBodyNotify (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error < PP_OK) {
    file_open_notify_callback_.Run(pp_error);
  } else if (pp_error == PP_OK) {
    if (streaming_to_user()) {
      data_stream_callback_source_->GetCallback().Run(PP_OK);
    }
    FileOpenNotify(PP_OK);
  } else {
    if (streaming_to_buffer()) {
      buffer_.insert(buffer_.end(), &temp_buffer_[0], &temp_buffer_[pp_error]);
    } else if (streaming_to_user()) {
      PLUGIN_PRINTF(("Running data_stream_callback, temp_buffer_=%p\n",
                     &temp_buffer_[0]));
      StreamCallback cb = data_stream_callback_source_->GetCallback();
      *(cb.output()) = &temp_buffer_;
      cb.Run(pp_error);
    }
    pp::CompletionCallback onread_callback =
        callback_factory_.NewOptionalCallback(
            &FileDownloader::URLReadBodyNotify);
    int32_t temp_size = static_cast<int32_t>(temp_buffer_.size());
    pp_error = url_loader_.ReadResponseBody(&temp_buffer_[0],
                                            temp_size,
                                            onread_callback);
    bool async_notify_ok = (pp_error == PP_OK_COMPLETIONPENDING);
    if (!async_notify_ok) {
      onread_callback.Run(pp_error);
    }
  }
}

bool FileDownloader::GetDownloadProgress(
    int64_t* bytes_received,
    int64_t* total_bytes_to_be_received) const {
  return url_loader_.GetDownloadProgress(bytes_received,
                                         total_bytes_to_be_received);
}

void FileDownloader::FileOpenNotify(int32_t pp_error) {
  PLUGIN_PRINTF(("FileDownloader::FileOpenNotify (pp_error=%"NACL_PRId32")\n",
                 pp_error));
  file_open_notify_callback_.Run(pp_error);
}

bool FileDownloader::streaming_to_file() const {
  return mode_ == DOWNLOAD_TO_FILE;
}

bool FileDownloader::streaming_to_buffer() const {
  return mode_ == DOWNLOAD_TO_BUFFER;
}

bool FileDownloader::streaming_to_user() const {
  return mode_ == DOWNLOAD_STREAM;
}

}  // namespace plugin
