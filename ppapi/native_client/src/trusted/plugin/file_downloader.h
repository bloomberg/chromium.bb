// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_FILE_DOWNLOADER_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_FILE_DOWNLOADER_H_

#include <deque>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/plugin/callback_source.h"
#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/c/trusted/ppb_file_io_trusted.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace plugin {

class Plugin;

typedef enum {
  DOWNLOAD_TO_FILE = 0,
  DOWNLOAD_TO_BUFFER,
  DOWNLOAD_STREAM,
  DOWNLOAD_NONE
} DownloadMode;

typedef enum {
  SCHEME_CHROME_EXTENSION,
  SCHEME_DATA,
  SCHEME_OTHER
} UrlSchemeType;

typedef std::vector<char>* FileStreamData;
typedef CallbackSource<FileStreamData> StreamCallbackSource;
typedef pp::CompletionCallbackWithOutput<FileStreamData> StreamCallback;

// A class that wraps PPAPI URLLoader and FileIO functionality for downloading
// the url into a file and providing an open file descriptor.
class FileDownloader {
 public:
  // Ctor initializes |instance_| to NULL, be sure to call Initialize() before
  // calling Open(), or Open() will fail.
  FileDownloader()
      : instance_(NULL),
        file_open_notify_callback_(pp::BlockUntilComplete()),
        file_handle_(PP_kInvalidFileHandle),
        file_io_trusted_interface_(NULL),
        url_loader_trusted_interface_(NULL),
        open_time_(-1),
        mode_(DOWNLOAD_NONE),
        url_scheme_(SCHEME_OTHER),
        data_stream_callback_source_(NULL) {}
  ~FileDownloader() {}

  // Initialize() can only be called once during the lifetime of this instance.
  void Initialize(Plugin* instance);

  // Issues a GET on |url| downloading the response into a file. The file is
  // then opened and a file descriptor is made available.
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

  // Same as Open, but used for streaming the file data directly to the
  // caller without buffering it. The callbacks provided by
  // |stream_callback_source| are expected to copy the data before returning.
  // |callback| will still be called when the stream is finished.
  bool OpenStream(const nacl::string& url,
                  const pp::CompletionCallback& callback,
                  StreamCallbackSource* stream_callback_source);

  // Bypasses downloading and takes a handle to the open file. To get the fd,
  // call GetPOSIXFileDescriptor().
  void OpenFast(const nacl::string& url, PP_FileHandle file_handle);

  // If downloading and opening succeeded, this returns a valid read-only
  // POSIX file descriptor.  On failure, the return value is an invalid
  // descriptor.  The file descriptor is owned by this instance, so the
  // delegate does not have to close it.
  int32_t GetPOSIXFileDescriptor();

  // Returns the time delta between the call to Open() and this function.
  int64_t TimeSinceOpenMilliseconds() const;

  // The value of |url_| changes over the life of this instance.  When the file
  // is first opened, |url_| is a copy of the URL used to open the file, which
  // can be a relative URL.  Once the GET request has finished, and the contents
  // of the file represented by |url_| are available, |url_| is the full URL
  // including the scheme, host and full path.
  const nacl::string& url() const { return url_; }

  // Returns the url passed to Open().
  const nacl::string& url_to_open() const { return url_to_open_; }

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
  bool not_streaming() const;

  int status_code() const { return status_code_; }

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
  bool InitialResponseIsValid(int32_t pp_error);
  void URLLoadStartNotify(int32_t pp_error);
  void URLLoadFinishNotify(int32_t pp_error);
  void URLBufferStartNotify(int32_t pp_error);
  void URLReadBodyNotify(int32_t pp_error);
  void FileOpenNotify(int32_t pp_error);

  Plugin* instance_;
  nacl::string url_to_open_;
  nacl::string url_;
  pp::CompletionCallback file_open_notify_callback_;
  pp::FileIO file_reader_;
  PP_FileHandle file_handle_;
  const PPB_FileIOTrusted* file_io_trusted_interface_;
  const PPB_URLLoaderTrusted* url_loader_trusted_interface_;
  pp::URLLoader url_loader_;
  pp::CompletionCallbackFactory<FileDownloader> callback_factory_;
  int64_t open_time_;
  int32_t status_code_;
  DownloadMode mode_;
  static const uint32_t kTempBufferSize = 2048;
  std::vector<char> temp_buffer_;
  std::deque<char> buffer_;
  UrlSchemeType url_scheme_;
  StreamCallbackSource* data_stream_callback_source_;
};
}  // namespace plugin;
#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_FILE_DOWNLOADER_H_
