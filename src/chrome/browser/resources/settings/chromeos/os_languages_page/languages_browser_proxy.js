// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the languages section
 * to interact with the browser.
 */

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/** @interface */
export class LanguagesBrowserProxy {
  /**
   * Sets the prospective UI language to the chosen language. This won't
   * affect the actual UI language until a restart.
   * @param {string} languageCode
   */
  setProspectiveUILanguage(languageCode) {}

  /** @return {!Promise<string>} */
  getProspectiveUILanguage() {}

  /** @return {!LanguageSettingsPrivate} */
  getLanguageSettingsPrivate() {}

  /** @return {!InputMethodPrivate} */
  getInputMethodPrivate() {}
}

/**
 * @implements {LanguagesBrowserProxy}
 */
export class LanguagesBrowserProxyImpl {
  /** @override */
  setProspectiveUILanguage(languageCode) {
    chrome.send('setProspectiveUILanguage', [languageCode]);
  }

  /** @override */
  getProspectiveUILanguage() {
    return sendWithPromise('getProspectiveUILanguage');
  }

  /** @override */
  getLanguageSettingsPrivate() {
    return /** @type {!LanguageSettingsPrivate} */ (
        chrome.languageSettingsPrivate);
  }

  /** @override */
  getInputMethodPrivate() {
    return /** @type {!InputMethodPrivate} */ (chrome.inputMethodPrivate);
  }

  /** @return {!LanguagesBrowserProxy} */
  static getInstance() {
    return instance || (instance = new LanguagesBrowserProxyImpl());
  }

  /** @param {!LanguagesBrowserProxy} obj */
  static setInstance(obj) {
    instance = obj;
  }
}

/** @type {?LanguagesBrowserProxy} */
let instance = null;
