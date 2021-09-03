// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/sessions/ios_chrome_local_session_event_router.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/check.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/main/all_web_state_list_observation_registrar.h"
#include "ios/chrome/browser/sync/glue/sync_start_util.h"
#include "ios/chrome/browser/sync/ios_chrome_synced_tab_delegate.h"
#include "ios/chrome/browser/tabs/tab_parenting_global_observer.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

sync_sessions::SyncedTabDelegate* GetSyncedTabDelegateFromWebState(
    web::WebState* web_state) {
  sync_sessions::SyncedTabDelegate* delegate =
      IOSChromeSyncedTabDelegate::FromWebState(web_state);
  return delegate;
}

}  // namespace

IOSChromeLocalSessionEventRouter::IOSChromeLocalSessionEventRouter(
    ChromeBrowserState* browser_state,
    sync_sessions::SyncSessionsClient* sessions_client,
    const syncer::SyncableService::StartSyncFlare& flare)
    : handler_(nullptr), sessions_client_(sessions_client), flare_(flare) {
  tab_parented_subscription_ =
      TabParentingGlobalObserver::GetInstance()->RegisterCallback(
          base::BindRepeating(&IOSChromeLocalSessionEventRouter::OnTabParented,
                              base::Unretained(this)));

  registrars_.insert(std::make_unique<AllWebStateListObservationRegistrar>(
      browser_state, std::make_unique<Observer>(this),
      AllWebStateListObservationRegistrar::Mode::REGULAR));
}

IOSChromeLocalSessionEventRouter::~IOSChromeLocalSessionEventRouter() {}

IOSChromeLocalSessionEventRouter::Observer::Observer(
    IOSChromeLocalSessionEventRouter* session_router)
    : router_(session_router) {}

IOSChromeLocalSessionEventRouter::Observer::~Observer() {}

void IOSChromeLocalSessionEventRouter::Observer::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  web_state->AddObserver(this);
}

void IOSChromeLocalSessionEventRouter::Observer::WebStateReplacedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int index) {
  router_->OnWebStateChange(old_web_state);
  old_web_state->RemoveObserver(this);
  DCHECK(new_web_state);
  new_web_state->AddObserver(this);
}

void IOSChromeLocalSessionEventRouter::Observer::WebStateDetachedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  router_->OnWebStateChange(web_state);
  web_state->RemoveObserver(this);
}

void IOSChromeLocalSessionEventRouter::Observer::TitleWasSet(
    web::WebState* web_state) {
  router_->OnWebStateChange(web_state);
}

void IOSChromeLocalSessionEventRouter::Observer::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  router_->OnWebStateChange(web_state);
}

void IOSChromeLocalSessionEventRouter::Observer::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  router_->OnWebStateChange(web_state);
}

void IOSChromeLocalSessionEventRouter::Observer::DidChangeBackForwardState(
    web::WebState* web_state) {
  router_->OnWebStateChange(web_state);
}

void IOSChromeLocalSessionEventRouter::Observer::WebStateDestroyed(
    web::WebState* web_state) {
  router_->OnWebStateChange(web_state);
  web_state->RemoveObserver(this);
}

void IOSChromeLocalSessionEventRouter::OnTabParented(web::WebState* web_state) {
  OnWebStateChange(web_state);
}

void IOSChromeLocalSessionEventRouter::Observer::WillBeginBatchOperation(
    WebStateList* web_state_list) {
  router_->OnSessionEventStarting();
}

void IOSChromeLocalSessionEventRouter::Observer::BatchOperationEnded(
    WebStateList* web_state_list) {
  router_->OnSessionEventEnded();
}

void IOSChromeLocalSessionEventRouter::OnSessionEventStarting() {
  batch_in_progress_++;
}

void IOSChromeLocalSessionEventRouter::OnSessionEventEnded() {
  DCHECK(batch_in_progress_ > 0);
  batch_in_progress_--;
  if (batch_in_progress_)
    return;
  // Batch operations are only used for restoration, close all tabs or undo
  // close all tabs. In any case, a full sync is necessary after this.
  if (handler_)
    handler_->OnSessionRestoreComplete();
  if (!flare_.is_null()) {
    flare_.Run(syncer::SESSIONS);
    flare_.Reset();
  }
}

void IOSChromeLocalSessionEventRouter::OnWebStateChange(
    web::WebState* web_state) {
  if (batch_in_progress_)
    return;
  sync_sessions::SyncedTabDelegate* tab =
      GetSyncedTabDelegateFromWebState(web_state);
  if (!tab)
    return;
  if (handler_)
    handler_->OnLocalTabModified(tab);
  if (!tab->ShouldSync(sessions_client_))
    return;

  if (!flare_.is_null()) {
    flare_.Run(syncer::SESSIONS);
    flare_.Reset();
  }
}

void IOSChromeLocalSessionEventRouter::StartRoutingTo(
    sync_sessions::LocalSessionEventHandler* handler) {
  DCHECK(!handler_);
  handler_ = handler;
}

void IOSChromeLocalSessionEventRouter::Stop() {
  handler_ = nullptr;
}
