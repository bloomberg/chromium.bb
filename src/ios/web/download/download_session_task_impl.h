// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_DOWNLOAD_DOWNLOAD_SESSION_TASK_IMPL_H_
#define IOS_WEB_DOWNLOAD_DOWNLOAD_SESSION_TASK_IMPL_H_

#include "ios/web/download/download_task_impl.h"

namespace net {
class URLRequestContextGetter;
class URLFetcherResponseWriter;
}

namespace web {
// Implementation of DownloadTaskImpl that uses NSURLRequest to perform the
// download
class DownloadSessionTaskImpl final : public DownloadTaskImpl {
 public:
  // Constructs a new DownloadSessionTaskImpl objects. |web_state|, |identifier|
  // and |delegate| must be valid.
  DownloadSessionTaskImpl(WebState* web_state,
                          const GURL& original_url,
                          NSString* http_method,
                          const std::string& content_disposition,
                          int64_t total_bytes,
                          const std::string& mime_type,
                          NSString* identifier,
                          Delegate* delegate);

  DownloadSessionTaskImpl(const DownloadSessionTaskImpl&) = delete;
  DownloadSessionTaskImpl& operator=(const DownloadSessionTaskImpl&) = delete;

  ~DownloadSessionTaskImpl() final;

  // DownloadTask overrides:
  NSData* GetResponseData() const final;
  const base::FilePath& GetResponsePath() const final;

  // DownloadTaskImpl overrides:
  void Start(const base::FilePath& path, Destination destination_hint) final;
  void Cancel() final;
  void ShutDown() final;
  void OnDownloadFinished(int error_code) final;

 private:
  // Called once the net::URLFetcherResponseWriter created in
  // Start() has been initialised. The download can be started
  // unless the initialisation has failed (as reported by the
  // |writer_initialization_status| result).
  void OnWriterInitialized(
      std::unique_ptr<net::URLFetcherResponseWriter> writer,
      int writer_initialization_status);

  // Creates background NSURLSession with given |identifier| and |cookies|.
  NSURLSession* CreateSession(NSString* identifier,
                              NSArray<NSHTTPCookie*>* cookies);

  // Asynchronously returns cookies for WebState associated with this task.
  // Must be called on UI thread. The callback will be invoked on the UI thread.
  void GetCookies(base::OnceCallback<void(NSArray<NSHTTPCookie*>*)> callback);

  // Asynchronously returns cookies for |context_getter|. Must
  // be called on IO thread. The callback will be invoked on the UI thread.
  static void GetCookiesFromContextGetter(
      scoped_refptr<net::URLRequestContextGetter> context_getter,
      base::OnceCallback<void(NSArray<NSHTTPCookie*>*)> callback);

  // Starts the download with given cookies.
  void StartWithCookies(NSArray<NSHTTPCookie*>* cookies);

  // Starts parsing data:// url. Separate code path is used because
  // NSURLSession does not support data URLs.
  void StartDataUrlParsing();

  // Called when data:// url parsing has completed and the data has been
  // written.
  void OnDataUrlWritten(int bytes_written);

  std::unique_ptr<net::URLFetcherResponseWriter> writer_;
  NSURLSession* session_ = nil;
  NSURLSessionTask* session_task_ = nil;

  base::WeakPtrFactory<DownloadSessionTaskImpl> weak_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_DOWNLOAD_DOWNLOAD_SESSION_TASK_IMPL_H
