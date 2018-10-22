// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'password-page',

  behaviors: [
    UiPageContainerBehavior,
  ],

  properties: {
    /**
     * Whether forward button should be disabled. In this context, the forward
     * button should be disabled if the user has not entered a password or if
     * the user has submitted an incorrect password and has not yet edited it.
     * @type {boolean}
     */
    forwardButtonDisabled: {
      type: Boolean,
      computed: 'shouldForwardButtonBeDisabled_(inputValue_, passwordInvalid_)',
      notify: true,
    },

    /** Overridden from UiPageContainerBehavior. */
    forwardButtonTextId: {
      type: String,
      value: 'done',
    },

    /** Overridden from UiPageContainerBehavior. */
    backwardButtonTextId: {
      type: String,
      value: 'cancel',
    },

    /** Overridden from UiPageContainerBehavior. */
    headerId: {
      type: String,
      value: 'passwordPageHeader',
    },

    /**
     * Authentication token; retrieved using the quickUnlockPrivate API.
     * @type {string}
     */
    authToken: {type: String, value: '', notify: true},

    /** @private {string} */
    profilePhotoUrl_: {
      type: String,
      value: '',
    },

    /** @private {string} */
    email_: {
      type: String,
      value: '',
    },

    /** @private {!QuickUnlockPrivate} */
    quickUnlockPrivate_: {type: Object, value: chrome.quickUnlockPrivate},

    /** @private {string} */
    inputValue_: {
      type: String,
      value: '',
      observer: 'onInputValueChange_',
    },

    /** @private {boolean} */
    passwordInvalid_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {?multidevice_setup.BrowserProxy} */
  browserProxy_: null,

  /**
   * Function which clears this.authToken after it has expired.
   * @private {number|undefined}
   */
  clearAccountPasswordTimeout_: undefined,

  clearPasswordTextInput: function() {
    this.$.passwordInput.value = '';
  },

  /** @override */
  created: function() {
    this.browserProxy_ = multidevice_setup.BrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    this.browserProxy_.getProfileInfo().then((profileInfo) => {
      this.profilePhotoUrl_ = profileInfo.profilePhotoUrl;
      this.email_ = profileInfo.email;
    });

    this.$.passwordInput.focus();
  },

  /** Overridden from UiPageContainerBehavior. */
  getCanNavigateToNextPage: function() {
    clearTimeout(this.clearAccountPasswordTimeout_);

    return new Promise((resolve) => {
      this.quickUnlockPrivate_.getAuthToken(this.inputValue_, (tokenInfo) => {
        if (chrome.runtime.lastError) {
          this.passwordInvalid_ = true;
          // Select the password text if the user entered an incorrect password.
          this.$.passwordInput.select();
          resolve(false /* canNavigate */);
          return;
        }

        this.authToken = tokenInfo.token;
        this.passwordInvalid_ = false;

        // Subtract time from the exiration time to account for IPC delays.
        // Treat values less than the minimum as 0 for testing.
        const IPC_SECONDS = 2;
        const lifetimeMs = tokenInfo.lifetimeSeconds > IPC_SECONDS ?
            (tokenInfo.lifetimeSeconds - IPC_SECONDS) * 1000 :
            0;
        this.clearAccountPasswordTimeout_ = setTimeout(() => {
          this.authToken = '';
        }, lifetimeMs);

        resolve(true /* canNavigate */);
      });
    });
  },

  /** @private */
  onInputValueChange_: function() {
    this.passwordInvalid_ = false;
  },

  /** @private */
  onUserPressedEnter_: function() {
    this.fire('user-submitted-password');
  },

  /**
   * @return {boolean} Whether the forward button should be disabled.
   * @private
   */
  shouldForwardButtonBeDisabled_: function() {
    return this.passwordInvalid_ || !this.inputValue_;
  },
});
