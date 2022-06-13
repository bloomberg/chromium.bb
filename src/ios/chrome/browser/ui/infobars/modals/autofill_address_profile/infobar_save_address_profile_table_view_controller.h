// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_AUTOFILL_ADDRESS_PROFILE_INFOBAR_SAVE_ADDRESS_PROFILE_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_AUTOFILL_ADDRESS_PROFILE_INFOBAR_SAVE_ADDRESS_PROFILE_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_save_address_profile_modal_consumer.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

@protocol InfobarSaveAddressProfileModalDelegate;

// InfobarSaveAddressProfileTableViewController represents the content for the
// Save Address Profile InfobarModal.
@interface InfobarSaveAddressProfileTableViewController
    : ChromeTableViewController <InfobarSaveAddressProfileModalConsumer>

- (instancetype)initWithModalDelegate:
    (id<InfobarSaveAddressProfileModalDelegate>)modalDelegate
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_AUTOFILL_ADDRESS_PROFILE_INFOBAR_SAVE_ADDRESS_PROFILE_TABLE_VIEW_CONTROLLER_H_
