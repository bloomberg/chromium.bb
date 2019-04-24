// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <WebKit/WebKit.h>

#include "base/strings/sys_string_conversions.h"
#import "ios/web/shell/test/earl_grey/shell_matchers.h"
#import "ios/web/shell/test/earl_grey/shell_matchers_shorthand.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

id<GREYMatcher> WebView() {
  return [ShellMatchers webView];
}

id<GREYMatcher> WebViewScrollView() {
  return [ShellMatchers webViewScrollView];
}

id<GREYMatcher> AddressFieldText(std::string text) {
  return [ShellMatchers addressFieldWithText:base::SysUTF8ToNSString(text)];
}

id<GREYMatcher> BackButton() {
  return [ShellMatchers backButton];
}

id<GREYMatcher> ForwardButton() {
  return [ShellMatchers forwardButton];
}

id<GREYMatcher> AddressField() {
  return [ShellMatchers addressField];
}

}  // namespace web
