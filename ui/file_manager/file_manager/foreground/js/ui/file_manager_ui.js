// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * The root of the file manager's view managing the DOM of Files.app.
 *
 * @param {HTMLElement} element Top level element of Files.app.
 * @param {DialogType} dialogType Dialog type.
 * @constructor.
 */
var FileManagerUI = function(element, dialogType) {
  /**
   * Top level element of Files.app.
   * @type {HTMLElement}
   * @private
   */
  this.element_ = element;

  /**
   * Dialog type.
   * @type {DialogType}
   * @private
   */
  this.dialogType_ = dialogType;

  /**
   * Error dialog.
   * @type {ErrorDialog}
   */
  this.errorDialog = null;

  /**
   * Alert dialog.
   * @type {cr.ui.dialogs.AlertDialog}
   */
  this.alertDialog = null;

  /**
   * Confirm dialog.
   * @type {cr.ui.dialogs.ConfirmDialog}
   */
  this.confirmDialog = null;

  /**
   * Confirm dialog for delete.
   * @type {cr.ui.dialogs.ConfirmDialog}
   */
  this.deleteConfirmDialog = null;

  /**
   * Prompt dialog.
   * @type {cr.ui.dialogs.PromptDialog}
   */
  this.promptDialog = null;

  /**
   * Share dialog.
   * @type {ShareDialog}
   */
  this.shareDialog = null;

  /**
   * Multi-profile share dialog.
   * @type {MultiProfileShareDialog}
   */
  this.multiProfileShareDialog = null;

  /**
   * Default task picker.
   * @type {DefaultActionDialog}
   */
  this.defaultTaskPicker = null;

  /**
   * Suggest apps dialog.
   * @type {SuggestAppsDialog}
   */
  this.suggestAppsDialog = null;

  /**
   * Conflict dialog.
   * @type {ConflictDialog}
   */
  this.conflictDialog = null;

  /**
   * Search box.
   * @type {SearchBox}
   */
  this.searchBox = null;

  /**
   * File type selector in the footer.
   * @type {HTMLElement}
   */
  this.fileTypeSelector = null;

  /**
   * OK button in the footer.
   * @type {HTMLElement}
   */
  this.okButton = null;

  /**
   * Cancel button in the footer.
   * @type {HTMLElement}
   */
  this.cancelButton = null;

  Object.seal(this);

  // Initialize the header.
  this.updateProfileBadge();
  this.element_.querySelector('#app-name').innerText =
      chrome.runtime.getManifest().name;

  // Initialize dialog type.
  this.initDialogType_();

  // Pre-populate the static localized strings.
  i18nTemplate.process(this.element_.ownerDocument, loadTimeData);
};

/**
 * Tweak the UI to become a particular kind of dialog, as determined by the
 * dialog type parameter passed to the constructor.
 *
 * @private
 */
FileManagerUI.prototype.initDialogType_ = function() {
  // Obtain elements.
  var hasFooterPanel =
      this.dialogType_ == DialogType.SELECT_SAVEAS_FILE;

  // If the footer panel exists, the buttons are placed there. Otherwise,
  // the buttons are on the preview panel.
  var parentPanelOfButtons = this.element_.ownerDocument.querySelector(
      !hasFooterPanel ? '.preview-panel' : '.dialog-footer');
  parentPanelOfButtons.classList.add('button-panel');
  this.fileTypeSelector = parentPanelOfButtons.querySelector('.file-type');
  this.okButton = parentPanelOfButtons.querySelector('.ok');
  this.cancelButton = parentPanelOfButtons.querySelector('.cancel');

  // Set attributes.
  var okLabel = str('OPEN_LABEL');

  switch (this.dialogType_) {
    case DialogType.SELECT_UPLOAD_FOLDER:
      okLabel = str('UPLOAD_LABEL');
      break;

    case DialogType.SELECT_SAVEAS_FILE:
      okLabel = str('SAVE_LABEL');
      break;

    case DialogType.SELECT_FOLDER:
    case DialogType.SELECT_OPEN_FILE:
    case DialogType.SELECT_OPEN_MULTI_FILE:
    case DialogType.FULL_PAGE:
      break;

    default:
      throw new Error('Unknown dialog type: ' + this.dialogType);
  }

  this.okButton.textContent = okLabel;
  this.element_.setAttribute('type', this.dialogType_);
};

/**
 * Initialize the dialogs.
 */
FileManagerUI.prototype.initDialogs = function() {
  // Initialize the dialog label.
  var dialogs = cr.ui.dialogs;
  dialogs.BaseDialog.OK_LABEL = str('OK_LABEL');
  dialogs.BaseDialog.CANCEL_LABEL = str('CANCEL_LABEL');
  var appState = window.appState || {};

  // Create the dialog instances.
  this.errorDialog = new ErrorDialog(this.element_);
  this.alertDialog = new dialogs.AlertDialog(this.element_);
  this.confirmDialog = new dialogs.ConfirmDialog(this.element_);
  this.deleteConfirmDialog = new dialogs.ConfirmDialog(this.element_);
  this.deleteConfirmDialog.setOkLabel(str('DELETE_BUTTON_LABEL'));
  this.promptDialog = new dialogs.PromptDialog(this.element_);
  this.shareDialog = new ShareDialog(this.element_);
  this.multiProfileShareDialog = new MultiProfileShareDialog(this.element_);
  this.defaultTaskPicker =
      new cr.filebrowser.DefaultActionDialog(this.element_);
  this.suggestAppsDialog = new SuggestAppsDialog(
      this.element_, appState.suggestAppsDialogState || {});
  this.conflictDialog = new ConflictDialog(this.element_);
};

/**
 * Initialize here elements, which are expensive
 * or hidden in the beginning.
 */
FileManagerUI.prototype.initAdditionalUI = function() {
  this.searchBox = new SearchBox(this.element_.querySelector('#search-box'));
};

/**
 * Updates visibility and image of the profile badge.
 */
FileManagerUI.prototype.updateProfileBadge = function() {
  if (this.dialogType_ !== DialogType.FULL_PAGE)
    return;

  chrome.fileBrowserPrivate.getProfiles(function(profiles,
                                                 currentId,
                                                 displayedId) {
    var profileImage;
    if (currentId !== displayedId) {
      for (var i = 0; i < profiles.length; i++) {
        if (profiles[i].profileId === currentId) {
          profileImage = profiles[i].profileImage;
          break;
        }
      }
    }
    var profileBadge = this.element_.querySelector('#profile-badge');
    if (profileImage) {
      profileBadge.style.background =
          '-webkit-image-set(' +
          'url(' + profileImage.scale1xUrl + ') 1x,' +
          'url(' + profileImage.scale2xUrl + ') 2x) no-repeat center';
      profileBadge.hidden = false;
    } else {
      profileBadge.style.background = '';
      profileBadge.hidden = true;
    }
  }.bind(this));
};
