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

  /** @private*/
  this.onDoneHandler_ = function() {}

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
 * @param {function():void} onDone Callback called when user clicks Ok,
 * presumably after installing the host. The handler must verify that the host
 * has been installed and call tryAgain() otherwise.
 * @param {function(remoting.Error):void} onError Callback called when user
 *    clicks Cancel button or there is some other unexpected error.
 * @return {void}
 */
remoting.HostInstallDialog.prototype.show = function(onDone, onError) {
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

  /** @type {function():void} */
  this.onDoneHandler_ = onDone;

  /** @type {function(remoting.Error):void} */
  this.onErrorHandler_ = onError;
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

  this.onDoneHandler_();
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
