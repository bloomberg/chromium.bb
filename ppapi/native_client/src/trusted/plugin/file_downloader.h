// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_FILE_DOWNLOADER_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_FILE_DOWNLOADER_H_

#include <deque>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/validator/nacl_file_info.h"
#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/c/private/ppb_file_io_private.h"
#include "ppapi/c/private/ppb_nacl_private.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_response_info.h"
#include "ppapi/native_client/src/trusted/plugin/callback_source.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace plugin {

class Plugin;

typedef enum {
  DOWNLOAD_TO_FILE = 0,
  DOWNLOAD_TO_BUFFER,
  DOWNLOAD_STREAM,
  DOWNLOAD_NONE
} DownloadMode;

typedef std::vector<char>* FileStreamData;
typedef CallbackSource<FileStreamData> StreamCallbackSource;
typedef pp::CompletionCallbackWithOutput<FileStreamData> StreamCallback;

// RAII-style wrapper class
class NaClFileInfoAutoCloser {
 public:
  NaClFileInfoAutoCloser();

  explicit NaClFileInfoAutoCloser(NaClFileInfo* pass_ownership);

  ~NaClFileInfoAutoCloser() {
    FreeResources();
  }

  // Frees owned resources
  void FreeResources();

  void TakeOwnership(NaClFileInfo* pass_ownership);

  // Return NaClFileInfo for temporary use, retaining ownership.
  const NaClFileInfo& get() { return info_; }

  // Returns POSIX descriptor for temporary use, retaining ownership.
  int get_desc() { return info_.desc; }

  // Returns ownership to caller
  NaClFileInfo Release();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(NaClFileInfoAutoCloser);

  NaClFileInfo info_;
};

// A class that wraps PPAPI URLLoader and FileIO functionality for downloading
// the url into a file and providing an open file descriptor.
class FileDownloader {
 public:
  // Ctor initializes |instance_| to NULL, be sure to call Initialize() before
  // calling Open(), or Open() will fail.
  FileDownloader()
      : instance_(NULL),
        file_open_notify_callback_(pp::BlockUntilComplete()),
        stream_finish_callback_(pp::BlockUntilComplete()),
        file_io_private_interface_(NULL),
        url_loader_trusted_interface_(NULL),
        open_time_(-1),
        mode_(DOWNLOAD_NONE),
        url_scheme_(PP_SCHEME_OTHER),
        data_stream_callback_source_(NULL) {}
  ~FileDownloader() {}

  // Initialize() can only be called once during the lifetime of this instance.
  void Initialize(Plugin* instance);

  // Issues a GET on |url| to start downloading the response into a file,
  // and finish streaming it. |callback| will be run after streaming is
  // done or if an error prevents streaming from completing.
  // Returns true when callback is scheduled to be called on success or failure.
  // Returns false if callback is NULL, Initialize() has not been called or if
  // the PPB_FileIO_Trusted interface is not available.
  // If |record_progress| is true, then download progress will be recorded,
  // and can be polled through GetDownloadProgress().
  // If |progress_callback| is not NULL and |record_progress| is true,
  // then the callback will be invoked for every progress update received
  // by the loader.
  bool Open(const nacl::string& url,
            DownloadMode mode,
            const pp::CompletionCallback& callback,
            bool record_progress,
            PP_URLLoaderTrusted_StatusCallback progress_callback);

  // Similar to Open(), but used for streaming the |url| data directly to the
  // caller without writing to a temporary file. The callbacks provided by
  // |stream_callback_source| are expected to copy the data before returning.
  // |callback| is called once the response headers are received,
  // and streaming must be completed separately via FinishStreaming().
  bool OpenStream(const nacl::string& url,
                  const pp::CompletionCallback& callback,
                  StreamCallbackSource* stream_callback_source);

  // Finish streaming the response body for a URL request started by either
  // Open() or OpenStream().  If DownloadMode is DOWNLOAD_TO_FILE,
  // then the response body is streamed to a file, the file is opened and
  // a file descriptor is made available.  Runs the given |callback| when
  // streaming is done.
  void FinishStreaming(const pp::CompletionCallback& callback);

  // Bypasses downloading and takes a handle to the open file. To get the fd,
  // call GetFileInfo().
  void OpenFast(const nacl::string& url, PP_FileHandle file_handle,
                uint64_t file_token_lo, uint64_t file_token_hi);

  // Return a structure describing the file opened, including a file desc.
  // If downloading and opening succeeded, this returns a valid read-only
  // POSIX file descriptor.  On failure, the return value is an invalid
  // descriptor.  The file descriptor is owned by this instance, so the
  // delegate does not have to close it.
  struct NaClFileInfo GetFileInfo();

  // Returns the time delta between the call to Open() and this function.
  int64_t TimeSinceOpenMilliseconds() const;

  // Returns the url passed to Open().
  const nacl::string& url() const { return url_; }

  // Once the GET request has finished, and the contents of the file
  // represented by |url_| are available, |full_url_| is the full URL including
  // the scheme, host and full path.
  // Returns an empty string before the GET request has finished.
  const nacl::string& full_url() const { return full_url_; }

  // Returns the PP_Resource of the active URL loader, or kInvalidResource.
  PP_Resource url_loader() const { return url_loader_.pp_resource(); }

  // GetDownloadProgress() returns the current download progress, which is
  // meaningful after Open() has been called. Progress only refers to the
  // response body and does not include the headers.
  //
  // This data is only available if the |record_progress| true in the
  // Open() call.  If progress is being recorded, then |bytes_received|
  // will be set to the number of bytes received thus far,
  // and |total_bytes_to_be_received| will be set to the total number
  // of bytes to be received.  The total bytes to be received may be unknown,
  // in which case |total_bytes_to_be_received| will be set to -1.
  bool GetDownloadProgress(int64_t* bytes_received,
                           int64_t* total_bytes_to_be_received) const;

  // Returns the buffer used for DOWNLOAD_TO_BUFFER mode.
  const std::deque<char>& buffer() const { return buffer_; }

  bool streaming_to_file() const;
  bool streaming_to_buffer() const;
  bool streaming_to_user() const;

  int status_code() const { return status_code_; }
  nacl::string GetResponseHeaders() const;

  void set_request_headers(const nacl::string& extra_request_headers) {
    extra_request_headers_ = extra_request_headers;
  }


 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(FileDownloader);
  // This class loads and opens the file in three steps for DOWNLOAD_TO_FILE:
  //   1) Ask the browser to start streaming |url_| as a file.
  //   2) Ask the browser to finish streaming if headers indicate success.
  //   3) Ask the browser to open the file, so we can get the file descriptor.
  // For DOWNLOAD_TO_BUFFER, the process is very similar:
  //   1) Ask the browser to start streaming |url_| to an internal buffer.
  //   2) Ask the browser to finish streaming to |temp_buffer_| on success.
  //   3) Wait for streaming to finish, filling |buffer_| incrementally.
  // Each step is done asynchronously using callbacks.  We create callbacks
  // through a factory to take advantage of ref-counting.
  // DOWNLOAD_STREAM is similar to DOWNLOAD_TO_BUFFER except the downloaded
  // data is passed directly to the user instead of saved in a buffer.
  // The public Open*() functions start step 1), and the public FinishStreaming
  // function proceeds to step 2) and 3).
  bool InitialResponseIsValid();
  void URLLoadStartNotify(int32_t pp_error);
  void URLLoadFinishNotify(int32_t pp_error);
  void URLReadBodyNotify(int32_t pp_error);
  void StreamFinishNotify(int32_t pp_error);
  void GotFileHandleNotify(int32_t pp_error, PP_FileHandle handle);

  Plugin* instance_;
  nacl::string url_;
  nacl::string full_url_;

  nacl::string extra_request_headers_;
  pp::URLResponseInfo url_response_;
  pp::CompletionCallback file_open_notify_callback_;
  pp::CompletionCallback stream_finish_callback_;
  pp::FileIO file_reader_;
  const PPB_FileIO_Private* file_io_private_interface_;
  const PPB_URLLoaderTrusted* url_loader_trusted_interface_;
  pp::URLLoader url_loader_;
  pp::CompletionCallbackFactory<FileDownloader> callback_factory_;
  int64_t open_time_;
  int32_t status_code_;
  DownloadMode mode_;
  static const uint32_t kTempBufferSize = 16384;
  std::vector<char> temp_buffer_;
  std::deque<char> buffer_;
  PP_UrlSchemeType url_scheme_;
  StreamCallbackSource* data_stream_callback_source_;
  NaClFileInfoAutoCloser file_info_;
};
}  // namespace plugin;
#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_FILE_DOWNLOADER_H_
