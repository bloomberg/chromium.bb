// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_views_utils.h"

#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/voice_search_keyboard_accessory_button.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/voice/voice_search_availability.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kVoiceSearchInputAccessoryViewID =
    @"kVoiceSearchInputAccessoryViewID";

namespace {

void SetUpButtonWithIcon(UIButton* button, NSString* iconName) {
  const CGFloat kButtonShadowOpacity = 0.35;
  const CGFloat kButtonShadowRadius = 1.0;
  const CGFloat kButtonShadowVerticalOffset = 1.0;

  [button setTranslatesAutoresizingMaskIntoConstraints:NO];
  UIImage* icon = [UIImage imageNamed:iconName];
  [button setImage:icon forState:UIControlStateNormal];
  button.layer.shadowColor = [UIColor blackColor].CGColor;
  button.layer.shadowOffset = CGSizeMake(0, kButtonShadowVerticalOffset);
  button.layer.shadowOpacity = kButtonShadowOpacity;
  button.layer.shadowRadius = kButtonShadowRadius;
}

}  // namespace

NSArray<UIButton*>* OmniboxAssistiveKeyboardLeadingButtons(
    id<OmniboxAssistiveKeyboardDelegate> delegate) {
  NSMutableArray<UIButton*>* buttons = [NSMutableArray<UIButton*> array];

  UIButton* voiceSearchButton = [[VoiceSearchKeyboardAccessoryButton alloc]
      initWithVoiceSearchAvailability:std::make_unique<
                                          VoiceSearchAvailability>()];
  SetUpButtonWithIcon(voiceSearchButton, @"keyboard_accessory_voice_search");
  NSString* accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_KEYBOARD_ACCESSORY_VIEW_VOICE_SEARCH);
  voiceSearchButton.accessibilityLabel = accessibilityLabel;
  voiceSearchButton.accessibilityIdentifier = kVoiceSearchInputAccessoryViewID;
  [voiceSearchButton
             addTarget:delegate
                action:@selector(keyboardAccessoryVoiceSearchTouchUpInside:)
      forControlEvents:UIControlEventTouchUpInside];
  [buttons addObject:voiceSearchButton];

  UIButton* cameraButton = [UIButton buttonWithType:UIButtonTypeCustom];
  SetUpButtonWithIcon(cameraButton, @"keyboard_accessory_qr_scanner");
  [cameraButton addTarget:delegate
                   action:@selector(keyboardAccessoryCameraSearchTouchUp)
         forControlEvents:UIControlEventTouchUpInside];
  SetA11yLabelAndUiAutomationName(
      cameraButton, IDS_IOS_KEYBOARD_ACCESSORY_VIEW_QR_CODE_SEARCH,
      @"QR code Search");
  [buttons addObject:cameraButton];

  return buttons;
}
