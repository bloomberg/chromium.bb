// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_PASSWORD_CELL_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_PASSWORD_CELL_H_

#import <UIKit/UIKit.h>

@protocol ManualFillContentDelegate;
@class ManualFillCredential;

// Cell to display a Credential where the username and password are interactable
// and send the data to the delegate.
@interface ManualFillPasswordCell : UITableViewCell

// Updates the cell with the |credential|. If the user iteracts with it, the
// |delegate| will be notified.
- (void)setUpWithCredential:(ManualFillCredential*)credential
                   delegate:(id<ManualFillContentDelegate>)delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_PASSWORD_CELL_H_
