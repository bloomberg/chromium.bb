// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/session_state/web_session_state_tab_helper.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#import "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#import "build/branding_buildflags.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/web/features.h"
#import "ios/chrome/browser/web/session_state/web_session_state_cache.h"
#import "ios/chrome/browser/web/session_state/web_session_state_cache_factory.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#include "ios/web/common/features.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#include "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Maximum size of session state NSData object in kilobyes.
const int64_t kMaxSessionState = 1024 * 5;  // 5MB

}  // anonymous namespace

// static
void WebSessionStateTabHelper::CreateForWebState(web::WebState* web_state) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(
        UserDataKey(),
        base::WrapUnique(new WebSessionStateTabHelper(web_state)));
  }
}

// static
bool WebSessionStateTabHelper::IsEnabled() {
  if (!base::FeatureList::IsEnabled(web::kRestoreSessionFromCache)) {
    return false;
  }

  // This API is only available on iOS 15.
  if (@available(iOS 15, *)) {
    return true;
  }
  return false;
}

WebSessionStateTabHelper::WebSessionStateTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
}

WebSessionStateTabHelper::~WebSessionStateTabHelper() = default;

ChromeBrowserState* WebSessionStateTabHelper::GetBrowserState() {
  return ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState());
}

bool WebSessionStateTabHelper::RestoreSessionFromCache() {
  if (!IsEnabled())
    return false;

  WebSessionStateCache* cache =
      WebSessionStateCacheFactory::GetForBrowserState(GetBrowserState());
  NSData* data = [cache sessionStateDataForWebState:web_state_];
  if (!data.length)
    return false;

  if (!web_state_->SetSessionStateData(data))
    return false;

  // If this fails (e.g., see crbug.com/1019672 for a previous failure), this
  // implies a bug in WebKit session restoration.
  web::NavigationManager* navigationManager =
      web_state_->GetNavigationManager();
  DCHECK(navigationManager->GetItemCount());
  web::GetWebClient()->CleanupNativeRestoreURLs(web_state_);
  return true;
}

void WebSessionStateTabHelper::SaveSessionStateIfStale() {
  if (!stale_)
    return;
  SaveSessionState();
}

void WebSessionStateTabHelper::SaveSessionState() {
  if (!IsEnabled())
    return;

  stale_ = false;

  NSData* data = web_state_->SessionStateData();
  if (data) {
    int64_t size_kb = data.length / 1024;
    UMA_HISTOGRAM_COUNTS_100000("Session.WebState.CustomWebViewSerializedSize",
                                size_kb);

    WebSessionStateCache* cache =
        WebSessionStateCacheFactory::GetForBrowserState(GetBrowserState());
    // To prevent very large session states from using too much space, don't
    // persist any |data| larger than 5MB.  If this happens, remove the now
    // stale session state data.
    if (size_kb > kMaxSessionState) {
      [cache removeSessionStateDataForWebState:web_state_];
      return;
    }

    [cache persistSessionStateData:data forWebState:web_state_];
  }
}

#pragma mark - WebStateObserver

void WebSessionStateTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
  if (stale_) {
    SaveSessionState();
  }
}

void WebSessionStateTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  MarkStale();
}

void WebSessionStateTabHelper::WebFrameDidBecomeAvailable(
    web::WebState* web_state,
    web::WebFrame* web_frame) {
  if (web_frame->IsMainFrame())
    return;

  // -WebFrameDidBecomeAvailable is called much more often than navigations, so
  // check if either |item_count_| or |last_committed_item_index_| has changed
  // before marking a page as stale.
  web::NavigationManager* navigationManager = web_state->GetNavigationManager();
  if (item_count_ == navigationManager->GetItemCount() &&
      last_committed_item_index_ ==
          navigationManager->GetLastCommittedItemIndex())
    return;

  MarkStale();
}

#pragma mark - Private

void WebSessionStateTabHelper::MarkStale() {
  if (!IsEnabled())
    return;

  web::NavigationManager* navigationManager =
      web_state_->GetNavigationManager();
  item_count_ = navigationManager->GetItemCount();
  last_committed_item_index_ = navigationManager->GetLastCommittedItemIndex();

  stale_ = true;
}

WEB_STATE_USER_DATA_KEY_IMPL(WebSessionStateTabHelper)
