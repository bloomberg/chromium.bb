// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The root of the file manager's view managing the DOM of Files.app.
 *
 * @param {!HTMLElement} element Top level element of Files.app.
 * @param {DialogType} dialogType Dialog type.
 * @constructor
 * @struct
 */
function FileManagerUI(element, dialogType) {
  /**
   * Top level element of Files.app.
   * @type {!HTMLElement}
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
   * @type {!SearchBox}
   */
  this.searchBox = new SearchBox(
      this.element_.querySelector('#search-box'),
      this.element_.querySelector('#search-button'),
      this.element_.querySelector('#no-search-results'));

  /**
   * Toggle-view button.
   * @type {!Element}
   */
  this.toggleViewButton = queryRequiredElement(this.element_, '#view-button');

  /**
   * List container.
   * @type {ListContainer}
   */
  this.listContainer = null;

  /**
   * @type {PreviewPanel}
   */
  this.previewPanel = null;

  /**
   * Dialog footer.
   * @type {!DialogFooter}
   */
  this.dialogFooter = DialogFooter.findDialogFooter(
      this.dialogType_,
      /** @type {!Document} */ (this.element_.ownerDocument));

  Object.seal(this);

  // Initialize attributes.
  this.element_.querySelector('#app-name').innerText =
      chrome.runtime.getManifest().name;
  this.element_.setAttribute('type', this.dialogType_);

  // Prevent opening an URL by dropping it onto the page.
  this.element_.addEventListener('drop', function(e) {
    e.preventDefault();
  });

  // Pre-populate the static localized strings.
  i18nTemplate.process(this.element_.ownerDocument, loadTimeData);
}

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
 * Initializes here elements, which are expensive or hidden in the beginning.
 *
 * @param {!FileTable} table
 * @param {!FileGrid} grid
 * @param {PreviewPanel} previewPanel
 *
 */
FileManagerUI.prototype.initAdditionalUI = function(table, grid, previewPanel) {
  // Listen to drag events to hide preview panel while user is dragging files.
  // Files.app prevents default actions in 'dragstart' in some situations,
  // so we listen to 'drag' to know the list is actually being dragged.
  var draggingBound = this.onDragging_.bind(this);
  var dragEndBound = this.onDragEnd_.bind(this);
  table.list.addEventListener('drag', draggingBound);
  grid.addEventListener('drag', draggingBound);
  table.list.addEventListener('dragend', dragEndBound);
  grid.addEventListener('dragend', dragEndBound);

  // Listen to dragselection events to hide preview panel while the user is
  // selecting files by drag operation.
  table.list.addEventListener('dragselectionstart', draggingBound);
  grid.addEventListener('dragselectionstart', draggingBound);
  table.list.addEventListener('dragselectionend', dragEndBound);
  grid.addEventListener('dragselectionend', dragEndBound);

  // List container.
  this.listContainer = new ListContainer(
      queryRequiredElement(this.element_, '#list-container'), table, grid);

  // Preview panel.
  this.previewPanel = previewPanel;
  this.previewPanel.addEventListener(
      PreviewPanel.Event.VISIBILITY_CHANGE,
      this.onPreviewPanelVisibilityChange_.bind(this));
  this.previewPanel.initialize();
};

/**
 * Relayout the UI.
 */
FileManagerUI.prototype.relayout = function() {
  this.locationLine.truncate();
  this.listContainer.currentView.relayout();
};

/**
 * Sets the current list type.
 * @param {ListContainer.ListType} listType New list type.
 */
FileManagerUI.prototype.setCurrentListType = function(listType) {
  this.listContainer.setCurrentListType(listType);

  switch (listType) {
    case ListContainer.ListType.DETAIL:
      this.toggleViewButton.classList.add('table');
      this.toggleViewButton.classList.remove('grid');
      break;

    case ListContainer.ListType.THUMBNAIL:
      this.toggleViewButton.classList.add('grid');
      this.toggleViewButton.classList.remove('table');
      break;

    default:
      assertNotReached();
      break;
  }
};

/**
 * Invoked while the drag is being performed on the list or the grid.
 * Note: this method may be called multiple times before onDragEnd_().
 * @private
 */
FileManagerUI.prototype.onDragging_ = function() {
  // On open file dialog, the preview panel is always shown.
  if (DialogType.isOpenDialog(this.dialogType_))
    return;
  this.previewPanel.visibilityType = PreviewPanel.VisibilityType.ALWAYS_HIDDEN;
};

/**
 * Invoked when the drag is ended on the list or the grid.
 * @private
 */
FileManagerUI.prototype.onDragEnd_ = function() {
  // On open file dialog, the preview panel is always shown.
  if (DialogType.isOpenDialog(this.dialogType_))
    return;
  this.previewPanel.visibilityType = PreviewPanel.VisibilityType.AUTO;
};

/**
 * Resize details and thumb views to fit the new window size.
 * @private
 */
FileManagerUI.prototype.onPreviewPanelVisibilityChange_ = function() {
  // This method may be called on initialization. Some object may not be
  // initialized.
  var panelHeight = this.previewPanel.visible ?
      this.previewPanel.height : 0;
  this.listContainer.table.setBottomMarginForPanel(panelHeight);
  this.listContainer.grid.setBottomMarginForPanel(panelHeight);
};
