// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

namespace ios {
class ChromeBrowserState;
}  // namespace ios

// The accessibility identifier of the privacy settings collection view.
extern NSString* const kPrivacyTableViewId;

@interface PrivacyTableViewController : SettingsRootTableViewController

// |browserState| cannot be nil
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithTableViewStyle:(UITableViewStyle)style
                           appBarStyle:
                               (ChromeTableViewControllerStyle)appBarStyle
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_TABLE_VIEW_CONTROLLER_H_
