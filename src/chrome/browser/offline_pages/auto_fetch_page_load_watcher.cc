// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/auto_fetch_page_load_watcher.h"

#include <memory>

#include "base/macros.h"
#include "chrome/browser/offline_pages/offline_page_auto_fetcher.h"
#include "chrome/browser/offline_pages/offline_page_auto_fetcher_service.h"
#include "chrome/browser/offline_pages/offline_page_auto_fetcher_service_factory.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace offline_pages {

// Observes a WebContents to relay navigation events to
// AutoFetchPageLoadWatcher.
class AutoFetchPageLoadWatcher::NavigationObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          AutoFetchPageLoadWatcher::NavigationObserver> {
 public:
  explicit NavigationObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {
    helper_ = OfflinePageAutoFetcherServiceFactory::GetForBrowserContext(
                  web_contents->GetBrowserContext())
                  ->page_load_watcher();
    DCHECK(helper_);
  }

  // content::WebContentsObserver implementation.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInMainFrame() ||
        !navigation_handle->HasCommitted() || navigation_handle->IsErrorPage())
      return;

    // Note: The redirect chain includes the final URL. We consider all URLs
    // along the redirect chain as successful.
    for (const auto& u : navigation_handle->GetRedirectChain()) {
      helper_->HandlePageNavigation(u);
    }
  }

 private:
  AutoFetchPageLoadWatcher* helper_;

  DISALLOW_COPY_AND_ASSIGN(NavigationObserver);
};

// static
void AutoFetchPageLoadWatcher::CreateForWebContents(
    content::WebContents* web_contents) {
  OfflinePageAutoFetcherService* service =
      OfflinePageAutoFetcherServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  // Don't try to create if the service isn't available (happens in incognito
  // mode).
  if (service) {
    NavigationObserver::CreateForWebContents(web_contents);
  }
}

AutoFetchPageLoadWatcher::AutoFetchPageLoadWatcher(
    RequestCoordinator* request_coordinator)
    : request_coordinator_(request_coordinator) {
  request_coordinator_->AddObserver(this);
  request_coordinator_->GetAllRequests(
      base::BindOnce(&AutoFetchPageLoadWatcher::ObserverInitialize,
                     weak_ptr_factory_.GetWeakPtr()));
}

AutoFetchPageLoadWatcher::~AutoFetchPageLoadWatcher() {
  request_coordinator_->RemoveObserver(this);
}

void AutoFetchPageLoadWatcher::OnAdded(const SavePageRequest& request) {
  if (!observer_ready_)
    return;
  if (request.client_id().name_space == kAutoAsyncNamespace) {
    live_auto_fetch_requests_[request.url()].push_back(request.request_id());
  }
}

void AutoFetchPageLoadWatcher::OnCompleted(
    const SavePageRequest& request,
    RequestNotifier::BackgroundSavePageResult status) {
  if (!observer_ready_)
    return;
  // A SavePageRequest is complete, remove our bookeeping of it in
  // |live_auto_fetch_requests_|.
  auto iter = live_auto_fetch_requests_.find(request.url());
  if (iter != live_auto_fetch_requests_.end()) {
    std::vector<int64_t>& id_list = iter->second;
    auto id_iter =
        std::find(id_list.begin(), id_list.end(), request.request_id());
    if (id_iter != id_list.end()) {
      id_list.erase(id_iter);
      if (id_list.empty())
        live_auto_fetch_requests_.erase(iter);
    }
  }
}

void AutoFetchPageLoadWatcher::RemoveRequests(
    const std::vector<int64_t>& request_ids) {
  request_coordinator_->RemoveRequests(request_ids, base::DoNothing());
}

void AutoFetchPageLoadWatcher::HandlePageNavigation(const GURL& url) {
  // Early exit for the common-case.
  if (observer_ready_ && live_auto_fetch_requests_.empty()) {
    return;
  }

  // Note: It is possible that this method is called before
  // ObserverInitialized, in which case we have to defer handling of the
  // event. Never accumulate more than a few, so we can't have a boundless
  // array if ObserverInitialize is never called.
  if (!observer_ready_) {
    if (pages_loaded_before_observer_ready_.size() < 10)
      pages_loaded_before_observer_ready_.push_back(url);
    return;
  }

  auto iter = live_auto_fetch_requests_.find(url);
  if (iter == live_auto_fetch_requests_.end())
    return;
  RemoveRequests(iter->second);
}

void AutoFetchPageLoadWatcher::ObserverInitialize(
    std::vector<std::unique_ptr<SavePageRequest>> all_requests) {
  observer_ready_ = true;
  for (const std::unique_ptr<SavePageRequest>& request : all_requests) {
    OnAdded(*request);
  }
  for (const GURL& url : pages_loaded_before_observer_ready_) {
    HandlePageNavigation(url);
  }
  pages_loaded_before_observer_ready_.clear();
}

}  // namespace offline_pages
