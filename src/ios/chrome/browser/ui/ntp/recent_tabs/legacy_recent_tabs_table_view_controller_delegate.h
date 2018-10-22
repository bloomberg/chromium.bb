// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

// Protocol used by LegacyRecentTabsTableViewController to communicate to its
// mediator.
@protocol LegacyRecentTabsTableViewControllerDelegate<NSObject>

// Tells the delegate to refresh the session view.
- (void)refreshSessionsView;

@end
