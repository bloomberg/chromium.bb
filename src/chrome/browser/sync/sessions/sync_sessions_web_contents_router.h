// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SESSIONS_SYNC_SESSIONS_WEB_CONTENTS_ROUTER_H_
#define CHROME_BROWSER_SYNC_SESSIONS_SYNC_SESSIONS_WEB_CONTENTS_ROUTER_H_

#include <memory>
#include <set>

#include "base/callback_list.h"

// Android has no BrowserList or TabStripModel, so we exclude code that refers
// to those two things. For non-android platforms, this code is used to
// ensure we are notified of tabs being added and moved between tab strips.

#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync_sessions/local_session_event_router.h"

namespace content {
class WebContents;
}  // namespace content

class Profile;

namespace sync_sessions {

class BrowserListRouterHelper;

// WebContentsObserver-based implementation of LocalSessionEventRouter. This
// class is responsible for notifying Sessions Sync when local tabs are
// modified. It does this by forwarding the events pushed to it by individual
// WebContentsObservers, which are scoped to a single WebContents/tab.
class SyncSessionsWebContentsRouter : public LocalSessionEventRouter,
                                      public KeyedService {
 public:
  explicit SyncSessionsWebContentsRouter(Profile* profile);

  // Notify the router that the tab corresponding to |web_contents| has been
  // modified in some way.
  void NotifyTabModified(content::WebContents* web_contents,
                         bool page_load_completed);
  // Notify the router that session restore has completed.
  void NotifySessionRestoreComplete();
  // Inject a flare that can be used to start sync. See the comment for
  // StartSyncFlare in syncable_service.h for more.
  void InjectStartSyncFlare(syncer::SyncableService::StartSyncFlare flare);

  // SessionsSyncManager::LocalEventRouter implementation.
  void StartRoutingTo(LocalSessionEventHandler* handler) override;
  void Stop() override;

  // KeyedService implementation.
  void Shutdown() override;

 protected:
  ~SyncSessionsWebContentsRouter() override;

 private:
  void OnFaviconsChanged(const std::set<GURL>& page_urls, const GURL& icon_url);

  std::unique_ptr<base::CallbackList<void(const std::set<GURL>&,
                                          const GURL&)>::Subscription>
      favicon_changed_subscription_;
  syncer::SyncableService::StartSyncFlare flare_;
  LocalSessionEventHandler* handler_ = nullptr;

#if !defined(OS_ANDROID)
  std::unique_ptr<BrowserListRouterHelper> browser_list_helper_;
#endif  // !defined(OS_ANDROID)

  DISALLOW_COPY_AND_ASSIGN(SyncSessionsWebContentsRouter);
};

}  // namespace sync_sessions

#endif  // CHROME_BROWSER_SYNC_SESSIONS_SYNC_SESSIONS_WEB_CONTENTS_ROUTER_H_
