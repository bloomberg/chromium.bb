// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/autofill/manual_fill/fallback_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_consumer.h"

namespace manual_fill {

extern NSString* const PasswordSearchBarAccessibilityIdentifier;
extern NSString* const PasswordTableViewAccessibilityIdentifier;

}  // namespace manual_fill

// This class presents a list of usernames and passwords in a table view.
@interface PasswordViewController
    : FallbackViewController<ManualFillPasswordConsumer>

- (instancetype)initWithSearchController:(UISearchController*)searchController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_VIEW_CONTROLLER_H_
