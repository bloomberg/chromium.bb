// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_FEATURES_H_
#define IOS_WEB_PUBLIC_FEATURES_H_

#include "base/feature_list.h"

namespace web {
namespace features {

// Returns true if pending item should be stored in NavigationContext after
// context is created. The item is still stored in NavigationManager if the
// navigated was requested, but context does not yet exist or when navigation
// was aborted.
bool StorePendingItemInContext();

// Used to always allow scaling of the web page, regardless of author intent.
extern const base::Feature kIgnoresViewportScaleLimits;

// Used to enable API to send messages directly to frames of a webpage.
extern const base::Feature kWebFrameMessaging;

// Used to enable the WKBackForwardList based navigation manager.
extern const base::Feature kSlimNavigationManager;

// Enable to store pending navigation item in NavigationContext after context
// is created. Requires kSlimNavigationManager enabled. The item is still stored
// in NavigationManager if the navigated was requested, but context does not yet
// exist or when navigation was aborted.
extern const base::Feature kStorePendingItemInContext;

// Used to enable using WKHTTPSystemCookieStore in main context URL requests.
extern const base::Feature kWKHTTPSystemCookieStore;

// Used to crash the browser if unexpected URL change is detected.
// https://crbug.com/841105.
extern const base::Feature kCrashOnUnexpectedURLChange;

// Used to make BrowserContainerViewController fullscreen.
extern const base::Feature kBrowserContainerFullscreen;

// Used to use the fullscreen implementation out of web.
extern const base::Feature kOutOfWebFullscreen;

// Used to enable the workaround for WKWebView history clobber bug
// (crbug.com/887497).
extern const base::Feature kHistoryClobberWorkaround;

// Used to prevent native apps from being opened when a universal link is tapped
// and the user is browsing in off the record mode.
extern const base::Feature kBlockUniversalLinksInOffTheRecordMode;

// Used to have the WebUI schemes being handled by the WKWebView directly.
extern const base::Feature kWebUISchemeHandling;

// Whether the WebUI scheme handling in the WKWebView is enabled and can be
// used.
bool WebUISchemeHandlingEnabled();

}  // namespace features
}  // namespace web

#endif  // IOS_WEB_PUBLIC_FEATURES_H_
