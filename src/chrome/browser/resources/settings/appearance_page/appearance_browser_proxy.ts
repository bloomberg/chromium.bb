// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// clang-format on

export interface AppearanceBrowserProxy {
  getDefaultZoom(): Promise<number>;
  getThemeInfo(themeId: string): Promise<chrome.management.ExtensionInfo>;

  /** @return Whether the current profile is a child account. */
  isChildAccount(): boolean;

  useDefaultTheme(): void;

  // <if expr="is_linux">
  useSystemTheme(): void;
  // </if>

  validateStartupPage(url: string): Promise<boolean>;
}

export class AppearanceBrowserProxyImpl implements AppearanceBrowserProxy {
  getDefaultZoom(): Promise<number> {
    return new Promise(function(resolve) {
      chrome.settingsPrivate.getDefaultZoom(resolve);
    });
  }

  getThemeInfo(themeId: string): Promise<chrome.management.ExtensionInfo> {
    return new Promise(function(resolve) {
      chrome.management.get(themeId, resolve);
    });
  }

  isChildAccount() {
    return loadTimeData.getBoolean('isChildAccount');
  }

  useDefaultTheme() {
    chrome.send('useDefaultTheme');
  }

  // <if expr="is_linux">
  useSystemTheme() {
    chrome.send('useSystemTheme');
  }
  // </if>

  validateStartupPage(url: string) {
    return sendWithPromise('validateStartupPage', url);
  }

  static getInstance(): AppearanceBrowserProxy {
    return instance || (instance = new AppearanceBrowserProxyImpl());
  }

  static setInstance(obj: AppearanceBrowserProxy) {
    instance = obj;
  }
}

let instance: AppearanceBrowserProxy|null = null;
