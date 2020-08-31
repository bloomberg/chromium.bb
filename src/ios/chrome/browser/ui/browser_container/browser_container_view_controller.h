// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/browser_container/browser_container_consumer.h"

// UIViewController which allows displaying and removing a content view.
@interface BrowserContainerViewController
    : UIViewController <BrowserContainerConsumer>

// The UIViewController used to display overlay UI over the web content area.
@property(nonatomic, strong, readonly)
    UIViewController* webContentsOverlayContainerViewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_CONTAINER_VIEW_CONTROLLER_H_
