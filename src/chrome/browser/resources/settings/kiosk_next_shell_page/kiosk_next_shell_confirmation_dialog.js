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
Polymer({
  is: 'settings-kiosk-next-shell-confirmation-dialog',

  behaviors: [I18nBehavior, PrefsBehavior],

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
  onCancelTap_: function(event) {
    this.$.dialog.close();
    event.stopPropagation();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onConfirmTap_: function(event) {
    const prefPath = 'ash.kiosk_next_shell.enabled';
    this.setPrefValue(prefPath, !this.getPref(prefPath).value);
    settings.LifetimeBrowserProxyImpl.getInstance().signOutAndRestart();
    event.stopPropagation();
  },

  /**
   * @private
   * @return {string}
   */
  getTitleText_: function(kioskNextShellEnabled) {
    return kioskNextShellEnabled ?
        this.i18n('kioskNextShellEnabledDialogTitle') :
        this.i18n('kioskNextShellDisabledDialogTitle');
  },

  /**
   * @private
   * @return {string}
   */
  getBodyText_: function(kioskNextShellEnabled) {
    return kioskNextShellEnabled ?
        this.i18n('kioskNextShellEnabledDialogBody') :
        this.i18n('kioskNextShellDisabledDialogBody');
  },

  /**
   * @private
   * @return {string}
   */
  getConfirmationText_: function(kioskNextShellEnabled) {
    return kioskNextShellEnabled ? this.i18n('kioskNextShellTurnOff') :
                                   this.i18n('kioskNextShellTurnOn');
  },
});
