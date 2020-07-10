// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_SERVICE_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_SERVICE_DELEGATE_H_

@class TableViewItem;

// Protocol to handle user actions from the Google services settings view.
@protocol GoogleServicesSettingsServiceDelegate <NSObject>

// Called when the UISwitch from a TableViewItem is toggled.
- (void)toggleSwitchItem:(TableViewItem*)switchItem withValue:(BOOL)value;

// Called when cell is tapped.
- (void)didSelectItem:(TableViewItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_SERVICE_DELEGATE_H_
