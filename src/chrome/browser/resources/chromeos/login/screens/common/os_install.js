// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for OS install screen.
 */

(function() {
const UIState = {
  INTRO: 'intro',
  IN_PROGRESS: 'in-progress',
  FAILED: 'failed',
  NO_DESTINATION_DEVICE_FOUND: 'no-destination-device-found',
  SUCCESS: 'success',
};

Polymer({
  is: 'os-install-element',

  behaviors: [
    OobeI18nBehavior,
    OobeDialogHostBehavior,
    LoginScreenBehavior,
    MultiStepBehavior,
  ],

  EXTERNAL_API: [
    'showStep',
    'setServiceLogs',
    'updateCountdownString',
  ],

  properties: {
    /**
     * Success step subtitile message.
     */
    osInstallDialogSuccessSubtitile_: {
      type: String,
      value: '',
    },
  },

  UI_STEPS: UIState,

  /**
   * @return {string}
   */
  defaultUIStep() {
    return UIState.INTRO;
  },

  ready() {
    this.initializeLoginScreen('OsInstallScreen', {
      resetAllowed: true,
    });
  },

  /**
   * Set and show screen step.
   * @param {string} step screen step.
   */
  showStep(step) {
    this.setUIStep(step);
  },

  /**
   * This is the 'on-click' event handler for the 'back' button.
   * @private
   */
  onBack_() {
    this.userActed('os-install-exit');
  },

  onIntroNextButtonPressed_() {
    this.$.osInstallDialogConfirm.showDialog();
    this.$.closeConfirmDialogButton.focus();
  },

  onConfirmNextButtonPressed_() {
    this.$.osInstallDialogConfirm.hideDialog();
    this.userActed('os-install-confirm-next');
  },

  onErrorSendFeedbackButtonPressed_() {
    this.userActed('os-install-error-send-feedback');
  },

  onErrorShutdownButtonPressed_() {
    this.userActed('os-install-error-shutdown');
  },

  onSuccessRestartButtonPressed_() {
    this.userActed('os-install-success-restart');
  },

  onCloseConfirmDialogButtonPressed_() {
    this.$.osInstallDialogConfirm.hideDialog();
    this.$.osInstallIntroNextButton.focus();
  },

  /**
   * @param {string} locale
   * @return {string}
   * @private
   */
  getConfirmBodyHtml_(locale) {
    return this.i18nAdvanced('osInstallDialogConfirmBody');
  },

  /**
   * @param {string} locale
   * @return {string}
   * @private
   */
  getErrorNoDestContentHtml_(locale) {
    return this.i18nAdvanced(
        'osInstallDialogErrorNoDestContent', {tags: ['p', 'ul', 'li']});
  },

  /**
   * @param {string} locale
   * @return {string}
   * @private
   */
  getErrorFailedSubtitleHtml_(locale) {
    return this.i18nAdvanced(
        'osInstallDialogErrorFailedSubtitle', {tags: ['p']});
  },

  /**
   * Shows service logs.
   * @private
   */
  onServiceLogsLinkClicked_() {
    this.$.serviceLogsDialog.showDialog();
    this.$.closeServiceLogsDialog.focus();
  },

  /**
   * On-click event handler for close button of the service logs dialog.
   * @private
   */
  hideServiceLogsDialog_() {
    this.$.serviceLogsDialog.hideDialog();
    this.focusLogsLink_();
  },

  /**
   * @private
   */
  focusLogsLink_() {
    if (this.uiStep == UIState.NO_DESTINATION_DEVICE_FOUND) {
      Polymer.RenderStatus.afterNextRender(
          this, () => this.$.noDestLogsLink.focus());
    } else if (this.uiStep == UIState.FAILED) {
      Polymer.RenderStatus.afterNextRender(
          this, () => this.$.serviceLogsLink.focus());
    }
  },

  /**
   * @param {string} serviceLogs Logs to show as plain text.
   */
  setServiceLogs(serviceLogs) {
    this.$.serviceLogsFrame.src = 'data:text/html;charset=utf-8,' +
        encodeURIComponent('<style>' +
                           'body {' +
                           '  font-family: Roboto, sans-serif;' +
                           '  color: RGBA(0,0,0,.87);' +
                           '  font-size: 14sp;' +
                           '  margin : 0;' +
                           '  padding : 0;' +
                           '  white-space: pre-wrap;' +
                           '}' +
                           '#logsContainer {' +
                           '  overflow: auto;' +
                           '  height: 99%;' +
                           '  padding-left: 16px;' +
                           '  padding-right: 16px;' +
                           '}' +
                           '#logsContainer::-webkit-scrollbar-thumb {' +
                           '  border-radius: 10px;' +
                           '}' +
                           '</style>' +
                           '<body><div id="logsContainer">' + serviceLogs +
                           '</div>' +
                           '</body>');
  },

  /**
   * @param {string} timeLeftMessage Countdown message on success step.
   */
  updateCountdownString(timeLeftMessage) {
    this.osInstallDialogSuccessSubtitile_ = timeLeftMessage;
  },
});
})();
