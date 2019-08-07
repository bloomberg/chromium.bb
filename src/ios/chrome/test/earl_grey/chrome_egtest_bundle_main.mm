// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_egtest_bundle_main.h"

#import <XCTest/XCTest.h>
#include <memory>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Contains startup code for a Chrome EG2 test module. Performs startup tasks
// when constructed and shutdown tasks when destroyed. Callers should create an
// instance of this object before running any tests and destroy the instance
// after tests have completed.
class TestMain {
 public:
  TestMain() {
    // Initialize the CommandLine, because ResourceBundle requires it to exist.
    base::CommandLine::Init(/*argc=*/0, /*argv=*/nullptr);

    // Load pak files into the ResourceBundle.
    l10n_util::OverrideLocaleWithCocoaLocale();
    const std::string loaded_locale =
        ui::ResourceBundle::InitSharedInstanceWithLocale(
            /*pref_locale=*/std::string(), /*delegate=*/nullptr,
            ui::ResourceBundle::LOAD_COMMON_RESOURCES);
    CHECK(!loaded_locale.empty());
  }

  ~TestMain() {}

 private:
  base::AtExitManager exit_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestMain);
};

}

@interface ChromeEGTestBundleMain () <XCTestObservation> {
  std::unique_ptr<TestMain> _testMain;
}
@end

@implementation ChromeEGTestBundleMain

- (instancetype)init {
  if ((self = [super init])) {
    [[XCTestObservationCenter sharedTestObservationCenter]
        addTestObserver:self];
  }
  return self;
}

#pragma mark - XCTestObservation

- (void)testBundleWillStart:(NSBundle*)testBundle {
  DCHECK(!_testMain);
  _testMain = std::make_unique<TestMain>();

  // Ensure that //ios/web and the bulk of //ios/chrome/browser are not compiled
  // into the test module. This is hard to assert at compile time, due to
  // transitive dependencies, but at runtime it's easy to check if certain key
  // classes are present or absent.
  CHECK(NSClassFromString(@"CRWWebController") == nil);
  CHECK(NSClassFromString(@"MainController") == nil);
  CHECK(NSClassFromString(@"BrowserViewController") == nil);
}

- (void)testBundleDidFinish:(NSBundle*)testBundle {
  [[XCTestObservationCenter sharedTestObservationCenter]
      removeTestObserver:self];

  _testMain.reset();
}

@end
