// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design App Downloading
 * screen.
 */

Polymer({
  is: 'app-downloading',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

  properties: {
    numOfApps: Number,

    /** Whether the user selected one app. */
    hasSingleApp: {
      type: Boolean,
      computed: 'hasSingleApp_(numOfApps)',
    },
  },

  focus() {
    this.$['app-downloading-dialog'].focus();
  },

  /** @private */
  onContinue_() {
    chrome.send(
        'login.AppDownloadingScreen.userActed',
        ['appDownloadingContinueSetup']);
  },

  /** @private */
  hasSingleApp_(numOfApps) {
    return numOfApps === 1;
  },

  /** @private */
  getDialogTitleA11yString_(numOfApps) {
    if (this.hasSingleApp_(numOfApps)) {
      return this.i18n('appDownloadingScreenTitleSingular');
    } else {
      return this.i18n('appDownloadingScreenTitlePlural', numOfApps);
    }
  },
});
