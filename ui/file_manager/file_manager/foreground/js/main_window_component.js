// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Component for the main window.
 *
 * The class receives UI events from UI components that does not have their own
 * controller, and do corresponding action by using models/other controllers.
 *
 * The class also observes model/browser API's event to update the misc
 * components.
 *
 * @param {DialogType} dialogType
 * @param {!FileManagerUI} ui
 * @param {!VolumeManagerWrapper} volumeManager
 * @param {!DirectoryModel} directoryModel
 * @param {!FileFilter} fileFilter
 * @param {!FileSelectionHandler} selectionHandler
 * @param {!NamingController} namingController
 * @param {!AppStateController} appStateController
 * @param {!TaskController} taskController
 * @constructor
 * @struct
 */
function MainWindowComponent(
    dialogType, ui, volumeManager, directoryModel, fileFilter, selectionHandler,
    namingController, appStateController, taskController) {
  /**
   * @type {DialogType}
   * @const
   * @private
   */
  this.dialogType_ = dialogType;

  /**
   * @type {!FileManagerUI}
   * @const
   * @private
   */
  this.ui_ = ui;

  /**
   * @type {!VolumeManagerWrapper}
   * @const
   * @private
   */
  this.volumeManager_ = volumeManager;

  /**
   * @type {!DirectoryModel}
   * @const
   * @private
   */
  this.directoryModel_ = directoryModel;

  /**
   * @type {!FileFilter}
   * @const
   * @private
   */
  this.fileFilter_ = fileFilter;

  /**
   * @type {!FileSelectionHandler}
   * @const
   * @private
   */
  this.selectionHandler_ = selectionHandler;

  /**
   * @type {!NamingController}
   * @const
   * @private
   */
  this.namingController_ = namingController;

  /**
   * @type {!AppStateController}
   * @const
   * @private
   */
  this.appStateController_ = appStateController;

  /**
   * @type {!TaskController}
   * @const
   * @private
   */
  this.taskController_ = taskController;

  /**
   * True while a user is pressing <Tab>.
   * This is used for identifying the trigger causing the filelist to
   * be focused.
   * @type {boolean}
   * @private
   */
  this.pressingTab_ = false;

  /**
   * The last clicked item in the file list.
   * @type {HTMLLIElement}
   * @private
   */
  this.lastClickedItem_ = null;

  // Register events.
  ui.listContainer.element.addEventListener(
      'keydown', this.onListKeyDown_.bind(this));
  ui.directoryTree.addEventListener(
      'keydown', this.onDirectoryTreeKeyDown_.bind(this));
  ui.listContainer.element.addEventListener(
      ListContainer.EventType.TEXT_SEARCH, this.onTextSearch_.bind(this));
  ui.listContainer.table.list.addEventListener(
      'click', this.onDetailClick_.bind(this));
  ui.listContainer.grid.addEventListener(
      'click', this.onDetailClick_.bind(this));
  ui.listContainer.table.list.addEventListener(
      'focus', this.onFileListFocus_.bind(this));
  ui.listContainer.grid.addEventListener(
      'focus', this.onFileListFocus_.bind(this));
  ui.locationLine.addEventListener(
      'pathclick', this.onBreadcrumbClick_.bind(this));
  ui.toggleViewButton.addEventListener(
      'click', this.onToggleViewButtonClick_.bind(this));
  ui.detailsButton.addEventListener(
      'click', this.onDetailsButtonClick_.bind(this));
  directoryModel.addEventListener(
      'directory-changed', this.onDirectoryChanged_.bind(this));
  volumeManager.addEventListener(
      'drive-connection-changed',
      this.onDriveConnectionChanged_.bind(this));
  this.onDriveConnectionChanged_();
  document.addEventListener('keydown', this.onKeyDown_.bind(this));
  document.addEventListener('keyup', this.onKeyUp_.bind(this));
  selectionHandler.addEventListener('change',
      this.onFileSelectionChanged_.bind(this));
}

/**
 * @param {Event} event Click event.
 * @private
 */
MainWindowComponent.prototype.onBreadcrumbClick_ = function(event) {
  this.directoryModel_.changeDirectoryEntry(event.entry);
};

/**
 * File list focus handler. Used to select the top most element on the list
 * if nothing was selected.
 *
 * @private
 */
MainWindowComponent.prototype.onFileListFocus_ = function() {
  // If the file list is focused by <Tab>, select the first item if no item
  // is selected.
  if (this.pressingTab_) {
    var selection = this.selectionHandler_.selection;
    if (selection && selection.totalCount == 0)
      this.directoryModel_.selectIndex(0);
  }
};

/**
 * Handles file selection event.
 *
 * @private
 */
MainWindowComponent.prototype.onFileSelectionChanged_ = function(event) {
  if (this.ui_.detailsContainer) {
    this.ui_.detailsContainer.onFileSelectionChanged(event);
  }
};

/**
 * Handles mouse click or tap.
 *
 * @param {Event} event The click event.
 * @private
 */
MainWindowComponent.prototype.onDetailClick_ = function(event) {
  if (this.namingController_.isRenamingInProgress()) {
    // Don't pay attention to clicks during a rename.
    return;
  }

  var listItem = this.ui_.listContainer.findListItemForNode(
      event.touchedElement || event.srcElement);
  var selection = this.selectionHandler_.selection;
  if (!listItem || !listItem.selected || selection.totalCount != 1) {
    return;
  }

  // React on double click, but only if both clicks hit the same item.
  // TODO(mtomasz): Simplify it, and use a double click handler if possible.
  var clickNumber = (this.lastClickedItem_ == listItem) ? 2 : undefined;
  this.lastClickedItem_ = listItem;

  if (event.detail != clickNumber)
    return;

  var entry = selection.entries[0];
  if (entry.isDirectory) {
    this.directoryModel_.changeDirectoryEntry(
        /** @type {!DirectoryEntry} */ (entry));
  } else {
    this.acceptSelection_();
  }
};

/**
 * Accepts the current selection depending on the mode.
 * @private
 */
MainWindowComponent.prototype.acceptSelection_ = function() {
  var selection = this.selectionHandler_.selection;
  if (this.dialogType_ == DialogType.FULL_PAGE) {
    this.taskController_.getFileTasks()
        .then(function(tasks) {
          tasks.executeDefault();
        })
        .catch(function(error) {
          if (error)
            console.error(error.stack || error);
        });
    return true;
  }
  if (!this.ui_.dialogFooter.okButton.disabled) {
    this.ui_.dialogFooter.okButton.click();
    return true;
  }
  return false;
}

/**
 * Handles click event on the toggle-view button.
 * @param {Event} event Click event.
 * @private
 */
MainWindowComponent.prototype.onToggleViewButtonClick_ = function(event) {
  var listType =
      this.ui_.listContainer.currentListType === ListContainer.ListType.DETAIL ?
      ListContainer.ListType.THUMBNAIL :
      ListContainer.ListType.DETAIL;

  this.ui_.setCurrentListType(listType);
  this.appStateController_.saveViewOptions();

  this.ui_.listContainer.focus();
};

/**
 * Handles click event on the toggle-view button.
 * @param {Event} event Click event.
 * @private
 */
MainWindowComponent.prototype.onDetailsButtonClick_ = function(event) {
  var visible = this.ui_.detailsContainer.visible;
  this.ui_.setDetailsVisibility(!visible);
  this.appStateController_.saveViewOptions();
  this.ui_.listContainer.focus();
};

/**
 * KeyDown event handler for the document.
 * @param {Event} event Key event.
 * @private
 */
MainWindowComponent.prototype.onKeyDown_ = function(event) {
  if (event.keyCode === 9)  // Tab
    this.pressingTab_ = true;

  if (event.srcElement === this.ui_.listContainer.renameInput) {
    // Ignore keydown handler in the rename input box.
    return;
  }

  switch (util.getKeyModifiers(event) + event.key) {
    case 'Escape':  // Escape => Cancel dialog.
    case 'Ctrl-w':  // Ctrl+W => Cancel dialog.
      if (this.dialogType_ != DialogType.FULL_PAGE) {
        // If there is nothing else for ESC to do, then cancel the dialog.
        event.preventDefault();
        this.ui_.dialogFooter.cancelButton.click();
      }
      break;
  }
};

/**
 * KeyUp event handler for the document.
 * @param {Event} event Key event.
 * @private
 */
MainWindowComponent.prototype.onKeyUp_ = function(event) {
  if (event.keyCode === 9)  // Tab
    this.pressingTab_ = false;
};

/**
 * KeyDown event handler for the directory tree element.
 * @param {Event} event Key event.
 * @private
 */
MainWindowComponent.prototype.onDirectoryTreeKeyDown_ = function(event) {
  // Enter => Change directory or perform default action.
  if (util.getKeyModifiers(event) + event.key === 'Enter') {
    var selectedItem = this.ui_.directoryTree.selectedItem;
    if (!selectedItem)
      return;
    selectedItem.activate();
    if (this.dialogType_ !== DialogType.FULL_PAGE &&
        !selectedItem.hasAttribute('renaming') &&
        util.isSameEntry(
            this.directoryModel_.getCurrentDirEntry(), selectedItem.entry) &&
        !this.ui_.dialogFooter.okButton.disabled) {
      this.ui_.dialogFooter.okButton.click();
    }
  }
};

/**
 * KeyDown event handler for the div#list-container element.
 * @param {Event} event Key event.
 * @private
 */
MainWindowComponent.prototype.onListKeyDown_ = function(event) {
  switch (util.getKeyModifiers(event) + event.key) {
    case 'Backspace':  // Backspace => Up one directory.
      event.preventDefault();
      // TODO(mtomasz): Use Entry.getParent() instead.
      var currentEntry = this.directoryModel_.getCurrentDirEntry();
      if (!currentEntry)
        break;
      var locationInfo = this.volumeManager_.getLocationInfo(currentEntry);
      // TODO(mtomasz): There may be a tiny race in here.
      if (locationInfo && !locationInfo.isRootEntry &&
          !locationInfo.isSpecialSearchRoot) {
        currentEntry.getParent(function(parentEntry) {
          this.directoryModel_.changeDirectoryEntry(parentEntry);
        }.bind(this), function() { /* Ignore errors. */});
      }
      break;

    case 'Enter':  // Enter => Change directory or perform default action.
      var selection = this.selectionHandler_.selection;
      if (selection.totalCount === 1 &&
          selection.entries[0].isDirectory &&
          !DialogType.isFolderDialog(this.dialogType_)) {
        var item = this.ui_.listContainer.currentList.getListItemByIndex(
            selection.indexes[0]);
        // If the item is in renaming process, we don't allow to change
        // directory.
        if (item && !item.hasAttribute('renaming')) {
          event.preventDefault();
          this.directoryModel_.changeDirectoryEntry(
              /** @type {!DirectoryEntry} */ (selection.entries[0]));
        }
      } else if (this.acceptSelection_()) {
        event.preventDefault();
      }
      break;
  }
};

/**
 * Performs a 'text search' - selects a first list entry with name
 * starting with entered text (case-insensitive).
 * @private
 */
MainWindowComponent.prototype.onTextSearch_ = function() {
  var text = this.ui_.listContainer.textSearchState.text;
  var dm = this.directoryModel_.getFileList();
  for (var index = 0; index < dm.length; ++index) {
    var name = dm.item(index).name;
    if (name.substring(0, text.length).toLowerCase() == text) {
      this.ui_.listContainer.currentList.selectionModel.selectedIndexes =
          [index];
      return;
    }
  }

  this.ui_.listContainer.textSearchState.text = '';
};

/**
 * Update the UI when the current directory changes.
 *
 * @param {Event} event The directory-changed event.
 * @private
 */
MainWindowComponent.prototype.onDirectoryChanged_ = function(event) {
  event = /** @type {DirectoryChangeEvent} */ (event);

  var newVolumeInfo = event.newDirEntry ?
      this.volumeManager_.getVolumeInfo(event.newDirEntry) : null;

  // Update unformatted volume status.
  if (newVolumeInfo && newVolumeInfo.error) {
    this.ui_.element.setAttribute('unformatted', '');

    if (newVolumeInfo.error ===
        VolumeManagerCommon.VolumeError.UNSUPPORTED_FILESYSTEM) {
      this.ui_.formatPanelError.textContent =
          str('UNSUPPORTED_FILESYSTEM_WARNING');
    } else {
      this.ui_.formatPanelError.textContent = str('UNKNOWN_FILESYSTEM_WARNING');
    }
  } else {
    this.ui_.element.removeAttribute('unformatted');
  }

  if (event.newDirEntry) {
    this.ui_.locationLine.show(event.newDirEntry);
    // Updates UI.
    if (this.dialogType_ === DialogType.FULL_PAGE) {
      var locationInfo = this.volumeManager_.getLocationInfo(event.newDirEntry);
      if (locationInfo) {
        document.title = locationInfo.isRootEntry
            ? util.getRootTypeLabel(locationInfo) : event.newDirEntry.name;
      } else {
        console.error('Could not find location info for entry: '
                      + event.newDirEntry.fullPath);
      }
    }
  } else {
    this.ui_.locationLine.hide();
  }
};

/**
 * @private
 */
MainWindowComponent.prototype.onDriveConnectionChanged_ = function() {
  var connection = this.volumeManager_.getDriveConnectionState();
  this.ui_.dialogContainer.setAttribute('connection', connection.type);
  this.ui_.shareDialog.hideWithResult(ShareDialog.Result.NETWORK_ERROR);
  this.ui_.suggestAppsDialog.onDriveConnectionChanged(connection.type);
};
