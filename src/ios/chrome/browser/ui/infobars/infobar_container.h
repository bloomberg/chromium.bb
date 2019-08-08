// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_H_

#import <Foundation/Foundation.h>

// Protocol for the InfobarCoordinators to communicate with the InfobarContainer
// Coordinator.
@protocol InfobarContainer

// Informs the InfobarContainer Coordinator that its child coordinator has
// stopped.
// TODO(crbug.com/961343): Add support to indicate which Coordinator has
// stopped.
- (void)childCoordinatorStopped;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_H_
