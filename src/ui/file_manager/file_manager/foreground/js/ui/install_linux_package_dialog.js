// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * InstallLinuxPackageDialog is used as the handler for .deb files.
 * TODO(timloh): Retrieve package info and display it in the dialog.
 */
cr.define('cr.filebrowser', function() {
  /**
   * Creates dialog in DOM tree.
   *
   * @param {HTMLElement} parentNode Node to be parent for this dialog.
   * @constructor
   * @extends {FileManagerDialogBase}
   */
  function InstallLinuxPackageDialog(parentNode) {
    FileManagerDialogBase.call(this, parentNode);

    this.frame_.id = 'install-linux-package-dialog';

    // TODO(timloh): Add a penguin icon

    // The OK button normally dismisses the dialog, so add a button we can
    // customize.
    this.installButton_ = this.okButton_.cloneNode();
    this.installButton_.textContent =
        str('INSTALL_LINUX_PACKAGE_INSTALL_BUTTON');
    this.installButton_.addEventListener(
        'click', this.onInstallClick_.bind(this));
    this.buttons.insertBefore(this.installButton_, this.okButton_);
    this.initialFocusElement_ = this.installButton_;
  }

  InstallLinuxPackageDialog.prototype = {
    __proto__: FileManagerDialogBase.prototype,
  };

  /**
   * Shows the dialog.
   *
   * @param {!Entry} entry
   */
  InstallLinuxPackageDialog.prototype.showInstallLinuxPackageDialog = function(
      entry) {
    // We re-use the same object, so reset any visual state that may be changed.
    this.installButton_.hidden = false;
    this.okButton_.hidden = true;
    this.cancelButton_.hidden = false;

    this.entry_ = entry;

    var title = str('INSTALL_LINUX_PACKAGE_TITLE');
    var message = str('INSTALL_LINUX_PACKAGE_DESCRIPTION');
    var show = FileManagerDialogBase.prototype.showOkCancelDialog.call(
        this, title, message, null, null);

    if (!show) {
      console.error('InstallLinuxPackageDialog can\'t be shown.');
      return;
    }
  };

  /**
   * Starts installing the Linux package.
   */
  InstallLinuxPackageDialog.prototype.onInstallClick_ = function() {
    // Add the event listener first to avoid potential races.
    chrome.fileManagerPrivate.installLinuxPackage(
        this.entry_, this.onInstallLinuxPackage_.bind(this));

    this.installButton_.hidden = true;
    this.cancelButton_.hidden = true;

    this.okButton_.hidden = false;
    this.okButton_.focus();
  };

  /**
   * The callback for installLinuxPackage(). Progress updates and completion
   * for succesfully started installations will be displayed in a notification,
   * rather than the file manager.
   * @param {!chrome.fileManagerPrivate.InstallLinuxPackageResponse} response
   *     Whether the install successfully started or not.
   * @param {string} failure_reason A textual reason for the 'failed' case.
   */
  InstallLinuxPackageDialog.prototype.onInstallLinuxPackage_ = function(
      response, failure_reason) {
    if (response == 'started') {
      this.text_.textContent =
          str('INSTALL_LINUX_PACKAGE_INSTALLATION_STARTED');
      return;
    }

    // Currently we always display a generic error message. Eventually we'll
    // want a different message for the 'install_already_active' case, and to
    // surface the provided failure reason if one is provided.
    this.title_.textContent = str('INSTALL_LINUX_PACKAGE_ERROR_TITLE');
    this.text_.textContent = str('INSTALL_LINUX_PACKAGE_ERROR_DESCRIPTION');
  };

  return {InstallLinuxPackageDialog: InstallLinuxPackageDialog};
});
