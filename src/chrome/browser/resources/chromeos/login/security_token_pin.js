// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for the security token PIN dialog shown during
 * sign-in.
 */

(function() {

// Only inform the user about the number of attempts left if it's smaller or
// equal to this constant. (This is a pure UX heuristic.)
// Please keep this constant in sync with the one in
// //chromeos/components/security_token_pin/error_generator.cc.
const ATTEMPTS_LEFT_THRESHOLD = 3;

Polymer({
  is: 'security-token-pin',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

  properties: {
    /**
     * Contains the OobeTypes.SecurityTokenPinDialogParameters object. It can be
     * null when our element isn't used.
     *
     * Changing this field resets the dialog state. (Please note that, due to
     * the Polymer's limitation, only assigning a new object is observed;
     * changing just a subproperty won't work.)
     */
    parameters: {
      type: Object,
      observer: 'onParametersChanged_',
    },

    /**
     * The i18n string ID containing the error label to be shown to the user.
     * Is null when there's no error label.
     * @private
     */
    errorLabelId_: {
      type: String,
      computed: 'computeErrorLabelId_(parameters)',
    },

    /**
     * Whether the current state is the wait for the processing completion
     * (i.e., the backend is verifying the entered PIN).
     * @private
     */
    processingCompletion_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the input is currently non-empty.
     * @private
     */
    hasValue_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the user has made changes in the input field since the dialog
     * was initialized or reset.
     * @private
     */
    userEdited_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the user can change the value in the input field.
     * @private
     */
    canEdit_: {
      type: Boolean,
      computed:
          'computeCanEdit_(parameters.attemptsLeft, processingCompletion_)',
    },

    /**
     * Whether the user can submit a login request.
     * @private
     */
    canSubmit_: {
      type: Boolean,
      computed: 'computeCanSubmit_(parameters.attemptsLeft, ' +
          'hasValue_, processingCompletion_)',
    },
  },

  focus() {
    // Note: setting the focus synchronously, to avoid flakiness in tests due to
    // racing between the asynchronous caret positioning and the PIN characters
    // input.
    this.$.pinKeyboard.focusInputSynchronously();
  },

  /** @override */
  onBeforeShow() {
    this.behaviors.forEach((behavior) => {
      if (behavior.onBeforeShow)
        behavior.onBeforeShow.call(this);
    });
    this.$$('#dialog').onBeforeShow();
  },

  /**
   * Returns the i18n string ID for the current error label.
   * @param {OobeTypes.SecurityTokenPinDialogParameters} parameters
   * @return {string|null}
   * @private
   */
  computeErrorLabelId_(parameters) {
    if (!parameters)
      return null;
    switch (parameters.errorLabel) {
      case OobeTypes.SecurityTokenPinDialogErrorType.NONE:
        return null;
      case OobeTypes.SecurityTokenPinDialogErrorType.UNKNOWN:
        return 'securityTokenPinDialogUnknownError';
      case OobeTypes.SecurityTokenPinDialogErrorType.INVALID_PIN:
        return 'securityTokenPinDialogUnknownInvalidPin';
      case OobeTypes.SecurityTokenPinDialogErrorType.INVALID_PUK:
        return 'securityTokenPinDialogUnknownInvalidPuk';
      case OobeTypes.SecurityTokenPinDialogErrorType.MAX_ATTEMPTS_EXCEEDED:
        return 'securityTokenPinDialogUnknownMaxAttemptsExceeded';
      default:
        assertNotReached(`Unexpected enum value: ${parameters.errorLabel}`);
    }
  },

  /**
   * Computes the value of the canEdit_ property.
   * @param {number} attemptsLeft
   * @param {boolean} processingCompletion
   * @return {boolean}
   * @private
   */
  computeCanEdit_(attemptsLeft, processingCompletion) {
    return attemptsLeft != 0 && !processingCompletion;
  },

  /**
   * Computes the value of the canSubmit_ property.
   * @param {number} attemptsLeft
   * @param {boolean} hasValue
   * @param {boolean} processingCompletion
   * @return {boolean}
   * @private
   */
  computeCanSubmit_(attemptsLeft, hasValue, processingCompletion) {
    return attemptsLeft != 0 && hasValue && !processingCompletion;
  },

  /**
   * Invoked when the "Back" button is clicked.
   * @private
   */
  onBackClicked_() {
    this.fire('cancel');
  },

  /**
   * Invoked when the "Next" button is clicked or Enter is pressed.
   * @private
   */
  onSubmit_() {
    if (!this.canSubmit_) {
      // Disallow submitting when it's not allowed or while proceeding the
      // previous submission.
      return;
    }
    this.processingCompletion_ = true;
    this.fire('completed', this.$.pinKeyboard.value);
  },

  /**
   * Observer that is called when the |parameters| property gets changed.
   * @private
   */
  onParametersChanged_() {
    // Reset the dialog to the initial state.
    this.$.pinKeyboard.value = '';
    this.processingCompletion_ = false;
    this.hasValue_ = false;
    this.userEdited_ = false;

    this.focus();
  },

  /**
   * Observer that is called when the user changes the PIN input field.
   * @param {!CustomEvent<{pin: string}>} e
   * @private
   */
  onPinChange_(e) {
    this.hasValue_ = e.detail.pin.length > 0;
    this.userEdited_ = true;
  },

  /**
   * Returns whether the error label should be shown.
   * @param {OobeTypes.SecurityTokenPinDialogParameters} parameters
   * @param {boolean} userEdited
   * @return {boolean}
   * @private
   */
  isErrorLabelVisible_(parameters, userEdited) {
    return parameters &&
        parameters.errorLabel !==
        OobeTypes.SecurityTokenPinDialogErrorType.NONE &&
        !userEdited;
  },

  /**
   * Returns whether the PIN attempts left count should be shown.
   * @param {OobeTypes.SecurityTokenPinDialogParameters} parameters
   * @return {boolean}
   * @private
   */
  isAttemptsLeftVisible_(parameters) {
    return parameters && parameters.attemptsLeft >= 0 &&
        parameters.attemptsLeft <= ATTEMPTS_LEFT_THRESHOLD;
  },

  /**
   * Returns whether there is a visible label for the PIN input field
   * @param {OobeTypes.SecurityTokenPinDialogParameters} parameters
   * @param {boolean} userEdited
   * @return {boolean}
   * @private
   */
  isLabelVisible_(parameters, userEdited) {
    return this.isErrorLabelVisible_(parameters, userEdited) ||
        this.isAttemptsLeftVisible_(parameters);
  },

  /**
   * Returns the label to be used for the PIN input field.
   * @param {string} locale
   * @param {OobeTypes.SecurityTokenPinDialogParameters} parameters
   * @param {string|null} errorLabelId
   * @param {boolean} userEdited
   * @return {string}
   * @private
   */
  getLabel_(locale, parameters, errorLabelId, userEdited) {
    if (!this.isLabelVisible_(parameters, userEdited)) {
      // Neither error nor the number of left attempts are to be displayed.
      return '';
    }
    if (!this.isErrorLabelVisible_(parameters, userEdited) &&
        this.isAttemptsLeftVisible_(parameters)) {
      // There's no error, but the number of left attempts has to be displayed.
      return this.i18n(
          'securityTokenPinDialogAttemptsLeft', parameters.attemptsLeft);
    }
    // If we get here, |parameters| must be defined, and |parameters.errorLabel|
    // != NONE, so |errorLabelId| will be defined.
    assert(errorLabelId);
    // Format the error and, if present, the number of left attempts.
    if ((parameters && !parameters.enableUserInput) ||
        !this.isAttemptsLeftVisible_(parameters)) {
      return this.i18n(errorLabelId);
    }
    return this.i18n(
        'securityTokenPinDialogErrorAttempts', this.i18n(errorLabelId),
        parameters.attemptsLeft);
  },
});
})();
