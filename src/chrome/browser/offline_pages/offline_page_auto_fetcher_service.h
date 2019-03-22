// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_AUTO_FETCHER_SERVICE_H_
#define CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_AUTO_FETCHER_SERVICE_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/offline_pages/auto_fetch_page_load_watcher.h"
#include "chrome/common/offline_page_auto_fetcher.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_pages/core/background/request_queue_results.h"
#include "url/gurl.h"

namespace offline_pages {
class RequestCoordinator;
class SavePageRequest;

// Implementation notes for the auto-fetch-on-net-error-page feature:
//
// The 'auto-fetch-on-net-error-page' (auto-fetch) feature kicks in when a
// 'dino' (offline error) page is reached. Chrome will schedule a request to
// save the page when the device gains connectivity. Users can cancel or
// explicitly request this behavior through UI on the dino page. Chrome attempts
// to avoid doing a background page save if the user ends up successfully
// navigating to the page. If the page is saved in the background, the a system
// notification is presented.
//
// Background page saves are implemented through |RequestCoordinator|. The
// |OfflinePageClientPolicy| for this feature is configured with the option
// |defer_background_fetch_while_page_is_active|. This instructs
// |RequestCoordinator| to first check if the page to be saved is currently
// active. If it is, the request is deferred. If a request is deferred 5 times,
// it is considered failed and removed. For this feature, we expect this
// condition to be rare because |RequestCoordinator| only processes requests
// when the device is connected, and the dino page automatically reloads when
// the device is connected.
//
// Additionally, save page requests are removed upon successful navigation
// commit. See |AutoFetchPageLoadWatcher|.

// A KeyedService that provides an interface to schedule and cancel auto-fetch
// requests.
class OfflinePageAutoFetcherService : public KeyedService {
 public:
  using OfflinePageAutoFetcherScheduleResult =
      chrome::mojom::OfflinePageAutoFetcherScheduleResult;
  using TryScheduleCallback = base::OnceCallback<void(
      chrome::mojom::OfflinePageAutoFetcherScheduleResult)>;

  explicit OfflinePageAutoFetcherService(
      RequestCoordinator* request_coordinator);
  ~OfflinePageAutoFetcherService() override;

  AutoFetchPageLoadWatcher* page_load_watcher() { return &page_load_watcher_; }

  // Auto fetching interface. Schedules and cancels fetch requests.

  void TrySchedule(bool user_requested,
                   const GURL& url,
                   int android_tab_id,
                   TryScheduleCallback callback);
  void CancelSchedule(const GURL& url);

  // KeyedService implementation.

  void Shutdown() override {}

  // Testing methods.
  bool IsTaskQueueEmptyForTesting();

 private:
  class TaskToken;
  using TaskCallback = base::OnceCallback<void(TaskToken)>;

  base::WeakPtr<OfflinePageAutoFetcherService> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Task management methods. Each request made to this class is serialized by
  // appending the tasks as callbacks to task_queue_.

  // Starts or enqueues a new task.
  void StartOrEnqueue(TaskCallback task);
  // Reports a task complete. Must be called when the task is completely
  // finished.
  void TaskComplete(TaskToken token);

  // Implementation details for public methods.

  void TryScheduleStep1(bool user_requested,
                        const GURL& url,
                        int android_tab_id,
                        TryScheduleCallback callback,
                        TaskToken token);
  void TryScheduleStep2(
      TaskToken token,
      bool user_requested,
      const GURL& url,
      int android_tab_id,
      TryScheduleCallback callback,
      RequestCoordinator* coordinator,
      std::vector<std::unique_ptr<SavePageRequest>> all_requests);
  void TryScheduleStep3(TaskToken token,
                        TryScheduleCallback callback,
                        AddRequestResult result);

  void CancelScheduleStep1(const GURL& url, TaskToken token);
  void CancelScheduleStep2(
      TaskToken token,
      const GURL& url,
      RequestCoordinator* coordinator,
      std::vector<std::unique_ptr<SavePageRequest>> requests);
  void CancelScheduleStep3(TaskToken token, const MultipleItemStatuses&);

  AutoFetchPageLoadWatcher page_load_watcher_;
  RequestCoordinator* request_coordinator_;
  // TODO(harringtond): Pull out task management into another class, or use
  // offline_pages::TaskQueue.
  std::queue<TaskCallback> task_queue_;
  base::WeakPtrFactory<OfflinePageAutoFetcherService> weak_ptr_factory_{this};
};

}  // namespace offline_pages
#endif  // CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_AUTO_FETCHER_SERVICE_H_
