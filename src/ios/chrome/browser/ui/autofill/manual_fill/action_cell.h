// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_ACTION_CELL_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_ACTION_CELL_H_

#import <UIKit/UIKit.h>

// A table view cell which contains a button and holds an action block, which
// is called when the button is touched.
@interface ManualFillActionCell : UITableViewCell

// Updates the cell with the passed title and action block.
- (void)setUpWithTitle:(NSString*)title action:(void (^)(void))action;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_ACTION_CELL_H_
