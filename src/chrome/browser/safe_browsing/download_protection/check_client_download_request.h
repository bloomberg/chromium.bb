// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_CLIENT_DOWNLOAD_REQUEST_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_CLIENT_DOWNLOAD_REQUEST_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/download_protection/check_client_download_request_base.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "components/download/public/common/download_item.h"
#include "components/safe_browsing/core/proto/webprotect.pb.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace safe_browsing {

class CheckClientDownloadRequest : public CheckClientDownloadRequestBase,
                                   public download::DownloadItem::Observer {
 public:
  CheckClientDownloadRequest(
      download::DownloadItem* item,
      CheckDownloadRepeatingCallback callback,
      DownloadProtectionService* service,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor);
  ~CheckClientDownloadRequest() override;

  // download::DownloadItem::Observer:
  void OnDownloadDestroyed(download::DownloadItem* download) override;
  void OnDownloadUpdated(download::DownloadItem* download) override;

  static bool IsSupportedDownload(const download::DownloadItem& item,
                                  const base::FilePath& target_path,
                                  DownloadCheckResultReason* reason,
                                  ClientDownloadRequest::DownloadType* type);

 private:
  // CheckClientDownloadRequestBase overrides:
  bool IsSupportedDownload(DownloadCheckResultReason* reason,
                           ClientDownloadRequest::DownloadType* type) override;
  content::BrowserContext* GetBrowserContext() const override;
  bool IsCancelled() override;
  void PopulateRequest(ClientDownloadRequest* request) override;
  base::WeakPtr<CheckClientDownloadRequestBase> GetWeakPtr() override;

  void NotifySendRequest(const ClientDownloadRequest* request) override;
  void SetDownloadPingToken(const std::string& token) override;
  void MaybeStorePingsForDownload(DownloadCheckResult result,
                                  bool upload_requested,
                                  const std::string& request_data,
                                  const std::string& response_body) override;

  // Uploads the binary for deep scanning if the reason and policies indicate
  // it should be.
  bool ShouldUploadBinary(DownloadCheckResultReason reason) override;
  void UploadBinary(DownloadCheckResultReason reason) override;

  // Called when this request is completed.
  void NotifyRequestFinished(DownloadCheckResult result,
                             DownloadCheckResultReason reason) override;

  // Called when finishing the download, to decide whether to prompt the user
  // for deep scanning or not.
  bool ShouldPromptForDeepScanning(
      DownloadCheckResultReason reason) const override;

  bool IsWhitelistedByPolicy() const override;

  // The DownloadItem we are checking. Will be NULL if the request has been
  // canceled. Must be accessed only on UI thread.
  download::DownloadItem* item_;
  CheckDownloadRepeatingCallback callback_;

  // Upload start time used for UMA duration histograms.
  base::TimeTicks upload_start_time_;

  base::WeakPtrFactory<CheckClientDownloadRequest> weakptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CheckClientDownloadRequest);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_CLIENT_DOWNLOAD_REQUEST_H_
