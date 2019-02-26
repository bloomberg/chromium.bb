// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/delete_registration_task.h"

#include <utility>

#include "base/barrier_closure.h"
#include "content/browser/background_fetch/background_fetch.pb.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"

namespace content {

namespace background_fetch {

namespace {
#if DCHECK_IS_ON()
// Checks that the |ActiveRegistrationUniqueIdKey| either does not exist, or is
// associated with a different |unique_id| than the given one which should have
// been already marked for deletion.
void DCheckRegistrationNotActive(const std::string& unique_id,
                                 const std::vector<std::string>& data,
                                 blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kOk:
      DCHECK_EQ(1u, data.size());
      DCHECK_NE(unique_id, data[0])
          << "Must call MarkRegistrationForDeletion before DeleteRegistration";
      return;
    case DatabaseStatus::kFailed:
      return;  // TODO(crbug.com/780025): Consider logging failure to UMA.
    case DatabaseStatus::kNotFound:
      return;
  }
}
#endif  // DCHECK_IS_ON()
}  // namespace

DeleteRegistrationTask::DeleteRegistrationTask(
    DatabaseTaskHost* host,
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& unique_id,
    HandleBackgroundFetchErrorCallback callback)
    : DatabaseTask(host),
      service_worker_registration_id_(service_worker_registration_id),
      origin_(origin),
      unique_id_(unique_id),
      callback_(std::move(callback)),
      weak_factory_(this) {}

DeleteRegistrationTask::~DeleteRegistrationTask() = default;

void DeleteRegistrationTask::Start() {
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      2u, base::BindOnce(&DeleteRegistrationTask::FinishWithError,
                         weak_factory_.GetWeakPtr(),
                         blink::mojom::BackgroundFetchError::NONE));

#if DCHECK_IS_ON()
  // Get the registration |developer_id| to check it was deactivated.
  service_worker_context()->GetRegistrationUserData(
      service_worker_registration_id_, {RegistrationKey(unique_id_)},
      base::BindOnce(&DeleteRegistrationTask::DidGetRegistration,
                     weak_factory_.GetWeakPtr(), barrier_closure));
#else
  DidGetRegistration(barrier_closure, {}, blink::ServiceWorkerStatusCode::kOk);
#endif  // DCHECK_IS_ON()

  cache_manager()->DeleteCache(
      origin_, CacheStorageOwner::kBackgroundFetch, unique_id_ /* cache_name */,
      base::BindOnce(&DeleteRegistrationTask::DidDeleteCache,
                     weak_factory_.GetWeakPtr(), barrier_closure));
}

void DeleteRegistrationTask::DidGetRegistration(
    base::OnceClosure done_closure,
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
#if DCHECK_IS_ON()
  if (ToDatabaseStatus(status) == DatabaseStatus::kOk) {
    DCHECK_EQ(1u, data.size());
    proto::BackgroundFetchMetadata metadata_proto;
    if (metadata_proto.ParseFromString(data[0]) &&
        metadata_proto.registration().has_developer_id()) {
      service_worker_context()->GetRegistrationUserData(
          service_worker_registration_id_,
          {ActiveRegistrationUniqueIdKey(
              metadata_proto.registration().developer_id())},
          base::BindOnce(&DCheckRegistrationNotActive, unique_id_));
    } else {
      // Service worker database has been corrupted. Abandon all fetches.
      SetStorageError(BackgroundFetchStorageError::kServiceWorkerStorageError);
      AbandonFetches(service_worker_registration_id_);
      std::move(done_closure).Run();
    }
  } else {
    // TODO(crbug.com/780025): Log failure to UMA.
  }
#endif  // DCHECK_IS_ON()

  std::vector<std::string> deletion_key_prefixes{
      RegistrationKey(unique_id_), UIOptionsKey(unique_id_),
      PendingRequestKeyPrefix(unique_id_), ActiveRequestKeyPrefix(unique_id_),
      CompletedRequestKeyPrefix(unique_id_)};

  service_worker_context()->ClearRegistrationUserDataByKeyPrefixes(
      service_worker_registration_id_, std::move(deletion_key_prefixes),
      base::BindOnce(&DeleteRegistrationTask::DidDeleteRegistration,
                     weak_factory_.GetWeakPtr(), std::move(done_closure)));
}

void DeleteRegistrationTask::DidDeleteRegistration(
    base::OnceClosure done_closure,
    blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kOk:
    case DatabaseStatus::kNotFound:
      break;
    case DatabaseStatus::kFailed:
      SetStorageError(BackgroundFetchStorageError::kServiceWorkerStorageError);
      break;
  }
  std::move(done_closure).Run();
}

void DeleteRegistrationTask::DidDeleteCache(
    base::OnceClosure done_closure,
    blink::mojom::CacheStorageError error) {
  if (error != blink::mojom::CacheStorageError::kSuccess &&
      error != blink::mojom::CacheStorageError::kErrorNotFound) {
    SetStorageError(BackgroundFetchStorageError::kCacheStorageError);
  }
  std::move(done_closure).Run();
}

void DeleteRegistrationTask::FinishWithError(
    blink::mojom::BackgroundFetchError error) {
  if (HasStorageError())
    error = blink::mojom::BackgroundFetchError::STORAGE_ERROR;
  ReportStorageError();
  std::move(callback_).Run(error);
  Finished();  // Destroys |this|.
}

std::string DeleteRegistrationTask::HistogramName() const {
  return "DeleteRegistrationTask";
}

}  // namespace background_fetch

}  // namespace content
