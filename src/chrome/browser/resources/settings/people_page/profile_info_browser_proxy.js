// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

cr.define('settings', function() {
  /**
   * An object describing the profile.
   * @typedef {{
   *   name: string,
   *   iconUrl: string
   * }}
   */
  /* #export */ let ProfileInfo;

  /** @interface */
  /* #export */ class ProfileInfoBrowserProxy {
    /**
     * Returns a Promise for the profile info.
     * @return {!Promise<!settings.ProfileInfo>}
     */
    getProfileInfo() {}

    /**
     * Requests the profile stats count. The result is returned by the
     * 'profile-stats-count-ready' WebUI listener event.
     */
    getProfileStatsCount() {}
  }

  /**
   * @implements {settings.ProfileInfoBrowserProxy}
   */
  /* #export */ class ProfileInfoBrowserProxyImpl {
    /** @override */
    getProfileInfo() {
      return cr.sendWithPromise('getProfileInfo');
    }

    /** @override */
    getProfileStatsCount() {
      chrome.send('getProfileStatsCount');
    }
  }

  cr.addSingletonGetter(ProfileInfoBrowserProxyImpl);

  // #cr_define_end
  return {
    ProfileInfo,
    ProfileInfoBrowserProxy,
    ProfileInfoBrowserProxyImpl,
  };
});
