// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/save_address_profile/save_address_profile_infobar_modal_overlay_coordinator.h"

#include "base/check.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_address_profile_infobar_modal_overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_save_address_profile_table_view_controller.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_overlay_coordinator+modal_configuration.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/save_address_profile/save_address_profile_infobar_modal_overlay_mediator.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using save_address_profile_infobar_overlays::
    SaveAddressProfileModalRequestConfig;

@interface SaveAddressProfileInfobarModalOverlayCoordinator ()
// Redefine ModalConfiguration properties as readwrite.
@property(nonatomic, strong, readwrite) OverlayRequestMediator* modalMediator;
@property(nonatomic, strong, readwrite) UIViewController* modalViewController;
// The request's config.
@property(nonatomic, assign, readonly)
    SaveAddressProfileModalRequestConfig* config;
@end

@implementation SaveAddressProfileInfobarModalOverlayCoordinator

#pragma mark - Accessors

- (SaveAddressProfileModalRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<SaveAddressProfileModalRequestConfig>()
             : nullptr;
}

#pragma mark - Public

+ (const OverlayRequestSupport*)requestSupport {
  return SaveAddressProfileModalRequestConfig::RequestSupport();
}

@end

@implementation
    SaveAddressProfileInfobarModalOverlayCoordinator (ModalConfiguration)

- (void)configureModal {
  DCHECK(!self.modalMediator);
  DCHECK(!self.modalViewController);
  SaveAddressProfileInfobarModalOverlayMediator* modalMediator =
      [[SaveAddressProfileInfobarModalOverlayMediator alloc]
          initWithRequest:self.request];
  InfobarSaveAddressProfileTableViewController* modalViewController =
      [[InfobarSaveAddressProfileTableViewController alloc]
          initWithModalDelegate:modalMediator];
  // TODO(crbug.com/1167062): Replace with proper localized string.
  modalViewController.title = @"Save Address Profile";
  modalMediator.consumer = modalViewController;
  self.modalMediator = modalMediator;
  self.modalViewController = modalViewController;
}

- (void)resetModal {
  DCHECK(self.modalMediator);
  DCHECK(self.modalViewController);
  self.modalMediator = nil;
  self.modalViewController = nil;
}

@end
