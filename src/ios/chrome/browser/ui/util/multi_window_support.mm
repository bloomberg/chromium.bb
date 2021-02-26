// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/multi_window_support.h"

#import <UIKit/UIKit.h>

#include "base/ios/ios_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool IsMultiwindowSupported() {
  return base::ios::IsMultiwindowSupported();
}

bool IsSceneStartupSupported() {
  return base::ios::IsSceneStartupSupported();
}

bool IsMultipleScenesSupported() {
  if (@available(iOS 13, *)) {
    return UIApplication.sharedApplication.supportsMultipleScenes;
  }
  return false;
}
