// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_AUTO_FETCH_PAGE_LOAD_WATCHER_H_
#define CHROME_BROWSER_OFFLINE_PAGES_AUTO_FETCH_PAGE_LOAD_WATCHER_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/client_id.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace offline_pages {
class SavePageRequest;
class RequestCoordinator;

// Cancels active auto fetch requests if a page loads.
class AutoFetchPageLoadWatcher : public RequestCoordinator::Observer {
 public:
  static void CreateForWebContents(content::WebContents* web_contents);

  explicit AutoFetchPageLoadWatcher(RequestCoordinator* request_coordinator);
  ~AutoFetchPageLoadWatcher() override;

  // RequestCoordinator::Observer methods. These keep
  // |live_auto_fetch_requests_| in sync with the state of RequestCoordinator.
  void OnAdded(const SavePageRequest& request) override;
  void OnCompleted(const SavePageRequest& request,
                   RequestNotifier::BackgroundSavePageResult status) override;
  void OnChanged(const SavePageRequest& request) override {}
  void OnNetworkProgress(const SavePageRequest& request,
                         int64_t received_bytes) override {}

 protected:
  class NavigationObserver;

  // Virtual for testing only.
  virtual void RemoveRequests(const std::vector<int64_t>& request_ids);

  // Called when navigation completes. Triggers cancellation of any
  // in-flight auto fetch requests that match a successful navigation.
  void HandlePageNavigation(const GURL& url);

  void ObserverInitialize(
      std::vector<std::unique_ptr<SavePageRequest>> all_requests);

  offline_pages::RequestCoordinator* GetRequestCoordinator();

  // Whether ObserverInitialize has been called.
  bool observer_ready_ = false;

  RequestCoordinator* request_coordinator_;
  std::vector<GURL> pages_loaded_before_observer_ready_;

  // This represents the set of auto fetch request IDs currently in the
  // RequestCoordinator's queue, mapped by URL.
  std::map<GURL, std::vector<int64_t>> live_auto_fetch_requests_;

  base::WeakPtrFactory<AutoFetchPageLoadWatcher> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AutoFetchPageLoadWatcher);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_AUTO_FETCH_PAGE_LOAD_WATCHER_H_
