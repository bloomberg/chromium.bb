// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

class LocationBarModel;

// Protocol receiving notification when the some events occur in the
// ToolbarCoordinator
@protocol ToolbarCoordinatorDelegate<NSObject>

// Called when the location bar gains keyboard focus.
- (void)locationBarDidBecomeFirstResponder;
// Called when the location bar loses keyboard focus.
- (void)locationBarDidResignFirstResponder;
// Called when the location bar receives a key press.
- (void)locationBarBeganEdit;
// Returns the location bar model.
- (LocationBarModel*)locationBarModel;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_DELEGATE_H_
