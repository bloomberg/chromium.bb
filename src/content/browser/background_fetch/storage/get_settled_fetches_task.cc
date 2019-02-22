// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/get_settled_fetches_task.h"

#include "base/barrier_closure.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "services/network/public/cpp/cors/cors.h"

namespace content {

namespace background_fetch {

GetSettledFetchesTask::GetSettledFetchesTask(
    DatabaseTaskHost* host,
    BackgroundFetchRegistrationId registration_id,
    std::unique_ptr<BackgroundFetchRequestMatchParams> match_params,
    SettledFetchesCallback callback)
    : DatabaseTask(host),
      registration_id_(registration_id),
      match_params_(std::move(match_params)),
      settled_fetches_callback_(std::move(callback)),
      weak_factory_(this) {}

GetSettledFetchesTask::~GetSettledFetchesTask() = default;

void GetSettledFetchesTask::Start() {
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      2u, base::BindOnce(&GetSettledFetchesTask::GetResponses,
                         weak_factory_.GetWeakPtr()));

  cache_manager()->OpenCache(
      registration_id_.origin(), CacheStorageOwner::kBackgroundFetch,
      registration_id_.unique_id() /* cache_name */,
      base::BindOnce(&GetSettledFetchesTask::DidOpenCache,
                     weak_factory_.GetWeakPtr(), barrier_closure));

  service_worker_context()->GetRegistrationUserDataByKeyPrefix(
      registration_id_.service_worker_registration_id(),
      {CompletedRequestKeyPrefix(registration_id_.unique_id())},
      base::BindOnce(&GetSettledFetchesTask::DidGetCompletedRequests,
                     weak_factory_.GetWeakPtr(), barrier_closure));
}

void GetSettledFetchesTask::DidOpenCache(
    base::OnceClosure done_closure,
    CacheStorageCacheHandle handle,
    blink::mojom::CacheStorageError error) {
  if (error != blink::mojom::CacheStorageError::kSuccess) {
    SetStorageError(BackgroundFetchStorageError::kCacheStorageError);
  } else {
    DCHECK(handle.value());
    handle_ = std::move(handle);
  }
  std::move(done_closure).Run();
}

void GetSettledFetchesTask::DidGetCompletedRequests(
    base::OnceClosure done_closure,
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kOk:
      break;
    case DatabaseStatus::kFailed:
      SetStorageError(BackgroundFetchStorageError::kServiceWorkerStorageError);
      break;
    case DatabaseStatus::kNotFound:
      failure_reason_ = blink::mojom::BackgroundFetchFailureReason::FETCH_ERROR;
      error_ = blink::mojom::BackgroundFetchError::INVALID_ID;
      break;
  }

  completed_requests_.reserve(data.size());
  for (const std::string& serialized_completed_request : data) {
    completed_requests_.emplace_back();
    if (!completed_requests_.back().ParseFromString(
            serialized_completed_request)) {
      // Service worker database has been corrupted. Abandon fetches.
      SetStorageError(BackgroundFetchStorageError::kServiceWorkerStorageError);
      failure_reason_ = blink::mojom::BackgroundFetchFailureReason::
          SERVICE_WORKER_UNAVAILABLE;
      AbandonFetches(registration_id_.service_worker_registration_id());
      break;
    }
  }
  std::move(done_closure).Run();
}

void GetSettledFetchesTask::GetResponses() {
  // Handle potential errors.
  if (HasStorageError()) {
    FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
    return;
  }
  if (error_ != blink::mojom::BackgroundFetchError::NONE) {
    FinishWithError(error_);
    return;
  }
  if (completed_requests_.empty()) {
    FinishWithError(blink::mojom::BackgroundFetchError::NONE);
    return;
  }

  if (!match_params_->FilterByRequest()) {
    // No request to match against has been specified. Process all completed
    // requests.
    // TODO(crbug.com/863016): Process all requests here, not just the
    // completed ones.
    base::RepeatingClosure barrier_closure = base::BarrierClosure(
        completed_requests_.size(),
        base::BindOnce(&GetSettledFetchesTask::FinishWithError,
                       weak_factory_.GetWeakPtr(),
                       blink::mojom::BackgroundFetchError::NONE));
    settled_fetches_.reserve(completed_requests_.size());
    for (const auto& completed_request : completed_requests_) {
      settled_fetches_.emplace_back();
      settled_fetches_.back().request =
          std::move(ServiceWorkerFetchRequest::ParseFromString(
              completed_request.serialized_request()));
      FillResponse(&settled_fetches_.back(), barrier_closure);
    }
    return;
  }

  // Get response(s) only for the relevant fetch.
  settled_fetches_.emplace_back();
  settled_fetches_.back().request = match_params_->request_to_match();

  if (match_params_->match_all()) {
    FillResponses(base::BindOnce(&GetSettledFetchesTask::FinishWithError,
                                 weak_factory_.GetWeakPtr(),
                                 blink::mojom::BackgroundFetchError::NONE));
    return;
  } else {
    FillResponse(&settled_fetches_.back(),
                 base::BindOnce(&GetSettledFetchesTask::FinishWithError,
                                weak_factory_.GetWeakPtr(),
                                blink::mojom::BackgroundFetchError::NONE));
    return;
  }
}

void GetSettledFetchesTask::FillResponse(
    BackgroundFetchSettledFetch* settled_fetch,
    base::OnceClosure callback) {
  DCHECK(settled_fetch);
  DCHECK(handle_.value());

  auto request =
      std::make_unique<ServiceWorkerFetchRequest>(settled_fetch->request);
  handle_.value()->Match(std::move(request),
                         match_params_->cloned_cache_query_params(),
                         base::BindOnce(&GetSettledFetchesTask::DidMatchRequest,
                                        weak_factory_.GetWeakPtr(),
                                        settled_fetch, std::move(callback)));
}

void GetSettledFetchesTask::FillResponses(base::OnceClosure callback) {
  DCHECK(match_params_->match_all());
  DCHECK(match_params_->FilterByRequest());
  DCHECK(!settled_fetches_.empty());
  DCHECK(handle_.value());

  // Make a copy.
  auto request = std::make_unique<ServiceWorkerFetchRequest>(
      match_params_->request_to_match());

  handle_.value()->MatchAll(
      std::move(request), match_params_->cloned_cache_query_params(),
      base::BindOnce(&GetSettledFetchesTask::DidMatchAllResponsesForRequest,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void GetSettledFetchesTask::DidMatchRequest(
    BackgroundFetchSettledFetch* settled_fetch,
    base::OnceClosure callback,
    blink::mojom::CacheStorageError error,
    blink::mojom::FetchAPIResponsePtr cache_response) {
  DCHECK(settled_fetch);

  // Handle error cases.
  if (error == blink::mojom::CacheStorageError::kErrorNotFound) {
    // This is currently being called once a fetch finishes, or when match() is
    // called.
    // In the first case, not finding a response is an error state. In the
    // second case, it just means the developer passed a non-matching request.
    // The if condition below picks the first one.
    // TODO(crbug.com/863016): Once we stop sending settled_fetches with
    // BackgroundFetch events, this won't be a storage error.
    if (!match_params_->FilterByRequest())
      SetStorageError(BackgroundFetchStorageError::kCacheStorageError);
  } else if (error != blink::mojom::CacheStorageError::kSuccess) {
    SetStorageError(BackgroundFetchStorageError::kCacheStorageError);
  }

  if (!cache_response) {
    FillUncachedResponse(settled_fetch, std::move(callback));
    return;
  }
  settled_fetch->response = std::move(cache_response);
  std::move(callback).Run();
}

void GetSettledFetchesTask::DidMatchAllResponsesForRequest(
    base::OnceClosure callback,
    blink::mojom::CacheStorageError error,
    std::vector<blink::mojom::FetchAPIResponsePtr> cache_responses) {
  if (error != blink::mojom::CacheStorageError::kSuccess &&
      error != blink::mojom::CacheStorageError::kErrorNotFound) {
    SetStorageError(BackgroundFetchStorageError::kCacheStorageError);
  }

  if (error != blink::mojom::CacheStorageError::kSuccess) {
    DCHECK(!settled_fetches_.empty());
    FillUncachedResponse(&settled_fetches_.back(), std::move(callback));
    return;
  }

  settled_fetches_.clear();
  settled_fetches_.reserve(cache_responses.size());
  for (size_t i = 0; i < cache_responses.size(); ++i) {
    settled_fetches_.emplace_back();
    settled_fetches_.back().request = match_params_->request_to_match();
    settled_fetches_.back().response = std::move(cache_responses[i]);
  }
  std::move(callback).Run();
}

// TODO(crbug.com/863016): Get rid of this method.
void GetSettledFetchesTask::FillUncachedResponse(
    BackgroundFetchSettledFetch* settled_fetch,
    base::OnceClosure callback) {
  failure_reason_ = blink::mojom::BackgroundFetchFailureReason::FETCH_ERROR;

  // TODO(rayankans): Fill unmatched response with error reports.
  DCHECK(!settled_fetch->response);
  settled_fetch->response = blink::mojom::FetchAPIResponse::New();
  settled_fetch->response->response_type =
      network::mojom::FetchResponseType::kError;
  settled_fetch->response->url_list.push_back(settled_fetch->request.url);

  std::move(callback).Run();
}

void GetSettledFetchesTask::FinishWithError(
    blink::mojom::BackgroundFetchError error) {
  if (HasStorageError())
    error = blink::mojom::BackgroundFetchError::STORAGE_ERROR;
  ReportStorageError();

  if (error == blink::mojom::BackgroundFetchError::NONE) {
    for (const auto& settled_fetch : settled_fetches_) {
      if (!settled_fetch.response->status_code) {
        // A status_code of 0 means no headers were returned.
        failure_reason_ =
            blink::mojom::BackgroundFetchFailureReason::FETCH_ERROR;
        break;
      }
      if (!network::cors::IsOkStatus(settled_fetch.response->status_code)) {
        failure_reason_ =
            blink::mojom::BackgroundFetchFailureReason::BAD_STATUS;
        break;
      }
    }
  }
  std::move(settled_fetches_callback_)
      .Run(error, failure_reason_, std::move(settled_fetches_),
           {} /* blob_data_handles */);
  Finished();  // Destroys |this|.
}

std::string GetSettledFetchesTask::HistogramName() const {
  return "GetSettledFetchesTask";
};

}  // namespace background_fetch

}  // namespace content
