// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the welcome page to interact with
 * the browser.
 */

cr.define('welcome', function() {

  /** @interface */
  class WelcomeBrowserProxy {
    /** @param {string} redirectUrl the URL to redirect to, after signing in. */
    handleActivateSignIn(redirectUrl) {}
    goToNewTabPage() {}
  }

  /** @implements {welcome.WelcomeBrowserProxy} */
  class WelcomeBrowserProxyImpl {
    /** @override */
    handleActivateSignIn(redirectUrl) {
      chrome.send('handleActivateSignIn', redirectUrl ? [redirectUrl] : []);
    }

    /** @override */
    goToNewTabPage() {
      window.location.replace('chrome://newtab');
    }
  }

  cr.addSingletonGetter(WelcomeBrowserProxyImpl);

  return {
    WelcomeBrowserProxy: WelcomeBrowserProxy,
    WelcomeBrowserProxyImpl: WelcomeBrowserProxyImpl,
  };
});
