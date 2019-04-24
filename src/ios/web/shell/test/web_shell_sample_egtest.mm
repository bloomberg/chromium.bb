// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "ios/third_party/earl_grey2/src/TestLib/EarlGreyImpl/EarlGrey.h"
#import "ios/web/shell/test/earl_grey/web_shell_test_case.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// WebShellSampleTestCase will temporarily hold EG2 versions of web shell tests,
// until the EG2 tests are running on the bots and the EG1 versions can be
// deleted.
@interface WebShellSampleTestCase : WebShellTestCase
@end

@implementation WebShellSampleTestCase

- (void)setUp {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    XCUIApplication* application = [[XCUIApplication alloc] init];
    [application launch];
  });

  [super setUp];
}

@end
