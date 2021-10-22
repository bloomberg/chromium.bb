// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_egtest_bundle_main.h"

#import <XCTest/XCTest.h>
#import <objc/runtime.h>
#include <memory>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/strings/sys_string_conversions.h"
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
    NSArray* arguments = NSProcessInfo.processInfo.arguments;

    // Convert NSArray to the required input type of |base::CommandLine::Init|.
    int argc = arguments.count;
    const char* argv[argc];
    std::vector<std::string> argv_store;
    // Avoid using std::vector::push_back (or any other method that could cause
    // the vector to grow) as this will cause the std::string to be copied or
    // moved (depends on the C++ implementation) which may invalidates the
    // pointer returned by std::string::c_str(). Even if the strings are moved,
    // this may cause garbage if std::string uses optimisation for small strings
    // (by returning pointer to the object internals in that case).
    argv_store.resize(argc);
    for (int i = 0; i < argc; i++) {
      argv_store[i] = base::SysNSStringToUTF8(arguments[i]);
      argv[i] = argv_store[i].c_str();
    }

    // Initialize the CommandLine with arguments. ResourceBundle requires
    // CommandLine to exist.
    base::CommandLine::Init(argc, argv);

    base::i18n::InitializeICU();

    // Load pak files into the ResourceBundle.
    l10n_util::OverrideLocaleWithCocoaLocale();
    const std::string loaded_locale =
        ui::ResourceBundle::InitSharedInstanceWithLocale(
            /*pref_locale=*/std::string(), /*delegate=*/nullptr,
            ui::ResourceBundle::LOAD_COMMON_RESOURCES);
    CHECK(!loaded_locale.empty());
  }

  TestMain(const TestMain&) = delete;
  TestMain& operator=(const TestMain&) = delete;

  ~TestMain() {}

 private:
  base::AtExitManager exit_manager_;
};

}

@class XCTSourceCodeSymbolInfo;
@protocol XCTSymbolInfoProviding <NSObject>
- (XCTSourceCodeSymbolInfo*)symbolInfoForAddressInCurrentProcess:(pid_t)pid
                                                           error:
                                                               (NSError**)error;
@end

@interface XCTSymbolicationService
+ (void)setSharedService:(id<XCTSymbolInfoProviding>)arg1;
@end

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

// -waitForQuiescenceIncludingAnimationsIdle tends to introduce a long
// unnecessary delay, as EarlGrey already checks for animations to complete.
// Swizzling and skipping the following call speeds up test runs.
- (void)disableWaitForIdle {
  SEL originalSelector =
      NSSelectorFromString(@"waitForQuiescenceIncludingAnimationsIdle:");
  SEL swizzledSelector = @selector(skipQuiescenceDelay);
  Method originalMethod = class_getInstanceMethod(
      objc_getClass("XCUIApplicationProcess"), originalSelector);
  Method swizzledMethod =
      class_getInstanceMethod([self class], swizzledSelector);
  method_exchangeImplementations(originalMethod, swizzledMethod);
}

// Empty swizzled method to be invoked by XCTest at the start of each test case.
// Since earl grey synchronizes automatically, do nothing here.
- (void)skipQuiescenceDelay {
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

  // Disable aggressive symbolication and disable symbolication service to work
  // around slow XCTest assertion failures. These failures are spending a very
  // long time attempting to symbolicate.
  Class symbolicationService = NSClassFromString(@"XCTSymbolicationService");
  if (symbolicationService != nil) {
    [symbolicationService setSharedService:nil];
  }
  [[NSUserDefaults standardUserDefaults]
      setBool:YES
       forKey:@"XCTDisableAggressiveSymbolication"];

  // Disable long wait for idle messages.
  [self disableWaitForIdle];
}

- (void)testBundleDidFinish:(NSBundle*)testBundle {
  [[XCTestObservationCenter sharedTestObservationCenter]
      removeTestObserver:self];

  _testMain.reset();
}

@end
