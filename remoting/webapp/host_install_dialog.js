// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * HostInstallDialog prompts the user to install host components.
 *
 * @constructor
 */
remoting.HostInstallDialog = function() {
  this.continueInstallButton_ = document.getElementById(
      'host-install-continue');
  this.cancelInstallButton_ = document.getElementById(
      'host-install-dismiss');
  this.retryInstallButton_ = document.getElementById(
      'host-install-retry');

  this.onOkClickedHandler_ = this.onOkClicked_.bind(this);
  this.onCancelClickedHandler_ = this.onCancelClicked_.bind(this);
  this.onRetryClickedHandler_ = this.onRetryClicked_.bind(this);

  this.continueInstallButton_.disabled = false;
  this.cancelInstallButton_.disabled = false;

  /** @param {remoting.HostController.AsyncResult} asyncResult @private*/
  this.onDoneHandler_ = function(asyncResult) {}

  /** @param {remoting.Error} error @private */
  this.onErrorHandler_ = function(error) {}
};

/** @type {Object.<string,string>} */
remoting.HostInstallDialog.hostDownloadUrls = {
  'Win32' : 'http://dl.google.com/dl/edgedl/chrome-remote-desktop/' +
      'chromeremotedesktophost.msi',
  'MacIntel' : 'https://dl.google.com/chrome-remote-desktop/' +
      'chromeremotedesktop.dmg',
  'Linux x86_64' : 'https://dl.google.com/linux/direct/' +
      'chrome-remote-desktop_current_amd64.deb',
  'Linux i386' : 'https://dl.google.com/linux/direct/' +
      'chrome-remote-desktop_current_i386.deb'
};

/**
 * Starts downloading host components and shows installation prompt.
 *
 * @param {remoting.HostPlugin} hostPlugin Used to install the host on Windows.
 * @param {function(remoting.HostController.AsyncResult):void} onDone Callback
 *     called when user clicks Ok, presumably after installing the host. The
 *     handler must verify that the host has been installed and call tryAgain()
 *     otherwise.
 * @param {function(remoting.Error):void} onError Callback called when user
 *    clicks Cancel button or there is some other unexpected error.
 * @return {void}
 */
remoting.HostInstallDialog.prototype.show = function(
    hostPlugin, onDone, onError) {
  // On Windows, host installation is automatic (handled by the NPAPI plugin)
  // and we don't show the dialog. On Mac and Linux, we show the dialog and the
  // user is expected to manually install the host before clicking OK.
  // TODO (weitaosu): Make host installation automatic for IT2Me (like Me2Me) on
  // Windows. Currently hostController is always null for IT2Me.
  if (navigator.platform == 'Win32' && hostPlugin != null) {
    // Currently we show two dialogs (each with a UAC prompt) when a user
    // enables the host for the first time, one for installing the host (by the
    // plugin) and the other for starting the host (by the native messaging
    // host). We'd like to reduce it to one but don't have a good solution
    // right now.
    // We also show the same message on the two dialogs because. We don't want
    // to confuse the user by saying "Installing Remote Desktop" because in
    // their mind "Remote Desktop" (the webapp) has already been installed.
    remoting.showSetupProcessingMessage(/*i18n-content*/'HOST_SETUP_STARTING');

    hostPlugin.installHost(onDone);
  } else {
    this.continueInstallButton_.addEventListener(
        'click', this.onOkClickedHandler_, false);
    this.cancelInstallButton_.addEventListener(
        'click', this.onCancelClickedHandler_, false);
    remoting.setMode(remoting.AppMode.HOST_INSTALL_PROMPT);

    var hostPackageUrl =
        remoting.HostInstallDialog.hostDownloadUrls[navigator.platform];
    if (hostPackageUrl === undefined) {
      this.onErrorHandler_(remoting.Error.CANCELLED);
      return;
    }

    // Start downloading the package.
    if (remoting.isAppsV2) {
      // TODO(jamiewalch): Use chrome.downloads when it is available to
      // apps v2 (http://crbug.com/174046)
      window.open(hostPackageUrl);
    } else {
      window.location = hostPackageUrl;
    }

    /** @type {function(remoting.HostController.AsyncResult):void} */
    this.onDoneHandler_ = onDone;

    /** @type {function(remoting.Error):void} */
    this.onErrorHandler_ = onError;
  }
}

/**
 * In manual host installation, onDone handler must call this method if it
 * detects that the host components are still unavailable. The same onDone
 * and onError callbacks will be used when user clicks Ok or Cancel.
 */
remoting.HostInstallDialog.prototype.tryAgain = function() {
  this.retryInstallButton_.addEventListener(
      'click', this.onRetryClickedHandler_.bind(this), false);
  remoting.setMode(remoting.AppMode.HOST_INSTALL_PENDING);
  this.continueInstallButton_.disabled = false;
  this.cancelInstallButton_.disabled = false;
};

remoting.HostInstallDialog.prototype.onOkClicked_ = function() {
  this.continueInstallButton_.removeEventListener(
      'click', this.onOkClickedHandler_, false);
  this.cancelInstallButton_.removeEventListener(
      'click', this.onCancelClickedHandler_, false);
  this.continueInstallButton_.disabled = true;
  this.cancelInstallButton_.disabled = true;

  this.onDoneHandler_(remoting.HostController.AsyncResult.OK);
}

remoting.HostInstallDialog.prototype.onCancelClicked_ = function() {
  this.continueInstallButton_.removeEventListener(
      'click', this.onOkClickedHandler_, false);
  this.cancelInstallButton_.removeEventListener(
      'click', this.onCancelClickedHandler_, false);
  this.onErrorHandler_(remoting.Error.CANCELLED);
}

remoting.HostInstallDialog.prototype.onRetryClicked_ = function() {
  this.retryInstallButton_.removeEventListener(
      'click', this.onRetryClickedHandler_.bind(this), false);
  this.continueInstallButton_.addEventListener(
      'click', this.onOkClickedHandler_, false);
  this.cancelInstallButton_.addEventListener(
      'click', this.onCancelClickedHandler_, false);
  remoting.setMode(remoting.AppMode.HOST_INSTALL_PROMPT);
};
