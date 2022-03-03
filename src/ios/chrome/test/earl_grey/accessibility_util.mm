// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#import <GTXiLib/GTXiLib.h>

#include "ios/chrome/test/earl_grey/accessibility_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

BOOL VerifyAccessibilityForCurrentScreen(NSError* error) {
  // TODO(crbug.com/972681): The GTX analytics ping is preventing the app from
  // idling, causing EG tests to fail.  Disabling analytics will allow tests to
  // run, but may not be the correct long-term solution.
  [GTXAnalytics setEnabled:NO];

  GTXToolKit* toolkit = [GTXToolKit defaultToolkit];
  for (UIWindow* window in [[UIApplication sharedApplication] windows]) {
    // Run the checks on all elements on the screen.
    BOOL success = [toolkit checkAllElementsFromRootElements:@[ window ]
                                                       error:&error];
    if (!success || error) {
      return NO;
    }
  }
  return YES;
}


}  // namespace chrome_test_util
