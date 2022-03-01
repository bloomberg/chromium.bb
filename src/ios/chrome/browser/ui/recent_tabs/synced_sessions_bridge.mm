// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/synced_sessions_bridge.h"

#include "base/bind.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync_sessions/session_sync_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/session_sync_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace synced_sessions {

#pragma mark - SyncedSessionsObserverBridge

SyncedSessionsObserverBridge::SyncedSessionsObserverBridge(
    id<SyncedSessionsObserver> owner,
    ChromeBrowserState* browserState)
    : owner_(owner),
      identity_manager_(
          IdentityManagerFactory::GetForBrowserState(browserState)) {
  identity_manager_observation_.Observe(identity_manager_);

  // base::Unretained() is safe below because the subscription itself is a class
  // member field and handles destruction well.
  foreign_session_updated_subscription_ =
      SessionSyncServiceFactory::GetForBrowserState(browserState)
          ->SubscribeToForeignSessionsChanged(base::BindRepeating(
              &SyncedSessionsObserverBridge::OnForeignSessionChanged,
              base::Unretained(this)));
}

SyncedSessionsObserverBridge::~SyncedSessionsObserverBridge() {}

#pragma mark - signin::IdentityManager::Observer

void SyncedSessionsObserverBridge::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSync)) {
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      // Ignored.
      break;
    case signin::PrimaryAccountChangeEvent::Type::kSet:
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      // Update the session Sync state if consent is given or removed.
      [owner_ reloadSessions];
      break;
  }
}

#pragma mark - Signin and syncing status

bool SyncedSessionsObserverBridge::HasSyncConsent() {
  return identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync);
}

void SyncedSessionsObserverBridge::OnForeignSessionChanged() {
  [owner_ reloadSessions];
}

}  // namespace synced_sessions
