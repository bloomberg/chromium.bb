// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/**
   @param {string} pin A candidate PIN.
   @return {boolean} Whether the parameter was a valid PIN.
   @private
 */
function isValidPIN(pin) {
  // The UTF-8 encoding of the PIN must be between 4 and 63 bytes, and the
  // final byte cannot be zero.
  const utf8Encoded = new TextEncoder().encode(pin);
  if (utf8Encoded.length < 4 || utf8Encoded.length > 63 ||
      utf8Encoded[utf8Encoded.length - 1] == 0) {
    return false;
  }

  // A PIN must contain at least four code-points. Javascript strings are UCS-2
  // and the |length| property counts UCS-2 elements, not code-points. (For
  // example, '\u{1f6b4}'.length == 2, but it's a single code-point.) Therefore,
  // iterate over the string (which does yield codepoints) and check that four
  // or more were seen.
  let length = 0;
  for (const codepoint of pin) {
    length++;
  }

  return length >= 4;
}

/**
 * @fileoverview 'settings-security-keys-set-pin-dialog' is a dialog for
 * setting and changing security key PINs.
 */
Polymer({
  is: 'settings-security-keys-set-pin-dialog',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Whether the value of the current PIN textbox is a valid PIN or not.
     * @private
     */
    currentPINValid_: Boolean,

    /** @private */
    newPINValid_: Boolean,

    /** @private */
    confirmPINValid_: Boolean,

    /**
     * Whether the dialog is in a state where the Set PIN button should be
     * enabled. Read by Polymer.
     * @private
     */
    setPINButtonValid_: {
      type: Boolean,
      value: false,
    },

    /**
     * The value of the new PIN textbox. Read/write by Polymer.
     * @private
     */
    newPIN_: {
      type: String,
      value: '',
    },

    /** @private */
    confirmPIN_: {
      type: String,
      value: '',
    },

    /** @private */
    currentPIN_: {
      type: String,
      value: '',
    },

    /**
     * The number of PIN attempts remaining.
     * @private
     */
    retries_: Number,

    /**
     * A CTAP error code when we don't recognise the specific error. Read by
     * Polymer.
     * @private
     */
    errorCode_: Number,

    /**
     * Whether the error message indicating an incorrect PIN should be visible.
     * @private
     */
    mismatchErrorVisible_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the dialog process has completed, successfully or otherwise.
     * @private
     */
    complete_: {
      type: Boolean,
      value: false,
    },

    /**
     * The id of an element on the page that is currently shown.
     * @private
     */
    shown_: {
      type: String,
      value: 'initial',
    },

    /** @private */
    title_: String,
  },

  /** @private {?settings.SecurityKeysBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached: function() {
    this.title_ = this.i18n('securityKeysSetPINInitialTitle');
    this.browserProxy_ = settings.SecurityKeysBrowserProxyImpl.getInstance();
    this.$.dialog.showModal();

    this.browserProxy_.startSetPIN().then(result => {
      if (result[0]) {
        // Operation is complete. result[1] is a CTAP error code. See
        // https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#error-responses
        if (result[1] == 1 /* INVALID_COMMAND */) {
          this.shown_ = 'noPINSupport';
          this.finish_();
        } else if (result[1] == 52 /* temporarily locked */) {
          this.shown_ = 'reinsert';
          this.finish_();
        } else if (result[1] == 50 /* locked */) {
          this.shown_ = 'locked';
          this.finish_();
        } else {
          this.errorCode_ = result[1];
          this.shown_ = 'error';
          this.finish_();
        }
      } else if (result[1] == 0) {
        // A device can also signal that it is locked by returning zero retries.
        this.shown_ = 'locked';
        this.finish_();
      } else {
        // Need to prompt for a pin. Initially set the text boxes to valid so
        // that they don't all appear red without the user typing anything.
        this.currentPINValid_ = true;
        this.newPINValid_ = true;
        this.confirmPINValid_ = true;

        this.retries_ = result[1];
        // retries_ may be null to indicate that there is currently no PIN set.
        let focusTarget;
        if (this.retries_ === null) {
          this.$.currentPINEntry.hidden = true;
          focusTarget = this.$.newPIN;
          this.title_ = this.i18n('securityKeysSetPINCreateTitle');
        } else {
          this.$.currentPINEntry.hidden = false;
          focusTarget = this.$.currentPIN;
          this.title_ = this.i18n('securityKeysSetPINChangeTitle');
        }

        this.shown_ = 'pinPrompt';
        // Focus cannot be set directly from within a backend callback.
        window.setTimeout(function() {
          focusTarget.focus();
        }, 0);
        this.fire('ui-ready');  // for test synchronization.
      }
    });
  },

  /** @private */
  closeDialog_: function() {
    this.$.dialog.close();
    this.finish_();
  },

  /** @private */
  finish_: function() {
    if (this.complete_) {
      return;
    }
    this.complete_ = true;
    this.browserProxy_.close();
  },

  /** @private */
  updatePINButtonValid_: function() {
    this.setPINButtonValid_ =
        (this.$.currentPINEntry.hidden ||
         (this.currentPINValid_ && this.currentPIN_.length > 0)) &&
        this.newPINValid_ && this.newPIN_.length > 0 && this.confirmPINValid_ &&
        this.confirmPIN_.length > 0;
  },

  /** @private */
  validateCurrentPIN_: function() {
    this.currentPINValid_ = isValidPIN(this.currentPIN_);
    this.updatePINButtonValid_();
    // Typing in the current PIN box after an error makes the error message
    // disappear.
    this.mismatchErrorVisible_ = false;
  },

  /** @private */
  validateNewPIN_: function() {
    this.newPINValid_ = isValidPIN(this.newPIN_);
    // The new PIN might have been changed to match the confirmation PIN, thus
    // changing it might make the confirmation PIN valid. An empty value is
    // considered valid to stop it immediately turning red before the user has
    // entered anything, but |updatePINButtonValid_| knows that it needs to be
    // non-empty before the dialog can be submitted.
    this.confirmPINValid_ = this.confirmPIN_.length == 0 ||
        (this.newPINValid_ && this.confirmPIN_ == this.newPIN_);
    this.updatePINButtonValid_();
  },

  /** @private */
  validateConfirmPIN_: function() {
    this.confirmPINValid_ = this.confirmPIN_ == this.newPIN_;
    this.updatePINButtonValid_();
  },

  /**
   * Called by Polymer when the Set PIN button is activated.
   * @private
   */
  pinSubmitNew_: function() {
    this.setPINButtonValid_ = false;
    this.browserProxy_.setPIN(this.currentPIN_, this.newPIN_).then(result => {
      // This call always completes the process so result[0] is always 1.
      // result[1] is a CTAP2 error code. See
      // https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#error-responses
      if (result[1] == 0 /* SUCCESS */) {
        this.shown_ = 'success';
        this.finish_();
      } else if (result[1] == 52 /* temporarily locked */) {
        this.shown_ = 'reinsert';
        this.finish_();
      } else if (result[1] == 50 /* locked */) {
        this.shown_ = 'locked';
        this.finish_();
      } else if (result[1] == 49 /* PIN_INVALID */) {
        this.currentPIN_ = '';
        this.currentPINValid_ = false;
        this.retries_--;
        this.mismatchErrorVisible_ = true;

        // Focus cannot be set directly from within a backend callback. Also,
        // directly focusing |currentPIN| doesn't always seem to work(!). Thus
        // focus something else first, which is a hack that seems to solve the
        // problem.
        const preFocusTarget = this.$.newPIN;
        const focusTarget = this.$.currentPIN;
        window.setTimeout(function() {
          preFocusTarget.focus();
          focusTarget.focus();
        }, 0);
        this.fire('ui-ready');  // for test synchronization.
      } else {
        // Unknown error.
        this.errorCode_ = result[1];
        this.shown_ = 'error';
        this.finish_();
      }
    });
  },

  /**
   * Called by Polymer when |errorCode_| changes to set the error string.
   * @param {number} code A CTAP error code.
   * @private
   */
  pinFailed_: function(code) {
    if (code === null) {
      return '';
    }
    return this.i18n('securityKeysPINError', code.toString());
  },

  /**
   * Called by Polymer to set the error text displayed when the user enters an
   * incorrect PIN.
   * @param {boolean} show Whether or not an error message should be shown.
   * @param {number} retries The number of PIN attempts remaining.
   * @return {string} The message to show under the text box.
   * @private
   */
  mismatchErrorText_: function(show, retries) {
    if (!show) {
      return '';
    }

    // Warn the user if the number of retries is getting low.
    if (1 < retries && retries <= 3) {
      return this.i18n('securityKeysPINIncorrectRetriesPl', retries.toString());
    }
    if (retries == 1) {
      return this.i18n('securityKeysPINIncorrectRetriesSin');
    }
    return this.i18n('securityKeysPINIncorrect');
  },

  /**
   * @param {boolean} complete Whether the dialog process is complete.
   * @return {string} The class of the Ok / Cancel button.
   * @private
   */
  maybeActionButton_: function(complete) {
    return complete ? 'action-button' : 'cancel-button';
  },

  /**
   * @param {boolean} complete Whether the dialog process is complete.
   * @return {string} The label of the Ok / Cancel button.
   * @private
   */
  closeText_: function(complete) {
    return this.i18n(complete ? 'ok' : 'cancel');
  },
});
})();
