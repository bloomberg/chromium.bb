// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview MultiDevice setup flow Polymer element to be used in the first
 *     run (i.e., after OOBE or during the user's first login on this
 *     Chromebook).
 */

cr.define('multidevice_setup', function() {
  /** @implements {multidevice_setup.MultiDeviceSetupDelegate} */
  class MultiDeviceSetupFirstRunDelegate {
    constructor() {
      /**
       * @private {?chromeos.multideviceSetup.mojom.
       *               PrivilegedHostDeviceSetterProxy}
       */
      this.proxy_ = null;
    }

    /** @override */
    isPasswordRequiredToSetHost() {
      return false;
    }

    /** @override */
    setHostDevice(hostDeviceId, opt_authToken) {
      // An authentication token is not expected since a password is not
      // required.
      assert(!opt_authToken);

      if (!this.proxy_) {
        this.proxy_ = chromeos.multideviceSetup.mojom.PrivilegedHostDeviceSetter
                          .getProxy();
      }

      return /** @type {!Promise<{success: boolean}>} */ (
          this.proxy_.setHostDevice(hostDeviceId));
    }

    /** @override */
    shouldExitSetupFlowAfterSettingHost() {
      return true;
    }

    /** @override */
    getStartSetupCancelButtonTextId() {
      return 'noThanks';
    }
  }

  const MultiDeviceSetupFirstRun = Polymer({
    is: 'multidevice-setup-first-run',

    behaviors: [I18nBehavior, WebUIListenerBehavior],

    properties: {
      /** @private {!multidevice_setup.MultiDeviceSetupDelegate} */
      delegate_: Object,

      /**
       * ID of loadTimeData string to be shown on the forward navigation button.
       * @private {string|undefined}
       */
      forwardButtonTextId_: {
        type: String,
      },

      /**
       * Whether the forward button should be disabled.
       * @private {boolean}
       */
      forwardButtonDisabled_: {
        type: Boolean,
        value: false,
      },

      /**
       * ID of loadTimeData string to be shown on the cancel button.
       * @private {string|undefined}
       */
      cancelButtonTextId_: {
        type: String,
      },

      /** Whether the webview overlay should be hidden. */
      webviewOverlayHidden_: {
        type: Boolean,
        value: true,
      },

      /**
       * URL for the webview to display.
       * @private {string|undefined}
       */
      webviewSrc_: {
        type: String,
        value: '',
      },
    },

    listeners: {
      'open-learn-more-webview-requested': 'onOpenLearnMoreWebviewRequested_',
    },

    /** @override */
    attached: function() {
      this.delegate_ = new MultiDeviceSetupFirstRunDelegate();
    },

    /** @override */
    ready: function() {
      this.updateLocalizedContent();
    },

    updateLocalizedContent: function() {
      this.i18nUpdateLocale();
      this.$.multideviceSetup.updateLocalizedContent();
    },

    onForwardButtonFocusRequested_: function() {
      this.$.nextButton.focus();
    },

    /**
     * @param {!CustomEvent<!{didUserCompleteSetup: boolean}>} event
     * @private
     */
    onExitRequested_: function(event) {
      if (event.detail.didUserCompleteSetup) {
        chrome.send(
            'login.MultiDeviceSetupScreen.userActed', ['setup-accepted']);
      } else {
        chrome.send(
            'login.MultiDeviceSetupScreen.userActed', ['setup-declined']);
      }
    },

    /**
     * @param {boolean} shouldShow
     * @param {string=} opt_url
     * @private
     */
    setWebviewOverlayVisibility_: function(shouldShow, opt_url) {
      if (opt_url) {
        this.webviewSrc_ = opt_url;
      }
      this.webviewOverlayHidden_ = !shouldShow;
    },

    /** @private */
    hideWebviewOverlay_: function() {
      this.setWebviewOverlayVisibility_(false /* shouldShow */);
    },

    /**
     * @param {!CustomEvent<string>} event
     * @private
     */
    onOpenLearnMoreWebviewRequested_: function(event) {
      this.setWebviewOverlayVisibility_(
          true /* shouldShow */, event.detail /* url */);
    },

    /** @private */
    getOverlayCloseTopTitle_: function() {
      return this.i18n('arcOverlayClose');
    },
  });

  return {
    MultiDeviceSetupFirstRun: MultiDeviceSetupFirstRun,
  };
});
