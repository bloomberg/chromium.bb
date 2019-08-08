// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar_badge_tab_helper.h"

#include "ios/chrome/browser/infobars/infobar.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper_delegate.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/infobars/infobar_ui_delegate.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - Public

// static
void InfobarBadgeTabHelper::CreateForWebState(web::WebState* web_state) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(
        UserDataKey(), base::WrapUnique(new InfobarBadgeTabHelper(web_state)));
  }
}

void InfobarBadgeTabHelper::SetDelegate(
    id<InfobarBadgeTabHelperDelegate> delegate) {
  delegate_ = delegate;
}

void InfobarBadgeTabHelper::UpdateBadgeForInfobarBannerDismissed() {
  delegate_.badgeState &= ~InfobarBadgeStateSelected;
}

void InfobarBadgeTabHelper::UpdateBadgeForInfobarModalPresented() {
  delegate_.badgeState |= InfobarBadgeStateSelected;
}

void InfobarBadgeTabHelper::UpdateBadgeForInfobarModalDismissed() {
  delegate_.badgeState &= ~InfobarBadgeStateSelected;
}

void InfobarBadgeTabHelper::UpdateBadgeForInfobarAccepted() {
  delegate_.badgeState |= InfobarBadgeStateAccepted;
}

bool InfobarBadgeTabHelper::IsInfobarBadgeDisplaying() {
  return is_infobar_displaying_;
}

InfobarBadgeTabHelper::~InfobarBadgeTabHelper() = default;

#pragma mark - Private

InfobarBadgeTabHelper::InfobarBadgeTabHelper(web::WebState* web_state)
    : infobar_observer_(this), is_infobar_displaying_(false) {
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(web_state);
  if (infoBarManager) {
    infobar_observer_.Add(infoBarManager);
  }
}

#pragma mark InfobarObserver

void InfobarBadgeTabHelper::OnInfoBarAdded(infobars::InfoBar* infobar) {
  this->UpdateBadgeForInfobar(infobar, true);
  // Set the badgeState to selected since the InfobarBanner has to be presented
  // on OnInfoBarAdded.
  delegate_.badgeState = InfobarBadgeStateSelected;
}

void InfobarBadgeTabHelper::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                             bool animate) {
  this->UpdateBadgeForInfobar(infobar, false);
}

void InfobarBadgeTabHelper::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  infobar_observer_.Remove(manager);
}

#pragma mark Helpers

void InfobarBadgeTabHelper::UpdateBadgeForInfobar(infobars::InfoBar* infobar,
                                                  bool display) {
  is_infobar_displaying_ = display;
  InfoBarIOS* infobar_ios = static_cast<InfoBarIOS*>(infobar);
  id<InfobarUIDelegate> controller_ = infobar_ios->InfobarUIDelegate();
  if (IsInfobarUIRebootEnabled() && [controller_ isPresented]) {
    [delegate_ displayBadge:display];
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(InfobarBadgeTabHelper)
