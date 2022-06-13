// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/browser_observer_bridge.h"

#include <CoreFoundation/CoreFoundation.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BrowserObserverBridge::BrowserObserverBridge(Browser* browser,
                                             id<BrowserObserving> observer)
    : observer_(observer) {
  browser_observation_.Observe(browser);
}

BrowserObserverBridge::~BrowserObserverBridge() {}

void BrowserObserverBridge::BrowserDestroyed(Browser* browser) {
  [observer_ browserDestroyed:browser];
  browser_observation_.Reset();
}
