// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/native_client/src/trusted/plugin/file_downloader.h"

#include <stdio.h>
#include <string.h>
#include <string>

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_time.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"
#include "ppapi/native_client/src/trusted/plugin/callback_source.h"
#include "ppapi/native_client/src/trusted/plugin/plugin.h"
#include "ppapi/native_client/src/trusted/plugin/utility.h"

namespace {

const int32_t kExtensionUrlRequestStatusOk = 200;
const int32_t kDataUriRequestStatusOk = 0;

struct NaClFileInfo NoFileInfo() {
  struct NaClFileInfo info;
  memset(&info, 0, sizeof(info));
  info.desc = -1;
  return info;
}

// Converts a PP_FileHandle to a POSIX file descriptor.
int32_t ConvertFileDescriptor(PP_FileHandle handle) {
  PLUGIN_PRINTF(("ConvertFileDescriptor, handle=%d\n", handle));
#if NACL_WINDOWS
  int32_t file_desc = NACL_NO_FILE_DESC;
  // On Windows, valid handles are 32 bit unsigned integers so this is safe.
  file_desc = reinterpret_cast<uintptr_t>(handle);
  // Convert the Windows HANDLE from Pepper to a POSIX file descriptor.
  int32_t posix_desc = _open_osfhandle(file_desc, _O_RDWR | _O_BINARY);
  if (posix_desc == -1) {
    // Close the Windows HANDLE if it can't be converted.
    CloseHandle(reinterpret_cast<HANDLE>(file_desc));
    return -1;
  }
  return posix_desc;
#else
  return handle;
#endif
}

}  // namespace

namespace plugin {

NaClFileInfoAutoCloser::NaClFileInfoAutoCloser()
    : info_(NoFileInfo()) {}

NaClFileInfoAutoCloser::NaClFileInfoAutoCloser(NaClFileInfo* pass_ownership)
    : info_(*pass_ownership) {
  *pass_ownership = NoFileInfo();
}

void NaClFileInfoAutoCloser::FreeResources() {
  if (-1 != get_desc()) {
    PLUGIN_PRINTF(("NaClFileInfoAutoCloser::FreeResources close(%d)\n",
                   get_desc()));
    close(get_desc());
  }
  info_.desc = -1;
}

void NaClFileInfoAutoCloser::TakeOwnership(NaClFileInfo* pass_ownership) {
  PLUGIN_PRINTF(("NaClFileInfoAutoCloser::set: taking ownership of %d\n",
                 pass_ownership->desc));
  CHECK(pass_ownership->desc == -1 || pass_ownership->desc != get_desc());
  FreeResources();
  info_ = *pass_ownership;
  *pass_ownership = NoFileInfo();
}

NaClFileInfo NaClFileInfoAutoCloser::Release() {
  NaClFileInfo info_to_return = info_;
  info_ = NoFileInfo();
  return info_to_return;
}

void FileDownloader::Initialize(Plugin* instance) {
  PLUGIN_PRINTF(("FileDownloader::FileDownloader (this=%p)\n",
                 static_cast<void*>(this)));
  CHECK(instance != NULL);
  CHECK(instance_ == NULL);  // Can only initialize once.
  instance_ = instance;
  callback_factory_.Initialize(this);
  file_io_private_interface_ = static_cast<const PPB_FileIO_Private*>(
      pp::Module::Get()->GetBrowserInterface(PPB_FILEIO_PRIVATE_INTERFACE));
  url_loader_trusted_interface_ = static_cast<const PPB_URLLoaderTrusted*>(
      pp::Module::Get()->GetBrowserInterface(PPB_URLLOADERTRUSTED_INTERFACE));
  temp_buffer_.resize(kTempBufferSize);
  file_info_.FreeResources();
}

bool FileDownloader::OpenStream(
    const nacl::string& url,
    const pp::CompletionCallback& callback,
    StreamCallbackSource* stream_callback_source) {
  open_and_stream_ = false;
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
      file_io_private_interface_ == NULL)
    return false;

  CHECK(instance_ != NULL);
  open_time_ = NaClGetTimeOfDayMicroseconds();
  status_code_ = -1;
  url_ = url;
  file_open_notify_callback_ = callback;
  mode_ = mode;
  buffer_.clear();
  file_info_.FreeResources();
  pp::URLRequestInfo url_request(instance_);

  // Allow CORS.
  // Note that "SetAllowCrossOriginRequests" (currently) has the side effect of
  // preventing credentials from being sent on same-origin requests.  We
  // therefore avoid setting this flag unless we know for sure it is a
  // cross-origin request, resulting in behavior similar to XMLHttpRequest.
  if (!instance_->DocumentCanRequest(url))
    url_request.SetAllowCrossOriginRequests(true);

  if (!extra_request_headers_.empty())
    url_request.SetHeaders(extra_request_headers_);

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

  // Request asynchronous download of the url providing an on-load callback.
  // As long as this step is guaranteed to be asynchronous, we can call
  // synchronously all other internal callbacks that eventually result in the
  // invocation of the user callback. The user code will not be reentered.
  pp::CompletionCallback onload_callback =
      callback_factory_.NewCallback(&FileDownloader::URLLoadStartNotify);
  int32_t pp_error = url_loader_.Open(url_request, onload_callback);
  PLUGIN_PRINTF(("FileDownloader::Open (pp_error=%" NACL_PRId32 ")\n",
                 pp_error));
  CHECK(pp_error == PP_OK_COMPLETIONPENDING);
  return true;
}

void FileDownloader::OpenFast(const nacl::string& url,
                              PP_FileHandle file_handle,
                              uint64_t file_token_lo, uint64_t file_token_hi) {
  PLUGIN_PRINTF(("FileDownloader::OpenFast (url=%s)\n", url.c_str()));

  file_info_.FreeResources();
  CHECK(instance_ != NULL);
  open_time_ = NaClGetTimeOfDayMicroseconds();
  status_code_ = NACL_HTTP_STATUS_OK;
  url_ = url;
  mode_ = DOWNLOAD_NONE;
  if (not_streaming() && file_handle != PP_kInvalidFileHandle) {
    NaClFileInfo tmp_info = NoFileInfo();
    tmp_info.desc = ConvertFileDescriptor(file_handle);
    tmp_info.file_token.lo = file_token_lo;
    tmp_info.file_token.hi = file_token_hi;
    file_info_.TakeOwnership(&tmp_info);
  }
}

NaClFileInfo FileDownloader::GetFileInfo() {
  NaClFileInfo info_to_return = NoFileInfo();

  PLUGIN_PRINTF(("FileDownloader::GetFileInfo, this %p\n", this));
  if (file_info_.get_desc() != -1) {
    info_to_return = file_info_.Release();
  }
  PLUGIN_PRINTF(("FileDownloader::GetFileInfo -- returning %d\n",
                 info_to_return.desc));
  return info_to_return;
}

int64_t FileDownloader::TimeSinceOpenMilliseconds() const {
  int64_t now = NaClGetTimeOfDayMicroseconds();
  // If Open() wasn't called or we somehow return an earlier time now, just
  // return the 0 rather than worse nonsense values.
  if (open_time_ < 0 || now < open_time_)
    return 0;
  return (now - open_time_) / NACL_MICROS_PER_MILLI;
}

bool FileDownloader::InitialResponseIsValid() {
  // Process the response, validating the headers to confirm successful loading.
  url_response_ = url_loader_.GetResponseInfo();
  if (url_response_.is_null()) {
    PLUGIN_PRINTF((
        "FileDownloader::InitialResponseIsValid (url_response_=NULL)\n"));
    return false;
  }

  pp::Var full_url = url_response_.GetURL();
  if (!full_url.is_string()) {
    PLUGIN_PRINTF((
        "FileDownloader::InitialResponseIsValid (url is not a string)\n"));
    return false;
  }
  full_url_ = full_url.AsString();

  // Note that URLs in the data-URI scheme produce different error
  // codes than other schemes.  This is because data-URI are really a
  // special kind of file scheme, and therefore do not produce HTTP
  // status codes.
  bool status_ok = false;
  status_code_ = url_response_.GetStatusCode();
  switch (url_scheme_) {
    case SCHEME_CHROME_EXTENSION:
      PLUGIN_PRINTF(("FileDownloader::InitialResponseIsValid (chrome-extension "
                     "response status_code=%" NACL_PRId32 ")\n", status_code_));
      status_ok = (status_code_ == kExtensionUrlRequestStatusOk);
      break;
    case SCHEME_DATA:
      PLUGIN_PRINTF(("FileDownloader::InitialResponseIsValid (data URI "
                     "response status_code=%" NACL_PRId32 ")\n", status_code_));
      status_ok = (status_code_ == kDataUriRequestStatusOk);
      break;
    case SCHEME_OTHER:
      PLUGIN_PRINTF(("FileDownloader::InitialResponseIsValid (HTTP response "
                     "status_code=%" NACL_PRId32 ")\n", status_code_));
      status_ok = (status_code_ == NACL_HTTP_STATUS_OK);
      break;
  }
  return status_ok;
}

void FileDownloader::URLLoadStartNotify(int32_t pp_error) {
  PLUGIN_PRINTF(("FileDownloader::URLLoadStartNotify (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    file_open_notify_callback_.RunAndClear(pp_error);
    return;
  }

  if (!InitialResponseIsValid()) {
    file_open_notify_callback_.RunAndClear(PP_ERROR_FAILED);
    return;
  }

  if (open_and_stream_) {
    FinishStreaming(file_open_notify_callback_);
    return;
  }

  file_open_notify_callback_.RunAndClear(PP_OK);
}

void FileDownloader::FinishStreaming(
    const pp::CompletionCallback& callback) {
  stream_finish_callback_ = callback;

  // Finish streaming the body providing an optional callback.
  if (streaming_to_file()) {
    pp::CompletionCallback onload_callback =
        callback_factory_.NewOptionalCallback(
            &FileDownloader::URLLoadFinishNotify);
    int32_t pp_error = url_loader_.FinishStreamingToFile(onload_callback);
    bool async_notify_ok = (pp_error == PP_OK_COMPLETIONPENDING);
    PLUGIN_PRINTF(("FileDownloader::FinishStreaming (async_notify_ok=%d)\n",
                   async_notify_ok));
    if (!async_notify_ok) {
      // Call manually to free allocated memory and report errors.  This calls
      // |stream_finish_callback_| with |pp_error| as the parameter.
      onload_callback.RunAndClear(pp_error);
    }
  } else {
    pp::CompletionCallback onread_callback =
        callback_factory_.NewOptionalCallback(
            &FileDownloader::URLReadBodyNotify);
    int32_t temp_size = static_cast<int32_t>(temp_buffer_.size());
    int32_t pp_error = url_loader_.ReadResponseBody(&temp_buffer_[0],
                                                    temp_size,
                                                    onread_callback);
    bool async_notify_ok = (pp_error == PP_OK_COMPLETIONPENDING);
    PLUGIN_PRINTF((
        "FileDownloader::FinishStreaming (async_notify_ok=%d)\n",
        async_notify_ok));
    if (!async_notify_ok) {
      onread_callback.RunAndClear(pp_error);
    }
  }
}

void FileDownloader::URLLoadFinishNotify(int32_t pp_error) {
  PLUGIN_PRINTF(("FileDownloader::URLLoadFinishNotify (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {  // Streaming failed.
    stream_finish_callback_.RunAndClear(pp_error);
    return;
  }

  // Validate response again on load (though it should be the same
  // as it was during InitialResponseIsValid?).
  url_response_ = url_loader_.GetResponseInfo();
  CHECK(url_response_.GetStatusCode() == NACL_HTTP_STATUS_OK ||
        url_response_.GetStatusCode() == kExtensionUrlRequestStatusOk);

  // Record the full url from the response.
  pp::Var full_url = url_response_.GetURL();
  PLUGIN_PRINTF(("FileDownloader::URLLoadFinishNotify (full_url=%s)\n",
                 full_url.DebugString().c_str()));
  if (!full_url.is_string()) {
    stream_finish_callback_.RunAndClear(PP_ERROR_FAILED);
    return;
  }
  full_url_ = full_url.AsString();

  // The file is now fully downloaded.
  pp::FileRef file(url_response_.GetBodyAsFileRef());
  if (file.is_null()) {
    PLUGIN_PRINTF(("FileDownloader::URLLoadFinishNotify (file=NULL)\n"));
    stream_finish_callback_.RunAndClear(PP_ERROR_FAILED);
    return;
  }

  // Open the file providing an optional callback.
  pp::CompletionCallback onopen_callback =
      callback_factory_.NewOptionalCallback(
          &FileDownloader::StreamFinishNotify);
  pp_error = file_reader_.Open(file, PP_FILEOPENFLAG_READ, onopen_callback);
  bool async_notify_ok = (pp_error == PP_OK_COMPLETIONPENDING);
  PLUGIN_PRINTF(("FileDownloader::URLLoadFinishNotify (async_notify_ok=%d)\n",
                 async_notify_ok));
  if (!async_notify_ok) {
    // Call manually to free allocated memory and report errors.  This calls
    // |stream_finish_callback_| with |pp_error| as the parameter.
    onopen_callback.RunAndClear(pp_error);
  }
}

void FileDownloader::URLReadBodyNotify(int32_t pp_error) {
  PLUGIN_PRINTF(("FileDownloader::URLReadBodyNotify (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error < PP_OK) {
    stream_finish_callback_.RunAndClear(pp_error);
  } else if (pp_error == PP_OK) {
    if (streaming_to_user()) {
      data_stream_callback_source_->GetCallback().RunAndClear(PP_OK);
    }
    StreamFinishNotify(PP_OK);
  } else {
    if (streaming_to_buffer()) {
      buffer_.insert(buffer_.end(), &temp_buffer_[0], &temp_buffer_[pp_error]);
    } else if (streaming_to_user()) {
      PLUGIN_PRINTF(("Running data_stream_callback, temp_buffer_=%p\n",
                     &temp_buffer_[0]));
      StreamCallback cb = data_stream_callback_source_->GetCallback();
      *(cb.output()) = &temp_buffer_;
      cb.RunAndClear(pp_error);
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
      onread_callback.RunAndClear(pp_error);
    }
  }
}

bool FileDownloader::GetDownloadProgress(
    int64_t* bytes_received,
    int64_t* total_bytes_to_be_received) const {
  return url_loader_.GetDownloadProgress(bytes_received,
                                         total_bytes_to_be_received);
}

nacl::string FileDownloader::GetResponseHeaders() const {
  pp::Var headers = url_response_.GetHeaders();
  if (!headers.is_string()) {
    PLUGIN_PRINTF((
        "FileDownloader::GetResponseHeaders (headers are not a string)\n"));
    return nacl::string();
  }
  return headers.AsString();
}

void FileDownloader::StreamFinishNotify(int32_t pp_error) {
  PLUGIN_PRINTF((
      "FileDownloader::StreamFinishNotify (pp_error=%" NACL_PRId32 ")\n",
      pp_error));

  // Run the callback if we have an error, or if we don't have a file_reader_
  // to get a file handle for.
  if (pp_error != PP_OK || file_reader_.pp_resource() == 0) {
    stream_finish_callback_.RunAndClear(pp_error);
    return;
  }

  pp::CompletionCallbackWithOutput<PP_FileHandle> cb =
      callback_factory_.NewCallbackWithOutput(
          &FileDownloader::GotFileHandleNotify);
  file_io_private_interface_->RequestOSFileHandle(
      file_reader_.pp_resource(), cb.output(), cb.pp_completion_callback());
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

bool FileDownloader::not_streaming() const {
  return mode_ == DOWNLOAD_NONE;
}

void FileDownloader::GotFileHandleNotify(int32_t pp_error,
                                         PP_FileHandle handle) {
  PLUGIN_PRINTF((
      "FileDownloader::GotFileHandleNotify (pp_error=%" NACL_PRId32 ")\n",
      pp_error));
  if (pp_error == PP_OK) {
    NaClFileInfo tmp_info = NoFileInfo();
    tmp_info.desc = ConvertFileDescriptor(handle);
    file_info_.TakeOwnership(&tmp_info);
  }

  stream_finish_callback_.RunAndClear(pp_error);
}

}  // namespace plugin
