// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/ppapi/file_downloader.h"

#include <stdio.h>
#include <string>

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_time.h"
#include "native_client/src/trusted/plugin/ppapi/plugin_ppapi.h"
#include "native_client/src/trusted/plugin/utility.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/dev/ppb_file_io_dev.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/cpp/dev/file_ref_dev.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"

namespace {

const char* const kChromeExtensionScheme = "chrome-extension:";
const int32_t kExtensionRequestStatusOk = 200;

// A helper function that tests to see if |url| is in the chrome-extension
// scheme.  Note that this routine assumes that the scheme part of |url| is
// all lower-case UTF8.
bool IsChromeExtensionUrl(const std::string& url) {
  // The scheme has to exist and be at the start of |url|.
  return url.find(kChromeExtensionScheme) == 0;
}
}

namespace plugin {

void FileDownloader::Initialize(PluginPpapi* instance) {
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
  open_time_ = NaClGetTimeOfDayMicroseconds();
  url_to_open_ = url;
  url_ = url;
  file_open_notify_callback_ = callback;
  // Reset the url loader and file reader.
  // Note that we have the only refernce to the underlying objects, so
  // this will implicitly close any pending IO and destroy them.
  url_loader_ = pp::URLLoader(instance_);

  if (!instance_->mime_type().empty() &&
      instance_->mime_type() != PluginPpapi::kNaClMIMEType &&
      IsChromeExtensionUrl(url)) {
    // This NEXE is being used as a content type handler rather than directly
    // by an HTML document. In that case, the NEXE runs in the security context
    // of the content it is rendering and the NEXE itself appears to be a
    // cross-origin resource stored in a Chrome extension. We request universal
    // access during this load so that we can read the NEXE.
    const PPB_URLLoaderTrusted* url_loaded_trusted =
        static_cast<const PPB_URLLoaderTrusted*>(
        pp::Module::Get()->GetBrowserInterface(PPB_URLLOADERTRUSTED_INTERFACE));
    if (url_loaded_trusted)
      url_loaded_trusted->GrantUniversalAccess(url_loader_.pp_resource());
  }

  file_reader_ = pp::FileIO_Dev(instance_);
  file_io_trusted_interface_ = static_cast<const PPB_FileIOTrusted_Dev*>(
      pp::Module::Get()->GetBrowserInterface(PPB_FILEIOTRUSTED_DEV_INTERFACE));
  if (file_io_trusted_interface_ == NULL)
    return false;  // Interface not supported by our browser

  // Prepare the url request.
  pp::URLRequestInfo url_request(instance_);
  url_request.SetURL(url_);
  url_request.SetStreamToFile(true);

  // Request asynchronous download of the url providing an on-load callback.
  pp::CompletionCallback onload_callback =
      callback_factory_.NewCallback(&FileDownloader::URLLoadStartNotify);
  int32_t pp_error = url_loader_.Open(url_request, onload_callback);
  bool async_notify_ok = (pp_error == PP_OK_COMPLETIONPENDING);
  PLUGIN_PRINTF(("FileDownloader::Open (async_notify_ok=%d)\n",
                 async_notify_ok));
  if (!async_notify_ok) {
    // Call manually to free allocated memory and report errors.  This calls
    // |file_open_notify_callback_| with |pp_error| as the parameter.
    onload_callback.Run(pp_error);
  }
  return async_notify_ok;
}

int32_t FileDownloader::GetPOSIXFileDescriptor() {
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
    file_open_notify_callback_.Run(PP_ERROR_FAILED);
    return;
  }
  // Note that URLs in the chrome-extension scheme produce different error
  // codes than other schemes.  This is because chrome-extension URLs are
  // really a special kind of file scheme, and therefore do not produce HTTP
  // status codes.
  pp::Var full_url = url_response.GetURL();
  if (!full_url.is_string()) {
    PLUGIN_PRINTF((
        "FileDownloader::URLLoadStartNotify (url is not a string)\n"));
    file_open_notify_callback_.Run(PP_ERROR_FAILED);
    return;
  }
  bool status_ok = false;
  int32_t status_code = url_response.GetStatusCode();
  if (IsChromeExtensionUrl(full_url.AsString())) {
    PLUGIN_PRINTF(("FileDownloader::URLLoadStartNotify (chrome-extension "
                   "response status_code=%"NACL_PRId32")\n", status_code));
    status_ok = (status_code == kExtensionRequestStatusOk);
  } else {
    PLUGIN_PRINTF(("FileDownloader::URLLoadStartNotify (HTTP response "
                   "status_code=%"NACL_PRId32")\n", status_code));
    status_ok = (status_code == NACL_HTTP_STATUS_OK);
  }

  if (!status_ok) {
    file_open_notify_callback_.Run(PP_ERROR_FAILED);
    return;
  }

  // Finish streaming the body asynchronously providing a callback.
  pp::CompletionCallback onload_callback =
      callback_factory_.NewCallback(&FileDownloader::URLLoadFinishNotify);
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
        url_response.GetStatusCode() == kExtensionRequestStatusOk);

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
  pp::FileRef_Dev file(url_response.GetBodyAsFileRef());
  if (file.is_null()) {
    PLUGIN_PRINTF(("FileDownloader::URLLoadFinishNotify (file=NULL)\n"));
    file_open_notify_callback_.Run(PP_ERROR_FAILED);
    return;
  }

  // Open the file asynchronously providing a callback.
  pp::CompletionCallback onopen_callback =
      callback_factory_.NewCallback(&FileDownloader::FileOpenNotify);
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


void FileDownloader::FileOpenNotify(int32_t pp_error) {
  PLUGIN_PRINTF(("FileDownloader::FileOpenNotify (pp_error=%"NACL_PRId32")\n",
                 pp_error));
  file_open_notify_callback_.Run(pp_error);
}

}  // namespace plugin
