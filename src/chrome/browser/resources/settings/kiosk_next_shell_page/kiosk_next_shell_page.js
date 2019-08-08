// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-kiosk-next-shell-page' is the settings page for enabling the
 * Kiosk Next Shell.
 */
Polymer({
  is: 'settings-kiosk-next-shell-page',

  behaviors: [PrefsBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    showConfirmationDialog_: Boolean,
  },

  /**
   * @private
   * @param {!Event} event
   */
  onToggleButtonClick_: function(event) {
    this.showConfirmationDialog_ = true;
    event.stopPropagation();
  },

  /**
   * @private
   * @param {!Event} event
   */
  onConfirmationDialogClose_: function(event) {
    this.showConfirmationDialog_ = false;
    event.stopPropagation();
  },

  /**
   * @private
   * @param {boolean} kioskNextShellEnabled
   * @return {string}
   */
  getSubtextLabel_: function(kioskNextShellEnabled) {
    return loadTimeData.getString(
      kioskNextShellEnabled ? 'kioskNextShellPageSubtextDisable' :
                              'kioskNextShellPageSubtextEnable');
  },

  /**
   * @private
   * @param {boolean} kioskNextShellEnabled
   * @return {string}
   */
  getButtonLabel_: function(kioskNextShellEnabled) {
    return loadTimeData.getString(
      kioskNextShellEnabled ? 'kioskNextShellTurnOff' :
                              'kioskNextShellTurnOn');
  }
});
