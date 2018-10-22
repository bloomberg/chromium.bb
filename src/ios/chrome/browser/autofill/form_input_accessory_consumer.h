// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_CONSUMER_H_

#import <UIKit/UIKit.h>

@protocol FormInputAccessoryViewDelegate;

@protocol FormInputAccessoryConsumer<NSObject>

// Restores the default input accessory view, removing (if necessary) any
// previously-added custom view.
- (void)restoreDefaultInputAccessoryView;

// Hides the default input accessory view and replaces it with one that shows
// |customView| and form navigation controls.
- (void)showCustomInputAccessoryView:(UIView*)view
                  navigationDelegate:
                      (id<FormInputAccessoryViewDelegate>)navigationDelegate;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_CONSUMER_H_
