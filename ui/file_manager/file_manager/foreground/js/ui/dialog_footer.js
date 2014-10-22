// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Footer shown when the Files.app is opened as a file/folder selecting dialog.
 * @param {DialogType} dialogType Dialog type.
 * @param {!Element} container Container of the dialog footer.
 * @constructor
 */
function DialogFooter(dialogType, container) {
  /**
   * Dialog type.
   * @type {DialogType}
   * @const
   * @private
   */
  this.dialogType_ = dialogType;

  /**
   * OK button in the footer.
   * @const
   * @type {!HTMLButtonElement}
   */
  this.okButton = /** @type {!HTMLButtonElement} */
      (container.querySelector('.ok'));

  /**
   * Cancel button in the footer.
   * @const
   * @type {!HTMLButtonElement}
   */
  this.cancelButton = /** @type {!HTMLButtonElement} */
      (container.querySelector('.cancel'));

  /**
   * File type selector in the footer.
   * @const
   * @type {!HTMLSelectElement}
   */
  this.fileTypeSelector = /** @type {!HTMLSelectElement} */
      (container.querySelector('.file-type'));

  // Initialize the element styles.
  container.classList.add('button-panel');
  this.okButton.textContent = DialogFooter.getOKButtonLabel_(dialogType);
}

DialogFooter.prototype = {
  /**
   * @return {number} Selected filter index.
   */
  get selectedFilterIndex() {
    return ~~this.fileTypeSelector.value;
  }
};

/**
 * Finds the dialog footer element for the dialog type.
 * @param {DialogType} dialogType Dialog type.
 * @param {!Document} document Document.
 * @return {!DialogFooter} Dialog footer created with the found element.
 */
DialogFooter.findDialogFooter = function(dialogType, document) {
  // If the footer panel exists, the buttons are placed there. Otherwise,
  // the buttons are on the preview panel.
  var hasFooterPanel = dialogType == DialogType.SELECT_SAVEAS_FILE;
  return new DialogFooter(
      dialogType,
      /** @type {!Element} */
      (document.querySelector(
          hasFooterPanel ? '.dialog-footer' : '.preview-panel')));
};

/**
 * Obtains the label of OK button for the dialog type.
 * @param {DialogType} dialogType Dialog type.
 * @return {string} OK button label.
 * @private
 */
DialogFooter.getOKButtonLabel_ = function(dialogType) {
  switch (dialogType) {
    case DialogType.SELECT_UPLOAD_FOLDER:
      return str('UPLOAD_LABEL');

    case DialogType.SELECT_SAVEAS_FILE:
      return str('SAVE_LABEL');

    case DialogType.SELECT_FOLDER:
    case DialogType.SELECT_OPEN_FILE:
    case DialogType.SELECT_OPEN_MULTI_FILE:
    case DialogType.FULL_PAGE:
      return str('OPEN_LABEL');

    default:
      throw new Error('Unknown dialog type: ' + dialogType);
  }
};
