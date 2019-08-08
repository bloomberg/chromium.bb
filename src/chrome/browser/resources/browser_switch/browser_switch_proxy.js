// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('browser_switch', function() {
  /** @interface */
  class BrowserSwitchProxy {
    /**
     * @param {string} url URL to open in alternative browser.
     * @return {Promise} A promise that can fail if unable to launch. It will
     *     never resolve, because the tab closes if this succeeds.
     */
    launchAlternativeBrowserAndCloseTab(url) {}

    gotoNewTabPage() {}
  }

  /** @implements {browser_switch.BrowserSwitchProxy} */
  class BrowserSwitchProxyImpl {
    /** @override */
    launchAlternativeBrowserAndCloseTab(url) {
      return cr.sendWithPromise('launchAlternativeBrowserAndCloseTab', url);
    }

    /** @override */
    gotoNewTabPage() {
      chrome.send('gotoNewTabPage');
    }
  }

  cr.addSingletonGetter(BrowserSwitchProxyImpl);

  return {
    BrowserSwitchProxy: BrowserSwitchProxy,
    BrowserSwitchProxyImpl: BrowserSwitchProxyImpl
  };
});
