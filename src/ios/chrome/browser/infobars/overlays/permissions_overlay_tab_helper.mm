// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/permissions_overlay_tab_helper.h"

#import "base/timer/timer.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#include "ios/chrome/browser/infobars/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/infobars/overlays/permissions_overlay_infobar_delegate.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/infobar_banner_placeholder_request_config.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#include "ios/chrome/browser/overlays/public/overlay_request_queue_util.h"
#import "ios/web/common/features.h"
#import "ios/web/public/permissions/permissions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const float kTimeoutInMillisecond = 250;
}  // namespace

PermissionsOverlayTabHelper::PermissionsOverlayTabHelper(
    web::WebState* web_state)
    : web_state_(web_state) {
  if (@available(iOS 15.0, *)) {
    if (web::features::IsMediaPermissionsControlEnabled()) {
      DCHECK(web_state);
      permissions_to_state_ =
          [web_state->GetStatesForAllPermissions() mutableCopy];
      banner_queue_ = OverlayRequestQueue::FromWebState(
          web_state, OverlayModality::kInfobarBanner);
      inserter_ = InfobarOverlayRequestInserter::FromWebState(web_state);
      web_state_->AddObserver(this);
    }
  }
}

PermissionsOverlayTabHelper::~PermissionsOverlayTabHelper() {}

void PermissionsOverlayTabHelper::PermissionStateChanged(
    web::WebState* web_state,
    web::Permission permission) {
  DCHECK_EQ(web_state_, web_state);
  web::PermissionState new_state =
      web_state_->GetStateForPermission(permission);
  // Removes infobar if no permission is accessible.
  if (new_state == web::PermissionStateNotAccessible) {
    permissions_to_state_[@(permission)] = @(new_state);
    BOOL shouldRemoveInfobar = YES;
    for (NSNumber* permission in permissions_to_state_) {
      if (permissions_to_state_[permission].unsignedIntegerValue >
          web::PermissionStateNotAccessible) {
        shouldRemoveInfobar = NO;
        break;
      }
    }
    if (infobar_ != nullptr) {
      if (shouldRemoveInfobar) {
        infobars::InfoBarManager* infobar_manager =
            InfoBarManagerImpl::FromWebState(web_state_);
        infobar_manager->RemoveInfoBar(infobar_);
      } else {
        UpdateIsInfoBarAccepted();
      }
    }
    return;
  }
  // Adds/replaces infobar if previous state was "NotAccessible".
  if (permissions_to_state_[@(permission)].unsignedIntegerValue ==
      web::PermissionStateNotAccessible) {
    // Delays infobar creation in case that multiple permissions are enabled
    // simultaneously to show one infobar with a message that says so, instead
    // of showing multiple infobar banners back to back.
    if (!timer_.IsRunning()) {
      recently_accessible_permissions_ = [NSMutableArray array];
      timer_.Start(FROM_HERE, base::Milliseconds(kTimeoutInMillisecond), this,
                   &PermissionsOverlayTabHelper::ShowInfoBar);
    }
    [recently_accessible_permissions_ addObject:@(permission)];
  }
  permissions_to_state_[@(permission)] = @(new_state);
  if (infobar_ != nullptr) {
    UpdateIsInfoBarAccepted();
  }
}

void PermissionsOverlayTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  DCHECK(banner_queue_);
  if (web::features::IsMediaPermissionsControlEnabled()) {
    web_state_->RemoveObserver(this);
  }
  web_state_ = nullptr;
  banner_queue_ = nullptr;
  inserter_ = nullptr;
}

void PermissionsOverlayTabHelper::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                                   bool animate) {
  if (infobar == infobar_) {
    infobar_manager_scoped_observation_.Reset();
    infobar_ = nullptr;
  }
}

void PermissionsOverlayTabHelper::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  DCHECK(infobar_manager_scoped_observation_.IsObservingSource(manager));
  infobar_manager_scoped_observation_.Reset();
}

void PermissionsOverlayTabHelper::ShowInfoBar() {
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state_);
  if (!infobar_manager_scoped_observation_.IsObservingSource(infobar_manager)) {
    infobar_manager_scoped_observation_.Observe(infobar_manager);
  }

  std::unique_ptr<PermissionsOverlayInfobarDelegate> delegate(
      std::make_unique<PermissionsOverlayInfobarDelegate>(
          recently_accessible_permissions_, web_state_));

  BOOL first_activation = infobar_ == nullptr;
  std::unique_ptr<infobars::InfoBar> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypePermissions, std::move(delegate));
  infobar_ = infobar_manager->AddInfoBar(std::move(infobar), true);

  UpdateIsInfoBarAccepted();
  // Infobar replacement does not display the banner a second time, hence here
  // we use overlay inserter to manually show it again.
  if (!first_activation) {
    size_t index = 0;
    bool request_found = GetInfobarOverlayRequestIndex(
        banner_queue_, static_cast<InfoBarIOS*>(infobar_), &index);
    if (request_found)  // The new banner is already shown.
      return;
    InsertParams params(static_cast<InfoBarIOS*>(infobar_));
    params.overlay_type = InfobarOverlayType::kBanner;
    params.insertion_index = banner_queue_->size();
    params.source = InfobarOverlayInsertionSource::kInfoBarDelegate;
    inserter_->InsertOverlayRequest(params);
  }
}

void PermissionsOverlayTabHelper::UpdateIsInfoBarAccepted() {
  if (infobar_ == nullptr) {
    return;
  }
  BOOL accepted = NO;
  for (NSNumber* permission in permissions_to_state_) {
    if (permissions_to_state_[permission].unsignedIntegerValue ==
        web::PermissionStateAllowed) {
      accepted = YES;
      break;
    }
  }
  // TODO(crbug.com/1295144): Slightly delay execution for thread safety.
  static_cast<InfoBarIOS*>(infobar_)->set_accepted(accepted);
}

WEB_STATE_USER_DATA_KEY_IMPL(PermissionsOverlayTabHelper)
