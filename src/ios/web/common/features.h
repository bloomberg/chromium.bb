// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_COMMON_FEATURES_H_
#define IOS_WEB_COMMON_FEATURES_H_

#include "base/feature_list.h"

namespace web {
namespace features {

// Used to crash the browser if unexpected URL change is detected.
// https://crbug.com/841105.
extern const base::Feature kCrashOnUnexpectedURLChange;

// Used to enable the workaround for WKWebView history clobber bug
// (crbug.com/887497).
extern const base::Feature kHistoryClobberWorkaround;

// Used to prevent native apps from being opened when a universal link is tapped
// and the user is browsing in off the record mode.
extern const base::Feature kBlockUniversalLinksInOffTheRecordMode;

// Used to ensure that the render is not suspended.
extern const base::Feature kKeepsRenderProcessAlive;

// Used to enable the workaround for a WKWebView WKNavigation leak.
// (crbug.com/1010765).  Clear older pending navigation records when a
// navigation finishes.
extern const base::Feature kClearOldNavigationRecordsWorkaround;

// Feature flag enabling persistent downloads.
extern const base::Feature kEnablePersistentDownloads;

// When enabled, for each navigation, the default user agent is chosen by the
// WebClient GetDefaultUserAgent() method. If it is disabled, the mobile version
// is requested by default.
// Use UseWebClientDefaultUserAgent() instead of checking this variable.
extern const base::Feature kUseDefaultUserAgentInWebClient;

// When enabled, preserves properties of the UIScrollView using CRWPropertyStore
// when the scroll view is recreated. When disabled, only preserve a small set
// of properties using hard coded logic.
extern const base::Feature kPreserveScrollViewProperties;

// When enabled, display an interstitial on legacy TLS connections.
extern const base::Feature kIOSLegacyTLSInterstitial;

// Records snapshot size of image (IOS.Snapshots.ImageSize histogram) and PDF
// (IOS.Snapshots.PDFSize histogram) if enabled. Enabling this flag will
// generate PDF when Page Snapshot is taken just to record PDF size.
extern const base::Feature kRecordSnapshotSize;

// When enabled, the |attribution| property of NSMutableURLRequests passed to
// WKWebView is set as NSURLRequestAttributionUser on iOS 15.
extern const base::Feature kSetRequestAttribution;

// When enabled, use the native context menu in web content.
extern const base::Feature kWebViewNativeContextMenu;

// When enabled, display non-live preview for context menus in web content.
extern const base::Feature kWebViewNativeContextMenuPhase2;

// When enabled, the default context menu from WKWebView is used.
extern const base::Feature kDefaultWebViewContextMenu;

// Disables the screenshots of non-HTML pages on iOS15.
extern const base::Feature kDisableNonHTMLScreenshotOnIOS15;

// Feature flag that enable Shared Highlighting color change in iOS.
extern const base::Feature kIOSSharedHighlightingColorChange;

// Feature flag that enables creating pending item on POST form submission.
// Also, for GET form submissions with same page navigation, retains the
// transition type of new navigation item.
extern const base::Feature kCreatePendingItemForPostFormSubmission;

// Enable the new download API if available.
extern const base::Feature kEnableNewDownloadAPI;

// When true, for each navigation, the default user agent is chosen by the
// WebClient GetDefaultUserAgent() method. If it is false, the mobile version
// is requested by default.
bool UseWebClientDefaultUserAgent();

// When true, the native context menu for the web content are used.
bool UseWebViewNativeContextMenuWeb();

// When true, the custom implementation of context menu using native ContextMenu
// for the web content is used.
bool UseWebViewNativeContextMenuSystem();

// When true, screenshots of non-HTML (e.g. PDF) pages should be taken.
bool ShouldTakeScreenshotOnNonHTMLContent();

// When true, the new download API should be used.
bool IsNewDownloadAPIEnabled();

}  // namespace features
}  // namespace web

#endif  // IOS_WEB_COMMON_FEATURES_H_
