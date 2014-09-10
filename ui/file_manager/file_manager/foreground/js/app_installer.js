// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Manage the installation of apps.
 *
 * @param {string} itemId Item id to be installed.
 * @constructor
 * @extends {cr.EventType}
 */
function AppInstaller(itemId) {
  this.itemId_ = itemId;
  this.callback_ = null;

  Object.seal(this);
}

AppInstaller.prototype = {
};

/**
 * Type of result.
 *
 * @enum {string}
 * @const
 */
AppInstaller.Result = {
  SUCCESS: 'AppInstaller.success',
  CANCELLED: 'AppInstaller.cancelled',
  ERROR: 'AppInstaller.error'
};
Object.freeze(AppInstaller.Result);

/**
 * Error message for user cancellation. This must be match with the constant
 * 'kUserCancelledError' in C/B/extensions/webstore_standalone_installer.cc.
 * @type {string}
 * @const
 * @private
 */
AppInstaller.USER_CANCELLED_ERROR_STR_ = 'User cancelled install';

/**
 * Start an installation.
 * @param {function(boolean, string)} callback Called when the installation is
 *     finished.
 */
AppInstaller.prototype.install = function(callback) {
  this.callback_ = callback;
  chrome.fileManagerPrivate.installWebstoreItem(
      this.itemId_,
      false,  // Shows installation prompt.
      function() {
        this.onInstallCompleted_(chrome.runtime.lastError);
      }.bind(this));
};

/**
 * Called when the installation is completed.
 *
 * @param {{message: string}?} error Null if the installation is success,
 *     otherwise an object which contains error message.
 * @private
 */
AppInstaller.prototype.onInstallCompleted_ = function(error) {
  var installerResult = AppInstaller.Result.SUCCESS;
  var errorMessage = '';
  if (error) {
    installerResult =
        error.message == AppInstaller.USER_CANCELLED_ERROR_STR_ ?
        AppInstaller.Result.CANCELLED :
        AppInstaller.Result.ERROR;
    errorMessage = error.message;
  }
  this.callback_(installerResult, errorMessage);
  this.callback_ = null;
};
