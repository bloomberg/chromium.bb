// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_request_match_params.h"
#include "content/public/common/origin_util.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "third_party/blink/public/platform/modules/background_fetch/background_fetch.mojom.h"

#include <utility>

#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

// Performs mixed content checks on the |request| for Background Fetch.
// Background Fetch depends on Service Workers, which are restricted for use
// on secure origins. We can therefore assume that the registration's origin
// is secure. This test ensures that the origin for the url of every
// request is also secure.
bool IsMixedContent(const BackgroundFetchRequestInfo& request) {
  // Empty request is valid, it shouldn't fail the mixed content check.
  if (request.fetch_request().url.is_empty())
    return false;

  return !IsOriginSecure(request.fetch_request().url);
}

// Whether the |request| needs CORS preflight.
// Requests that require CORS preflights are temporarily blocked, because the
// browser side of Background Fetch doesn't yet support performing CORS
// checks. TODO(crbug.com/711354): Remove this temporary block.
bool RequiresCorsPreflight(const BackgroundFetchRequestInfo& request,
                           const url::Origin& origin) {
  const blink::mojom::FetchAPIRequest& fetch_request = request.fetch_request();

  // Same origin requests don't require a CORS preflight.
  // https://fetch.spec.whatwg.org/#main-fetch
  // TODO(crbug.com/711354): Make sure that cross-origin redirects are disabled.
  if (url::IsSameOriginWith(origin.GetURL(), fetch_request.url))
    return false;

  // Requests that are more involved than what is possible with HTML's form
  // element require a CORS-preflight request.
  // https://fetch.spec.whatwg.org/#main-fetch
  if (!fetch_request.method.empty() &&
      !network::cors::IsCorsSafelistedMethod(fetch_request.method)) {
    return true;
  }

  net::HttpRequestHeaders::HeaderVector headers;
  for (const auto& header : fetch_request.headers)
    headers.emplace_back(header.first, header.second);

  return !network::cors::CorsUnsafeRequestHeaderNames(headers).empty();
}

}  // namespace

using blink::mojom::BackgroundFetchError;
using blink::mojom::BackgroundFetchFailureReason;

BackgroundFetchJobController::BackgroundFetchJobController(
    BackgroundFetchDataManager* data_manager,
    BackgroundFetchDelegateProxy* delegate_proxy,
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchOptions& options,
    const SkBitmap& icon,
    uint64_t bytes_downloaded,
    ProgressCallback progress_callback,
    FinishedCallback finished_callback)
    : data_manager_(data_manager),
      delegate_proxy_(delegate_proxy),
      registration_id_(registration_id),
      options_(options),
      icon_(icon),
      complete_requests_downloaded_bytes_cache_(bytes_downloaded),
      progress_callback_(std::move(progress_callback)),
      finished_callback_(std::move(finished_callback)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void BackgroundFetchJobController::InitializeRequestStatus(
    int completed_downloads,
    int total_downloads,
    std::vector<scoped_refptr<BackgroundFetchRequestInfo>>
        active_fetch_requests,
    bool start_paused) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Don't allow double initialization.
  DCHECK_GT(total_downloads, 0);
  DCHECK_EQ(total_downloads_, 0);

  completed_downloads_ = completed_downloads;
  total_downloads_ = total_downloads;

  // TODO(nator): Update this when we support uploads.
  total_downloads_size_ = options_.download_total;

  std::vector<std::string> active_guids;
  active_guids.reserve(active_fetch_requests.size());
  for (const auto& request_info : active_fetch_requests)
    active_guids.push_back(request_info->download_guid());

  auto fetch_description = std::make_unique<BackgroundFetchDescription>(
      registration_id().unique_id(), options_.title, registration_id().origin(),
      icon_, completed_downloads, total_downloads,
      complete_requests_downloaded_bytes_cache_, total_downloads_size_,
      std::move(active_guids), start_paused);

  delegate_proxy_->CreateDownloadJob(GetWeakPtr(), std::move(fetch_description),
                                     std::move(active_fetch_requests));
}

BackgroundFetchJobController::~BackgroundFetchJobController() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

bool BackgroundFetchJobController::HasMoreRequests() {
  return completed_downloads_ < total_downloads_;
}

void BackgroundFetchJobController::StartRequest(
    scoped_refptr<BackgroundFetchRequestInfo> request,
    RequestFinishedCallback request_finished_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_LT(completed_downloads_, total_downloads_);
  DCHECK(request_finished_callback);
  DCHECK(request);

  active_request_downloaded_bytes_ = 0;
  active_request_finished_callback_ = std::move(request_finished_callback);

  if (IsMixedContent(*request.get()) ||
      RequiresCorsPreflight(*request.get(), registration_id_.origin())) {
    request->SetEmptyResultWithFailureReason(
        BackgroundFetchResult::FailureReason::FETCH_ERROR);

    ++completed_downloads_;
    std::move(active_request_finished_callback_).Run(request);
    return;
  }

  delegate_proxy_->StartRequest(registration_id().unique_id(),
                                registration_id().origin(), request);
}

void BackgroundFetchJobController::DidStartRequest(
    const scoped_refptr<BackgroundFetchRequestInfo>& request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(crbug.com/884672): Either add CORS check here or remove this function
  // and do the CORS check in BackgroundFetchDelegateImpl (since
  // download::Client::OnDownloadStarted returns a value that can abort the
  // download).
}

void BackgroundFetchJobController::DidUpdateRequest(
    const scoped_refptr<BackgroundFetchRequestInfo>& request,
    uint64_t bytes_downloaded) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (active_request_downloaded_bytes_ == bytes_downloaded)
    return;

  active_request_downloaded_bytes_ = bytes_downloaded;

  auto registration = NewRegistration();
  registration->downloaded += GetInProgressDownloadedBytes();
  progress_callback_.Run(*registration);
}

void BackgroundFetchJobController::DidCompleteRequest(
    const scoped_refptr<BackgroundFetchRequestInfo>& request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // It's possible for the DidCompleteRequest() callback to have been in-flight
  // while this Job Controller was being aborted, in which case the
  // |active_request_finished_callback_| will have been reset.
  if (!active_request_finished_callback_)
    return;

  active_request_downloaded_bytes_ = 0;

  complete_requests_downloaded_bytes_cache_ += request->GetFileSize();
  ++completed_downloads_;

  std::move(active_request_finished_callback_).Run(request);
}

std::unique_ptr<BackgroundFetchRegistration>
BackgroundFetchJobController::NewRegistration() const {
  return std::make_unique<BackgroundFetchRegistration>(
      registration_id().developer_id(), registration_id().unique_id(),
      0 /* upload_total */, 0 /* uploaded */, total_downloads_size_,
      complete_requests_downloaded_bytes_cache_,
      blink::mojom::BackgroundFetchResult::UNSET, failure_reason_);
}

uint64_t BackgroundFetchJobController::GetInProgressDownloadedBytes() {
  return active_request_downloaded_bytes_;
}

void BackgroundFetchJobController::AbortFromDelegate(
    BackgroundFetchFailureReason failure_reason) {
  failure_reason_ = failure_reason;

  // Stop propagating any in-flight events to the scheduler.
  active_request_finished_callback_.Reset();

  Finish(failure_reason_, base::DoNothing());
}

void BackgroundFetchJobController::Abort(
    BackgroundFetchFailureReason failure_reason,
    ErrorCallback callback) {
  failure_reason_ = failure_reason;

  // Stop propagating any in-flight events to the scheduler.
  active_request_finished_callback_.Reset();

  // Cancel any in-flight downloads and UI through the BGFetchDelegate.
  delegate_proxy_->Abort(registration_id().unique_id());

  Finish(failure_reason_, std::move(callback));
}

void BackgroundFetchJobController::Finish(
    BackgroundFetchFailureReason reason_to_abort,
    ErrorCallback callback) {
  DCHECK(reason_to_abort != BackgroundFetchFailureReason::NONE ||
         !HasMoreRequests());

  // Race conditions make it possible for a controller to finish twice. This
  // should be removed when the scheduler starts owning the controllers.
  if (!finished_callback_) {
    std::move(callback).Run(BackgroundFetchError::INVALID_ID);
    return;
  }

  std::move(finished_callback_)
      .Run(registration_id_, reason_to_abort, std::move(callback));
}

void BackgroundFetchJobController::DidPopNextRequest(
    BackgroundFetchError error,
    scoped_refptr<BackgroundFetchRequestInfo> request_info) {
  if (error != BackgroundFetchError::NONE) {
    Abort(BackgroundFetchFailureReason::SERVICE_WORKER_UNAVAILABLE,
          base::DoNothing());
    return;
  }

  StartRequest(
      std::move(request_info),
      base::BindOnce(&BackgroundFetchJobController::MarkRequestAsComplete,
                     GetWeakPtr()));
}

void BackgroundFetchJobController::MarkRequestAsComplete(
    scoped_refptr<BackgroundFetchRequestInfo> request_info) {
  data_manager_->MarkRequestAsComplete(
      registration_id(), std::move(request_info),
      base::BindOnce(&BackgroundFetchJobController::DidMarkRequestAsComplete,
                     GetWeakPtr()));
}

void BackgroundFetchJobController::DidMarkRequestAsComplete(
    BackgroundFetchError error) {
  switch (error) {
    case BackgroundFetchError::NONE:
      break;
    case BackgroundFetchError::STORAGE_ERROR:
      Abort(BackgroundFetchFailureReason::SERVICE_WORKER_UNAVAILABLE,
            base::DoNothing());
      return;
    case BackgroundFetchError::QUOTA_EXCEEDED:
      Abort(BackgroundFetchFailureReason::QUOTA_EXCEEDED, base::DoNothing());
      return;
    default:
      NOTREACHED();
  }

  if (HasMoreRequests()) {
    data_manager_->PopNextRequest(
        registration_id(),
        base::BindOnce(&BackgroundFetchJobController::DidPopNextRequest,
                       GetWeakPtr()));
    return;
  }
  Finish(BackgroundFetchFailureReason::NONE, base::DoNothing());
}

void BackgroundFetchJobController::GetUploadData(
    blink::mojom::FetchAPIRequestPtr request,
    BackgroundFetchDelegate::GetUploadDataCallback callback) {
  data_manager_->MatchRequests(
      registration_id(),
      std::make_unique<BackgroundFetchRequestMatchParams>(
          std::move(request), /* match_params= */ nullptr,
          /* match_all= */ false),
      base::BindOnce(&BackgroundFetchJobController::DidGetUploadData,
                     GetWeakPtr(), std::move(callback)));
}

void BackgroundFetchJobController::DidGetUploadData(
    BackgroundFetchDelegate::GetUploadDataCallback callback,
    BackgroundFetchError error,
    std::vector<BackgroundFetchSettledFetch> fetches) {
  if (error != BackgroundFetchError::NONE) {
    Abort(BackgroundFetchFailureReason::SERVICE_WORKER_UNAVAILABLE,
          base::DoNothing());
    std::move(callback).Run(/* request_body= */ nullptr);
    return;
  }

  DCHECK_EQ(fetches.size(), 1u);
  DCHECK(fetches[0].request->blob);

  network::mojom::DataPipeGetterPtr data_pipe_getter_ptr;
  blink::mojom::BlobPtr blob_ptr(std::move(fetches[0].request->blob->blob));
  blob_ptr->AsDataPipeGetter(MakeRequest(&data_pipe_getter_ptr));

  auto request_body = base::MakeRefCounted<network::ResourceRequestBody>();
  request_body->AppendDataPipe(std::move(data_pipe_getter_ptr));
  std::move(callback).Run(std::move(request_body));
}

}  // namespace content
