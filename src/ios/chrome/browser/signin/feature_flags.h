// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_FEATURE_FLAGS_H_
#define IOS_CHROME_BROWSER_SIGNIN_FEATURE_FLAGS_H_

#include "base/feature_list.h"

// Feature flag to enable WKWebView in SSO.
extern const base::Feature kSSOWithWKWebView;

// Returns true if the WKWebView should be enabled in SSO.
bool ShouldEnableWKWebViewWithSSO();

#endif  // IOS_CHROME_BROWSER_SIGNIN_FEATURE_FLAGS_H_
