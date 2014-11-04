// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The root of the file manager's view managing the DOM of Files.app.
 *
 * @param {HTMLElement} element Top level element of Files.app.
 * @param {DialogType} dialogType Dialog type.
 * @constructor
 * @struct
 */
function FileManagerUI(element, dialogType) {
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
   * @type {cr.filebrowser.DefaultActionDialog}
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
   * Location line.
   * @type {LocationLine}
   */
  this.locationLine = null;

  /**
   * Search box.
   * @type {SearchBox}
   */
  this.searchBox = null;

  /**
   * Toggle-view button.
   * @type {Element}
   */
  this.toggleViewButton = null;

  /**
   * List container.
   * @type {ListContainer}
   */
  this.listContainer = null;

  /**
   * Dialog footer.
   * @type {DialogFooter}
   */
  this.dialogFooter = null;

  Object.seal(this);

  // Initialize the header.
  this.element_.querySelector('#app-name').innerText =
      chrome.runtime.getManifest().name;

  // Initialize dialog type.
  this.initDialogType_();

  // Pre-populate the static localized strings.
  i18nTemplate.process(this.element_.ownerDocument, loadTimeData);
}

/**
 * Tweak the UI to become a particular kind of dialog, as determined by the
 * dialog type parameter passed to the constructor.
 *
 * @private
 */
FileManagerUI.prototype.initDialogType_ = function() {
  // Obtain elements.
  this.dialogFooter = DialogFooter.findDialogFooter(
      this.dialogType_, /** @type {!Document} */(this.element_.ownerDocument));

  // Set attributes.
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
 * Initializes here elements, which are expensive
 * or hidden in the beginning.
 */
FileManagerUI.prototype.initAdditionalUI = function() {
  this.searchBox = new SearchBox(
      this.element_.querySelector('#search-box'),
      this.element_.querySelector('#search-button'),
      this.element_.querySelector('#no-search-results'));

  this.toggleViewButton = this.element_.querySelector('#view-button');
};
