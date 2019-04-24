// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Kiosk Next Home API implementation.
 */

/** @implements {kioskNextHome.Bridge} */
class KioskNextHomeBridge {
  constructor() {
    /** @type {!Array<!kioskNextHome.Listener>} */
    this.listeners = [];

    chrome.arcAppsPrivate.onInstalled.addListener(installedApp => {
      const app = {
        appType: kioskNextHome.AppType.ARC,
        appId: installedApp.packageName,
        displayName: installedApp.packageName,
        suspended: false,
        thumbnailImage: '',
      };
      for (const listener of this.listeners) {
        listener.onInstalledAppChanged(
            app, kioskNextHome.AppEventType.INSTALLED);
      }
    });
  }

  /** @override */
  addListener(listener) {
    this.listeners.push(listener);
  }

  /** @override */
  getAccessToken(scopes) {
    return new Promise((resolve, reject) => {
      chrome.identity.getAuthToken({'scopes': scopes}, token => {
        if (token) {
          resolve(token);
        } else {
          reject('Unable to get access token.');
        }
      });
    });
  }

  /** @override */
  getInstalledApps() {
    return new Promise((resolve, reject) => {
      chrome.arcAppsPrivate.getLaunchableApps(launchableApps => {
        const installedApps = [];
        for (const launchableApp of launchableApps) {
          installedApps.push({
            appType: kioskNextHome.AppType.ARC,
            appId: launchableApp.packageName,
            displayName: launchableApp.packageName,
            suspended: false,
            thumbnailImage: '',
          });
        }
        resolve(installedApps);
      });
    });
  }

  /** @override */
  launchContent(contentSource, contentId, opt_params) {
    if (contentSource === kioskNextHome.ContentSource.ARC_INTENT) {
      // TODO(brunoad): create and migrate to a more generic API.
      chrome.arcAppsPrivate.launchApp(contentId);
    }
    return Promise.resolve(true);
  }
}

/**
 * Provides bridge implementation.
 * @return {!kioskNextHome.Bridge} Bridge instance that can be used to interact
 *     with ChromeOS.
 */
kioskNextHome.getChromeOsBridge = function() {
  return new KioskNextHomeBridge();
};
