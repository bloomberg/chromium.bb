// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fingerprint enrollment screen implementation.
 */

login.createScreen('FingerprintSetupScreen', 'fingerprint-setup', function() {
  return {
    EXTERNAL_API: ['onEnrollScanDone', 'enableAddAnotherFinger'],

    /** Initial UI State for screen */
    getOobeUIInitialState() {
      return OOBE_UI_STATE.ONBOARDING;
    },

    /**
     * Fingerprint setup module.
     * @private
     */
    fingerprintSetupModule_: null,


    /** @override */
    decorate() {
      this.fingerprintSetupModule_ = $('fingerprint-setup-impl');
    },

    /**
     * Returns the control which should receive initial focus.
     */
    get defaultControl() {
      return this.fingerprintSetupModule_;
    },

    /*
     * Executed on language change.
     */
    updateLocalizedContent() {
      this.fingerprintSetupModule_.i18nUpdateLocale();
    },

    /**
     * Called when a fingerprint enroll scan result is received.
     * @param {number} scanResult Result of the enroll scan.
     * @param {boolean} isComplete Whether fingerprint enrollment is complete.
     * @param {number} percentComplete Percentage of completion of enrollment.
     */
    onEnrollScanDone(scanResult, isComplete, percentComplete) {
      this.fingerprintSetupModule_.onEnrollScanDone(
          scanResult, isComplete, percentComplete);
    },

    /**
     * Enable/disable add another finger.
     * @param {boolean} enable True if add another fingerprint is enabled.
     */
    enableAddAnotherFinger(enable) {
      this.fingerprintSetupModule_.canAddFinger = enable;
    },
  };
});
