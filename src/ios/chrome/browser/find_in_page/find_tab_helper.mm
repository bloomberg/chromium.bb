// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/find_tab_helper.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/find_in_page/find_in_page_controller.h"
#import "ios/chrome/browser/find_in_page/find_in_page_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kFindActionName[] = "Find";
const char kFindNextActionName[] = "FindNext";
const char kFindPreviousActionName[] = "FindPrevious";

FindTabHelper::FindTabHelper(web::WebState* web_state) {
  web_state->AddObserver(this);
  controller_ = [[FindInPageController alloc] initWithWebState:web_state];
}

FindTabHelper::~FindTabHelper() {}

void FindTabHelper::SetResponseDelegate(
    id<FindInPageResponseDelegate> response_delegate) {
  controller_.responseDelegate = response_delegate;
}

void FindTabHelper::StartFinding(NSString* search_term) {
  base::RecordAction(base::UserMetricsAction(kFindActionName));
  [controller_ findStringInPage:search_term];
}

void FindTabHelper::ContinueFinding(FindDirection direction) {
  if (direction == FORWARD) {
    base::RecordAction(base::UserMetricsAction(kFindNextActionName));
    [controller_ findNextStringInPage];

  } else if (direction == REVERSE) {
    base::RecordAction(base::UserMetricsAction(kFindPreviousActionName));
    [controller_ findPreviousStringInPage];

  } else {
    NOTREACHED();
  }
}

void FindTabHelper::StopFinding() {
  SetFindUIActive(false);
  [controller_ disableFindInPage];
}

FindInPageModel* FindTabHelper::GetFindResult() const {
  return controller_.findInPageModel;
}

bool FindTabHelper::CurrentPageSupportsFindInPage() const {
  return [controller_ canFindInPage];
}

bool FindTabHelper::IsFindUIActive() const {
  return controller_.findInPageModel.enabled;
}

void FindTabHelper::SetFindUIActive(bool active) {
  controller_.findInPageModel.enabled = active;
}

void FindTabHelper::PersistSearchTerm() {
  [controller_ saveSearchTerm];
}

void FindTabHelper::RestoreSearchTerm() {
  [controller_ restoreSearchTerm];
}

void FindTabHelper::WebStateDestroyed(web::WebState* web_state) {
  [controller_ detachFromWebState];
  web_state->RemoveObserver(this);
}

void FindTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (IsFindUIActive()) {
    StopFinding();
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(FindTabHelper)
