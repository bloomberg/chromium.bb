// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_EARL_GREY_H_
#define IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_EARL_GREY_H_

#import <Foundation/Foundation.h>

#include "ios/web/public/test/element_selector.h"
#include "url/gurl.h"

// Test methods that perform actions on Web Shell. These methods may read or
// alter Web Shell's internal state programmatically or via the UI, but in both
// cases will properly synchronize the UI for Earl Grey tests.
namespace shell_test_util {

// Loads |URL| in the current WebState with transition of type
// ui::PAGE_TRANSITION_TYPED, and waits for the page to complete loading, or
// a timeout. Returns false if the page doesn't finish loading, true if it
// does.
bool LoadUrl(const GURL& url) WARN_UNUSED_RESULT;

// Waits for the current web view to contain |text|. If the condition is not met
// within a timeout, it returns false, otherwise, true.
bool WaitForWebViewContainingText(const std::string& text) WARN_UNUSED_RESULT;

}  // namespace shell_test_util

#endif  // IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_EARL_GREY_H_
