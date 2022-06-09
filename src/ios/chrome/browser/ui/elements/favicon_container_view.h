// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ELEMENTS_FAVICON_CONTAINER_VIEW_H_
#define IOS_CHROME_BROWSER_UI_ELEMENTS_FAVICON_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>

@class FaviconView;

@interface FaviconContainerView : UIView

@property(nonatomic, readonly, strong) FaviconView* faviconView;

@end

#endif  // IOS_CHROME_BROWSER_UI_ELEMENTS_FAVICON_CONTAINER_VIEW_H_
