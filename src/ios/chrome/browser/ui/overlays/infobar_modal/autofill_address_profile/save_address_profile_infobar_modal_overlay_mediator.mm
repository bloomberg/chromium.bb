// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/autofill_address_profile/save_address_profile_infobar_modal_overlay_mediator.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_address_profile_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_address_profile_infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_edit_address_profile_modal_consumer.h"
#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_save_address_profile_modal_consumer.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_overlay_coordinator+modal_configuration.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator+subclassing.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill_address_profile_infobar_overlays::
    SaveAddressProfileModalRequestConfig;
using save_address_profile_infobar_modal_responses::EditedProfileSaveAction;
using save_address_profile_infobar_modal_responses::CancelViewAction;

@interface SaveAddressProfileInfobarModalOverlayMediator ()
// The save address profile modal config from the request.
@property(nonatomic, assign, readonly)
    SaveAddressProfileModalRequestConfig* config;
// YES if the edit modal is being shown.
@property(nonatomic, assign) BOOL currentViewIsEditView;
@end

@implementation SaveAddressProfileInfobarModalOverlayMediator

#pragma mark - Accessors

- (SaveAddressProfileModalRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<SaveAddressProfileModalRequestConfig>()
             : nullptr;
}

- (void)setConsumer:(id<InfobarSaveAddressProfileModalConsumer>)consumer {
  if (_consumer == consumer)
    return;

  _consumer = consumer;

  SaveAddressProfileModalRequestConfig* config = self.config;
  if (!_consumer || !config)
    return;

  NSDictionary* prefs = @{
    kAddressPrefKey : base::SysUTF16ToNSString(config->address()),
    kPhonePrefKey : base::SysUTF16ToNSString(config->phoneNumber()),
    kEmailPrefKey : base::SysUTF16ToNSString(config->emailAddress()),
    kCurrentAddressProfileSavedPrefKey :
        @(config->current_address_profile_saved()),
    kIsUpdateModalPrefKey : @(config->IsUpdateModal()),
    kProfileDataDiffKey : config->profile_diff(),
    kUpdateModalDescriptionKey :
        base::SysUTF16ToNSString(config->update_modal_description())
  };

  [_consumer setupModalViewControllerWithPrefs:prefs];
}

- (void)setEditAddressConsumer:
    (id<InfobarEditAddressProfileModalConsumer>)editAddressConsumer {
  if (_editAddressConsumer == editAddressConsumer)
    return;

  _editAddressConsumer = editAddressConsumer;

  SaveAddressProfileModalRequestConfig* config = self.config;
  if (!_editAddressConsumer || !config)
    return;

  [_editAddressConsumer setIsEditForUpdate:config->IsUpdateModal()];

  [_editAddressConsumer
      setupModalViewControllerWithData:config->GetProfileInfo()];
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return SaveAddressProfileModalRequestConfig::RequestSupport();
}

#pragma mark - InfobarSaveAddressProfileModalDelegate

- (void)showEditView {
  if (!self.request) {
    return;
  }

  self.currentViewIsEditView = YES;
  [self.saveAddressProfileMediatorDelegate showEditView];
}

#pragma mark - InfobarEditAddressProfileModalDelegate

- (void)saveEditedProfileWithData:(NSDictionary*)profileData {
  [self
      dispatchResponse:OverlayResponse::CreateWithInfo<EditedProfileSaveAction>(
                           profileData)];
  [self dismissOverlay];
}

- (void)dismissInfobarModal:(id)infobarModal {
  [self dispatchResponse:OverlayResponse::CreateWithInfo<CancelViewAction>(
                             self.currentViewIsEditView)];
  base::RecordAction(base::UserMetricsAction(kInfobarModalCancelButtonTapped));
  [self dismissOverlay];

  self.currentViewIsEditView = NO;
}

@end
