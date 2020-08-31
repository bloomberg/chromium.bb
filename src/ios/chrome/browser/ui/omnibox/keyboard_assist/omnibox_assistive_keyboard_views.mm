// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_views.h"

#include "base/check.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_input_assistant_items.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_keyboard_accessory_view.h"
#include "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void ConfigureAssistiveKeyboardViews(
    UITextField* textField,
    NSString* dotComTLD,
    id<OmniboxAssistiveKeyboardDelegate> delegate) {
  DCHECK(dotComTLD);
  NSArray<NSString*>* buttonTitles = @[ @":", @"-", @"/", dotComTLD ];

  if (IsIPadIdiom()) {
    textField.inputAssistantItem.leadingBarButtonGroups =
        OmniboxAssistiveKeyboardLeadingBarButtonGroups(delegate);
    textField.inputAssistantItem.trailingBarButtonGroups =
        OmniboxAssistiveKeyboardTrailingBarButtonGroups(delegate, buttonTitles);
  } else {
    textField.inputAssistantItem.leadingBarButtonGroups = @[];
    textField.inputAssistantItem.trailingBarButtonGroups = @[];
    UIView* keyboardAccessoryView =
        [[OmniboxKeyboardAccessoryView alloc] initWithButtons:buttonTitles
                                                     delegate:delegate];
    [keyboardAccessoryView setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
    [textField setInputAccessoryView:keyboardAccessoryView];
  }
}
