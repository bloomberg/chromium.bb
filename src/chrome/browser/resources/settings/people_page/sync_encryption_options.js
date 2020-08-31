// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

/**
 * Names of the radio buttons which allow the user to choose their encryption
 * mechanism.
 * @enum {string}
 */
const RadioButtonNames = {
  ENCRYPT_WITH_GOOGLE: 'encrypt-with-google',
  ENCRYPT_WITH_PASSPHRASE: 'encrypt-with-passphrase',
};

Polymer({
  is: 'settings-sync-encryption-options',

  properties: {
    /**
     * @type {settings.SyncPrefs}
     */
    syncPrefs: {
      type: Object,
      notify: true,
    },

    /** @type {settings.SyncStatus} */
    syncStatus: Object,

    /**
     * Whether the "create passphrase" inputs should be shown. These inputs
     * give the user the opportunity to use a custom passphrase instead of
     * authenticating with their Google credentials.
     * @private
     */
    creatingNewPassphrase_: {
      type: Boolean,
      value: false,
    },

    /**
     * The passphrase input field value.
     * @private
     */
    passphrase_: {
      type: String,
      value: '',
    },

    /**
     * The passphrase confirmation input field value.
     * @private
     */
    confirmation_: {
      type: String,
      value: '',
    },

    /** @private */
    disableEncryptionOptions_: {
      type: Boolean,
      computed: 'computeDisableEncryptionOptions_(' +
          'syncPrefs, syncStatus)',
      observer: 'disableEncryptionOptionsChanged_',
    },
  },

  /**
   * Whether we should disable the radio buttons that allow choosing the
   * encryption options for Sync.
   * We disable the buttons if:
   * (a) full data encryption is enabled, or,
   * (b) full data encryption is not allowed (so far, only applies to
   * supervised accounts), or,
   * (c) current encryption keys are missing, or,
   * (d) the user is a supervised account.
   * @return {boolean}
   * @private
   */
  computeDisableEncryptionOptions_() {
    return !!(
        (this.syncPrefs &&
         (this.syncPrefs.encryptAllData ||
          !this.syncPrefs.encryptAllDataAllowed ||
          this.syncPrefs.trustedVaultKeysRequired)) ||
        (this.syncStatus && this.syncStatus.supervisedUser));
  },

  /** @private */
  disableEncryptionOptionsChanged_() {
    if (this.disableEncryptionOptions_) {
      this.creatingNewPassphrase_ = false;
    }
  },

  /**
   * @param {string} passphrase The passphrase input field value
   * @param {string} confirmation The passphrase confirmation input field value.
   * @return {boolean} Whether the passphrase save button should be enabled.
   * @private
   */
  isSaveNewPassphraseEnabled_(passphrase, confirmation) {
    return passphrase !== '' && confirmation !== '';
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onNewPassphraseInputKeypress_(e) {
    if (e.type == 'keypress' && e.key != 'Enter') {
      return;
    }
    this.saveNewPassphrase_();
  },

  /** @private */
  onSaveNewPassphraseClick_() {
    this.saveNewPassphrase_();
  },

  /**
   * Sends the newly created custom sync passphrase to the browser.
   * @private
   */
  saveNewPassphrase_() {
    assert(this.creatingNewPassphrase_);
    chrome.metricsPrivate.recordUserAction('Sync_SaveNewPassphraseClicked');
    // Might happen within the transient time between the request to
    // |setSyncEncryption| and receiving the response.
    if (this.syncPrefs.encryptAllData) {
      return;
    }
    // If a new password has been entered but it is invalid, do not send the
    // sync state to the API.
    if (!this.validateCreatedPassphrases_()) {
      return;
    }

    this.syncPrefs.encryptAllData = true;
    this.syncPrefs.setNewPassphrase = true;
    this.syncPrefs.passphrase = this.passphrase_;

    settings.SyncBrowserProxyImpl.getInstance()
        .setSyncEncryption(this.syncPrefs)
        .then(pageStatus => {
          this.fire('passphrase-changed', pageStatus);
        });
  },

  /**
   * Called when the encryption
   * @param {!CustomEvent<{value: string}>} event
   * @private
   */
  onEncryptionRadioSelectionChanged_(event) {
    this.creatingNewPassphrase_ =
        event.detail.value == RadioButtonNames.ENCRYPT_WITH_PASSPHRASE;
  },

  /**
   * Computed binding returning the selected encryption radio button.
   * @private
   */
  selectedEncryptionRadio_() {
    return this.syncPrefs.encryptAllData || this.creatingNewPassphrase_ ?
        RadioButtonNames.ENCRYPT_WITH_PASSPHRASE :
        RadioButtonNames.ENCRYPT_WITH_GOOGLE;
  },

  /**
   * Checks the supplied passphrases to ensure that they are not empty and that
   * they match each other. Additionally, displays error UI if they are invalid.
   * @return {boolean} Whether the check was successful (i.e., that the
   *     passphrases were valid).
   * @private
   */
  validateCreatedPassphrases_() {
    const emptyPassphrase = !this.passphrase_;
    const mismatchedPassphrase = this.passphrase_ != this.confirmation_;

    this.$$('#passphraseInput').invalid = emptyPassphrase;
    this.$$('#passphraseConfirmationInput').invalid =
        !emptyPassphrase && mismatchedPassphrase;

    return !emptyPassphrase && !mismatchedPassphrase;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onLearnMoreClick_(event) {
    if (event.target.tagName == 'A') {
      // Stop the propagation of events, so that clicking on links inside
      // checkboxes or radio buttons won't change the value.
      event.stopPropagation();
    }
  },
});
})();
