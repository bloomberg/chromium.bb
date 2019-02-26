// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_LEGACY_AUTOFILL_EDIT_COLLECTION_VIEW_CONTROLLER_PROTECTED_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_LEGACY_AUTOFILL_EDIT_COLLECTION_VIEW_CONTROLLER_PROTECTED_H_

#import "ios/chrome/browser/ui/settings/legacy_autofill_edit_collection_view_controller.h"

// The collection view for an Autofill edit entry menu.
@interface LegacyAutofillEditCollectionViewController (Protected)

// Returns the indexPath for the currently focused text field when in edit mode.
- (NSIndexPath*)indexPathForCurrentTextField;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_LEGACY_AUTOFILL_EDIT_COLLECTION_VIEW_CONTROLLER_PROTECTED_H_
