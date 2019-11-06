// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_IDENTITY_CHOOSER_IDENTITY_CHOOSER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_IDENTITY_CHOOSER_IDENTITY_CHOOSER_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// IdentityChooserItem holds the model data for an identity.
@interface IdentityChooserItem : TableViewItem

// Gaia ID.
@property(nonatomic, strong) NSString* gaiaID;
// User name.
@property(nonatomic, strong) NSString* name;
// User email.
@property(nonatomic, strong) NSString* email;
// User avatar.
@property(nonatomic, strong) UIImage* avatar;
// If YES, the identity is selected.
@property(nonatomic, assign) BOOL selected;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_IDENTITY_CHOOSER_IDENTITY_CHOOSER_ITEM_H_
