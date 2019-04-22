// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey2/chrome_earl_grey_edo.h"

#import "ios/chrome/test/app/tab_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation GREYHostApplicationDistantObject (RemoteTest)

- (NSUInteger)GetMainTabCount {
  return chrome_test_util::GetMainTabCount();
}
@end
