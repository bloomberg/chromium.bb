// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_GAIA_AUTH_FETCHER_IOS_WK_WEBVIEW_BRIDGE_H_
#define IOS_CHROME_BROWSER_SIGNIN_GAIA_AUTH_FETCHER_IOS_WK_WEBVIEW_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "components/signin/ios/browser/active_state_manager.h"
#include "ios/chrome/browser/signin/gaia_auth_fetcher_ios_bridge.h"

@class GaiaAuthFetcherNavigationDelegate;
@class NSHTTPCookie;
@class WKWebView;

// Specialization of GaiaAuthFetcher on iOS, using WKWebView to send requests.
class GaiaAuthFetcherIOSWKWebViewBridge : public GaiaAuthFetcherIOSBridge,
                                          ActiveStateManager::Observer {
 public:
  GaiaAuthFetcherIOSWKWebViewBridge(
      GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridgeDelegate* delegate,
      web::BrowserState* browser_state);
  ~GaiaAuthFetcherIOSWKWebViewBridge() override;

  // GaiaAuthFetcherIOSBridge.
  void Cancel() override;

 protected:
  friend class GaiaAuthFetcherIOSTest;

  // GaiaAuthFetcherIOSBridge.
  void FetchPendingRequest() override;

  // Returns the cached WKWebView if it exists, or creates one if necessary.
  // Can return nil if the browser state is not active.
  WKWebView* GetWKWebView();
  // Actually creates a WKWebView. Virtual for testing.
  virtual WKWebView* BuildWKWebView();
  // Stops any page loading in the WKWebView currently in use and releases it.
  void ResetWKWebView();

  // ActiveStateManager::Observer implementation.
  void OnActive() override;
  void OnInactive() override;

 private:
  // Navigation delegate of |web_view_| that informs the bridge of relevant
  // navigation events.
  GaiaAuthFetcherNavigationDelegate* navigation_delegate_;
  // Web view used to do the network requests.
  WKWebView* web_view_;

  DISALLOW_COPY_AND_ASSIGN(GaiaAuthFetcherIOSWKWebViewBridge);
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_GAIA_AUTH_FETCHER_IOS_WK_WEBVIEW_BRIDGE_H_
