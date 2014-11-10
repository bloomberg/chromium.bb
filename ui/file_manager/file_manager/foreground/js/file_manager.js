// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * FileManager constructor.
 *
 * FileManager objects encapsulate the functionality of the file selector
 * dialogs, as well as the full screen file manager application (though the
 * latter is not yet implemented).
 *
 * @constructor
 * @struct
 */
function FileManager() {
  // --------------------------------------------------------------------------
  // Services FileManager depends on.

  /**
   * Volume manager.
   * @type {VolumeManagerWrapper}
   * @private
   */
  this.volumeManager_ = null;

  /**
   * Metadata cache.
   * @type {MetadataCache}
   * @private
   */
  this.metadataCache_ = null;

  /**
   * File operation manager.
   * @type {FileOperationManager}
   * @private
   */
  this.fileOperationManager_ = null;

  /**
   * File transfer controller.
   * @type {FileTransferController}
   * @private
   */
  this.fileTransferController_ = null;

  /**
   * File filter.
   * @type {FileFilter}
   * @private
   */
  this.fileFilter_ = null;

  /**
   * File watcher.
   * @type {FileWatcher}
   * @private
   */
  this.fileWatcher_ = null;

  /**
   * Model of current directory.
   * @type {DirectoryModel}
   * @private
   */
  this.directoryModel_ = null;

  /**
   * Model of folder shortcuts.
   * @type {FolderShortcutsDataModel}
   * @private
   */
  this.folderShortcutsModel_ = null;

  /**
   * VolumeInfo of the current volume.
   * @type {VolumeInfo}
   * @private
   */
  this.currentVolumeInfo_ = null;

  /**
   * Handler for command events.
   * @type {CommandHandler}
   */
  this.commandHandler = null;

  /**
   * Handler for the change of file selection.
   * @type {FileSelectionHandler}
   * @private
   */
  this.selectionHandler_ = null;

  /**
   * Dialog action controller.
   * @type {DialogActionController}
   * @private
   */
  this.dialogActionController_ = null;

  // --------------------------------------------------------------------------
  // Parameters determining the type of file manager.

  /**
   * Dialog type of this window.
   * @type {DialogType}
   */
  this.dialogType = DialogType.FULL_PAGE;

  /**
   * List of acceptable file types for open dialog.
   * @type {!Array.<Object>}
   * @private
   */
  this.fileTypes_ = [];

  /**
   * Startup parameters for this application.
   * @type {?{includeAllFiles:boolean,
   *          action:string,
   *          shouldReturnLocalPath:boolean}}
   * @private
   */
  this.params_ = null;

  /**
   * Startup preference about the view.
   * @type {Object}
   * @private
   */
  this.viewOptions_ = {};

  /**
   * The user preference.
   * @type {Object}
   * @private
   */
  this.preferences_ = null;

  // --------------------------------------------------------------------------
  // UI components.

  /**
   * UI management class of file manager.
   * @type {FileManagerUI}
   * @private
   */
  this.ui_ = null;

  /**
   * Progress center panel.
   * @type {ProgressCenterPanel}
   * @private
   */
  this.progressCenterPanel_ = null;

  /**
   * Directory tree.
   * @type {DirectoryTree}
   * @private
   */
  this.directoryTree_ = null;

  /**
   * Naming controller.
   * @type {NamingController}
   * @private
   */
  this.namingController_ = null;

  /**
   * Controller for search UI.
   * @type {SearchController}
   * @private
   */
  this.searchController_ = null;

  /**
   * Controller for directory scan.
   * @type {ScanController}
   * @private
   */
  this.scanController_ = null;

  /**
   * Controller for spinner.
   * @type {SpinnerController}
   * @private
   */
  this.spinnerController_ = null;

  /**
   * Banners in the file list.
   * @type {FileListBannerController}
   * @private
   */
  this.bannersController_ = null;

  // --------------------------------------------------------------------------
  // Dialogs.

  /**
   * Error dialog.
   * @type {ErrorDialog}
   */
  this.error = null;

  /**
   * Alert dialog.
   * @type {cr.ui.dialogs.AlertDialog}
   */
  this.alert = null;

  /**
   * Confirm dialog.
   * @type {cr.ui.dialogs.ConfirmDialog}
   */
  this.confirm = null;

  /**
   * Prompt dialog.
   * @type {cr.ui.dialogs.PromptDialog}
   */
  this.prompt = null;

  /**
   * Share dialog.
   * @type {ShareDialog}
   * @private
   */
  this.shareDialog_ = null;

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

  // --------------------------------------------------------------------------
  // Menus.

  /**
   * Context menu for texts.
   * @type {cr.ui.Menu}
   * @private
   */
  this.textContextMenu_ = null;

  // --------------------------------------------------------------------------
  // DOM elements.

  /**
   * Background page.
   * @type {BackgroundWindow}
   * @private
   */
  this.backgroundPage_ = null;

  /**
   * The root DOM element of this app.
   * @type {HTMLBodyElement}
   * @private
   */
  this.dialogDom_ = null;

  /**
   * The document object of this app.
   * @type {HTMLDocument}
   * @private
   */
  this.document_ = null;

  /**
   * The menu item to toggle "Do not use mobile data for sync".
   * @type {HTMLMenuItemElement}
   */
  this.syncButton = null;

  /**
   * The menu item to toggle "Show Google Docs files".
   * @type {HTMLMenuItemElement}
   */
  this.hostedButton = null;

  /**
   * The menu item for doing an action.
   * @type {HTMLMenuItemElement}
   * @private
   */
  this.actionMenuItem_ = null;

  /**
   * The button to open gear menu.
   * @type {cr.ui.MenuButton}
   * @private
   */
  this.gearButton_ = null;

  /**
   * The combo button to specify the task.
   * @type {cr.ui.ComboButton}
   * @private
   */
  this.taskItems_ = null;

  /**
   * The container element of the dialog.
   * @type {HTMLDivElement}
   * @private
   */
  this.dialogContainer_ = null;

  /**
   * Open-with command in the context menu.
   * @type {cr.ui.Command}
   * @private
   */
  this.openWithCommand_ = null;

  // --------------------------------------------------------------------------
  // Bound functions.

  /**
   * Bound function for onCopyProgress_.
   * @type {?function(this:FileManager, Event)}
   * @private
   */
  this.onCopyProgressBound_ = null;

  /**
   * Bound function for onEntriesChanged_.
   * @type {?function(this:FileManager, Event)}
   * @private
   */
  this.onEntriesChangedBound_ = null;

  // --------------------------------------------------------------------------
  // Miscellaneous FileManager's states.

  /**
   * Queue for ordering FileManager's initialization process.
   * @type {!AsyncUtil.Group}
   * @private
   */
  this.initializeQueue_ = new AsyncUtil.Group();

  /**
   * True while a user is pressing <Tab>.
   * This is used for identifying the trigger causing the filelist to
   * be focused.
   * @type {boolean}
   * @private
   */
  this.pressingTab_ = false;

  /**
   * True while a user is pressing <Ctrl>.
   *
   * TODO(fukino): This key is used only for controlling gear menu, so it
   * should be moved to GearMenu class. crbug.com/366032.
   *
   * @type {boolean}
   * @private
   */
  this.pressingCtrl_ = false;

  /**
   * True if shown gear menu is in secret mode.
   *
   * TODO(fukino): The state of gear menu should be moved to GearMenu class.
   * crbug.com/366032.
   *
   * @type {boolean}
   * @private
   */
  this.isSecretGearMenuShown_ = false;

  /**
   * The last clicked item in the file list.
   * @type {HTMLLIElement}
   * @private
   */
  this.lastClickedItem_ = null;

  /**
   * Count of the SourceNotFound error.
   * @type {number}
   * @private
   */
  this.sourceNotFoundErrorCount_ = 0;

  /**
   * Whether the app should be closed on unmount.
   * @type {boolean}
   * @private
   */
  this.closeOnUnmount_ = false;

  /**
   * The key for storing startup preference.
   * @type {string}
   * @private
   */
  this.startupPrefName_ = '';

  /**
   * URL of directory which should be initial current directory.
   * @type {string}
   * @private
   */
  this.initCurrentDirectoryURL_ = '';

  /**
   * URL of entry which should be initially selected.
   * @type {string}
   * @private
   */
  this.initSelectionURL_ = '';

  /**
   * The name of target entry (not URL).
   * @type {string}
   * @private
   */
  this.initTargetName_ = '';


  // Object.seal() has big performance/memory overhead for now, so we use
  // Object.preventExtensions() here. crbug.com/412239.
  Object.preventExtensions(this);
}

FileManager.prototype = /** @struct */ {
  __proto__: cr.EventTarget.prototype,
  /**
   * @return {DirectoryModel}
   */
  get directoryModel() {
    return this.directoryModel_;
  },
  /**
   * @return {DirectoryTree}
   */
  get directoryTree() {
    return this.directoryTree_;
  },
  /**
   * @return {HTMLDocument}
   */
  get document() {
    return this.document_;
  },
  /**
   * @return {FileTransferController}
   */
  get fileTransferController() {
    return this.fileTransferController_;
  },
  /**
   * @return {NamingController}
   */
  get namingController() {
    return this.namingController_;
  },
  /**
   * @return {FileOperationManager}
   */
  get fileOperationManager() {
    return this.fileOperationManager_;
  },
  /**
   * @return {BackgroundWindow}
   */
  get backgroundPage() {
    return this.backgroundPage_;
  },
  /**
   * @return {VolumeManagerWrapper}
   */
  get volumeManager() {
    return this.volumeManager_;
  },
  /**
   * @return {FileManagerUI}
   */
  get ui() {
    return this.ui_;
  }
};

/**
 * List of dialog types.
 *
 * Keep this in sync with FileManagerDialog::GetDialogTypeAsString, except
 * FULL_PAGE which is specific to this code.
 *
 * @enum {string}
 * @const
 */
var DialogType = {
  SELECT_FOLDER: 'folder',
  SELECT_UPLOAD_FOLDER: 'upload-folder',
  SELECT_SAVEAS_FILE: 'saveas-file',
  SELECT_OPEN_FILE: 'open-file',
  SELECT_OPEN_MULTI_FILE: 'open-multi-file',
  FULL_PAGE: 'full-page'
};

/**
 * @param {DialogType} type Dialog type.
 * @return {boolean} Whether the type is modal.
 */
DialogType.isModal = function(type) {
  return type == DialogType.SELECT_FOLDER ||
      type == DialogType.SELECT_UPLOAD_FOLDER ||
      type == DialogType.SELECT_SAVEAS_FILE ||
      type == DialogType.SELECT_OPEN_FILE ||
      type == DialogType.SELECT_OPEN_MULTI_FILE;
};

/**
 * @param {DialogType} type Dialog type.
 * @return {boolean} Whether the type is open dialog.
 */
DialogType.isOpenDialog = function(type) {
  return type == DialogType.SELECT_OPEN_FILE ||
         type == DialogType.SELECT_OPEN_MULTI_FILE ||
         type == DialogType.SELECT_FOLDER ||
         type == DialogType.SELECT_UPLOAD_FOLDER;
};

/**
 * @param {DialogType} type Dialog type.
 * @return {boolean} Whether the type is open dialog for file(s).
 */
DialogType.isOpenFileDialog = function(type) {
  return type == DialogType.SELECT_OPEN_FILE ||
         type == DialogType.SELECT_OPEN_MULTI_FILE;
};

/**
 * @param {DialogType} type Dialog type.
 * @return {boolean} Whether the type is folder selection dialog.
 */
DialogType.isFolderDialog = function(type) {
  return type == DialogType.SELECT_FOLDER ||
         type == DialogType.SELECT_UPLOAD_FOLDER;
};

Object.freeze(DialogType);

/**
 * Bottom margin of the list and tree for transparent preview panel.
 * @const
 */
var BOTTOM_MARGIN_FOR_PREVIEW_PANEL_PX = 52;

// Anonymous "namespace".
(function() {
  // Private variables and helper functions.

  /**
   * Number of milliseconds in a day.
   */
  var MILLISECONDS_IN_DAY = 24 * 60 * 60 * 1000;

  /**
   * Some UI elements react on a single click and standard double click handling
   * leads to confusing results. We ignore a second click if it comes soon
   * after the first.
   */
  var DOUBLE_CLICK_TIMEOUT = 200;

  /**
   * Updates the element to display the information about remaining space for
   * the storage.
   *
   * @param {!Object<string, number>} sizeStatsResult Map containing remaining
   *     space information.
   * @param {!Element} spaceInnerBar Block element for a percentage bar
   *     representing the remaining space.
   * @param {!Element} spaceInfoLabel Inline element to contain the message.
   * @param {!Element} spaceOuterBar Block element around the percentage bar.
   */
  var updateSpaceInfo = function(
      sizeStatsResult, spaceInnerBar, spaceInfoLabel, spaceOuterBar) {
    spaceInnerBar.removeAttribute('pending');
    if (sizeStatsResult) {
      var sizeStr = util.bytesToString(sizeStatsResult.remainingSize);
      spaceInfoLabel.textContent = strf('SPACE_AVAILABLE', sizeStr);

      var usedSpace =
          sizeStatsResult.totalSize - sizeStatsResult.remainingSize;
      spaceInnerBar.style.width =
          (100 * usedSpace / sizeStatsResult.totalSize) + '%';

      spaceOuterBar.hidden = false;
    } else {
      spaceOuterBar.hidden = true;
      spaceInfoLabel.textContent = str('FAILED_SPACE_INFO');
    }
  };

  FileManager.prototype.initPreferences_ = function(callback) {
    var group = new AsyncUtil.Group();

    // DRIVE preferences should be initialized before creating DirectoryModel
    // to rebuild the roots list.
    group.add(this.getPreferences_.bind(this));

    // Get startup preferences.
    group.add(function(done) {
      chrome.storage.local.get(this.startupPrefName_, function(values) {
        var value = values[this.startupPrefName_];
        if (!value) {
          done();
          return;
        }
        // Load the global default options.
        try {
          this.viewOptions_ = JSON.parse(value);
        } catch (ignore) {}
        // Override with window-specific options.
        if (window.appState && window.appState.viewOptions) {
          for (var key in window.appState.viewOptions) {
            if (window.appState.viewOptions.hasOwnProperty(key))
              this.viewOptions_[key] = window.appState.viewOptions[key];
          }
        }
        done();
      }.bind(this));
    }.bind(this));

    group.run(callback);
  };

  /**
   * One time initialization for the file system and related things.
   *
   * @param {function()} callback Completion callback.
   * @private
   */
  FileManager.prototype.initFileSystemUI_ = function(callback) {
    this.ui_.listContainer.startBatchUpdates();

    this.initFileList_();
    this.setupCurrentDirectory_();

    // PyAuto tests monitor this state by polling this variable
    this.__defineGetter__('workerInitialized_', function() {
      return this.metadataCache_.isInitialized();
    }.bind(this));

    this.initDateTimeFormatters_();

    var self = this;

    // Get the 'allowRedeemOffers' preference before launching
    // FileListBannerController.
    this.getPreferences_(function(pref) {
      /** @type {boolean} */
      var showOffers = !!pref['allowRedeemOffers'];
      self.bannersController_ = new FileListBannerController(
          self.directoryModel_, self.volumeManager_, self.document_,
          showOffers);
      self.bannersController_.addEventListener('relayout',
                                               self.onResize_.bind(self));
    });

    var dm = this.directoryModel_;
    dm.addEventListener('directory-changed',
                        this.onDirectoryChanged_.bind(this));

    var listBeingUpdated = null;
    dm.addEventListener('begin-update-files', function() {
      self.ui_.listContainer.currentList.startBatchUpdates();
      // Remember the list which was used when updating files started, so
      // endBatchUpdates() is called on the same list.
      listBeingUpdated = self.ui_.listContainer.currentList;
    });
    dm.addEventListener('end-update-files', function() {
      self.namingController_.restoreItemBeingRenamed();
      listBeingUpdated.endBatchUpdates();
      listBeingUpdated = null;
    });

    this.initContextMenus_();
    this.initCommands_();
    assert(this.directoryModel_);
    assert(this.spinnerController_);
    assert(this.commandHandler);
    assert(this.selectionHandler_);
    this.scanController_ = new ScanController(
        this.directoryModel_,
        this.ui_.listContainer,
        this.spinnerController_,
        this.commandHandler,
        this.selectionHandler_);

    this.directoryTree_.addEventListener('change', function() {
      this.ensureDirectoryTreeItemNotBehindPreviewPanel_();
    }.bind(this));

    var stateChangeHandler =
        this.onPreferencesChanged_.bind(this);
    chrome.fileManagerPrivate.onPreferencesChanged.addListener(
        stateChangeHandler);
    stateChangeHandler();

    var driveConnectionChangedHandler =
        this.onDriveConnectionChanged_.bind(this);
    this.volumeManager_.addEventListener('drive-connection-changed',
        driveConnectionChangedHandler);
    driveConnectionChangedHandler();

    // Set the initial focus.
    this.refocus();
    // Set it as a fallback when there is no focus.
    this.document_.addEventListener('focusout', function(e) {
      setTimeout(function() {
        // When there is no focus, the active element is the <body>.
        if (this.document_.activeElement == this.document_.body)
          this.refocus();
      }.bind(this), 0);
    }.bind(this));

    this.initDataTransferOperations_();

    this.updateFileTypeFilter_();
    this.selectionHandler_.onFileSelectionChanged();
    this.ui_.listContainer.endBatchUpdates();

    callback();
  };

  /**
   * If |item| in the directory tree is behind the preview panel, scrolls up the
   * parent view and make the item visible. This should be called when:
   *  - the selected item is changed in the directory tree.
   *  - the visibility of the the preview panel is changed.
   *
   * @private
   */
  FileManager.prototype.ensureDirectoryTreeItemNotBehindPreviewPanel_ =
      function() {
    var selectedSubTree = this.directoryTree_.selectedItem;
    if (!selectedSubTree)
      return;
    var item = selectedSubTree.rowElement;
    var parentView = this.directoryTree_;

    var itemRect = item.getBoundingClientRect();
    if (!itemRect)
      return;

    var listRect = parentView.getBoundingClientRect();
    if (!listRect)
      return;

    var previewPanel = this.dialogDom_.querySelector('.preview-panel');
    var previewPanelRect = previewPanel.getBoundingClientRect();
    var panelHeight = previewPanelRect ? previewPanelRect.height : 0;

    var itemBottom = itemRect.bottom;
    var listBottom = listRect.bottom - panelHeight;

    if (itemBottom > listBottom) {
      var scrollOffset = itemBottom - listBottom;
      parentView.scrollTop += scrollOffset;
    }
  };

  /**
   * @private
   */
  FileManager.prototype.initDateTimeFormatters_ = function() {
    var use12hourClock = !this.preferences_['use24hourClock'];
    this.ui_.listContainer.table.setDateTimeFormat(use12hourClock);
  };

  /**
   * @private
   */
  FileManager.prototype.initDataTransferOperations_ = function() {
    this.fileOperationManager_ =
        this.backgroundPage_.background.fileOperationManager;

    // CopyManager are required for 'Delete' operation in
    // Open and Save dialogs. But drag-n-drop and copy-paste are not needed.
    if (this.dialogType != DialogType.FULL_PAGE) return;

    // TODO(hidehiko): Extract FileOperationManager related code from
    // FileManager to simplify it.
    this.onCopyProgressBound_ = this.onCopyProgress_.bind(this);
    this.fileOperationManager_.addEventListener(
        'copy-progress', this.onCopyProgressBound_);

    this.onEntriesChangedBound_ = this.onEntriesChanged_.bind(this);
    this.fileOperationManager_.addEventListener(
        'entries-changed', this.onEntriesChangedBound_);

    var controller = this.fileTransferController_ =
        new FileTransferController(
                this.document_,
                this.fileOperationManager_,
                this.metadataCache_,
                this.directoryModel_,
                this.volumeManager_,
                this.ui_.multiProfileShareDialog,
                this.backgroundPage_.background.progressCenter);
    controller.attachDragSource(this.ui_.listContainer.table.list);
    controller.attachFileListDropTarget(this.ui_.listContainer.table.list);
    controller.attachDragSource(this.ui_.listContainer.grid);
    controller.attachFileListDropTarget(this.ui_.listContainer.grid);
    controller.attachTreeDropTarget(this.directoryTree_);
    controller.attachCopyPasteHandlers();
    controller.addEventListener('selection-copied',
        this.blinkSelection.bind(this));
    controller.addEventListener('selection-cut',
        this.blinkSelection.bind(this));
    controller.addEventListener('source-not-found',
        this.onSourceNotFound_.bind(this));
  };

  /**
   * Handles an error that the source entry of file operation is not found.
   * @private
   */
  FileManager.prototype.onSourceNotFound_ = function(event) {
    var item = new ProgressCenterItem();
    item.id = 'source-not-found-' + this.sourceNotFoundErrorCount_;
    if (event.progressType === ProgressItemType.COPY)
      item.message = strf('COPY_SOURCE_NOT_FOUND_ERROR', event.fileName);
    else if (event.progressType === ProgressItemType.MOVE)
      item.message = strf('MOVE_SOURCE_NOT_FOUND_ERROR', event.fileName);
    item.state = ProgressItemState.ERROR;
    this.backgroundPage_.background.progressCenter.updateItem(item);
    this.sourceNotFoundErrorCount_++;
  };

  /**
   * One-time initialization of context menus.
   * @private
   */
  FileManager.prototype.initContextMenus_ = function() {
    assert(this.ui_.listContainer.grid);
    assert(this.ui_.listContainer.table);
    assert(this.document_);
    assert(this.dialogDom_);

    // Set up the context menu for the file list.
    var fileContextMenu = queryRequiredElement(
        this.dialogDom_, '#file-context-menu');
    cr.ui.Menu.decorate(fileContextMenu);
    fileContextMenu = /** @type {!cr.ui.Menu} */ (fileContextMenu);

    cr.ui.contextMenuHandler.setContextMenu(
        this.ui_.listContainer.grid, fileContextMenu);
    cr.ui.contextMenuHandler.setContextMenu(
        this.ui_.listContainer.table.list, fileContextMenu);
    cr.ui.contextMenuHandler.setContextMenu(
        queryRequiredElement(this.document_, '.drive-welcome.page'),
        fileContextMenu);

    // Set up the context menu for the volume/shortcut items in directory tree.
    var rootsContextMenu = queryRequiredElement(
        this.dialogDom_, '#roots-context-menu');
    cr.ui.Menu.decorate(rootsContextMenu);
    rootsContextMenu = /** @type {!cr.ui.Menu} */ (rootsContextMenu);

    this.directoryTree_.contextMenuForRootItems = rootsContextMenu;

    // Set up the context menu for the folder items in directory tree.
    var directoryTreeContextMenu = queryRequiredElement(
        this.dialogDom_, '#directory-tree-context-menu');
    cr.ui.Menu.decorate(directoryTreeContextMenu);
    directoryTreeContextMenu =
        /** @type {!cr.ui.Menu} */ (directoryTreeContextMenu);

    this.directoryTree_.contextMenuForSubitems = directoryTreeContextMenu;

    // Set up the context menu for the text editing.
    var textContextMenu = queryRequiredElement(
        this.dialogDom_, '#text-context-menu');
    cr.ui.Menu.decorate(textContextMenu);
    this.textContextMenu_ = /** @type {!cr.ui.Menu} */ (textContextMenu);

    var gearButton = queryRequiredElement(this.dialogDom_, '#gear-button');
    gearButton.addEventListener('menushow', this.onShowGearMenu_.bind(this));
    this.dialogDom_.querySelector('#gear-menu').menuItemSelector =
        'menuitem, hr';
    cr.ui.decorate(gearButton, cr.ui.MenuButton);
    this.gearButton_ = /** @type {!cr.ui.MenuButton} */ (gearButton);

    this.syncButton.checkable = true;
    this.hostedButton.checkable = true;

    if (util.runningInBrowser()) {
      // Suppresses the default context menu.
      this.dialogDom_.addEventListener('contextmenu', function(e) {
        e.preventDefault();
        e.stopPropagation();
      });
    }
  };

  FileManager.prototype.onShowGearMenu_ = function() {
    this.refreshRemainingSpace_(false);  /* Without loading caption. */

    // If the menu is opened while CTRL key pressed, secret menu itemscan be
    // shown.
    this.isSecretGearMenuShown_ = this.pressingCtrl_;

    // Update view of drive-related settings.
    this.commandHandler.updateAvailability();
    this.document_.getElementById('drive-separator').hidden =
        !this.shouldShowDriveSettings();

    // Force to update the gear menu position.
    // TODO(hirono): Remove the workaround for the crbug.com/374093 after fixing
    // it.
    var gearMenu = this.document_.querySelector('#gear-menu');
    gearMenu.style.left = '';
    gearMenu.style.right = '';
    gearMenu.style.top = '';
    gearMenu.style.bottom = '';
  };

  /**
   * One-time initialization of commands.
   * @private
   */
  FileManager.prototype.initCommands_ = function() {
    assert(this.textContextMenu_);

    this.commandHandler = new CommandHandler(this);

    // TODO(hirono): Move the following block to the UI part.
    var commandButtons = this.dialogDom_.querySelectorAll('button[command]');
    for (var j = 0; j < commandButtons.length; j++)
      CommandButton.decorate(commandButtons[j]);

    var inputs = this.dialogDom_.querySelectorAll(
        'input[type=text], input[type=search], textarea');
    for (var i = 0; i < inputs.length; i++) {
      cr.ui.contextMenuHandler.setContextMenu(inputs[i], this.textContextMenu_);
      this.registerInputCommands_(inputs[i]);
    }

    cr.ui.contextMenuHandler.setContextMenu(this.ui_.listContainer.renameInput,
                                            this.textContextMenu_);
    this.registerInputCommands_(this.ui_.listContainer.renameInput);
    this.document_.addEventListener(
        'command',
        this.ui_.listContainer.clearHover.bind(this.ui_.listContainer));
  };

  /**
   * Registers cut, copy, paste and delete commands on input element.
   *
   * @param {Node} node Text input element to register on.
   * @private
   */
  FileManager.prototype.registerInputCommands_ = function(node) {
    CommandUtil.forceDefaultHandler(node, 'cut');
    CommandUtil.forceDefaultHandler(node, 'copy');
    CommandUtil.forceDefaultHandler(node, 'paste');
    CommandUtil.forceDefaultHandler(node, 'delete');
    node.addEventListener('keydown', function(e) {
      var key = util.getKeyModifiers(e) + e.keyCode;
      if (key === '190' /* '/' */ || key === '191' /* '.' */) {
        // If this key event is propagated, this is handled search command,
        // which calls 'preventDefault' method.
        e.stopPropagation();
      }
    });
  };

  /**
   * Entry point of the initialization.
   * This method is called from main.js.
   */
  FileManager.prototype.initializeCore = function() {
    this.initializeQueue_.add(this.initGeneral_.bind(this), [], 'initGeneral');
    this.initializeQueue_.add(this.initBackgroundPage_.bind(this),
                              [], 'initBackgroundPage');
    this.initializeQueue_.add(this.initPreferences_.bind(this),
                              ['initGeneral'], 'initPreferences');
    this.initializeQueue_.add(this.initVolumeManager_.bind(this),
                              ['initGeneral', 'initBackgroundPage'],
                              'initVolumeManager');

    this.initializeQueue_.run();
    window.addEventListener('pagehide', this.onUnload_.bind(this));
  };

  FileManager.prototype.initializeUI = function(dialogDom, callback) {
    this.dialogDom_ = dialogDom;
    this.document_ = this.dialogDom_.ownerDocument;

    this.initializeQueue_.add(
        this.initEssentialUI_.bind(this),
        ['initGeneral', 'initBackgroundPage'],
        'initEssentialUI');
    this.initializeQueue_.add(this.initAdditionalUI_.bind(this),
        ['initEssentialUI'], 'initAdditionalUI');
    this.initializeQueue_.add(
        this.initFileSystemUI_.bind(this),
        ['initAdditionalUI', 'initPreferences'], 'initFileSystemUI');

    // Run again just in case if all pending closures have completed and the
    // queue has stopped and monitor the completion.
    this.initializeQueue_.run(callback);
  };

  /**
   * Initializes general purpose basic things, which are used by other
   * initializing methods.
   *
   * @param {function()} callback Completion callback.
   * @private
   */
  FileManager.prototype.initGeneral_ = function(callback) {
    // Initialize the application state.
    // TODO(mtomasz): Unify window.appState with location.search format.
    if (window.appState) {
      this.params_ = window.appState.params || {};
      this.initCurrentDirectoryURL_ = window.appState.currentDirectoryURL;
      this.initSelectionURL_ = window.appState.selectionURL;
      this.initTargetName_ = window.appState.targetName;
    } else {
      // Used by the select dialog only.
      this.params_ = location.search ?
                     JSON.parse(decodeURIComponent(location.search.substr(1))) :
                     {};
      this.initCurrentDirectoryURL_ = this.params_.currentDirectoryURL;
      this.initSelectionURL_ = this.params_.selectionURL;
      this.initTargetName_ = this.params_.targetName;
    }

    // Initialize the member variables that depend this.params_.
    this.dialogType = this.params_.type || DialogType.FULL_PAGE;
    this.startupPrefName_ = 'file-manager-' + this.dialogType;
    this.fileTypes_ = this.params_.typeList || [];

    callback();
  };

  /**
   * Initializes the background page.
   * @param {function()} callback Completion callback.
   * @private
   */
  FileManager.prototype.initBackgroundPage_ = function(callback) {
    chrome.runtime.getBackgroundPage(/** @type {function(Window=)} */ (
        function(opt_backgroundPage) {
          assert(opt_backgroundPage);
          this.backgroundPage_ =
              /** @type {!BackgroundWindow} */ (opt_backgroundPage);
          this.backgroundPage_.background.ready(function() {
            loadTimeData.data = this.backgroundPage_.background.stringData;
            if (util.runningInBrowser())
              this.backgroundPage_.registerDialog(window);
            callback();
          }.bind(this));
        }.bind(this)));
  };

  /**
   * Initializes the VolumeManager instance.
   * @param {function()} callback Completion callback.
   * @private
   */
  FileManager.prototype.initVolumeManager_ = function(callback) {
    // Auto resolving to local path does not work for folders (e.g., dialog for
    // loading unpacked extensions).
    var noLocalPathResolution = DialogType.isFolderDialog(this.params_.type);

    // If this condition is false, VolumeManagerWrapper hides all drive
    // related event and data, even if Drive is enabled on preference.
    // In other words, even if Drive is disabled on preference but Files.app
    // should show Drive when it is re-enabled, then the value should be set to
    // true.
    // Note that the Drive enabling preference change is listened by
    // DriveIntegrationService, so here we don't need to take care about it.
    var driveEnabled =
        !noLocalPathResolution || !this.params_.shouldReturnLocalPath;
    this.volumeManager_ = new VolumeManagerWrapper(
        /** @type {VolumeManagerWrapper.DriveEnabledStatus} */ (driveEnabled),
        this.backgroundPage_);
    callback();
  };

  /**
   * One time initialization of the Files.app's essential UI elements. These
   * elements will be shown to the user. Only visible elements should be
   * initialized here. Any heavy operation should be avoided. Files.app's
   * window is shown at the end of this routine.
   *
   * @param {function()} callback Completion callback.
   * @private
   */
  FileManager.prototype.initEssentialUI_ = function(callback) {
    // Record stats of dialog types. New values must NOT be inserted into the
    // array enumerating the types. It must be in sync with
    // FileDialogType enum in tools/metrics/histograms/histogram.xml.
    metrics.recordEnum('Create', this.dialogType,
        [DialogType.SELECT_FOLDER,
         DialogType.SELECT_UPLOAD_FOLDER,
         DialogType.SELECT_SAVEAS_FILE,
         DialogType.SELECT_OPEN_FILE,
         DialogType.SELECT_OPEN_MULTI_FILE,
         DialogType.FULL_PAGE]);

    // Create the metadata cache.
    this.metadataCache_ = MetadataCache.createFull(this.volumeManager_);

    // Create the root view of FileManager.
    assert(this.dialogDom_);
    this.ui_ = new FileManagerUI(this.dialogDom_, this.dialogType);

    // Show the window as soon as the UI pre-initialization is done.
    if (this.dialogType == DialogType.FULL_PAGE && !util.runningInBrowser()) {
      chrome.app.window.current().show();
      setTimeout(callback, 100);  // Wait until the animation is finished.
    } else {
      callback();
    }
  };

  /**
   * One-time initialization of dialogs.
   * @private
   */
  FileManager.prototype.initDialogs_ = function() {
    // Initialize the dialog.
    this.ui_.initDialogs();
    FileManagerDialogBase.setFileManager(this);

    // Obtains the dialog instances from FileManagerUI.
    // TODO(hirono): Remove the properties from the FileManager class.
    this.error = this.ui_.errorDialog;
    this.alert = this.ui_.alertDialog;
    this.confirm = this.ui_.confirmDialog;
    this.prompt = this.ui_.promptDialog;
    this.shareDialog_ = this.ui_.shareDialog;
    this.defaultTaskPicker = this.ui_.defaultTaskPicker;
    this.suggestAppsDialog = this.ui_.suggestAppsDialog;
  };

  /**
   * One-time initialization of various DOM nodes. Loads the additional DOM
   * elements visible to the user. Initialize here elements, which are expensive
   * or hidden in the beginning.
   *
   * @param {function()} callback Completion callback.
   * @private
   */
  FileManager.prototype.initAdditionalUI_ = function(callback) {
    assert(this.metadataCache_);
    assert(this.volumeManager_);

    // Cache nodes we'll be manipulating.
    var dom = this.dialogDom_;
    assert(dom);

    this.initDialogs_();

    var table = queryRequiredElement(dom, '.detail-table');
    FileTable.decorate(
        table,
        this.metadataCache_,
        this.volumeManager_,
        this.dialogType == DialogType.FULL_PAGE);
    var grid = queryRequiredElement(dom, '.thumbnail-grid');
    FileGrid.decorate(grid, this.metadataCache_, this.volumeManager_);

    this.ui_.initAdditionalUI(
        assertInstanceof(table, FileTable),
        assertInstanceof(grid, FileGrid),
        new PreviewPanel(
            queryRequiredElement(dom, '.preview-panel'),
            DialogType.isOpenDialog(this.dialogType) ?
                PreviewPanel.VisibilityType.ALWAYS_VISIBLE :
                PreviewPanel.VisibilityType.AUTO,
            this.metadataCache_,
            this.volumeManager_));

    this.dialogDom_.addEventListener('click',
                                     this.onExternalLinkClick_.bind(this));

    this.ui_.locationLine = new LocationLine(
        queryRequiredElement(dom, '#location-breadcrumbs'),
        queryRequiredElement(dom, '#location-volume-icon'),
        this.metadataCache_,
        this.volumeManager_);
    this.ui_.locationLine.addEventListener(
        'pathclick', this.onBreadcrumbClick_.bind(this));

    // Initialize progress center panel.
    this.progressCenterPanel_ = new ProgressCenterPanel(
        queryRequiredElement(dom, '#progress-center'));
    this.backgroundPage_.background.progressCenter.addPanel(
        this.progressCenterPanel_);

    this.document_.addEventListener('keydown', this.onKeyDown_.bind(this));
    this.document_.addEventListener('keyup', this.onKeyUp_.bind(this));

    this.ui_.listContainer.element.addEventListener(
        'keydown', this.onListKeyDown_.bind(this));
    this.ui_.listContainer.element.addEventListener(
        ListContainer.EventType.TEXT_SEARCH, this.onTextSearch_.bind(this));

    // TODO(hirono): Rename the handler after creating the DialogFooter class.
    this.ui_.dialogFooter.filenameInput.addEventListener(
        'input', this.onFilenameInputInput_.bind(this));
    this.ui_.dialogFooter.filenameInput.addEventListener(
        'keydown', this.onFilenameInputKeyDown_.bind(this));
    this.ui_.dialogFooter.filenameInput.addEventListener(
        'focus', this.onFilenameInputFocus_.bind(this));

    this.decorateSplitter(
        this.dialogDom_.querySelector('#navigation-list-splitter'));

    this.dialogContainer_ = /** @type {!HTMLDivElement} */
        (this.dialogDom_.querySelector('.dialog-container'));

    this.syncButton = /** @type {!HTMLMenuItemElement} */
        (queryRequiredElement(this.dialogDom_,
                              '#gear-menu-drive-sync-settings'));
    this.hostedButton = /** @type {!HTMLMenuItemElement} */
        (queryRequiredElement(this.dialogDom_,
                             '#gear-menu-drive-hosted-settings'));

    this.ui_.toggleViewButton.addEventListener('click',
        this.onToggleViewButtonClick_.bind(this));

    var taskItems = queryRequiredElement(dom, '#tasks');
    cr.ui.ComboButton.decorate(taskItems);
    this.taskItems_ = assertInstanceof(taskItems, cr.ui.ComboButton);
    this.taskItems_.showMenu = function(shouldSetFocus) {
      // Prevent the empty menu from opening.
      if (!this.menu.length)
        return;
      cr.ui.ComboButton.prototype.showMenu.call(this, shouldSetFocus);
    };
    this.taskItems_.addEventListener('select',
        this.onTaskItemClicked_.bind(this));

    this.dialogDom_.ownerDocument.defaultView.addEventListener(
        'resize', this.onResize_.bind(this));

    this.actionMenuItem_ = /** @type {!HTMLMenuItemElement} */
        (queryRequiredElement(this.dialogDom_, '#default-action'));

    this.openWithCommand_ = /** @type {cr.ui.Command} */
        (this.dialogDom_.querySelector('#open-with'));

    this.actionMenuItem_.addEventListener('activate',
        this.onActionMenuItemActivated_.bind(this));

    this.ui_.dialogFooter.initFileTypeFilter(
        this.fileTypes_, this.params_.includeAllFiles);
    this.ui_.dialogFooter.fileTypeSelector.addEventListener(
        'change', this.updateFileTypeFilter_.bind(this));

    util.addIsFocusedMethod();

    // Populate the static localized strings.
    i18nTemplate.process(this.document_, loadTimeData);

    // Arrange the file list.
    this.ui_.listContainer.table.normalizeColumns();
    this.ui_.listContainer.table.redraw();

    callback();
  };

  /**
   * @param {Event} event Click event.
   * @private
   */
  FileManager.prototype.onBreadcrumbClick_ = function(event) {
    this.directoryModel_.changeDirectoryEntry(event.entry);
  };

  /**
   * Constructs table and grid (heavy operation).
   * @private
   **/
  FileManager.prototype.initFileList_ = function() {
    var singleSelection =
        this.dialogType == DialogType.SELECT_OPEN_FILE ||
        this.dialogType == DialogType.SELECT_FOLDER ||
        this.dialogType == DialogType.SELECT_UPLOAD_FOLDER ||
        this.dialogType == DialogType.SELECT_SAVEAS_FILE;

    assert(this.metadataCache_);
    this.fileFilter_ = new FileFilter(
        this.metadataCache_,
        false  /* Don't show dot files and *.crdownload by default. */);

    this.fileWatcher_ = new FileWatcher(this.metadataCache_);
    this.fileWatcher_.addEventListener(
        'watcher-metadata-changed',
        this.onWatcherMetadataChanged_.bind(this));

    this.directoryModel_ = new DirectoryModel(
        singleSelection,
        this.fileFilter_,
        this.fileWatcher_,
        this.metadataCache_,
        this.volumeManager_);

    this.folderShortcutsModel_ = new FolderShortcutsDataModel(
        this.volumeManager_);

    this.selectionHandler_ = new FileSelectionHandler(this);

    var dataModel = this.directoryModel_.getFileList();
    dataModel.addEventListener('permuted',
                               this.updateStartupPrefs_.bind(this));

    this.directoryModel_.getFileListSelection().addEventListener('change',
        this.selectionHandler_.onFileSelectionChanged.bind(
            this.selectionHandler_));

    var onDetailClickBound = this.onDetailClick_.bind(this);
    this.ui_.listContainer.table.list.addEventListener(
        'click', onDetailClickBound);
    this.ui_.listContainer.grid.addEventListener(
        'click', onDetailClickBound);

    var fileListFocusBound = this.onFileListFocus_.bind(this);
    this.ui_.listContainer.table.list.addEventListener(
        'focus', fileListFocusBound);
    this.ui_.listContainer.grid.addEventListener('focus', fileListFocusBound);

    // TODO(mtomasz, yoshiki): Create navigation list earlier, and here just
    // attach the directory model.
    this.initDirectoryTree_();

    this.ui_.listContainer.table.addEventListener('column-resize-end',
                                 this.updateStartupPrefs_.bind(this));

    // Restore preferences.
    this.directoryModel_.getFileList().sort(
        this.viewOptions_.sortField || 'modificationTime',
        this.viewOptions_.sortDirection || 'desc');
    if (this.viewOptions_.columns) {
      var cm = this.ui_.listContainer.table.columnModel;
      for (var i = 0; i < cm.size; i++) {
        if (this.viewOptions_.columns[i] > 0)
          cm.setWidth(i, this.viewOptions_.columns[i]);
      }
    }

    this.ui_.listContainer.dataModel = this.directoryModel_.getFileList();
    this.ui_.listContainer.selectionModel =
        this.directoryModel_.getFileListSelection();
    this.setListType(
        this.viewOptions_.listType || ListContainer.ListType.DETAIL);

    this.closeOnUnmount_ = (this.params_.action == 'auto-open');

    if (this.closeOnUnmount_) {
      this.volumeManager_.addEventListener('externally-unmounted',
          this.onExternallyUnmounted_.bind(this));
    }

    // Create search controller.
    this.searchController_ = new SearchController(
        this.ui_.searchBox,
        this.ui_.locationLine,
        this.directoryModel_,
        this.volumeManager_,
        {
          // TODO (hirono): Make the real task controller and pass it here.
          doAction: this.doEntryAction_.bind(this)
        });

    // Create naming controller.
    assert(this.ui_.alertDialog);
    assert(this.ui_.confirmDialog);
    this.namingController_ = new NamingController(
        this.ui_.listContainer,
        this.ui_.alertDialog,
        this.ui_.confirmDialog,
        this.directoryModel_,
        this.fileFilter_,
        this.selectionHandler_);

    // Create spinner controller.
    this.spinnerController_ = new SpinnerController(
        this.ui_.listContainer.spinner, this.directoryModel_);
    this.spinnerController_.show();

    // Create dialog action controller.
    this.dialogActionController_ = new DialogActionController(
        this.dialogType,
        this.ui_.dialogFooter,
        this.directoryModel_,
        this.metadataCache_,
        this.namingController_,
        this.params_.shouldReturnLocalPath);

    // Update metadata to change 'Today' and 'Yesterday' dates.
    var today = new Date();
    today.setHours(0);
    today.setMinutes(0);
    today.setSeconds(0);
    today.setMilliseconds(0);
    setTimeout(this.dailyUpdateModificationTime_.bind(this),
               today.getTime() + MILLISECONDS_IN_DAY - Date.now() + 1000);
  };

  /**
   * @private
   */
  FileManager.prototype.initDirectoryTree_ = function() {
    var fakeEntriesVisible =
        this.dialogType !== DialogType.SELECT_SAVEAS_FILE;
    this.directoryTree_ = /** @type {DirectoryTree} */
        (this.dialogDom_.querySelector('#directory-tree'));
    DirectoryTree.decorate(this.directoryTree_,
                           this.directoryModel_,
                           this.volumeManager_,
                           this.metadataCache_,
                           fakeEntriesVisible);
    this.directoryTree_.dataModel = new NavigationListModel(
        this.volumeManager_, this.folderShortcutsModel_);

    // Visible height of the directory tree depends on the size of progress
    // center panel. When the size of progress center panel changes, directory
    // tree has to be notified to adjust its components (e.g. progress bar).
    var observer = new MutationObserver(
        this.directoryTree_.relayout.bind(this.directoryTree_));
    observer.observe(this.progressCenterPanel_.element,
                     /** @type {MutationObserverInit} */
                     ({subtree: true, attributes: true, childList: true}));
  };

  /**
   * @private
   */
  FileManager.prototype.updateStartupPrefs_ = function() {
    var sortStatus = this.directoryModel_.getFileList().sortStatus;
    var prefs = {
      sortField: sortStatus.field,
      sortDirection: sortStatus.direction,
      columns: [],
      listType: this.ui_.listContainer.currentListType
    };
    var cm = this.ui_.listContainer.table.columnModel;
    for (var i = 0; i < cm.size; i++) {
      prefs.columns.push(cm.getWidth(i));
    }
    // Save the global default.
    var items = {};
    items[this.startupPrefName_] = JSON.stringify(prefs);
    chrome.storage.local.set(items);

    // Save the window-specific preference.
    if (window.appState) {
      window.appState.viewOptions = prefs;
      util.saveAppState();
    }
  };

  FileManager.prototype.refocus = function() {
    var targetElement;
    if (this.dialogType == DialogType.SELECT_SAVEAS_FILE)
      targetElement = this.ui_.dialogFooter.filenameInput;
    else
      targetElement = this.ui.listContainer.currentList;

    // Hack: if the tabIndex is disabled, we can assume a modal dialog is
    // shown. Focus to a button on the dialog instead.
    if (!targetElement.hasAttribute('tabIndex') || targetElement.tabIndex == -1)
      targetElement = document.querySelector('button:not([tabIndex="-1"])');

    if (targetElement)
      targetElement.focus();
  };

  /**
   * File list focus handler. Used to select the top most element on the list
   * if nothing was selected.
   *
   * @private
   */
  FileManager.prototype.onFileListFocus_ = function() {
    // If the file list is focused by <Tab>, select the first item if no item
    // is selected.
    if (this.pressingTab_) {
      if (this.getSelection() && this.getSelection().totalCount == 0)
        this.directoryModel_.selectIndex(0);
    }
  };

  /**
   * Sets the current list type.
   * @param {ListContainer.ListType} type New list type.
   */
  FileManager.prototype.setListType = function(type) {
    if ((type && type == this.ui_.listContainer.currentListType) ||
        !this.directoryModel_) {
      return;
    }

    this.ui_.setCurrentListType(type);
    this.updateStartupPrefs_();
    this.onResize_();
  };

  /**
   * @private
   */
  FileManager.prototype.onCopyProgress_ = function(event) {
    if (event.reason == 'ERROR' &&
        event.error.code == util.FileOperationErrorType.FILESYSTEM_ERROR &&
        event.error.data.toDrive &&
        event.error.data.name == util.FileError.QUOTA_EXCEEDED_ERR) {
      this.alert.showHtml(
          strf('DRIVE_SERVER_OUT_OF_SPACE_HEADER'),
          strf('DRIVE_SERVER_OUT_OF_SPACE_MESSAGE',
              decodeURIComponent(
                  event.error.data.sourceFileUrl.split('/').pop()),
              str('GOOGLE_DRIVE_BUY_STORAGE_URL')),
          null, null, null);
    }
  };

  /**
   * Handler of file manager operations. Called when an entry has been
   * changed.
   * This updates directory model to reflect operation result immediately (not
   * waiting for directory update event). Also, preloads thumbnails for the
   * images of new entries.
   * See also FileOperationManager.EventRouter.
   *
   * @param {Event} event An event for the entry change.
   * @private
   */
  FileManager.prototype.onEntriesChanged_ = function(event) {
    var kind = event.kind;
    var entries = event.entries;
    this.directoryModel_.onEntriesChanged(kind, entries);
    this.selectionHandler_.onFileSelectionChanged();

    if (kind !== util.EntryChangedKind.CREATED)
      return;

    var preloadThumbnail = function(entry) {
      var locationInfo = this.volumeManager_.getLocationInfo(entry);
      if (!locationInfo)
        return;
      this.metadataCache_.getOne(entry, 'thumbnail|external',
          function(metadata) {
            var thumbnailLoader_ = new ThumbnailLoader(
                entry,
                ThumbnailLoader.LoaderType.CANVAS,
                metadata,
                undefined,  // Media type.
                locationInfo.isDriveBased ?
                    ThumbnailLoader.UseEmbedded.USE_EMBEDDED :
                    ThumbnailLoader.UseEmbedded.NO_EMBEDDED,
                10);  // Very low priority.
            thumbnailLoader_.loadDetachedImage(function(success) {});
          });
    }.bind(this);

    for (var i = 0; i < entries.length; i++) {
      // Preload a thumbnail if the new copied entry an image.
      if (FileType.isImage(entries[i]))
        preloadThumbnail(entries[i]);
    }
  };

  /**
   * Filters file according to the selected file type.
   * @private
   */
  FileManager.prototype.updateFileTypeFilter_ = function() {
    this.fileFilter_.removeFilter('fileType');
    var selectedIndex = this.ui_.dialogFooter.selectedFilterIndex;
    if (selectedIndex > 0) { // Specific filter selected.
      var regexp = new RegExp('\\.(' +
          this.fileTypes_[selectedIndex - 1].extensions.join('|') + ')$', 'i');
      var filter = function(entry) {
        return entry.isDirectory || regexp.test(entry.name);
      };
      this.fileFilter_.addFilter('fileType', filter);

      // In save dialog, update the destination name extension.
      if (this.dialogType === DialogType.SELECT_SAVEAS_FILE) {
        var current = this.ui_.dialogFooter.filenameInput.value;
        var newExt = this.fileTypes_[selectedIndex - 1].extensions[0];
        if (newExt && !regexp.test(current)) {
          var i = current.lastIndexOf('.');
          if (i >= 0) {
            this.ui_.dialogFooter.filenameInput.value =
                current.substr(0, i) + '.' + newExt;
            this.selectTargetNameInFilenameInput_();
          }
        }
      }
    }
  };

  /**
   * Resize details and thumb views to fit the new window size.
   * @private
   */
  FileManager.prototype.onResize_ = function() {
    // May not be available during initialization.
    if (this.directoryTree_)
      this.directoryTree_.relayout();

    this.ui_.relayout();
  };

  /**
   * Handles local metadata changes in the currect directory.
   * @param {Event} event Change event.
   * @this {FileManager}
   * @private
   */
  FileManager.prototype.onWatcherMetadataChanged_ = function(event) {
    this.ui_.listContainer.currentView.updateListItemsMetadata(
        event.metadataType, event.entries);
  };

  /**
   * Sets up the current directory during initialization.
   * @private
   */
  FileManager.prototype.setupCurrentDirectory_ = function() {
    var tracker = this.directoryModel_.createDirectoryChangeTracker();
    var queue = new AsyncUtil.Queue();

    // Wait until the volume manager is initialized.
    queue.run(function(callback) {
      tracker.start();
      this.volumeManager_.ensureInitialized(callback);
    }.bind(this));

    var nextCurrentDirEntry;
    var selectionEntry;

    // Resolve the selectionURL to selectionEntry or to currentDirectoryEntry
    // in case of being a display root or a default directory to open files.
    queue.run(function(callback) {
      if (!this.initSelectionURL_) {
        callback();
        return;
      }
      window.webkitResolveLocalFileSystemURL(
          this.initSelectionURL_,
          function(inEntry) {
            var locationInfo = this.volumeManager_.getLocationInfo(inEntry);
            // If location information is not available, then the volume is
            // no longer (or never) available.
            if (!locationInfo) {
              callback();
              return;
            }
            // If the selection is root, then use it as a current directory
            // instead. This is because, selecting a root entry is done as
            // opening it.
            if (locationInfo.isRootEntry)
              nextCurrentDirEntry = inEntry;

            // If this dialog attempts to open file(s) and the selection is a
            // directory, the selection should be the current directory.
            if (DialogType.isOpenFileDialog(this.dialogType) &&
                inEntry.isDirectory) {
              nextCurrentDirEntry = inEntry;
            }

            // By default, the selection should be selected entry and the
            // parent directory of it should be the current directory.
            if (!nextCurrentDirEntry)
              selectionEntry = inEntry;

            callback();
          }.bind(this), callback);
    }.bind(this));
    // Resolve the currentDirectoryURL to currentDirectoryEntry (if not done
    // by the previous step).
    queue.run(function(callback) {
      if (nextCurrentDirEntry || !this.initCurrentDirectoryURL_) {
        callback();
        return;
      }
      window.webkitResolveLocalFileSystemURL(
          this.initCurrentDirectoryURL_,
          function(inEntry) {
            var locationInfo = this.volumeManager_.getLocationInfo(inEntry);
            if (!locationInfo) {
              callback();
              return;
            }
            nextCurrentDirEntry = inEntry;
            callback();
          }.bind(this), callback);
      // TODO(mtomasz): Implement reopening on special search, when fake
      // entries are converted to directory providers.
    }.bind(this));

    // If the directory to be changed to is not available, then first fallback
    // to the parent of the selection entry.
    queue.run(function(callback) {
      if (nextCurrentDirEntry || !selectionEntry) {
        callback();
        return;
      }
      selectionEntry.getParent(function(inEntry) {
        nextCurrentDirEntry = inEntry;
        callback();
      }.bind(this));
    }.bind(this));

    // Check if the next current directory is not a virtual directory which is
    // not available in UI. This may happen to shared on Drive.
    queue.run(function(callback) {
      if (!nextCurrentDirEntry) {
        callback();
        return;
      }
      var locationInfo = this.volumeManager_.getLocationInfo(
          nextCurrentDirEntry);
      // If we can't check, assume that the directory is illegal.
      if (!locationInfo) {
        nextCurrentDirEntry = null;
        callback();
        return;
      }
      // Having root directory of DRIVE_OTHER here should be only for shared
      // with me files. Fallback to Drive root in such case.
      if (locationInfo.isRootEntry && locationInfo.rootType ===
              VolumeManagerCommon.RootType.DRIVE_OTHER) {
        var volumeInfo = this.volumeManager_.getVolumeInfo(nextCurrentDirEntry);
        if (!volumeInfo) {
          nextCurrentDirEntry = null;
          callback();
          return;
        }
        volumeInfo.resolveDisplayRoot().then(
            function(entry) {
              nextCurrentDirEntry = entry;
              callback();
            }).catch(function(error) {
              console.error(error.stack || error);
              nextCurrentDirEntry = null;
              callback();
            });
      } else {
        callback();
      }
    }.bind(this));

    // If the directory to be changed to is still not resolved, then fallback
    // to the default display root.
    queue.run(function(callback) {
      if (nextCurrentDirEntry) {
        callback();
        return;
      }
      this.volumeManager_.getDefaultDisplayRoot(function(displayRoot) {
        nextCurrentDirEntry = displayRoot;
        callback();
      }.bind(this));
    }.bind(this));

    // If selection failed to be resolved (eg. didn't exist, in case of saving
    // a file, or in case of a fallback of the current directory, then try to
    // resolve again using the target name.
    queue.run(function(callback) {
      if (selectionEntry || !nextCurrentDirEntry || !this.initTargetName_) {
        callback();
        return;
      }
      // Try to resolve as a file first. If it fails, then as a directory.
      nextCurrentDirEntry.getFile(
          this.initTargetName_,
          {},
          function(targetEntry) {
            selectionEntry = targetEntry;
            callback();
          }, function() {
            // Failed to resolve as a file
            nextCurrentDirEntry.getDirectory(
                this.initTargetName_,
                {},
                function(targetEntry) {
                  selectionEntry = targetEntry;
                  callback();
                }, function() {
                  // Failed to resolve as either file or directory.
                  callback();
                });
          }.bind(this));
    }.bind(this));

    // Finalize.
    queue.run(function(callback) {
      // Check directory change.
      tracker.stop();
      if (tracker.hasChanged) {
        callback();
        return;
      }
      // Finish setup current directory.
      this.finishSetupCurrentDirectory_(
          nextCurrentDirEntry,
          selectionEntry,
          this.initTargetName_);
      callback();
    }.bind(this));
  };

  /**
   * @param {DirectoryEntry} directoryEntry Directory to be opened.
   * @param {Entry=} opt_selectionEntry Entry to be selected.
   * @param {string=} opt_suggestedName Suggested name for a non-existing\
   *     selection.
   * @private
   */
  FileManager.prototype.finishSetupCurrentDirectory_ = function(
      directoryEntry, opt_selectionEntry, opt_suggestedName) {
    // Open the directory, and select the selection (if passed).
    if (util.isFakeEntry(directoryEntry)) {
      this.directoryModel_.specialSearch(directoryEntry, '');
    } else {
      this.directoryModel_.changeDirectoryEntry(directoryEntry, function() {
        if (opt_selectionEntry)
          this.directoryModel_.selectEntry(opt_selectionEntry);
      }.bind(this));
    }

    if (this.dialogType === DialogType.FULL_PAGE) {
      // In the FULL_PAGE mode if the restored URL points to a file we might
      // have to invoke a task after selecting it.
      if (this.params_.action === 'select')
        return;

      var task = null;

      // TODO(mtomasz): Implement remounting archives after crash.
      //                See: crbug.com/333139

      // If there is a task to be run, run it after the scan is completed.
      if (task) {
        var listener = function() {
          if (!util.isSameEntry(this.directoryModel_.getCurrentDirEntry(),
                                directoryEntry)) {
            // Opened on a different URL. Probably fallbacked. Therefore,
            // do not invoke a task.
            return;
          }
          this.directoryModel_.removeEventListener(
              'scan-completed', listener);
          task();
        }.bind(this);
        this.directoryModel_.addEventListener('scan-completed', listener);
      }
    } else if (this.dialogType === DialogType.SELECT_SAVEAS_FILE) {
      this.ui_.dialogFooter.filenameInput.value = opt_suggestedName || '';
      this.selectTargetNameInFilenameInput_();
    }
  };

  /**
   * @private
   */
  FileManager.prototype.refreshCurrentDirectoryMetadata_ = function() {
    var entries = this.directoryModel_.getFileList().slice();
    var directoryEntry = this.directoryModel_.getCurrentDirEntry();
    if (!directoryEntry)
      return;
    // We don't pass callback here. When new metadata arrives, we have an
    // observer registered to update the UI.

    // TODO(dgozman): refresh content metadata only when modificationTime
    // changed.
    var isFakeEntry = util.isFakeEntry(directoryEntry);
    var getEntries = (isFakeEntry ? [] : [directoryEntry]).concat(entries);
    if (!isFakeEntry)
      this.metadataCache_.clearRecursively(directoryEntry, '*');
    this.metadataCache_.get(getEntries, 'filesystem|external', null);

    var visibleItems = this.ui.listContainer.currentList.items;
    var visibleEntries = [];
    for (var i = 0; i < visibleItems.length; i++) {
      var index = this.ui.listContainer.currentList.getIndexOfListItem(
          visibleItems[i]);
      var entry = this.directoryModel_.getFileList().item(index);
      // The following check is a workaround for the bug in list: sometimes item
      // does not have listIndex, and therefore is not found in the list.
      if (entry) visibleEntries.push(entry);
    }
    // Refreshes the metadata.
    this.metadataCache_.getLatest(visibleEntries, 'thumbnail', null);
  };

  /**
   * @private
   */
  FileManager.prototype.dailyUpdateModificationTime_ = function() {
    var entries = this.directoryModel_.getFileList().slice();
    this.metadataCache_.get(
        entries,
        'filesystem',
        function() {
          this.ui_.listContainer.currentView.updateListItemsMetadata(
              'filesystem', entries);
        }.bind(this));

    setTimeout(this.dailyUpdateModificationTime_.bind(this),
               MILLISECONDS_IN_DAY);
  };

  /**
   * TODO(mtomasz): Move this to a utility function working on the root type.
   * @return {boolean} True if the current directory content is from Google
   *     Drive.
   */
  FileManager.prototype.isOnDrive = function() {
    var rootType = this.directoryModel_.getCurrentRootType();
    return rootType != null &&
        VolumeManagerCommon.getVolumeTypeFromRootType(rootType) ==
            VolumeManagerCommon.VolumeType.DRIVE;
  };

  /**
   * Check if the drive-related setting items should be shown on currently
   * displayed gear menu.
   * @return {boolean} True if those setting items should be shown.
   */
  FileManager.prototype.shouldShowDriveSettings = function() {
    return this.isOnDrive() && this.isSecretGearMenuShown_;
  };

  /**
   * Overrides default handling for clicks on hyperlinks.
   * In a packaged apps links with targer='_blank' open in a new tab by
   * default, other links do not open at all.
   *
   * @param {Event} event Click event.
   * @private
   */
  FileManager.prototype.onExternalLinkClick_ = function(event) {
    if (event.target.tagName != 'A' || !event.target.href)
      return;

    if (this.dialogType != DialogType.FULL_PAGE)
      this.ui_.dialogFooter.cancelButton.click();
  };

  /**
   * Task combobox handler.
   *
   * @param {Object} event Event containing task which was clicked.
   * @private
   */
  FileManager.prototype.onTaskItemClicked_ = function(event) {
    var selection = this.getSelection();
    if (!selection.tasks) return;

    if (event.item.task) {
      // Task field doesn't exist on change-default dropdown item.
      selection.tasks.execute(event.item.task.taskId);
    } else {
      var extensions = [];

      for (var i = 0; i < selection.entries.length; i++) {
        var match = /\.(\w+)$/g.exec(selection.entries[i].toURL());
        if (match) {
          var ext = match[1].toUpperCase();
          if (extensions.indexOf(ext) == -1) {
            extensions.push(ext);
          }
        }
      }

      var format = '';

      if (extensions.length == 1) {
        format = extensions[0];
      }

      // Change default was clicked. We should open "change default" dialog.
      selection.tasks.showTaskPicker(this.defaultTaskPicker,
          loadTimeData.getString('CHANGE_DEFAULT_MENU_ITEM'),
          strf('CHANGE_DEFAULT_CAPTION', format),
          this.onDefaultTaskDone_.bind(this),
          true);
    }
  };

  /**
   * Sets the given task as default, when this task is applicable.
   *
   * @param {Object} task Task to set as default.
   * @private
   */
  FileManager.prototype.onDefaultTaskDone_ = function(task) {
    // TODO(dgozman): move this method closer to tasks.
    var selection = this.getSelection();
    // TODO(mtomasz): Move conversion from entry to url to custom bindings.
    // crbug.com/345527.
    chrome.fileManagerPrivate.setDefaultTask(
        task.taskId,
        util.entriesToURLs(selection.entries),
        selection.mimeTypes);
    selection.tasks = new FileTasks(this);
    selection.tasks.init(selection.entries, selection.mimeTypes);
    selection.tasks.display(this.taskItems_);
    this.refreshCurrentDirectoryMetadata_();
    this.selectionHandler_.onFileSelectionChanged();
  };

  /**
   * @private
   */
  FileManager.prototype.onPreferencesChanged_ = function() {
    var self = this;
    this.getPreferences_(function(prefs) {
      self.initDateTimeFormatters_();
      self.refreshCurrentDirectoryMetadata_();

      if (prefs.cellularDisabled)
        self.syncButton.setAttribute('checked', '');
      else
        self.syncButton.removeAttribute('checked');

      if (self.hostedButton.hasAttribute('checked') ===
          prefs.hostedFilesDisabled && self.isOnDrive()) {
        self.directoryModel_.rescan(false);
      }

      if (!prefs.hostedFilesDisabled)
        self.hostedButton.setAttribute('checked', '');
      else
        self.hostedButton.removeAttribute('checked');
    },
    true /* refresh */);
  };

  FileManager.prototype.onDriveConnectionChanged_ = function() {
    var connection = this.volumeManager_.getDriveConnectionState();
    if (this.commandHandler)
      this.commandHandler.updateAvailability();
    if (this.dialogContainer_)
      this.dialogContainer_.setAttribute('connection', connection.type);
    this.shareDialog_.hideWithResult(ShareDialog.Result.NETWORK_ERROR);
    this.suggestAppsDialog.onDriveConnectionChanged(connection.type);
  };

  /**
   * Tells whether the current directory is read only.
   * TODO(mtomasz): Remove and use EntryLocation directly.
   * @return {boolean} True if read only, false otherwise.
   */
  FileManager.prototype.isOnReadonlyDirectory = function() {
    return this.directoryModel_.isReadOnly();
  };

  /**
   * @param {Event} event Unmount event.
   * @private
   */
  FileManager.prototype.onExternallyUnmounted_ = function(event) {
    if (event.volumeInfo === this.currentVolumeInfo_) {
      if (this.closeOnUnmount_) {
        // If the file manager opened automatically when a usb drive inserted,
        // user have never changed current volume (that implies the current
        // directory is still on the device) then close this window.
        window.close();
      }
    }
  };

  /**
   * @return {Array.<Entry>} List of all entries in the current directory.
   */
  FileManager.prototype.getAllEntriesInCurrentDirectory = function() {
    return this.directoryModel_.getFileList().slice();
  };

  /**
   * Return DirectoryEntry of the current directory or null.
   * @return {DirectoryEntry} DirectoryEntry of the current directory. Returns
   *     null if the directory model is not ready or the current directory is
   *     not set.
   */
  FileManager.prototype.getCurrentDirectoryEntry = function() {
    return this.directoryModel_ && this.directoryModel_.getCurrentDirEntry();
  };

  /**
   * Shows the share dialog for the selected file or directory.
   */
  FileManager.prototype.shareSelection = function() {
    var entries = this.getSelection().entries;
    if (entries.length != 1) {
      console.warn('Unable to share multiple items at once.');
      return;
    }
    // Add the overlapped class to prevent the applicaiton window from
    // captureing mouse events.
    this.shareDialog_.show(entries[0], function(result) {
      if (result == ShareDialog.Result.NETWORK_ERROR)
        this.error.show(str('SHARE_ERROR'));
    }.bind(this));
  };

  /**
   * Creates a folder shortcut.
   * @param {Entry} entry A shortcut which refers to |entry| to be created.
   */
  FileManager.prototype.createFolderShortcut = function(entry) {
    // Duplicate entry.
    if (this.folderShortcutExists(entry))
      return;

    this.folderShortcutsModel_.add(entry);
  };

  /**
   * Checkes if the shortcut which refers to the given folder exists or not.
   * @param {Entry} entry Entry of the folder to be checked.
   */
  FileManager.prototype.folderShortcutExists = function(entry) {
    return this.folderShortcutsModel_.exists(entry);
  };

  /**
   * Removes the folder shortcut.
   * @param {Entry} entry The shortcut which refers to |entry| is to be removed.
   */
  FileManager.prototype.removeFolderShortcut = function(entry) {
    this.folderShortcutsModel_.remove(entry);
  };

  /**
   * Blinks the selection. Used to give feedback when copying or cutting the
   * selection.
   */
  FileManager.prototype.blinkSelection = function() {
    var selection = this.getSelection();
    if (!selection || selection.totalCount == 0)
      return;

    for (var i = 0; i < selection.entries.length; i++) {
      var selectedIndex = selection.indexes[i];
      var listItem =
          this.ui.listContainer.currentList.getListItemByIndex(selectedIndex);
      if (listItem)
        this.blinkListItem_(listItem);
    }
  };

  /**
   * @param {Element} listItem List item element.
   * @private
   */
  FileManager.prototype.blinkListItem_ = function(listItem) {
    listItem.classList.add('blink');
    setTimeout(function() {
      listItem.classList.remove('blink');
    }, 100);
  };

  /**
   * @private
   */
  FileManager.prototype.selectTargetNameInFilenameInput_ = function() {
    var input = this.ui_.dialogFooter.filenameInput;
    input.focus();
    var selectionEnd = input.value.lastIndexOf('.');
    if (selectionEnd == -1) {
      input.select();
    } else {
      input.selectionStart = 0;
      input.selectionEnd = selectionEnd;
    }
  };

  /**
   * Handles mouse click or tap.
   *
   * @param {Event} event The click event.
   * @private
   */
  FileManager.prototype.onDetailClick_ = function(event) {
    if (this.namingController_.isRenamingInProgress()) {
      // Don't pay attention to clicks during a rename.
      return;
    }

    var listItem = this.ui_.listContainer.findListItemForNode(
        event.touchedElement || event.srcElement);
    var selection = this.getSelection();
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
      this.onDirectoryAction_(entry);
    } else {
      this.dispatchSelectionAction_();
    }
  };

  /**
   * @private
   */
  FileManager.prototype.dispatchSelectionAction_ = function() {
    if (this.dialogType == DialogType.FULL_PAGE) {
      var selection = this.getSelection();
      var tasks = selection.tasks;
      var mimeTypes = selection.mimeTypes;
      if (tasks)
        tasks.executeDefault();
      return true;
    }
    if (!this.ui_.dialogFooter.okButton.disabled) {
      this.ui_.dialogFooter.okButton.click();
      return true;
    }
    return false;
  };

  /**
   * Handles activate event of action menu item.
   *
   * @private
   */
  FileManager.prototype.onActionMenuItemActivated_ = function() {
    var tasks = this.getSelection().tasks;
    if (tasks)
      tasks.execute(this.actionMenuItem_.taskId);
  };

  /**
   * Opens the suggest file dialog.
   *
   * @param {Entry} entry Entry of the file.
   * @param {function()} onSuccess Success callback.
   * @param {function()} onCancelled User-cancelled callback.
   * @param {function()} onFailure Failure callback.
   * @private
   */
  FileManager.prototype.openSuggestAppsDialog =
      function(entry, onSuccess, onCancelled, onFailure) {
    if (!url) {
      onFailure();
      return;
    }

    this.metadataCache_.getOne(entry, 'external', function(prop) {
      if (!prop || !prop.contentMimeType) {
        onFailure();
        return;
      }

      var basename = entry.name;
      var splitted = util.splitExtension(basename);
      var filename = splitted[0];
      var extension = splitted[1];
      var mime = prop.contentMimeType;

      // Returns with failure if the file has neither extension nor mime.
      if (!extension || !mime) {
        onFailure();
        return;
      }

      var onDialogClosed = function(result) {
        switch (result) {
          case SuggestAppsDialog.Result.INSTALL_SUCCESSFUL:
            onSuccess();
            break;
          case SuggestAppsDialog.Result.FAILED:
            onFailure();
            break;
          default:
            onCancelled();
        }
      };

      if (FileTasks.EXECUTABLE_EXTENSIONS.indexOf(extension) !== -1) {
        this.suggestAppsDialog.showByFilename(filename, onDialogClosed);
      } else {
        this.suggestAppsDialog.showByExtensionAndMime(
            extension, mime, onDialogClosed);
      }
    }.bind(this));
  };

  /**
   * Called when a dialog is shown or hidden.
   * @param {boolean} show True if a dialog is shown, false if hidden.
   */
  FileManager.prototype.onDialogShownOrHidden = function(show) {
    if (show) {
      // If a dialog is shown, activate the window.
      var appWindow = chrome.app.window.current();
      if (appWindow)
        appWindow.focus();
    }

    // Set/unset a flag to disable dragging on the title area.
    this.dialogContainer_.classList.toggle('disable-header-drag', show);
  };

  /**
   * Executes directory action (i.e. changes directory).
   *
   * @param {DirectoryEntry} entry Directory entry to which directory should be
   *                               changed.
   * @private
   */
  FileManager.prototype.onDirectoryAction_ = function(entry) {
    return this.directoryModel_.changeDirectoryEntry(entry);
  };

  /**
   * Update the window title.
   * @private
   */
  FileManager.prototype.updateTitle_ = function() {
    if (this.dialogType != DialogType.FULL_PAGE)
      return;

    if (!this.currentVolumeInfo_)
      return;

    this.document_.title = this.currentVolumeInfo_.label;
  };

  /**
   * Update the gear menu.
   * @private
   */
  FileManager.prototype.updateGearMenu_ = function() {
    this.refreshRemainingSpace_(true);  // Show loading caption.
  };

  /**
   * Refreshes space info of the current volume.
   * @param {boolean} showLoadingCaption Whether show loading caption or not.
   * @private
   */
  FileManager.prototype.refreshRemainingSpace_ = function(showLoadingCaption) {
    if (!this.currentVolumeInfo_)
      return;

    var volumeSpaceInfo = /** @type {!HTMLElement} */
        (this.dialogDom_.querySelector('#volume-space-info'));
    var volumeSpaceInfoSeparator = /** @type {!HTMLElement} */
        (this.dialogDom_.querySelector('#volume-space-info-separator'));
    var volumeSpaceInfoLabel = /** @type {!HTMLElement} */
        (this.dialogDom_.querySelector('#volume-space-info-label'));
    var volumeSpaceInnerBar = /** @type {!HTMLElement} */
        (this.dialogDom_.querySelector('#volume-space-info-bar'));
    var volumeSpaceOuterBar = /** @type {!HTMLElement} */
        (this.dialogDom_.querySelector('#volume-space-info-bar').parentNode);

    var currentVolumeInfo = this.currentVolumeInfo_;

    // TODO(mtomasz): Add support for remaining space indication for provided
    // file systems.
    if (currentVolumeInfo.volumeType ==
        VolumeManagerCommon.VolumeType.PROVIDED) {
      volumeSpaceInfo.hidden = true;
      volumeSpaceInfoSeparator.hidden = true;
      return;
    }

    volumeSpaceInfo.hidden = false;
    volumeSpaceInfoSeparator.hidden = false;
    volumeSpaceInnerBar.setAttribute('pending', '');

    if (showLoadingCaption) {
      volumeSpaceInfoLabel.innerText = str('WAITING_FOR_SPACE_INFO');
      volumeSpaceInnerBar.style.width = '100%';
    }

    chrome.fileManagerPrivate.getSizeStats(
        currentVolumeInfo.volumeId, function(result) {
          var volumeInfo = this.volumeManager_.getVolumeInfo(
              this.directoryModel_.getCurrentDirEntry());
          if (currentVolumeInfo !== this.currentVolumeInfo_)
            return;
          updateSpaceInfo(result,
                          volumeSpaceInnerBar,
                          volumeSpaceInfoLabel,
                          volumeSpaceOuterBar);
        }.bind(this));
  };

  /**
   * Update the UI when the current directory changes.
   *
   * @param {Event} event The directory-changed event.
   * @private
   */
  FileManager.prototype.onDirectoryChanged_ = function(event) {
    var oldCurrentVolumeInfo = this.currentVolumeInfo_;

    // Remember the current volume info.
    this.currentVolumeInfo_ = this.volumeManager_.getVolumeInfo(
        event.newDirEntry);

    // If volume has changed, then update the gear menu.
    if (oldCurrentVolumeInfo !== this.currentVolumeInfo_) {
      this.updateGearMenu_();
      // If the volume has changed, and it was previously set, then do not
      // close on unmount anymore.
      if (oldCurrentVolumeInfo)
        this.closeOnUnmount_ = false;
    }

    this.selectionHandler_.onFileSelectionChanged();
    this.searchController_.clear();
    // TODO(mtomasz): Consider remembering the selection.
    util.updateAppState(
        this.getCurrentDirectoryEntry() ?
        this.getCurrentDirectoryEntry().toURL() : '',
        '' /* selectionURL */,
        '' /* opt_param */);

    if (this.commandHandler)
      this.commandHandler.updateAvailability();

    this.updateUnformattedVolumeStatus_();
    this.updateTitle_();

    var currentEntry = this.getCurrentDirectoryEntry();
    this.ui_.locationLine.show(currentEntry);
    this.ui_.previewPanel.currentEntry = util.isFakeEntry(currentEntry) ?
        null : currentEntry;
  };

  FileManager.prototype.updateUnformattedVolumeStatus_ = function() {
    var volumeInfo = this.volumeManager_.getVolumeInfo(
        this.directoryModel_.getCurrentDirEntry());

    if (volumeInfo && volumeInfo.error) {
      this.dialogDom_.setAttribute('unformatted', '');

      var errorNode = this.dialogDom_.querySelector('#format-panel > .error');
      if (volumeInfo.error ===
          VolumeManagerCommon.VolumeError.UNSUPPORTED_FILESYSTEM) {
        errorNode.textContent = str('UNSUPPORTED_FILESYSTEM_WARNING');
      } else {
        errorNode.textContent = str('UNKNOWN_FILESYSTEM_WARNING');
      }

      // Update 'canExecute' for format command so the format button's disabled
      // property is properly set.
      if (this.commandHandler)
        this.commandHandler.updateAvailability();
    } else {
      this.dialogDom_.removeAttribute('unformatted');
    }
  };

  /**
   * Unload handler for the page.
   * @private
   */
  FileManager.prototype.onUnload_ = function() {
    if (this.directoryModel_)
      this.directoryModel_.dispose();
    if (this.volumeManager_)
      this.volumeManager_.dispose();
    if (this.fileTransferController_) {
      for (var i = 0;
           i < this.fileTransferController_.pendingTaskIds.length;
           i++) {
        var taskId = this.fileTransferController_.pendingTaskIds[i];
        var item =
            this.backgroundPage_.background.progressCenter.getItemById(taskId);
        item.message = '';
        item.state = ProgressItemState.CANCELED;
        this.backgroundPage_.background.progressCenter.updateItem(item);
      }
    }
    if (this.progressCenterPanel_) {
      this.backgroundPage_.background.progressCenter.removePanel(
          this.progressCenterPanel_);
    }
    if (this.fileOperationManager_) {
      if (this.onCopyProgressBound_) {
        this.fileOperationManager_.removeEventListener(
            'copy-progress', this.onCopyProgressBound_);
      }
      if (this.onEntriesChangedBound_) {
        this.fileOperationManager_.removeEventListener(
            'entries-changed', this.onEntriesChangedBound_);
      }
    }
    window.closing = true;
    if (this.backgroundPage_)
      this.backgroundPage_.background.tryClose();
  };

  /**
   * @private
   */
  FileManager.prototype.onFilenameInputInput_ = function() {
    this.selectionHandler_.updateOkButton();
  };

  /**
   * @param {Event} event Key event.
   * @private
   */
  FileManager.prototype.onFilenameInputKeyDown_ = function(event) {
    if ((util.getKeyModifiers(event) + event.keyCode) === '13' /* Enter */)
      this.ui_.dialogFooter.okButton.click();
  };

  /**
   * @param {Event} event Focus event.
   * @private
   */
  FileManager.prototype.onFilenameInputFocus_ = function(event) {
    var input = this.ui_.dialogFooter.filenameInput;

    // On focus we want to select everything but the extension, but
    // Chrome will select-all after the focus event completes.  We
    // schedule a timeout to alter the focus after that happens.
    setTimeout(function() {
      var selectionEnd = input.value.lastIndexOf('.');
      if (selectionEnd == -1) {
        input.select();
      } else {
        input.selectionStart = 0;
        input.selectionEnd = selectionEnd;
      }
    }, 0);
  };

  FileManager.prototype.createNewFolder = function() {
    var defaultName = str('DEFAULT_NEW_FOLDER_NAME');

    // Find a name that doesn't exist in the data model.
    var files = this.directoryModel_.getFileList();
    var hash = {};
    for (var i = 0; i < files.length; i++) {
      var name = files.item(i).name;
      // Filtering names prevents from conflicts with prototype's names
      // and '__proto__'.
      if (name.substring(0, defaultName.length) == defaultName)
        hash[name] = 1;
    }

    var baseName = defaultName;
    var separator = '';
    var suffix = '';
    var index = '';

    var advance = function() {
      separator = ' (';
      suffix = ')';
      index++;
    };

    var current = function() {
      return baseName + separator + index + suffix;
    };

    // Accessing hasOwnProperty is safe since hash properties filtered.
    while (hash.hasOwnProperty(current())) {
      advance();
    }

    var self = this;
    var list = self.ui_.listContainer.currentList;

    var onSuccess = function(entry) {
      metrics.recordUserAction('CreateNewFolder');
      list.selectedItem = entry;

      self.ui_.listContainer.endBatchUpdates();

      self.namingController_.initiateRename();
    };

    var onError = function(error) {
      self.ui_.listContainer.endBatchUpdates();

      self.alert.show(strf('ERROR_CREATING_FOLDER', current(),
                           util.getFileErrorString(error.name)),
                      null, null);
    };

    var onAbort = function() {
      self.ui_.listContainer.endBatchUpdates();
    };

    this.ui_.listContainer.startBatchUpdates();
    this.directoryModel_.createDirectory(current(),
                                         onSuccess,
                                         onError,
                                         onAbort);
  };

  /**
   * Handles click event on the toggle-view button.
   * @param {Event} event Click event.
   * @private
   */
  FileManager.prototype.onToggleViewButtonClick_ = function(event) {
    if (this.ui_.listContainer.currentListType ===
        ListContainer.ListType.DETAIL) {
      this.setListType(ListContainer.ListType.THUMBNAIL);
    } else {
      this.setListType(ListContainer.ListType.DETAIL);
    }

    event.target.blur();
  };

  /**
   * KeyDown event handler for the document.
   * @param {Event} event Key event.
   * @private
   */
  FileManager.prototype.onKeyDown_ = function(event) {
    if (event.keyCode === 9)  // Tab
      this.pressingTab_ = true;
    if (event.keyCode === 17)  // Ctrl
      this.pressingCtrl_ = true;

    if (event.srcElement === this.ui_.listContainer.renameInput) {
      // Ignore keydown handler in the rename input box.
      return;
    }

    switch (util.getKeyModifiers(event) + event.keyIdentifier) {
      case 'Ctrl-U+00BE':  // Ctrl-. => Toggle filter files.
        this.fileFilter_.setFilterHidden(
            !this.fileFilter_.isFilterHiddenOn());
        event.preventDefault();
        return;

      case 'U+001B':  // Escape => Cancel dialog.
        if (this.dialogType != DialogType.FULL_PAGE) {
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
  FileManager.prototype.onKeyUp_ = function(event) {
    if (event.keyCode === 9)  // Tab
      this.pressingTab_ = false;
    if (event.keyCode == 17)  // Ctrl
      this.pressingCtrl_ = false;
  };

  /**
   * KeyDown event handler for the div#list-container element.
   * @param {Event} event Key event.
   * @private
   */
  FileManager.prototype.onListKeyDown_ = function(event) {
    switch (util.getKeyModifiers(event) + event.keyIdentifier) {
      case 'U+0008':  // Backspace => Up one directory.
        event.preventDefault();
        // TODO(mtomasz): Use Entry.getParent() instead.
        if (!this.getCurrentDirectoryEntry())
          break;
        var currentEntry = this.getCurrentDirectoryEntry();
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
        // TODO(dgozman): move directory action to dispatchSelectionAction.
        var selection = this.getSelection();
        if (selection.totalCount === 1 &&
            selection.entries[0].isDirectory &&
            !DialogType.isFolderDialog(this.dialogType)) {
          var item = this.ui.listContainer.currentList.getListItemByIndex(
              selection.indexes[0]);
          // If the item is in renaming process, we don't allow to change
          // directory.
          if (!item.hasAttribute('renaming')) {
            event.preventDefault();
            this.onDirectoryAction_(selection.entries[0]);
          }
        } else if (this.dispatchSelectionAction_()) {
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
  FileManager.prototype.onTextSearch_ = function() {
    var text = this.ui_.listContainer.textSearchState.text;
    var dm = this.directoryModel_.getFileList();
    for (var index = 0; index < dm.length; ++index) {
      var name = dm.item(index).name;
      if (name.substring(0, text.length).toLowerCase() == text) {
        this.ui.listContainer.currentList.selectionModel.selectedIndexes =
            [index];
        return;
      }
    }

    this.ui_.listContainer.textSearchState.text = '';
  };

  /**
   * Verifies the user entered name for file or folder to be created or
   * renamed to. See also util.validateFileName.
   *
   * @param {DirectoryEntry} parentEntry The URL of the parent directory entry.
   * @param {string} name New file or folder name.
   * @param {function(boolean)} onDone Function to invoke when user closes the
   *    warning box or immediatelly if file name is correct. If the name was
   *    valid it is passed true, and false otherwise.
   * @private
   */
  FileManager.prototype.validateFileName_ = function(
      parentEntry, name, onDone) {
    var fileNameErrorPromise = util.validateFileName(
        parentEntry,
        name,
        this.fileFilter_.isFilterHiddenOn());
    fileNameErrorPromise.then(onDone.bind(null, true), function(message) {
      this.alert.show(message, onDone.bind(null, false));
    }.bind(this)).catch(function(error) {
      console.error(error.stack || error);
    });
  };

  /**
   * Toggle whether mobile data is used for sync.
   */
  FileManager.prototype.toggleDriveSyncSettings = function() {
    // If checked, the sync is disabled.
    var nowCellularDisabled = this.syncButton.hasAttribute('checked');
    var changeInfo = {cellularDisabled: !nowCellularDisabled};
    chrome.fileManagerPrivate.setPreferences(changeInfo);
  };

  /**
   * Toggle whether Google Docs files are shown.
   */
  FileManager.prototype.toggleDriveHostedSettings = function() {
    // If checked, showing drive hosted files is enabled.
    var nowHostedFilesEnabled = this.hostedButton.hasAttribute('checked');
    var nowHostedFilesDisabled = !nowHostedFilesEnabled;
    /*
    var changeInfo = {hostedFilesDisabled: !nowHostedFilesDisabled};
    */
    var changeInfo = {};
    changeInfo['hostedFilesDisabled'] = !nowHostedFilesDisabled;
    chrome.fileManagerPrivate.setPreferences(changeInfo);
  };

  FileManager.prototype.decorateSplitter = function(splitterElement) {
    var self = this;

    var Splitter = cr.ui.Splitter;

    var customSplitter = cr.ui.define('div');

    customSplitter.prototype = {
      __proto__: Splitter.prototype,

      handleSplitterDragStart: function(e) {
        Splitter.prototype.handleSplitterDragStart.apply(this, arguments);
        this.ownerDocument.documentElement.classList.add('col-resize');
      },

      handleSplitterDragMove: function(deltaX) {
        Splitter.prototype.handleSplitterDragMove.apply(this, arguments);
        self.onResize_();
      },

      handleSplitterDragEnd: function(e) {
        Splitter.prototype.handleSplitterDragEnd.apply(this, arguments);
        this.ownerDocument.documentElement.classList.remove('col-resize');
      }
    };

    customSplitter.decorate(splitterElement);
  };

  /**
   * Updates action menu item to match passed task items.
   *
   * @param {Array.<Object>=} opt_items List of items.
   */
  FileManager.prototype.updateContextMenuActionItems = function(opt_items) {
    var items = opt_items || [];

    // When only one task is available, show it as default item.
    if (items.length === 1) {
      var actionItem = items[0];

      if (actionItem.iconType) {
        this.actionMenuItem_.style.backgroundImage = '';
        this.actionMenuItem_.setAttribute('file-type-icon',
                                          actionItem.iconType);
      } else if (actionItem.iconUrl) {
        this.actionMenuItem_.style.backgroundImage =
            'url(' + actionItem.iconUrl + ')';
      } else {
        this.actionMenuItem_.style.backgroundImage = '';
      }

      this.actionMenuItem_.label =
          actionItem.taskId === FileTasks.ZIP_UNPACKER_TASK_ID ?
          str('ACTION_OPEN') : actionItem.title;
      this.actionMenuItem_.disabled = !!actionItem.disabled;
      this.actionMenuItem_.taskId = actionItem.taskId;
    }

    this.actionMenuItem_.hidden = items.length !== 1;

    // When multiple tasks are available, show them in open with.
    this.openWithCommand_.canExecuteChange();
    this.openWithCommand_.setHidden(items.length < 2);
    this.openWithCommand_.disabled = items.length < 2;

    // Hide default action separator when there does not exist available task.
    var defaultActionSeparator =
        this.dialogDom_.querySelector('#default-action-separator');
    defaultActionSeparator.hidden = items.length === 0;
  };

  /**
   * @return {FileSelection} Selection object.
   */
  FileManager.prototype.getSelection = function() {
    return this.selectionHandler_.selection;
  };

  /**
   * @return {cr.ui.ArrayDataModel} File list.
   */
  FileManager.prototype.getFileList = function() {
    return this.directoryModel_.getFileList();
  };

  /**
   * @return {!cr.ui.List} Current list object.
   */
  FileManager.prototype.getCurrentList = function() {
    return this.ui.listContainer.currentList;
  };

  /**
   * Retrieve the preferences of the files.app. This method caches the result
   * and returns it unless opt_update is true.
   * @param {function(Object.<string, *>)} callback Callback to get the
   *     preference.
   * @param {boolean=} opt_update If is's true, don't use the cache and
   *     retrieve latest preference. Default is false.
   * @private
   */
  FileManager.prototype.getPreferences_ = function(callback, opt_update) {
    if (!opt_update && this.preferences_ !== null) {
      callback(this.preferences_);
      return;
    }

    chrome.fileManagerPrivate.getPreferences(function(prefs) {
      this.preferences_ = prefs;
      callback(prefs);
    }.bind(this));
  };

  /**
   * @param {FileEntry} entry
   * @private
   */
  FileManager.prototype.doEntryAction_ = function(entry) {
    if (this.dialogType == DialogType.FULL_PAGE) {
      this.metadataCache_.get([entry], 'external', function(props) {
        var tasks = new FileTasks(this);
        tasks.init([entry], [props[0].contentMimeType || '']);
        tasks.executeDefault_();
      }.bind(this));
    } else {
      var selection = this.getSelection();
      if (selection.entries.length === 1 &&
          util.isSameEntry(selection.entries[0], entry)) {
        this.ui_.dialogFooter.okButton.click();
      }
    }
  };

  /**
   * Outputs the current state for debugging.
   */
  FileManager.prototype.debugMe = function() {
    var out = 'Debug information.\n' +
        '1. fileManager.initializeQueue_.pendingTasks_\n';
    var keys = Object.keys(this.initializeQueue_.pendingTasks);
    out += 'Length: ' + keys.length + '\n';
    keys.forEach(function(key) {
      out += this.initializeQueue_.pendingTasks[key].toString() + '\n';
    }.bind(this));

    out += '2. VolumeManagerWrapper\n' +
        this.volumeManager_.toString() + '\n';

    out += 'End of debug information.';
    console.log(out);
  };
})();
