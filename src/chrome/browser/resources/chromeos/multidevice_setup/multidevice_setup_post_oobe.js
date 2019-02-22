// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * MultiDevice setup flow which is shown after OOBE has completed.
 */
Polymer({
  is: 'multidevice-setup-post-oobe',

  properties: {
    /** @private {!multidevice_setup.MultiDeviceSetupDelegate} */
    delegate_: Object,

    /**
     * Text to be shown on the forward navigation button.
     * @private {string|undefined}
     */
    forwardButtonText: {
      type: String,
      value: '',
    },

    /**
     * Whether the forward button should be disabled.
     * @private
     */
    forwardButtonDisabled_: {
      type: Boolean,
      value: false,
    },

    /**
     * Text to be shown on the cancel navigation button.
     * @private {string|undefined}
     */
    cancelButtonText_: {
      type: String,
      value: '',
    },

    /**
     * Text to be shown on the backward navigation button.
     * @private {string|undefined}
     */
    backwardButtonText_: {
      type: String,
      value: '',
    },
  },

  /** @override */
  attached: function() {
    this.delegate_ = new multidevice_setup.PostOobeDelegate();
    this.$$('multidevice-setup').initializeSetupFlow();
    this.onForwardButtonFocusRequested_();
  },

  /** @private */
  onExitRequested_: function() {
    chrome.send('dialogClose');
  },

  /** @private */
  onForwardButtonFocusRequested_: function() {
    this.$$('#forward-button').focus();
  }
});
