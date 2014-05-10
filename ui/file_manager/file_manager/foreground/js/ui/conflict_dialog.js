// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Dialog to confirm the operation for conflicted file operations.
 *
 * @param {HTMLElement} parentNode Node to be parent for this dialog.
 * @constructor
 * @extends {FileManagerDialogBase}
 */
function ConflictDialog(parentNode) {
  FileManagerDialogBase.call(this, parentNode);

  /**
   * Callback to be called when the showing task is completed. The first
   * argument is which button is pressed. The second argument is whether to
   * apply all or not.
   *
   * @type {function(ConflictDialog.Result, boolean)}
   * @private
   */
  this.callback_ = null;

  /**
   * Checkbox to specify whether to apply the selection to all entries or not.
   * @type {HTMLElement}
   * @private
   */
  this.applyAllCheckbox_ = parentNode.ownerDocument.createElement('input');
  this.applyAllCheckbox_.id = 'conflict-confirm-dialog-apply-all-checkbox';
  this.applyAllCheckbox_.type = 'checkbox';

  // Apply all line.
  var applyAllLabel = parentNode.ownerDocument.createElement('label');
  applyAllLabel.textContent = str('CONFLICT_DIALOG_APPLY_TO_ALL');
  applyAllLabel.setAttribute('for', this.applyAllCheckbox_.id);

  /**
   * Element of the keep both button.
   * @type {HTMLElement}
   * @private
   */
  this.keepBothButton_ = parentNode.ownerDocument.createElement('button');
  this.keepBothButton_.textContent = str('CONFLICT_DIALOG_KEEP_BOTH');
  this.keepBothButton_.addEventListener(
      'click',
      this.hideWithResult_.bind(this, ConflictDialog.Result.KEEP_BOTH));

  /**
   * Element of the replace button.
   * @type {HTMLElement}
   * @private
   */
  this.replaceButton_ = parentNode.ownerDocument.createElement('button');
  this.replaceButton_.textContent = str('CONFLICT_DIALOG_REPLACE');
  this.replaceButton_.addEventListener(
      'click',
      this.hideWithResult_.bind(this, ConflictDialog.Result.REPLACE));

  // Buttons line.
  var buttons = this.okButton_.parentNode;
  buttons.insertBefore(this.applyAllCheckbox_, this.okButton_);
  buttons.insertBefore(applyAllLabel, this.okButton_);
  buttons.replaceChild(this.keepBothButton_, this.okButton_);
  buttons.appendChild(this.replaceButton_);

  // Frame
  this.frame_.id = 'conflict-confirm-dialog';
}

/**
 * Result of conflict confirm dialogs.
 * @enum {string}
 * @const
 */
ConflictDialog.Result = Object.freeze({
  KEEP_BOTH: 'keepBoth',
  CANCEL: 'cancel',
  REPLACE: 'replace'
});

ConflictDialog.prototype = {
  __proto__: FileManagerDialogBase.prototype
};

/**
 * Shows the conflict confirm dialog.
 *
 * @param {string} fileName Filename that is conflicted.
 * @param {function(ConflictDialog.Result, boolean)} callback Complete
 *     callback. See also ConflictDialog#callback_.
 * @return {boolean} True if the dialog can show successfully. False if the
 *     dialog failed to show due to an existing dialog.
 */
ConflictDialog.prototype.show = function(fileName, callback) {
  if (this.callback_)
    return false;

  this.callback_ = callback;
  FileManagerDialogBase.prototype.showOkCancelDialog.call(
      this,
      '', // We dont't show the title for the dialog.
      strf('CONFLICT_DIALOG_MESSAGE', fileName));
  return true;
};

/**
 * Handles cancellation.
 * @param {Event} event Click event.
 * @private
 */
ConflictDialog.prototype.onCancelClick_ = function(event) {
  this.hideWithResult_(ConflictDialog.Result.CANCEL);
};

/**
 * Hides the dialog box with the result.
 * @param {ConflictDialog.Result} result Result.
 * @private
 */
ConflictDialog.prototype.hideWithResult_ = function(result) {
  this.hide(function() {
    if (!this.callback_)
      return;
    this.callback_(result, this.applyAllCheckbox_.checked);
    this.callback_ = null;
    this.applyAllCheckbox_.checked = false;
  }.bind(this));
};
