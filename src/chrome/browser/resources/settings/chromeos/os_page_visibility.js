// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Specifies page visibility based on incognito status and Chrome OS guest mode.
 * @typedef {{
 *   a11y: (boolean|undefined),
 *   advancedSettings: (boolean|undefined),
 *   appearance: (OSAppearancePageVisibility|undefined),
 *   autofill: (boolean|undefined),
 *   bluetooth: (boolean|undefined),
 *   dateTime: (boolean|undefined),
 *   device: (boolean|undefined),
 *   downloads: (DownloadsPageVisibility|undefined),
 *   internet: (boolean|undefined),
 *   languages: (LanguagesPageVisibility|undefined),
 *   multidevice: (boolean|undefined),
 *   onStartup: (boolean|undefined),
 *   people: (boolean|undefined|PeoplePageVisibility),
 *   printing: (boolean|undefined),
 *   privacy: (OSPrivacyPageVisibility|undefined),
 *   reset: (boolean|undefined),
 * }}
 */
let OSPageVisibility;

/**
 * @typedef {{
 *   bookmarksBar: boolean,
 *   homeButton: boolean,
 *   pageZoom: boolean,
 *   setTheme: boolean,
 *   setWallpaper: boolean,
 * }}
 */
let OSAppearancePageVisibility;

/**
 * @typedef {{
 *   googleDrive: boolean,
 *   smbShares: boolean,
 * }}
 */
let DownloadsPageVisibility;

/**
 * @typedef {{
 *   googleAccounts: boolean,
 *   kerberosAccounts: boolean,
 *   lockScreen: boolean,
 *   manageUsers: boolean,
 * }}
 */
let PeoplePageVisibility;

/**
 * @typedef {{
 *   contentProtectionAttestation: boolean,
 *   networkPrediction: boolean,
 *   searchPrediction: boolean,
 *   wakeOnWifi: boolean,
 * }}
 */
let OSPrivacyPageVisibility;

/**
 * @typedef {{
 *   manageInputMethods: boolean,
 *   inputMethodsList: boolean,
 * }}
 */
let LanguagesPageVisibility;

cr.define('settings', function() {
  /**
   * Dictionary defining page visibility.
   * @type {!OSPageVisibility}
   */
  let osPageVisibility;

  const isAccountManagerEnabled =
      loadTimeData.valueExists('isAccountManagerEnabled') &&
      loadTimeData.getBoolean('isAccountManagerEnabled');
  const isKerberosEnabled = loadTimeData.valueExists('isKerberosEnabled') &&
      loadTimeData.getBoolean('isKerberosEnabled');

  if (loadTimeData.getBoolean('isGuest')) {
    osPageVisibility = {
      internet: true,
      bluetooth: true,
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
      device: true,
      advancedSettings: true,
      dateTime: true,
      privacy: {
        contentProtectionAttestation: true,
        searchPrediction: false,
        networkPrediction: false,
        wakeOnWifi: true,
      },
      downloads: {
        googleDrive: false,
        smbShares: false,
      },
      a11y: true,
      extensions: false,
      printing: true,
      languages: {
        manageInputMethods: true,
        inputMethodsList: true,
      },
    };
  } else {
    osPageVisibility = {
      internet: true,
      bluetooth: true,
      multidevice: true,
      autofill: true,
      people: {
        lockScreen: true,
        kerberosAccounts: isKerberosEnabled,
        googleAccounts: isAccountManagerEnabled,
        manageUsers: true,
      },
      onStartup: true,
      reset: true,
      appearance: {
        setWallpaper: true,
        setTheme: true,
        homeButton: true,
        bookmarksBar: true,
        pageZoom: true,
      },
      device: true,
      advancedSettings: true,
      dateTime: true,
      privacy: {
        contentProtectionAttestation: true,
        searchPrediction: true,
        networkPrediction: true,
        wakeOnWifi: true,
      },
      downloads: {
        googleDrive: true,
        smbShares: true,
      },
      a11y: true,
      extensions: true,
      printing: true,
      languages: {
        manageInputMethods: true,
        inputMethodsList: true,
      },
    };
  }

  // #cr_define_end
  return {osPageVisibility: osPageVisibility};
});
