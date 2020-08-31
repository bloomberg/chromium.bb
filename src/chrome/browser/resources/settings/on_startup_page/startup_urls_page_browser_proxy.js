// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/**
 * @typedef {{
 *   modelIndex: number,
 *   title: string,
 *   tooltip: string,
 *   url: string
 * }}
 */
export let StartupPageInfo;

/** @interface */
export class StartupUrlsPageBrowserProxy {
  loadStartupPages() {}
  useCurrentPages() {}

  /**
   * @param {string} url
   * @return {!Promise<boolean>} Whether the URL is valid.
   */
  validateStartupPage(url) {}

  /**
   * @param {string} url
   * @return {!Promise<boolean>} Whether the URL was actually added, or
   *     ignored because it was invalid.
   */
  addStartupPage(url) {}

  /**
   * @param {number} modelIndex
   * @param {string} url
   * @return {!Promise<boolean>} Whether the URL was actually edited, or
   *     ignored because it was invalid.
   */
  editStartupPage(modelIndex, url) {}

  /** @param {number} index */
  removeStartupPage(index) {}
}

/**
 * @implements {StartupUrlsPageBrowserProxy}
 */
export class StartupUrlsPageBrowserProxyImpl {
  /** @override */
  loadStartupPages() {
    chrome.send('onStartupPrefsPageLoad');
  }

  /** @override */
  useCurrentPages() {
    chrome.send('setStartupPagesToCurrentPages');
  }

  /** @override */
  validateStartupPage(url) {
    return sendWithPromise('validateStartupPage', url);
  }

  /** @override */
  addStartupPage(url) {
    return sendWithPromise('addStartupPage', url);
  }

  /** @override */
  editStartupPage(modelIndex, url) {
    return sendWithPromise('editStartupPage', modelIndex, url);
  }

  /** @override */
  removeStartupPage(index) {
    chrome.send('removeStartupPage', [index]);
  }
}

addSingletonGetter(StartupUrlsPageBrowserProxyImpl);
