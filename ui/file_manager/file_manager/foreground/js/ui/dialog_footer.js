// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Footer shown when the Files.app is opened as a file/folder selecting dialog.
 * @param {DialogType} dialogType Dialog type.
 * @param {!Element} container Container of the dialog footer.
 * @param {!Element} filenameInput Filename input element.
 * @constructor
 */
function DialogFooter(dialogType, container, filenameInput) {
  /**
   * Root element of the footer.
   * @type {!Element}
   * @const
   */
  this.element = container;

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

  /**
   * @const
   * @type {!Element}
   */
  this.filenameInput = filenameInput;

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
      queryRequiredElement(
          document, hasFooterPanel ? '.dialog-footer' : '.preview-panel'),
      queryRequiredElement(document, '#filename-input-box input'));
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

/**
 * Fills the file type list or hides it.
 * @param {!Array.<{extensions: Array.<string>, description: string}>} fileTypes
 *     List of file type.
 * @param {boolean} includeAllFiles Whether the filter includes the 'all files'
 *     item or not.
 */
DialogFooter.prototype.initFileTypeFilter = function(
    fileTypes, includeAllFiles) {
  if (includeAllFiles) {
    var option = document.createElement('option');
    option.innerText = str('ALL_FILES_FILTER');
    option.value = 0;
    this.fileTypeSelector.appendChild(option);
  }

  for (var i = 0; i < fileTypes.length; i++) {
    var fileType = fileTypes[i];
    var option = document.createElement('option');
    var description = fileType.description;
    if (!description) {
      // See if all the extensions in the group have the same description.
      for (var j = 0; j !== fileType.extensions.length; j++) {
        var currentDescription = FileType.typeToString(
            FileType.getTypeForName('.' + fileType.extensions[j]));
        if (!description)  {
          // Set the first time.
          description = currentDescription;
        } else if (description != currentDescription) {
          // No single description, fall through to the extension list.
          description = null;
          break;
        }
      }

      if (!description) {
        // Convert ['jpg', 'png'] to '*.jpg, *.png'.
        description = fileType.extensions.map(function(s) {
          return '*.' + s;
        }).join(', ');
      }
    }
    option.innerText = description;
    option.value = i + 1;

    if (fileType.selected)
      option.selected = true;

    this.fileTypeSelector.appendChild(option);
  }

  var options = this.fileTypeSelector.querySelectorAll('option');
  if (options.length >= 2) {
    // There is in fact no choice, show the selector.
    this.fileTypeSelector.hidden = false;
  }
};
