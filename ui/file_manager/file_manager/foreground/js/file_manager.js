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
   * History loader. Gimme summa 'dat history!
   * @type {importer.HistoryLoader}
   * @private
   */
  this.historyLoader_ = null;

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
   * UI management class of file manager.
   * @type {FileManagerUI}
   * @private
   */
  this.ui_ = null;

  // --------------------------------------------------------------------------
  // Parameters determining the type of file manager.

  /**
   * Dialog type of this window.
   * @type {DialogType}
   */
  this.dialogType = DialogType.FULL_PAGE;

  /**
   * Startup parameters for this application.
   * @type {LaunchParam}
   * @private
   */
  this.launchParams_ = null;

  // --------------------------------------------------------------------------
  // Controllers.

  /**
   * File transfer controller.
   * @type {FileTransferController}
   * @private
   */
  this.fileTransferController_ = null;

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

  /**
   * Gear menu controller.
   * @type {GearMenuController}
   * @private
   */
  this.gearMenuController_ = null;

  /**
   * App state controller.
   * @type {AppStateController}
   * @private
   */
  this.appStateController_ = null;

  /**
   * Dialog action controller.
   * @type {DialogActionController}
   * @private
   */
  this.dialogActionController_ = null;

  /**
   * List update controller.
   * @type {MetadataUpdateController}
   * @private
   */
  this.metadataUpdateController_ = null;

  /**
   * Component for main window and its misc UI parts.
   * @type {MainWindowComponent}
   * @private
   */
  this.mainWindowComponent_ = null;

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
   * The menu item for doing an action.
   * @type {HTMLMenuItemElement}
   * @private
   */
  this.actionMenuItem_ = null;

  /**
   * Open-with command in the context menu.
   * @type {cr.ui.Command}
   * @private
   */
  this.openWithCommand_ = null;

  // --------------------------------------------------------------------------
  // Miscellaneous FileManager's states.

  /**
   * Queue for ordering FileManager's initialization process.
   * @type {!AsyncUtil.Group}
   * @private
   */
  this.initializeQueue_ = new AsyncUtil.Group();

  /**
   * Count of the SourceNotFound error.
   * @type {number}
   * @private
   */
  this.sourceNotFoundErrorCount_ = 0;

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
   * @return {FolderShortcutsDataModel}
   */
  get folderShortcutsModel() {
    return this.folderShortcutsModel_;
  },
  /**
   * @return {DirectoryTree}
   */
  get directoryTree() {
    return this.ui_.directoryTree;
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
   * @return {importer.HistoryLoader}
   */
  get historyLoader() {
    return this.historyLoader_;
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

// Anonymous "namespace".
(function() {
  FileManager.prototype.initSettings_ = function(callback) {
    this.appStateController_ = new AppStateController(this.dialogType);
    this.appStateController_.loadInitialViewOptions().then(callback);
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

    var self = this;

    // Get the 'allowRedeemOffers' preference before launching
    // FileListBannerController.
    chrome.fileManagerPrivate.getPreferences(function(pref) {
      self.bannersController_ = new FileListBannerController(
          self.directoryModel_,
          self.volumeManager_,
          self.document_,
          pref.allowRedeemOffers);
      self.bannersController_.addEventListener(
          'relayout', self.ui_.relayout.bind(self.ui_));
    });

    var listBeingUpdated = null;
    this.directoryModel_.addEventListener('begin-update-files', function() {
      self.ui_.listContainer.currentList.startBatchUpdates();
      // Remember the list which was used when updating files started, so
      // endBatchUpdates() is called on the same list.
      listBeingUpdated = self.ui_.listContainer.currentList;
    });
    this.directoryModel_.addEventListener('end-update-files', function() {
      self.namingController_.restoreItemBeingRenamed();
      listBeingUpdated.endBatchUpdates();
      listBeingUpdated = null;
    });

    this.initCommands_();

    assert(this.directoryModel_);
    assert(this.spinnerController_);
    assert(this.commandHandler);
    assert(this.selectionHandler_);
    assert(this.launchParams_);
    assert(this.volumeManager_);

    this.scanController_ = new ScanController(
        this.directoryModel_,
        this.ui_.listContainer,
        this.spinnerController_,
        this.commandHandler,
        this.selectionHandler_);
    this.gearMenuController_ = new GearMenuController(
        this.ui_.gearButton,
        this.ui_.gearMenu,
        this.directoryModel_,
        this.commandHandler);

    assert(this.fileFilter_);
    assert(this.namingController_);
    assert(this.appStateController_);
    this.mainWindowComponent_ = new MainWindowComponent(
        this.dialogType,
        this.ui_,
        this.volumeManager_,
        this.directoryModel_,
        this.fileFilter_,
        this.selectionHandler_,
        this.namingController_,
        this.appStateController_,
        {dispatchSelectionAction: this.dispatchSelectionAction_.bind(this)});

    this.initDataTransferOperations_();

    this.selectionHandler_.onFileSelectionChanged();
    this.ui_.listContainer.endBatchUpdates();

    callback();
  };

  /**
   * @private
   */
  FileManager.prototype.initDataTransferOperations_ = function() {
    // CopyManager are required for 'Delete' operation in
    // Open and Save dialogs. But drag-n-drop and copy-paste are not needed.
    if (this.dialogType != DialogType.FULL_PAGE)
      return;

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
    controller.attachTreeDropTarget(this.ui_.directoryTree);
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
   * One-time initialization of commands.
   * @private
   */
  FileManager.prototype.initCommands_ = function() {
    assert(this.ui_.textContextMenu);

    this.commandHandler = new CommandHandler(this);

    // TODO(hirono): Move the following block to the UI part.
    var commandButtons = this.dialogDom_.querySelectorAll('button[command]');
    for (var j = 0; j < commandButtons.length; j++)
      CommandButton.decorate(commandButtons[j]);

    var inputs = this.dialogDom_.querySelectorAll(
        'input[type=text], input[type=search], textarea');
    for (var i = 0; i < inputs.length; i++) {
      cr.ui.contextMenuHandler.setContextMenu(
          inputs[i], this.ui_.textContextMenu);
      this.registerInputCommands_(inputs[i]);
    }

    cr.ui.contextMenuHandler.setContextMenu(this.ui_.listContainer.renameInput,
                                            this.ui_.textContextMenu);
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
    this.initializeQueue_.add(this.initSettings_.bind(this),
                              ['initGeneral'], 'initSettings');
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
        ['initAdditionalUI', 'initSettings'], 'initFileSystemUI');

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
      var params = {};
      for (var name in window.appState) {
        params[name] = window.appState[name];
      }
      for (var name in window.appState.params) {
        params[name] = window.appState.params[name];
      }
      this.launchParams_ = new LaunchParam(params);
    } else {
      // Used by the select dialog only.
      var json = location.search ?
          JSON.parse(decodeURIComponent(location.search.substr(1))) : {};
      this.launchParams_ = new LaunchParam(json instanceof Object ? json : {});
    }

    // Initialize the member variables that depend this.launchParams_.
    this.dialogType = this.launchParams_.type;
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
            this.fileOperationManager_ =
                this.backgroundPage_.background.fileOperationManager;
            this.backgroundPage_.background.historyLoaderPromise.then(
                /**
                 * @param {!importer.HistoryLoader} loader
                 * @this {FileManager}
                 */
                function(loader) {
                  this.historyLoader_ = loader;
                  callback();
                }.bind(this));
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
    var noLocalPathResolution =
        DialogType.isFolderDialog(this.launchParams_.type);

    // If this condition is false, VolumeManagerWrapper hides all drive
    // related event and data, even if Drive is enabled on preference.
    // In other words, even if Drive is disabled on preference but Files.app
    // should show Drive when it is re-enabled, then the value should be set to
    // true.
    // Note that the Drive enabling preference change is listened by
    // DriveIntegrationService, so here we don't need to take care about it.
    var driveEnabled =
        !noLocalPathResolution || !this.launchParams_.shouldReturnLocalPath;
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
    assert(this.launchParams_);
    this.ui_ = new FileManagerUI(this.dialogDom_, this.launchParams_);

    // Show the window as soon as the UI pre-initialization is done.
    if (this.dialogType == DialogType.FULL_PAGE && !util.runningInBrowser()) {
      chrome.app.window.current().show();
      setTimeout(callback, 100);  // Wait until the animation is finished.
    } else {
      callback();
    }
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
    assert(this.historyLoader_);
    assert(this.dialogDom_);

    // Cache nodes we'll be manipulating.
    var dom = this.dialogDom_;
    assert(dom);

    // Initialize the dialog.
    FileManagerDialogBase.setFileManager(this);

    var table = queryRequiredElement(dom, '.detail-table');
    // TODO(smckay): Work out the cloud import UI when in list view.
    FileTable.decorate(
        table,
        this.metadataCache_,
        this.volumeManager_,
        this.dialogType == DialogType.FULL_PAGE);
    var grid = queryRequiredElement(dom, '.thumbnail-grid');
    FileGrid.decorate(
        grid,
        this.metadataCache_,
        this.volumeManager_,
        this.historyLoader_);

    this.ui_.initAdditionalUI(
        assertInstanceof(table, FileTable),
        assertInstanceof(grid, FileGrid),
        new PreviewPanel(
            queryRequiredElement(dom, '.preview-panel'),
            DialogType.isOpenDialog(this.dialogType) ?
                PreviewPanel.VisibilityType.ALWAYS_VISIBLE :
                PreviewPanel.VisibilityType.AUTO,
            this.metadataCache_,
            this.volumeManager_,
            this.historyLoader_),
        new LocationLine(
            queryRequiredElement(dom, '#location-breadcrumbs'),
            queryRequiredElement(dom, '#location-volume-icon'),
            this.metadataCache_,
            this.volumeManager_));

    // Handle UI events.
    this.backgroundPage_.background.progressCenter.addPanel(
        this.ui_.progressCenterPanel);
    this.ui_.taskMenuButton.addEventListener('select',
        this.onTaskItemClicked_.bind(this));

    this.actionMenuItem_ = /** @type {!HTMLMenuItemElement} */
        (queryRequiredElement(this.dialogDom_, '#default-action'));

    this.openWithCommand_ = /** @type {cr.ui.Command} */
        (this.dialogDom_.querySelector('#open-with'));

    this.actionMenuItem_.addEventListener('activate',
        this.onActionMenuItemActivated_.bind(this));

    util.addIsFocusedMethod();

    // Populate the static localized strings.
    i18nTemplate.process(this.document_, loadTimeData);

    // Arrange the file list.
    this.ui_.listContainer.table.normalizeColumns();
    this.ui_.listContainer.table.redraw();

    callback();
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

    assert(this.volumeManager_);
    assert(this.fileOperationManager_);
    this.directoryModel_ = new DirectoryModel(
        singleSelection,
        this.fileFilter_,
        this.fileWatcher_,
        this.metadataCache_,
        this.volumeManager_,
        this.fileOperationManager_);

    this.folderShortcutsModel_ = new FolderShortcutsDataModel(
        this.volumeManager_);

    this.selectionHandler_ = new FileSelectionHandler(this);

    this.directoryModel_.getFileListSelection().addEventListener('change',
        this.selectionHandler_.onFileSelectionChanged.bind(
            this.selectionHandler_));

    // TODO(mtomasz, yoshiki): Create navigation list earlier, and here just
    // attach the directory model.
    this.initDirectoryTree_();

    this.ui_.listContainer.dataModel = this.directoryModel_.getFileList();
    this.ui_.listContainer.selectionModel =
        this.directoryModel_.getFileListSelection();

    this.appStateController_.initialize(this.ui_, this.directoryModel_);

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
    assert(this.launchParams_);
    this.dialogActionController_ = new DialogActionController(
        this.dialogType,
        this.ui_.dialogFooter,
        this.directoryModel_,
        this.metadataCache_,
        this.volumeManager_,
        this.fileFilter_,
        this.namingController_,
        this.selectionHandler_,
        this.launchParams_);

    // Create metadata update controller.
    this.metadataUpdateController_ = new MetadataUpdateController(
        this.ui_.listContainer,
        this.directoryModel_,
        this.volumeManager_,
        this.metadataCache_,
        this.fileWatcher_,
        this.fileOperationManager_);
  };

  /**
   * @private
   */
  FileManager.prototype.initDirectoryTree_ = function() {
    var directoryTree = /** @type {DirectoryTree} */
        (this.dialogDom_.querySelector('#directory-tree'));
    var fakeEntriesVisible =
        this.dialogType !== DialogType.SELECT_SAVEAS_FILE;
    DirectoryTree.decorate(directoryTree,
                           this.directoryModel_,
                           this.volumeManager_,
                           this.metadataCache_,
                           fakeEntriesVisible);
    directoryTree.dataModel = new NavigationListModel(
        this.volumeManager_, this.folderShortcutsModel_);

    this.ui_.initDirectoryTree(directoryTree);
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
      if (!this.launchParams_.selectionURL) {
        callback();
        return;
      }

      window.webkitResolveLocalFileSystemURL(
          this.launchParams_.selectionURL,
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
      if (nextCurrentDirEntry || !this.launchParams_.currentDirectoryURL) {
        callback();
        return;
      }

      window.webkitResolveLocalFileSystemURL(
          this.launchParams_.currentDirectoryURL,
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
      // entries are converted to directory providers. crbug.com/433161.
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
      if (selectionEntry ||
          !nextCurrentDirEntry ||
          !this.launchParams_.targetName) {
        callback();
        return;
      }
      // Try to resolve as a file first. If it fails, then as a directory.
      nextCurrentDirEntry.getFile(
          this.launchParams_.targetName,
          {},
          function(targetEntry) {
            selectionEntry = targetEntry;
            callback();
          }, function() {
            // Failed to resolve as a file
            nextCurrentDirEntry.getDirectory(
                this.launchParams_.targetName,
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
          this.launchParams_.targetName);
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
    this.directoryModel_.changeDirectoryEntry(directoryEntry, function() {
      if (opt_selectionEntry)
        this.directoryModel_.selectEntry(opt_selectionEntry);
    }.bind(this));

    if (this.dialogType === DialogType.FULL_PAGE) {
      // In the FULL_PAGE mode if the restored URL points to a file we might
      // have to invoke a task after selecting it.
      if (this.launchParams_.action === 'select')
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
      this.ui_.dialogFooter.selectTargetNameInFilenameInput();
    }
  };

  /**
   * TODO(mtomasz): Move this to a utility function working on the root type.
   * @return {boolean} True if the current directory content is from Google
   *     Drive.
   */
  FileManager.prototype.isOnDrive = function() {
    return this.directoryModel_.isOnDrive();
  };

  /**
   * Check if the drive-related setting items should be shown on currently
   * displayed gear menu.
   * @return {boolean} True if those setting items should be shown.
   */
  FileManager.prototype.shouldShowDriveSettings = function() {
    return this.isOnDrive();
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
      selection.tasks.showTaskPicker(this.ui_.defaultTaskPicker,
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
    selection.tasks.display(this.ui_.taskMenuButton);
    this.metadataUpdateController_.refreshCurrentDirectoryMetadata();
    this.selectionHandler_.onFileSelectionChanged();
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
   * Return DirectoryEntry of the current directory or null.
   * @return {DirectoryEntry} DirectoryEntry of the current directory. Returns
   *     null if the directory model is not ready or the current directory is
   *     not set.
   */
  FileManager.prototype.getCurrentDirectoryEntry = function() {
    return this.directoryModel_ && this.directoryModel_.getCurrentDirEntry();
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
        this.ui_.suggestAppsDialog.showByFilename(filename, onDialogClosed);
      } else {
        this.ui_.suggestAppsDialog.showByExtensionAndMime(
            extension, mime, onDialogClosed);
      }
    }.bind(this));
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
    this.backgroundPage_.background.progressCenter.removePanel(
        this.ui_.progressCenterPanel);
    window.closing = true;
    if (this.backgroundPage_)
      this.backgroundPage_.background.tryClose();
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
