// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * This class is an extended class, to manage the status of the dialogs.
 *
 * @param {HTMLElement} parentNode Parent node of the dialog.
 * @extends {cr.ui.dialogs.BaseDialog}
 * @constructor
 */
var FileManagerDialogBase = function(parentNode) {
  cr.ui.dialogs.BaseDialog.call(this, parentNode);
};

FileManagerDialogBase.prototype = {
  __proto__: cr.ui.dialogs.BaseDialog.prototype
};

/**
 * The FileManager object. This is used to notify events of showing or hiding
 * dialog to file manager.
 *
 * @type {FileManager}
 * @private
 */
FileManagerDialogBase.fileManager_ = null;

/**
 * Setter of FileManagerDialogBase.fileManager_.
 * @param {FileManager} fileManager The fileManager object.
 */
FileManagerDialogBase.setFileManager = function(fileManager) {
  FileManagerDialogBase.fileManager_ = fileManager;
};

/**
 * The flag if any dialog is shown. True if a dialog is visible, false
 *     otherwise.
 * @type {boolean}
 */
FileManagerDialogBase.shown = false;

/**
 * @param {string} title Title.
 * @param {string} message Message.
 * @param {function()} onOk Called when the OK button is pressed.
 * @param {function()} onCancel Called when the cancel button is pressed.
 * @return {boolean} True if the dialog can show successfully. False if the
 *     dialog failed to show due to an existing dialog.
 */
FileManagerDialogBase.prototype.showOkCancelDialog = function(
    title, message, onOk, onCancel) {
  return this.showImpl_(title, message, onOk, onCancel);
};

/**
 * @param {string} title Title.
 * @param {string} message Message.
 * @param {function()} onOk Called when the OK button is pressed.
 * @param {function()} onCancel Called when the cancel button is pressed.
 * @return {boolean} True if the dialog can show successfully. False if the
 *     dialog failed to show due to an existing dialog.
 * @private
 */
FileManagerDialogBase.prototype.showImpl_ = function(
    title, message, onOk, onCancel) {
  if (FileManagerDialogBase.shown)
    return false;

  FileManagerDialogBase.shown = true;
  if (FileManagerDialogBase.fileManager_)
    FileManagerDialogBase.fileManager_.onDialogShownOrHidden(true);
  cr.ui.dialogs.BaseDialog.prototype.showWithTitle.call(
      this, title, message, onOk, onCancel, null);

  return true;
};

/**
 * @return {boolean} True if the dialog can show successfully. False if the
 *     dialog failed to show due to an existing dialog.
 */
FileManagerDialogBase.prototype.showBlankDialog = function() {
  return this.showImpl_('', '', null, null, null);
};

/**
 * @param {string} title Title.
 * @return {boolean} True if the dialog can show successfully. False if the
 *     dialog failed to show due to an existing dialog.
 */
FileManagerDialogBase.prototype.showTitleOnlyDialog = function(title) {
  return this.showImpl_(title, '', null, null, null);
};

/**
 * @param {string} title Title.
 * @param {string} text Text to be shown in the dialog.
 * @return {boolean} True if the dialog can show successfully. False if the
 *     dialog failed to show due to an existing dialog.
 */
FileManagerDialogBase.prototype.showTitleAndTextDialog = function(title, text) {
  this.buttons.style.display = 'none';
  return this.showImpl_(title, text, null, null, null);
};

/**
 * @param {function()=} opt_onHide Called when the dialog is hidden.
 */
FileManagerDialogBase.prototype.hide = function(opt_onHide) {
  cr.ui.dialogs.BaseDialog.prototype.hide.call(
      this,
      function() {
        if (opt_onHide)
          opt_onHide();
        if (FileManagerDialogBase.fileManager_)
          FileManagerDialogBase.fileManager_.onDialogShownOrHidden(false);
        FileManagerDialogBase.shown = false;
      });
};
