// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Manages actions for the current selection.
 */
class ActionsController {
  /**
   * @param {!VolumeManager} volumeManager
   * @param {!MetadataModel} metadataModel
   * @param {!DirectoryModel} directoryModel
   * @param {!FolderShortcutsDataModel} shortcutsModel
   * @param {!DriveSyncHandler} driveSyncHandler
   * @param {!FileSelectionHandler} selectionHandler
   * @param {!FileManagerUI} ui
   */
  constructor(
      volumeManager, metadataModel, directoryModel, shortcutsModel,
      driveSyncHandler, selectionHandler, ui) {
    /**
     * @private
     * @const
     */
    this.volumeManager_ = volumeManager;

    /**
     * @private
     * @const
     */
    this.metadataModel_ = metadataModel;

    /**
     * @private
     * @const
     */
    this.directoryModel_ = directoryModel;

    /**
     * @private
     * @const
     */
    this.shortcutsModel_ = shortcutsModel;

    /**
     * @private
     * @const
     */
    this.driveSyncHandler_ = driveSyncHandler;

    /**
     * @private
     * @const
     */
    this.selectionHandler_ = selectionHandler;

    /**
     * @private
     * @const
     */
    this.ui_ = ui;

    /**
     * @private {ActionsModel}
     */
    this.fileListActionsModel_ = null;

    /**
     * @private {ActionsModel}
     */
    this.navigationListActionsModel_ = null;

    /**
     * @private {number}
     */
    this.navigationListSequence_ = 0;

    /**
     * @private {ActionsController.Context}
     */
    this.menuContext_ = ActionsController.Context.UNKNOWN;

    this.ui_.directoryTree.addEventListener(
        'change', this.onNavigationListSelectionChanged_.bind(this), true);
    this.selectionHandler_.addEventListener(
        FileSelectionHandler.EventType.CHANGE,
        this.onSelectionChanged_.bind(this));
    this.selectionHandler_.addEventListener(
        FileSelectionHandler.EventType.CHANGE_THROTTLED,
        this.onSelectionChangeThrottled_.bind(this));
    cr.ui.contextMenuHandler.addEventListener(
        'show', this.onContextMenuShow_.bind(this));
  }

  /**
   * @param {Element} element
   * @return {ActionsController.Context}
   * @private
   */
  getContextFor_(element) {
    // Element can be null, eg. when invoking a command via a keyboard shortcut.
    // By default, all actions refer to the file list, so return FILE_LIST.
    if (element === null) {
      return ActionsController.Context.FILE_LIST;
    }

    if (this.ui_.listContainer.element.contains(element) ||
        this.ui_.toolbar.contains(element)) {
      return ActionsController.Context.FILE_LIST;
    } else if (this.ui_.directoryTree.contains(element)) {
      return ActionsController.Context.NAVIGATION_LIST;
    } else {
      return ActionsController.Context.UNKNOWN;
    }
  }

  /**
   * @private
   */
  updateUI_() {
    const actionsModel = this.getActionsModelForContext(this.menuContext_);
    // TODO(mtomasz): Prevent flickering somehow.
    this.ui_.actionsSubmenu.setActionsModel(actionsModel);
  }

  /**
   * @param {!Event} event
   * @private
   */
  onContextMenuShow_(event) {
    this.menuContext_ = this.getContextFor_(event.element);
    this.updateUI_();
  }

  /**
   * @private
   */
  onSelectionChanged_() {
    if (this.fileListActionsModel_) {
      this.fileListActionsModel_.destroy();
      this.fileListActionsModel_ = null;
    }
    this.updateUI_();
  }

  /**
   * @private
   */
  onSelectionChangeThrottled_() {
    assert(!this.fileListActionsModel_);
    const selection = this.selectionHandler_.selection;

    const entries = selection.entries;
    if (!entries) {
      return;
    }

    const actionsModel = new ActionsModel(
        this.volumeManager_, this.metadataModel_, this.shortcutsModel_,
        this.driveSyncHandler_, this.ui_, entries);

    const initializeAndUpdateUI =
        /** @type {function(Event=)} */ (opt_event => {
          if (selection !== this.selectionHandler_.selection) {
            return;
          }
          actionsModel.initialize().then(() => {
            if (selection !== this.selectionHandler_.selection) {
              return;
            }
            this.fileListActionsModel_ = actionsModel;
            // Before updating the UI we need to ensure that this.menuContext_
            // has a reasonable value or nothing will happen. We will save and
            // restore the existing value.
            const oldMenuContext = this.menuContext_;
            if (this.menuContext_ === ActionsController.Context.UNKNOWN) {
              // FILE_LIST should be a reasonable default.
              this.menuContext_ = ActionsController.Context.FILE_LIST;
            }
            this.updateUI_();
            this.menuContext_ = oldMenuContext;
          });
        });

    actionsModel.addEventListener('invalidated', initializeAndUpdateUI);
    initializeAndUpdateUI();
  }

  /**
   * @private
   */
  onNavigationListSelectionChanged_() {
    if (this.navigationListActionsModel_) {
      this.navigationListActionsModel_.destroy();
      this.navigationListActionsModel_ = null;
    }
    this.updateUI_();

    const entry = this.ui_.directoryTree.selectedItem ?
        (this.ui_.directoryTree.selectedItem.entry || null) :
        null;
    if (!entry) {
      return;
    }

    const sequence = ++this.navigationListSequence_;
    const actionsModel = new ActionsModel(
        this.volumeManager_, this.metadataModel_, this.shortcutsModel_,
        this.driveSyncHandler_, this.ui_, [entry]);

    const initializeAndUpdateUI =
        /** @type {function(Event=)} */ (opt_event => {
          actionsModel.initialize().then(() => {
            if (this.navigationListSequence_ !== sequence) {
              return;
            }
            this.navigationListActionsModel_ = actionsModel;
            this.updateUI_();
          });
        });

    actionsModel.addEventListener('invalidated', initializeAndUpdateUI);
    initializeAndUpdateUI();
  }

  /**
   * @param {!ActionsController.Context} context
   * @return {ActionsModel} Actions model.
   */
  getActionsModelForContext(context) {
    switch (context) {
      case ActionsController.Context.FILE_LIST:
        return this.fileListActionsModel_;

      case ActionsController.Context.NAVIGATION_LIST:
        return this.navigationListActionsModel_;

      default:
        return null;
    }
  }

  /**
   * @param {EventTarget} target
   * @return {ActionsModel} Actions model.
   */
  getActionsModelFor(target) {
    const element = /** @type {Element} */ (target);
    return this.getActionsModelForContext(this.getContextFor_(element));
  }
}

/**
 * @enum {string}
 */
ActionsController.Context = {
  FILE_LIST: 'file-list',
  NAVIGATION_LIST: 'navigation-list',
  UNKNOWN: 'unknown'
};
