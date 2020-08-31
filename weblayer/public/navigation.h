// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_NAVIGATION_H_
#define WEBLAYER_PUBLIC_NAVIGATION_H_

#include <string>
#include <vector>

class GURL;

namespace weblayer {

// These types are sent over IPC and across different versions. Never remove
// or change the order.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.weblayer_private
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ImplNavigationState
enum class NavigationState {
  // Waiting to receive initial response data.
  kWaitingResponse = 0,
  // Processing the response.
  kReceivingBytes = 1,
  // The navigation succeeded. Any NavigationObservers would have had
  // NavigationCompleted() called.
  kComplete = 2,
  // The navigation failed. This could be because of an error (in which case
  // IsErrorPage() will return true) or the navigation got turned into a
  // download (in which case IsDownload() will return true).
  // NavigationObservers would have had NavigationFailed() called.
  kFailed = 3,
};

class Navigation {
 public:
  virtual ~Navigation() {}

  // The URL the frame is navigating to. This may change during the navigation
  // when encountering a server redirect.
  virtual GURL GetURL() = 0;

  // Returns the redirects that occurred on the way to the current page. The
  // current page is the last one in the list (so even when there's no redirect,
  // there will be one entry in the list).
  virtual const std::vector<GURL>& GetRedirectChain() = 0;

  virtual NavigationState GetState() = 0;

  // Returns the status code of the navigation. Returns 0 if the navigation
  // hasn't completed yet or if a response wasn't received.
  virtual int GetHttpStatusCode() = 0;

  // Whether the navigation happened without changing document. Examples of
  // same document navigations are:
  // * reference fragment navigations
  // * pushState/replaceState
  // * same page history navigation
  virtual bool IsSameDocument() = 0;

  // Whether the navigation resulted in an error page (e.g. interstitial). Note
  // that if an error page reloads, this will return true even though
  // GetNetErrorCode will be kNoError.
  virtual bool IsErrorPage() = 0;

  // Returns true if this navigation resulted in a download. Returns false if
  // this navigation did not result in a download, or if download status is not
  // yet known for this navigation.  Download status is determined for a
  // navigation when processing final (post redirect) HTTP response headers.
  // This means the only time the embedder can know if it's a download is in
  // NavigationObserver::NavigationFailed.
  virtual bool IsDownload() = 0;

  // Returns true if the navigation was stopped before it could complete because
  // NavigationController::Stop() was called.
  virtual bool WasStopCalled() = 0;

  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.weblayer_private
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: ImplLoadError
  enum LoadError {
    kNoError = 0,            // Navigation completed successfully.
    kHttpClientError = 1,    // Server responded with 4xx status code.
    kHttpServerError = 2,    // Server responded with 5xx status code.
    kSSLError = 3,           // Certificate error.
    kConnectivityError = 4,  // Problem connecting to server.
    kOtherError = 5,         // An error not listed above occurred.
  };

  // Return information about the error, if any, that was encountered while
  // loading the page.
  virtual LoadError GetLoadError() = 0;

  // Set a request's header. If the header is already present, its value is
  // overwritten. This function can only be called at two times, during start
  // and redirect. When called during start, the header applies to both the
  // start and redirect. |name| must be rfc 2616 compliant and |value| must
  // not contain '\0', '\n' or '\r'.
  virtual void SetRequestHeader(const std::string& name,
                                const std::string& value) = 0;

  // Sets the user-agent string used for this navigation. The user-agent is
  // not sticky, it applies to this navigation only (and any redirects). This
  // function may only be called from NavigationObserver::NavigationStarted().
  // Any value specified during start carries through to a redirect. |value|
  // must not contain any illegal characters as documented in
  // SetRequestHeader().
  virtual void SetUserAgentString(const std::string& value) = 0;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_NAVIGATION_H_
