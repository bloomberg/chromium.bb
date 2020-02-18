// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_URL_DOWNLOADER_H_
#define CONTENT_BROWSER_DOWNLOAD_URL_DOWNLOADER_H_

#include <stdint.h>

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/url_download_handler.h"
#include "content/browser/download/download_request_core.h"
#include "content/public/common/referrer.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"

namespace download {
struct DownloadCreateInfo;
}  // namespace download

namespace net {
class UploadProgress;
}  // namespace net

namespace network {
class UploadProgressTracker;
}  // namespace network

namespace content {
class ByteStreamReader;

class UrlDownloader : public net::URLRequest::Delegate,
                      public DownloadRequestCore::Delegate,
                      public download::UrlDownloadHandler {
 public:
  UrlDownloader(std::unique_ptr<net::URLRequest> request,
                base::WeakPtr<UrlDownloadHandler::Delegate> delegate,
                bool is_parallel_request,
                const std::string& request_origin,
                bool follow_cross_origin_redirects,
                download::DownloadSource download_source,
                const download::DownloadUrlParameters::UploadProgressCallback&
                    upload_callback);
  ~UrlDownloader() override;

  static std::unique_ptr<UrlDownloader> BeginDownload(
      base::WeakPtr<download::UrlDownloadHandler::Delegate> delegate,
      std::unique_ptr<net::URLRequest> request,
      download::DownloadUrlParameters* params,
      bool is_parallel_request);

 private:
  void Start();

  // URLRequest::Delegate:
  void OnReceivedRedirect(net::URLRequest* request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer_redirect) override;
  void OnResponseStarted(net::URLRequest* request, int net_error) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

  void StartReading(bool is_continuation);
  void ResponseCompleted(int net_error);

  // DownloadRequestCore::Delegate
  void OnStart(
      std::unique_ptr<download::DownloadCreateInfo> download_create_info,
      std::unique_ptr<ByteStreamReader> stream_reader,
      const download::DownloadUrlParameters::OnStartedCallback& callback)
      override;
  void OnReadyToRead() override;

  // UrlDownloadHandler implementations.
  void PauseRequest() override;
  void ResumeRequest() override;
  void CancelRequest() override;

  // UploadProgressTracker callback.
  void OnUploadProgressReport(const net::UploadProgress& upload_progress);

  // Called when the UrlDownloader is done with the request. Posts a task to
  // remove itself from its download manager, which in turn would cause the
  // UrlDownloader to be freed.
  void Destroy();

  std::unique_ptr<net::URLRequest> request_;

  // Live on UI thread, post task to call |delegate_| functions.
  base::WeakPtr<download::UrlDownloadHandler::Delegate> delegate_;
  DownloadRequestCore core_;

  const bool follow_cross_origin_redirects_;

  download::DownloadUrlParameters::UploadProgressCallback upload_callback_;
  std::unique_ptr<network::UploadProgressTracker> upload_progress_tracker_;

  base::WeakPtrFactory<UrlDownloader> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_URL_DOWNLOADER_H_
