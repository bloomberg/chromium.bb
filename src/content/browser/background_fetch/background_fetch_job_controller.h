// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "content/browser/background_fetch/background_fetch_delegate_proxy.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/background_fetch/background_fetch_scheduler.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {

class BackgroundFetchDataManager;

// The JobController will be responsible for coordinating communication with the
// DownloadManager. It will get requests from the RequestManager and dispatch
// them to the DownloadService. It lives entirely on the IO thread.
//
// Lifetime: It is created lazily only once a Background Fetch registration
// starts downloading, and it is destroyed once no more communication with the
// DownloadService or Offline Items Collection is necessary (i.e. once the
// registration has been aborted, or once it has completed/failed and the
// waitUntil promise has been resolved so UpdateUI can no longer be called).
class CONTENT_EXPORT BackgroundFetchJobController
    : public BackgroundFetchDelegateProxy::Controller {
 public:
  using ErrorCallback =
      base::OnceCallback<void(blink::mojom::BackgroundFetchError)>;
  using FinishedCallback =
      base::OnceCallback<void(const BackgroundFetchRegistrationId&,
                              blink::mojom::BackgroundFetchFailureReason,
                              ErrorCallback)>;
  using ProgressCallback =
      base::RepeatingCallback<void(const BackgroundFetchRegistration&)>;
  using RequestFinishedCallback =
      base::OnceCallback<void(scoped_refptr<BackgroundFetchRequestInfo>)>;

  BackgroundFetchJobController(
      BackgroundFetchDataManager* data_manager,
      BackgroundFetchDelegateProxy* delegate_proxy,
      const BackgroundFetchRegistrationId& registration_id,
      const BackgroundFetchOptions& options,
      const SkBitmap& icon,
      uint64_t bytes_downloaded,
      ProgressCallback progress_callback,
      FinishedCallback finished_callback);
  ~BackgroundFetchJobController() override;

  // Initializes the job controller with the status of the active and completed
  // downloads, as well as the title to use.
  // Only called when this has been loaded from the database.
  void InitializeRequestStatus(
      int completed_downloads,
      int total_downloads,
      std::vector<scoped_refptr<BackgroundFetchRequestInfo>>
          active_fetch_requests,
      bool start_paused);

  // Gets the number of bytes downloaded for jobs that are currently running.
  uint64_t GetInProgressDownloadedBytes();

  // Returns a unique_ptr to a BackgroundFetchRegistration object
  // created with member fields.
  std::unique_ptr<BackgroundFetchRegistration> NewRegistration() const;

  // Returns the options with which this job is fetching data.
  const BackgroundFetchOptions& options() const { return options_; }

  // Returns total downloaded bytes.
  int downloaded() const { return complete_requests_downloaded_bytes_cache_; }

  // Returns total size of downloads, as indicated by the developer.
  int download_total() const { return total_downloads_size_; }

  // Returns the number of requests that comprise the whole job.
  int total_downloads() const { return total_downloads_; }

  const BackgroundFetchRegistrationId& registration_id() const {
    return registration_id_;
  }

  base::WeakPtr<BackgroundFetchJobController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // BackgroundFetchDelegateProxy::Controller implementation:
  void DidStartRequest(
      const scoped_refptr<BackgroundFetchRequestInfo>& request) override;
  void DidUpdateRequest(
      const scoped_refptr<BackgroundFetchRequestInfo>& request,
      uint64_t bytes_downloaded) override;
  void DidCompleteRequest(
      const scoped_refptr<BackgroundFetchRequestInfo>& request) override;
  void AbortFromDelegate(
      blink::mojom::BackgroundFetchFailureReason failure_reason) override;
  void GetUploadData(
      blink::mojom::FetchAPIRequestPtr request,
      BackgroundFetchDelegate::GetUploadDataCallback callback) override;

  // Aborts the fetch. |callback| will run with the result of marking the
  // registration for deletion.
  void Abort(blink::mojom::BackgroundFetchFailureReason failure_reason,
             ErrorCallback callback);

  // Request processing callbacks.
  void StartRequest(scoped_refptr<BackgroundFetchRequestInfo> request,
                    RequestFinishedCallback request_finished_callback);
  void DidPopNextRequest(
      blink::mojom::BackgroundFetchError error,
      scoped_refptr<BackgroundFetchRequestInfo> request_info);
  void MarkRequestAsComplete(
      scoped_refptr<BackgroundFetchRequestInfo> request_info);

 private:
  // Called after the request is completely processed, and the next one can be
  // started.
  void DidMarkRequestAsComplete(blink::mojom::BackgroundFetchError error);

  // Whether there are more requests to process as part of this job.
  bool HasMoreRequests();

  // Called when the job completes or has been aborted. |callback| will run
  // with the result of marking the registration for deletion.
  void Finish(blink::mojom::BackgroundFetchFailureReason reason_to_abort,
              ErrorCallback callback);

  void DidGetUploadData(BackgroundFetchDelegate::GetUploadDataCallback callback,
                        blink::mojom::BackgroundFetchError error,
                        std::vector<BackgroundFetchSettledFetch> fetches);

  // Manager for interacting with the DB. It is owned by the
  // BackgroundFetchContext.
  BackgroundFetchDataManager* data_manager_;

  // Proxy for interacting with the BackgroundFetchDelegate across thread
  // boundaries. It is owned by the BackgroundFetchContext.
  BackgroundFetchDelegateProxy* delegate_proxy_;

  // The registration ID of the fetch this controller represents.
  BackgroundFetchRegistrationId registration_id_;

  // Options for the represented background fetch registration.
  BackgroundFetchOptions options_;

  // Icon for the represented background fetch registration.
  SkBitmap icon_;

  // Number of bytes downloaded for the active request.
  uint64_t active_request_downloaded_bytes_ = 0;

  // Finished callback to invoke when the active request has finished.
  RequestFinishedCallback active_request_finished_callback_;

  // Cache of downloaded byte count stored by the DataManager, to enable
  // delivering progress events without having to read from the database.
  uint64_t complete_requests_downloaded_bytes_cache_;

  // Total downloads size, as indicated by the developer.
  int total_downloads_size_ = 0;

  // Callback run each time download progress updates.
  ProgressCallback progress_callback_;

  // Number of requests that comprise the whole job.
  int total_downloads_ = 0;

  // Number of the requests that have been completed so far.
  int completed_downloads_ = 0;

  // The reason background fetch was aborted.
  blink::mojom::BackgroundFetchFailureReason failure_reason_ =
      blink::mojom::BackgroundFetchFailureReason::NONE;

  // Custom callback that runs after the controller is finished.
  FinishedCallback finished_callback_;

  base::WeakPtrFactory<BackgroundFetchJobController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchJobController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_
