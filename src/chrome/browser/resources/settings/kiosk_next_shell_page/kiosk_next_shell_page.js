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

  behaviors: [I18nBehavior, PrefsBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    showConfirmationDialog_: Boolean,
  },

  /**
   * @private
   * @param {!Event} event
   */
  onToggleButtonPressed_: function(event) {
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
    return kioskNextShellEnabled
        ? this.i18n('kioskNextShellPageSubtextDisable')
        : this.i18n('kioskNextShellPageSubtextEnable');
  },

  /**
   * @private
   * @param {boolean} kioskNextShellEnabled
   * @return {string}
   */
  getButtonLabel_: function(kioskNextShellEnabled) {
    return kioskNextShellEnabled
        ? this.i18n('kioskNextShellTurnOff')
        : this.i18n('kioskNextShellTurnOn');
  }
});
