// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/earl_grey/shell_earl_grey.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/shell/test/earl_grey/shell_earl_grey_app_interface.h"

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_2)
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(ShellEarlGreyAppInterface)
#endif

namespace shell_test_util {

bool LoadUrl(const GURL& url) {
  [ShellEarlGreyAppInterface loadURL:base::SysUTF8ToNSString(url.spec())];

  bool load_success = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return ![ShellEarlGreyAppInterface isCurrentWebStateLoading];
  });
  if (!load_success) {
    return false;
  }

  bool injection_success =
      [ShellEarlGreyAppInterface waitForWindowIDInjectedInCurrentWebState];
  if (!injection_success) {
    return false;
  }

  // Ensure any UI elements handled by EarlGrey become idle for any subsequent
  // EarlGrey steps.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  return true;
}

bool WaitForWebViewContainingText(const std::string& text) {
  return WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return [ShellEarlGreyAppInterface
        currentWebStateContainsText:base::SysUTF8ToNSString(text)];
  });
}

}  // namespace shell_test_util
