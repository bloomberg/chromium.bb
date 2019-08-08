// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_MODEL_OBSERVER_H_
#define IOS_CHROME_BROWSER_TABS_TAB_MODEL_OBSERVER_H_

#import <Foundation/Foundation.h>

@class Tab;
@class TabModel;

// Observers implement these methods to be alerted to changes in the model.
// These methods are all optional.
@protocol TabModelObserver<NSObject>
@optional

// Some properties about the given tab changed, such as the URL or title.
- (void)tabModel:(TabModel*)model didChangeTab:(Tab*)tab;

@end

#endif  // IOS_CHROME_BROWSER_TABS_TAB_MODEL_OBSERVER_H_
