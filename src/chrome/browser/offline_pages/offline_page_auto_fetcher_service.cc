// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_auto_fetcher_service.h"

#include <string>
#include <utility>

#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "components/offline_pages/core/auto_fetch.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/client_id.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "url/gurl.h"

namespace offline_pages {

namespace {
constexpr int kMaximumInFlight = 3;

bool URLMatches(const GURL& a, const GURL& b) {
  // We strip the fragment, because when loading an offline page, only the most
  // recent page saved with the URL (fragment stripped) can be loaded.
  GURL::Replacements remove_fragment;
  remove_fragment.ClearRef();
  return a.ReplaceComponents(remove_fragment) ==
         b.ReplaceComponents(remove_fragment);
}

SavePageRequest* FindRequest(
    const std::vector<std::unique_ptr<SavePageRequest>>& all_requests,
    const GURL& url) {
  for (const auto& request : all_requests) {
    if (request->client_id().name_space == kAutoAsyncNamespace &&
        URLMatches(request->url(), url))
      return request.get();
  }
  return nullptr;
}
}  // namespace

// This is an attempt to verify that a task callback eventually calls
// TaskComplete exactly once. If the token is never std::move'd, it will DCHECK
// when it is destroyed.
class OfflinePageAutoFetcherService::TaskToken {
 public:
  // The static methods should only be called by StartOrEnqueue or TaskComplete.
  static TaskToken NewToken() { return TaskToken(); }
  static void Finalize(TaskToken* token) { token->alive_ = false; }

  TaskToken(TaskToken&& other) : alive_(other.alive_) {
    DCHECK(other.alive_);
    other.alive_ = false;
  }
  ~TaskToken() { DCHECK(!alive_); }

 private:
  TaskToken() {}

  bool alive_ = true;
  DISALLOW_COPY_AND_ASSIGN(TaskToken);
};

OfflinePageAutoFetcherService::OfflinePageAutoFetcherService(
    RequestCoordinator* request_coordinator)
    : page_load_watcher_(request_coordinator),
      request_coordinator_(request_coordinator) {}
OfflinePageAutoFetcherService::~OfflinePageAutoFetcherService() {}

void OfflinePageAutoFetcherService::TrySchedule(bool user_requested,
                                                const GURL& url,
                                                int android_tab_id,
                                                TryScheduleCallback callback) {
  StartOrEnqueue(base::BindOnce(
      &OfflinePageAutoFetcherService::TryScheduleStep1, GetWeakPtr(),
      user_requested, url, android_tab_id, std::move(callback)));
}

void OfflinePageAutoFetcherService::CancelSchedule(const GURL& url) {
  StartOrEnqueue(base::BindOnce(
      &OfflinePageAutoFetcherService::CancelScheduleStep1, GetWeakPtr(), url));
}

void OfflinePageAutoFetcherService::TryScheduleStep1(
    bool user_requested,
    const GURL& url,
    int android_tab_id,
    TryScheduleCallback callback,
    TaskToken token) {
  // Return an early failure if the URL is not suitable.
  if (!OfflinePageModel::CanSaveURL(url)) {
    std::move(callback).Run(OfflinePageAutoFetcherScheduleResult::kOtherError);
    TaskComplete(std::move(token));
    return;
  }

  // We need to do some checks on in-flight requests before scheduling the
  // fetch. So first, get the list of all requests, and proceed to step 2.
  request_coordinator_->GetAllRequests(
      base::BindOnce(&OfflinePageAutoFetcherService::TryScheduleStep2,
                     GetWeakPtr(), std::move(token), user_requested, url,
                     android_tab_id, std::move(callback),
                     // Unretained is OK because coordinator is calling us back.
                     base::Unretained(request_coordinator_)));
}

void OfflinePageAutoFetcherService::TryScheduleStep2(
    TaskToken token,
    bool user_requested,
    const GURL& url,
    int android_tab_id,
    TryScheduleCallback callback,
    RequestCoordinator* coordinator,
    std::vector<std::unique_ptr<SavePageRequest>> all_requests) {
  // If a request for this page is already scheduled, report scheduling as
  // successful without doing anything.
  if (FindRequest(all_requests, url)) {
    std::move(callback).Run(
        OfflinePageAutoFetcherScheduleResult::kAlreadyScheduled);
    TaskComplete(std::move(token));
    return;
  }

  // Respect kMaximumInFlight.
  if (!user_requested) {
    int in_flight_count = 0;
    for (const auto& request : all_requests) {
      if (request->client_id().name_space == kAutoAsyncNamespace) {
        ++in_flight_count;
      }
    }
    if (in_flight_count >= kMaximumInFlight) {
      std::move(callback).Run(
          OfflinePageAutoFetcherScheduleResult::kNotEnoughQuota);
      TaskComplete(std::move(token));
      return;
    }
  }

  // Finally, schedule a new request, and proceed to step 3.
  RequestCoordinator::SavePageLaterParams params;
  params.url = url;
  auto_fetch::ClientIdMetadata metadata(android_tab_id);
  params.client_id = auto_fetch::MakeClientId(metadata);
  params.user_requested = false;
  params.availability =
      RequestCoordinator::RequestAvailability::ENABLED_FOR_OFFLINER;
  coordinator->SavePageLater(
      params,
      base::BindOnce(&OfflinePageAutoFetcherService::TryScheduleStep3,
                     GetWeakPtr(), std::move(token), std::move(callback)));
}

void OfflinePageAutoFetcherService::TryScheduleStep3(
    TaskToken token,
    TryScheduleCallback callback,
    AddRequestResult result) {
  // Just forward the response to the mojo caller.
  std::move(callback).Run(
      result == AddRequestResult::SUCCESS
          ? OfflinePageAutoFetcherScheduleResult::kScheduled
          : OfflinePageAutoFetcherScheduleResult::kOtherError);
  TaskComplete(std::move(token));
}

void OfflinePageAutoFetcherService::CancelScheduleStep1(const GURL& url,
                                                        TaskToken token) {
  // Get all requests, and proceed to step 2.
  request_coordinator_->GetAllRequests(base::BindOnce(
      &OfflinePageAutoFetcherService::CancelScheduleStep2, GetWeakPtr(),
      std::move(token), url, request_coordinator_));
}

void OfflinePageAutoFetcherService::CancelScheduleStep2(
    TaskToken token,
    const GURL& url,
    RequestCoordinator* coordinator,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  // Cancel the request if it's found in the list of all requests.
  SavePageRequest* matching_request = FindRequest(requests, url);
  if (matching_request) {
    coordinator->RemoveRequests(
        {matching_request->request_id()},
        base::BindOnce(&OfflinePageAutoFetcherService::CancelScheduleStep3,
                       GetWeakPtr(), std::move(token)));
    return;
  }
  TaskComplete(std::move(token));
}

void OfflinePageAutoFetcherService::CancelScheduleStep3(
    TaskToken token,
    const MultipleItemStatuses&) {
  TaskComplete(std::move(token));
}

void OfflinePageAutoFetcherService::StartOrEnqueue(TaskCallback task) {
  bool can_run = task_queue_.empty();
  task_queue_.push(std::move(task));
  if (can_run)
    std::move(task_queue_.front()).Run(TaskToken::NewToken());
}

void OfflinePageAutoFetcherService::TaskComplete(TaskToken token) {
  TaskToken::Finalize(&token);
  DCHECK(!task_queue_.empty());
  DCHECK(!task_queue_.front());
  task_queue_.pop();
  if (!task_queue_.empty())
    std::move(task_queue_.front()).Run(TaskToken::NewToken());
}

bool OfflinePageAutoFetcherService::IsTaskQueueEmptyForTesting() {
  return task_queue_.empty();
}

}  // namespace offline_pages
