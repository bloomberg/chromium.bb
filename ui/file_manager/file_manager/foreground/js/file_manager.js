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

  /** @private {importer.HistoryLoader} */
  this.historyLoader_ = null;

  /**
   * ImportHistory. Non-null only once history observer is added in
   * {@code addHistoryObserver}.
   *
   * @type {importer.ImportHistory}
   * @private
   */
  this.importHistory_ = null;

  /**
   * Bound observer for use with {@code importer.ImportHistory.Observer}.
   * The instance is bound once here as {@code ImportHistory.removeObserver}
   * uses object equivilency to remove observers.
   *
   * @private {function(!importer.ImportHistory.ChangedEvent)}
   */
  this.onHistoryChangedBound_ = this.onHistoryChanged_.bind(this);

  /** @private {importer.MediaScanner} */
  this.mediaScanner_ = null;

  /** @private {importer.ImportController} */
  this.importController_ = null;

  /** @private {importer.MediaImportHandler} */
  this.mediaImportHandler_ = null;

  /**
   * @private {MetadataModel}
   */
  this.metadataModel_ = null;

  /**
   * @private {ThumbnailModel}
   */
  this.thumbnailModel_ = null;

  /**
   * File operation manager.
   * @type {FileOperationManager}
   * @private
   */
  this.fileOperationManager_ = null;

  /**
   * File filter.
   * @private {!FileFilter}
   * @const
   */
  this.fileFilter_ = new FileFilter(
      false  /* Don't show dot files and *.crdownload by default. */);

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

  /**
   * @private {analytics.Tracker}
   */
  this.tracker_ = null;

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
   * Gear menu controller.
   * @type {GearMenuController}
   * @private
   */
  this.gearMenuController_ = null;

  /**
   * Toolbar controller.
   * @type {ToolbarController}
   * @private
   */
  this.toolbarController_ = null;

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

  /**
   * @type {TaskController}
   * @private
   */
  this.taskController_ = null;

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

  // --------------------------------------------------------------------------
  // Miscellaneous FileManager's states.

  /**
   * Queue for ordering FileManager's initialization process.
   * @type {!AsyncUtil.Group}
   * @private
   */
  this.initializeQueue_ = new AsyncUtil.Group();

  /**
   * Indicates whether cloud import is enabled.  This is initialized in
   * #initSettings.
   * @private {?boolean}
   */
  this.importEnabled_ = null;
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
   * @return {!FileFilter}
   */
  get fileFilter() {
    return this.fileFilter_;
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
   * @return {TaskController}
   */
  get taskController() {
    return this.taskController_;
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
   * @return {importer.ImportController}
   */
  get importController() {
    return this.importController_;
  },
  /**
   * @return {importer.HistoryLoader}
   */
  get historyLoader() {
    return this.historyLoader_;
  },
  /**
   * @return {importer.MediaImportHandler}
   */
  get mediaImportHandler() {
    return this.mediaImportHandler_;
  },
  /**
   * @return {FileManagerUI}
   */
  get ui() {
    return this.ui_;
  }
};

// Anonymous "namespace".
(function() {
  FileManager.prototype.initSettings_ = function(callback) {
    this.appStateController_ = new AppStateController(this.dialogType);
    var whenViewOptionsLoaded =
        this.appStateController_.loadInitialViewOptions();
    var whenImportFlagLoaded = importer.importEnabled().then(
        function(enabled) {
          this.importEnabled_ = enabled;
        }.bind(this));

    Promise.all([
      whenViewOptionsLoaded,
      whenImportFlagLoaded
    ]).then(callback);
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

    var self = this;

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
    this.toolbarController_ = new ToolbarController(
        this.ui_.toolbar,
        this.ui_.dialogNavigationList,
        assert(this.ui_.locationLine),
        this.selectionHandler_,
        this.directoryModel_);

    importer.importEnabled().then(
        function(enabled) {
          if (enabled) {
            this.importController_ = new importer.ImportController(
                new importer.RuntimeControllerEnvironment(
                    this,
                    /** @type {!FileSelectionHandler} */ (
                        this.selectionHandler_)),
                /** @type {!importer.MediaScanner} */ (
                    this.mediaScanner_),
                /** @type {!importer.ImportRunner} */ (
                    this.mediaImportHandler_),
                new importer.RuntimeCommandWidget(),
                assert(this.tracker_));
          }
        }.bind(this));

    assert(this.fileFilter_);
    assert(this.namingController_);
    assert(this.appStateController_);
    assert(this.taskController_);
    this.mainWindowComponent_ = new MainWindowComponent(
        this.dialogType,
        this.ui_,
        this.volumeManager_,
        this.directoryModel_,
        this.fileFilter_,
        this.selectionHandler_,
        this.namingController_,
        this.appStateController_,
        this.taskController_);

    this.initDataTransferOperations_();

    this.selectionHandler_.onFileSelectionChanged();
    this.ui_.listContainer.endBatchUpdates();

    // Get the 'allowRedeemOffers' preference for banners.
    chrome.fileManagerPrivate.getPreferences(function(pref) {
      this.ui_.initBanners(
          new Banners(
              this.directoryModel_,
              this.volumeManager_,
              this.document_,
              // Whether to show offers on the welcome banner.
              pref.allowRedeemOffers,
              // Whether to show any welcome banner.
              this.dialogType === DialogType.FULL_PAGE));
      callback();
    }.bind(this));
  };

  /**
   * @private
   */
  FileManager.prototype.initDataTransferOperations_ = function() {
    // CopyManager are required for 'Delete' operation in
    // Open and Save dialogs. But drag-n-drop and copy-paste are not needed.
    if (this.dialogType !== DialogType.FULL_PAGE)
      return;

    this.fileTransferController_ = new FileTransferController(
        assert(this.document_),
        assert(this.ui_.listContainer),
        assert(this.ui_.directoryTree),
        this.ui_.multiProfileShareDialog,
        assert(this.backgroundPage_.background.progressCenter),
        assert(this.fileOperationManager_),
        assert(this.metadataModel_),
        assert(this.thumbnailModel_),
        assert(this.directoryModel_),
        assert(this.volumeManager_),
        assert(this.selectionHandler_));
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
            this.mediaImportHandler_ =
                this.backgroundPage_.background.mediaImportHandler;
            this.mediaScanner_ =
                this.backgroundPage_.background.mediaScanner;
            this.historyLoader_ = this.backgroundPage_.background.historyLoader;
            this.tracker_ = this.backgroundPage_.background.tracker;
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
    var noLocalPathResolution =
        DialogType.isFolderDialog(this.launchParams_.type);

    // If this condition is false, VolumeManagerWrapper hides all drive
    // related event and data, even if Drive is enabled on preference.
    // In other words, even if Drive is disabled on preference but Files.app
    // should show Drive when it is re-enabled, then the value should be set to
    // true.
    // Note that the Drive enabling preference change is listened by
    // DriveIntegrationService, so here we don't need to take care about it.
    var nonNativeEnabled =
        !noLocalPathResolution || !this.launchParams_.shouldReturnLocalPath;
    this.volumeManager_ = new VolumeManagerWrapper(
        /** @type {VolumeManagerWrapper.NonNativeVolumeStatus} */
        (nonNativeEnabled),
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
    assert(this.volumeManager_);
    this.metadataModel_ = MetadataModel.create(this.volumeManager_);
    this.thumbnailModel_ = new ThumbnailModel(this.metadataModel_);

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
    assert(this.metadataModel_);
    assert(this.volumeManager_);
    assert(this.historyLoader_);
    assert(this.dialogDom_);
    assert(this.metadataModel_);

    // Cache nodes we'll be manipulating.
    var dom = this.dialogDom_;
    assert(dom);

    // Initialize the dialog.
    FileManagerDialogBase.setFileManager(this);

    var table = queryRequiredElement(dom, '.detail-table');
    // TODO(kenobi): See crbug/460135.  The import status column in table view
    // is disabled until it works properly.
    table.importEnabled = false;
    FileTable.decorate(
        table,
        this.metadataModel_,
        this.volumeManager_,
        this.historyLoader_,
        this.dialogType == DialogType.FULL_PAGE);
    var grid = queryRequiredElement(dom, '.thumbnail-grid');
    FileGrid.decorate(
        grid,
        this.metadataModel_,
        this.volumeManager_,
        this.historyLoader_);

    this.addHistoryObserver_();

    this.ui_.initAdditionalUI(
        assertInstanceof(table, FileTable),
        assertInstanceof(grid, FileGrid),
        new LocationLine(
            queryRequiredElement(dom, '#location-breadcrumbs'),
            this.volumeManager_));

    // Handle UI events.
    this.backgroundPage_.background.progressCenter.addPanel(
        this.ui_.progressCenterPanel);

    util.addIsFocusedMethod();

    // Populate the static localized strings.
    i18nTemplate.process(this.document_, loadTimeData);

    // Arrange the file list.
    this.ui_.listContainer.table.normalizeColumns();
    this.ui_.listContainer.table.redraw();

    callback();
  };

  /**
   * One-time initialization of import history observer. Provides
   * the glue that updates the UI when history changes.
   *
   * @private
   */
  FileManager.prototype.addHistoryObserver_ = function() {
    // If, and only if history is ever fully loaded (it may not be),
    // we want to update grid/list view when it changes.
    this.historyLoader_.addHistoryLoadedListener(
        /**
         * @param {!importer.ImportHistory} history
         * @this {FileManager}
         */
        function(history) {
          this.importHistory_ = history;
          history.addObserver(this.onHistoryChangedBound_);
        }.bind(this));

  };

  /**
   * Handles events when import history changed.
   *
   * @param {!importer.ImportHistory.ChangedEvent} event
   * @private
   */
  FileManager.prototype.onHistoryChanged_ = function(event) {
    // Ignore any entry that isn't an immediate child of the
    // current directory.
    util.isChildEntry(event.entry, this.getCurrentDirectoryEntry())
        .then(
            /**
             * @param {boolean} isChild
             * @this {FileManager}
             */
            function(isChild) {
              if (isChild) {
                this.ui_.listContainer.grid.updateListItemsMetadata(
                    'import-history',
                    [event.entry]);
                this.ui_.listContainer.table.updateListItemsMetadata(
                    'import-history',
                    [event.entry]);
              }
            }.bind(this));
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

    assert(this.volumeManager_);
    assert(this.fileOperationManager_);
    assert(this.metadataModel_);
    this.directoryModel_ = new DirectoryModel(
        singleSelection,
        this.fileFilter_,
        this.metadataModel_,
        this.volumeManager_,
        this.fileOperationManager_);

    this.folderShortcutsModel_ = new FolderShortcutsDataModel(
        this.volumeManager_);

    this.selectionHandler_ = new FileSelectionHandler(this);

    this.directoryModel_.getFileListSelection().addEventListener('change',
        this.selectionHandler_.onFileSelectionChanged.bind(
            this.selectionHandler_));

    this.directoryModel_.addEventListener(
        'directory-changed',
        function(event) {
          if (event.volumeChanged) {
            var volumeInfo =
                this.volumeManager_.getVolumeInfo(event.newDirEntry);
            // NOTE: That dynamic values, like volume name MUST NOT
            // be sent to GA as that value can contain PII.
            // VolumeType is an enum.
            this.tracker_.sendAppView(volumeInfo.volumeType);
          }
        }.bind(this));

    // TODO(mtomasz, yoshiki): Create navigation list earlier, and here just
    // attach the directory model.
    this.initDirectoryTree_();

    this.ui_.listContainer.listThumbnailLoader = new ListThumbnailLoader(
        assert(this.directoryModel_.getFileList()),
        assert(this.thumbnailModel_));
    this.ui_.listContainer.dataModel = this.directoryModel_.getFileList();
    this.ui_.listContainer.selectionModel =
        this.directoryModel_.getFileListSelection();

    this.appStateController_.initialize(this.ui_, this.directoryModel_);

    // Create metadata update controller.
    this.metadataUpdateController_ = new MetadataUpdateController(
        this.ui_.listContainer,
        this.directoryModel_,
        this.metadataModel_);

    // Create task controller.
    this.taskController_ = new TaskController(
        this.dialogType,
        this.ui_,
        this.metadataModel_,
        this.selectionHandler_,
        this.metadataUpdateController_,
        function() { return new FileTasks(this); }.bind(this));

    // Create search controller.
    this.searchController_ = new SearchController(
        this.ui_.searchBox,
        assert(this.ui_.locationLine),
        this.directoryModel_,
        this.volumeManager_,
        assert(this.taskController_));

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
        this.metadataModel_,
        this.volumeManager_,
        this.fileFilter_,
        this.namingController_,
        this.selectionHandler_,
        this.launchParams_);
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
                           assert(this.directoryModel_),
                           assert(this.volumeManager_),
                           assert(this.metadataModel_),
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
   * Unload handler for the page.
   * @private
   */
  FileManager.prototype.onUnload_ = function() {
    if (this.importHistory_)
      this.importHistory_.removeObserver(this.onHistoryChangedBound_);
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
   * @return {!MetadataModel}
   */
  FileManager.prototype.getMetadataModel = function() {
    return assert(this.metadataModel_);
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
