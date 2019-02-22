// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_GET_SETTLED_FETCHES_TASK_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_GET_SETTLED_FETCHES_TASK_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/background_fetch/background_fetch.pb.h"
#include "content/browser/background_fetch/background_fetch_request_match_params.h"
#include "content/browser/background_fetch/storage/database_task.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

namespace content {

class BackgroundFetchRequestMatchParams;

namespace background_fetch {

class GetSettledFetchesTask : public DatabaseTask {
 public:
  // TODO(nator): Remove BlobDataHandle since we're not using them.
  using SettledFetchesCallback = base::OnceCallback<void(
      blink::mojom::BackgroundFetchError,
      blink::mojom::BackgroundFetchFailureReason,
      std::vector<BackgroundFetchSettledFetch>,
      std::vector<std::unique_ptr<storage::BlobDataHandle>>)>;

  // Gets settled fetches from cache storage, filtered according to
  // |match_params|.
  GetSettledFetchesTask(
      DatabaseTaskHost* host,
      BackgroundFetchRegistrationId registration_id,
      std::unique_ptr<BackgroundFetchRequestMatchParams> match_params,
      SettledFetchesCallback callback);

  ~GetSettledFetchesTask() override;

  // DatabaseTask implementation:
  void Start() override;

 private:
  void DidOpenCache(base::OnceClosure done_closure,
                    CacheStorageCacheHandle handle,
                    blink::mojom::CacheStorageError error);

  void DidGetCompletedRequests(base::OnceClosure done_closure,
                               const std::vector<std::string>& data,
                               blink::ServiceWorkerStatusCode status);

  void GetResponses();

  void FillUncachedResponse(BackgroundFetchSettledFetch* settled_fetch,
                            base::OnceClosure callback);

  void FillResponse(BackgroundFetchSettledFetch* settled_fetch,
                    base::OnceClosure callback);

  void FillResponses(base::OnceClosure callback);

  void DidMatchRequest(BackgroundFetchSettledFetch* settled_fetch,
                       base::OnceClosure callback,
                       blink::mojom::CacheStorageError error,
                       blink::mojom::FetchAPIResponsePtr cache_response);

  void DidMatchAllResponsesForRequest(
      base::OnceClosure callback,
      blink::mojom::CacheStorageError error,
      std::vector<blink::mojom::FetchAPIResponsePtr> cache_responses);

  void FinishWithError(blink::mojom::BackgroundFetchError error) override;

  std::string HistogramName() const override;

  BackgroundFetchRegistrationId registration_id_;
  std::unique_ptr<BackgroundFetchRequestMatchParams> match_params_;
  SettledFetchesCallback settled_fetches_callback_;

  // SettledFetchesCallback params.
  std::vector<BackgroundFetchSettledFetch> settled_fetches_;
  blink::mojom::BackgroundFetchFailureReason failure_reason_ =
      blink::mojom::BackgroundFetchFailureReason::NONE;

  // Storage params.
  CacheStorageCacheHandle handle_;
  std::vector<proto::BackgroundFetchCompletedRequest> completed_requests_;
  blink::mojom::BackgroundFetchError error_ =
      blink::mojom::BackgroundFetchError::NONE;

  base::WeakPtrFactory<GetSettledFetchesTask> weak_factory_;  // Keep as last.

  DISALLOW_COPY_AND_ASSIGN(GetSettledFetchesTask);
};

}  // namespace background_fetch

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_GET_SETTLED_FETCHES_TASK_H_
