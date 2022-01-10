// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the languages section
 * to interact with the browser.
 */

// <if expr="is_win">
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
// </if>

export interface LanguagesBrowserProxy {
  // <if expr="is_win">
  /**
   * Sets the prospective UI language to the chosen language. This won't
   * affect the actual UI language until a restart.
   */
  setProspectiveUILanguage(languageCode: string): void;

  getProspectiveUILanguage(): Promise<string>;

  // </if>

  getLanguageSettingsPrivate(): typeof chrome.languageSettingsPrivate;
}

export class LanguagesBrowserProxyImpl implements LanguagesBrowserProxy {
  // <if expr="is_win">
  setProspectiveUILanguage(languageCode: string) {
    chrome.send('setProspectiveUILanguage', [languageCode]);
  }

  getProspectiveUILanguage() {
    return sendWithPromise('getProspectiveUILanguage');
  }

  // </if>

  getLanguageSettingsPrivate() {
    return chrome.languageSettingsPrivate;
  }

  static getInstance(): LanguagesBrowserProxy {
    return instance || (instance = new LanguagesBrowserProxyImpl());
  }

  static setInstance(obj: LanguagesBrowserProxy) {
    instance = obj;
  }
}

let instance: LanguagesBrowserProxy|null = null;
