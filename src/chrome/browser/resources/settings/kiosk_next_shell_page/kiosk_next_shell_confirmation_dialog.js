// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-kiosk-next-shell-confirmation-dialog' is a dialog shown to confirm
 * if a Kiosk Next Shell change is really wanted. Since enabling/disabling the
 * shell requires a sign out, we need to provide this dialog to avoid surprising
 * users.
 */

/**
 * Histogram name for KioskNextShell enabled state.
 * @type {string}
 */
const KIOSK_NEXT_SHELL_ENABLED_STATE_UMA_NAME = 'KioskNextShell.EnabledState';

Polymer({
  is: 'settings-kiosk-next-shell-confirmation-dialog',

  behaviors: [PrefsBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },
  },

  /** @override */
  attached: function() {
    this.$.dialog.showModal();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onCancelClick_: function(event) {
    this.$.dialog.cancel();
    event.stopPropagation();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onConfirmClick_: function(event) {
    const prefPath = 'ash.kiosk_next_shell.enabled';
    // Toggle previous enabled state.
    const isEnabled = !this.getPref(prefPath).value;
    chrome.send(
        'metricsHandler:recordBooleanHistogram',
        [KIOSK_NEXT_SHELL_ENABLED_STATE_UMA_NAME, isEnabled]);
    this.setPrefValue(prefPath, isEnabled);
    settings.LifetimeBrowserProxyImpl.getInstance().signOutAndRestart();
    this.$.dialog.close();
    event.stopPropagation();
  },

  /**
   * @param {boolean} kioskNextShellEnabled
   * @return {string}
   * @private
   */
  getTitleText_: function(kioskNextShellEnabled) {
    return loadTimeData.getString(
        kioskNextShellEnabled ? 'kioskNextShellEnabledDialogTitle' :
                                'kioskNextShellDisabledDialogTitle');
  },

  /**
   * @param {boolean} kioskNextShellEnabled
   * @return {string}
   * @private
   */
  getBodyText_: function(kioskNextShellEnabled) {
    return loadTimeData.getString(
        kioskNextShellEnabled ? 'kioskNextShellEnabledDialogBody' :
                                'kioskNextShellDisabledDialogBody');
  },

  /**
   * @param {boolean} kioskNextShellEnabled
   * @return {string}
   * @private
   */
  getConfirmationText_: function(kioskNextShellEnabled) {
    return loadTimeData.getString(
        kioskNextShellEnabled ? 'kioskNextShellTurnOff' :
                                'kioskNextShellTurnOn');
  },
});
