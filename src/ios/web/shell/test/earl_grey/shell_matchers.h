// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_MATCHERS_H_
#define IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_MATCHERS_H_

#import <Foundation/Foundation.h>

#include <string>

// ObjC matcher class for use in EG2 tests (Test and App process).
// Shell_matchers_shorthand.h/mm is C++ and for use when writing EG1 tests.
@protocol GREYMatcher;

@interface ShellMatchers : NSObject

// Matcher for the WKWebView.
+ (id<GREYMatcher>)webView;

// Matcher for WKWebView's scroll view.
+ (id<GREYMatcher>)webViewScrollView;

// Matcher for web shell address field text property equal to |text|.
+ (id<GREYMatcher>)addressFieldWithText:(NSString*)text;

// Matcher for back button in web shell.
+ (id<GREYMatcher>)backButton;

// Matcher for forward button in web shell.
+ (id<GREYMatcher>)forwardButton;

// Matcher for address field in web shell.
+ (id<GREYMatcher>)addressField;

@end

#endif  // IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_MATCHERS_H_
