// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_FEATURE_FLAGS_H_
#define IOS_CHROME_BROWSER_SIGNIN_FEATURE_FLAGS_H_

#include "base/feature_list.h"

// Feature flag to enable NSURLSession for GAIAAuthFetcherIOS.
extern const base::Feature kUseNSURLSessionForGaiaSigninRequests;

// Feature flag to enable display of current user identity on New Tab Page.
extern const base::Feature kIdentityDisc;

// Whether Identity Disc feature is enabled.
bool IsIdentityDiscFeatureEnabled();

#endif  // IOS_CHROME_BROWSER_SIGNIN_FEATURE_FLAGS_H_
