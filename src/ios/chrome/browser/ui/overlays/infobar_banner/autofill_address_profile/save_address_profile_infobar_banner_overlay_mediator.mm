// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/autofill_address_profile/save_address_profile_infobar_banner_overlay_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/overlays/public/infobar_banner/infobar_banner_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_address_profile_infobar_banner_overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/ui/icons/chrome_symbol.h"
#import "ios/chrome/browser/ui/icons/infobar_icon.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator+subclassing.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill_address_profile_infobar_overlays::
    SaveAddressProfileBannerRequestConfig;

@interface SaveAddressProfileInfobarBannerOverlayMediator ()
// The save address profile banner config from the request.
@property(nonatomic, assign, readonly)
    SaveAddressProfileBannerRequestConfig* config;
@end

@implementation SaveAddressProfileInfobarBannerOverlayMediator

#pragma mark - Accessors

- (SaveAddressProfileBannerRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<SaveAddressProfileBannerRequestConfig>()
             : nullptr;
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return SaveAddressProfileBannerRequestConfig::RequestSupport();
}

#pragma mark - InfobarOverlayRequestMediator

- (void)bannerInfobarButtonWasPressed:(UIButton*)sender {
  // Present the modal if the save/update button is pressed.
  [self presentInfobarModalFromBanner];
}

@end

@implementation SaveAddressProfileInfobarBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  if (!self.consumer || !self.config)
    return;

  [self.consumer
      setButtonText:base::SysUTF16ToNSString(self.config->button_label_text())];
  [self.consumer
      setTitleText:base::SysUTF16ToNSString(self.config->message_text())];
  [self.consumer
      setSubtitleText:base::SysUTF16ToNSString(self.config->description())];
  [self.consumer setRestrictSubtitleTextToSingleLine:YES];

  if (UseSymbols()) {
    [self.consumer setIconImage:DefaultSymbolWithPointSize(
                                    kPinFillSymbol, kSymbolImagePointSize)];
  } else {
    [self.consumer
        setIconImage:[UIImage imageNamed:self.config->icon_image_name()]];
  }
  // This is done to hide the settings image from the banner view. The modal
  // would still be presented when the user chooses to pick the Save/Update
  // action.
  [self.consumer setPresentsModal:NO];
}

@end
