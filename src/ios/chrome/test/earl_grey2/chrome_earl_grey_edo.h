// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY2_CHROME_EARL_GREY_EDO_H_
#define IOS_CHROME_TEST_EARL_GREY2_CHROME_EARL_GREY_EDO_H_

#import <CommonLib/DistantObject/GREYHostApplicationDistantObject.h>

@interface GREYHostApplicationDistantObject (RemoteTest)

// Returns the number of main tabs.
- (NSUInteger)GetMainTabCount;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY2_CHROME_EARL_GREY_EDO_H_
