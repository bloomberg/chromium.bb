// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Specifies page visibility based on incognito status, Chrome OS guest mode,
 * and whether or not to include OS settings. Once the Chrome OS SplitSettings
 * project is completed this can be changed to only consider incognito and
 * guest mode. https://crbug.com/950007
 * @typedef {{
 *   advancedSettings: (boolean|undefined),
 *   appearance: (boolean|undefined|AppearancePageVisibility),
 *   autofill: (boolean|undefined),
 *   bluetooth: (boolean|undefined),
 *   dateTime: (boolean|undefined|DateTimePageVisibility),
 *   defaultBrowser: (boolean|undefined),
 *   device: (boolean|undefined),
 *   downloads: (boolean|undefined|DownloadsPageVisibility),
 *   internet: (boolean|undefined),
 *   multidevice: (boolean|undefined),
 *   onStartup: (boolean|undefined),
 *   people: (boolean|undefined),
 *   privacy: (boolean|undefined|PrivacyPageVisibility),
 *   reset:(boolean|undefined),
 * }}
 */
let PageVisibility;

/**
 * @typedef {{
 *   bookmarksBar: boolean,
 *   homeButton: boolean,
 *   pageZoom: boolean,
 *   setTheme: boolean,
 *   setWallpaper: boolean,
 * }}
 */
let AppearancePageVisibility;

/**
 * @typedef {{
 *   timeZoneSelector: boolean,
 * }}
 */
let DateTimePageVisibility;

/**
 * @typedef {{
 *   googleDrive: boolean
 * }}
 */
let DownloadsPageVisibility;

/**
 * @typedef {{
 *   networkPrediction: boolean,
 *   searchPrediction: boolean,
 * }}
 */
let PrivacyPageVisibility;

cr.define('settings', function() {
  /**
   * Dictionary defining page visibility.
   * @type {!PageVisibility}
   */
  let pageVisibility;

  const showOSSettings = loadTimeData.getBoolean('showOSSettings');

  if (loadTimeData.getBoolean('isGuest')) {
    // "if not chromeos" and "if chromeos" in two completely separate blocks
    // to work around closure compiler.
    // <if expr="not chromeos">
    pageVisibility = {
      autofill: false,
      people: false,
      onStartup: false,
      reset: false,
      appearance: false,
      defaultBrowser: false,
      advancedSettings: false,
      extensions: false,
    };
    // </if>
    // <if expr="chromeos">
    pageVisibility = {
      internet: showOSSettings,
      bluetooth: showOSSettings,
      multidevice: false,
      autofill: false,
      people: false,
      onStartup: false,
      reset: false,
      appearance: {
        setWallpaper: false,
        setTheme: false,
        homeButton: false,
        bookmarksBar: false,
        pageZoom: false,
      },
      device: showOSSettings,
      advancedSettings: true,
      privacy: {
        searchPrediction: false,
        networkPrediction: false,
      },
      downloads: {
        googleDrive: false,
      },
      extensions: false,
    };
    // </if>
  } else {
    // All pages are visible when not in chromeos. Since polymer only notifies
    // after a property is set.
    // <if expr="chromeos">
    pageVisibility = {
      internet: showOSSettings,
      bluetooth: showOSSettings,
      multidevice: showOSSettings,
      autofill: true,
      people: true,
      onStartup: true,
      reset: true,
      appearance: {
        setWallpaper: true,
        setTheme: true,
        homeButton: true,
        bookmarksBar: true,
        pageZoom: true,
      },
      device: showOSSettings,
      advancedSettings: true,
      privacy: {
        searchPrediction: true,
        networkPrediction: true,
      },
      downloads: {
        googleDrive: true,
      },
      extensions: true,
    };
    // </if>
  }

  return {pageVisibility: pageVisibility};
});
