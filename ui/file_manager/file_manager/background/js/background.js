// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Type of a Files.app's instance launch.
 * @enum {number}
 */
var LaunchType = {
  ALWAYS_CREATE: 0,
  FOCUS_ANY_OR_CREATE: 1,
  FOCUS_SAME_OR_CREATE: 2
};

/**
 * Root class of the background page.
 * @constructor
 * @extends {BackgroundBase}
 * @struct
 */
function FileBrowserBackground() {
  BackgroundBase.call(this);

  /** @type {!analytics.Tracker} */
  this.tracker = metrics.getTracker();

  /**
   * Synchronous queue for asynchronous calls.
   * @type {!AsyncUtil.Queue}
   */
  this.queue = new AsyncUtil.Queue();

  /**
   * Progress center of the background page.
   * @type {!ProgressCenter}
   */
  this.progressCenter = new ProgressCenter();

  /**
   * File operation manager.
   * @type {FileOperationManager}
   */
  this.fileOperationManager = null;

  /**
   * Class providing loading of import history, used in
   * cloud import.
   *
   * @type {!importer.HistoryLoader}
   */
  this.historyLoader = new importer.RuntimeHistoryLoader(this.tracker);

  /**
   * Event handler for progress center.
   * @private {FileOperationHandler}
   */
  this.fileOperationHandler_ = null;

  /**
   * Event handler for C++ sides notifications.
   * @private {!DeviceHandler}
   */
  this.deviceHandler_ = new DeviceHandler();

  // Handle device navigation requests.
  this.deviceHandler_.addEventListener(
      DeviceHandler.VOLUME_NAVIGATION_REQUESTED,
      this.handleViewEvent_.bind(this));

  /**
   * Drive sync handler.
   * @type {!DriveSyncHandler}
   */
  this.driveSyncHandler = new DriveSyncHandler(this.progressCenter);

  /**
   * Provides support for scaning media devices as part of Cloud Import.
   * @type {!importer.MediaScanner}
   */
  this.mediaScanner = new importer.DefaultMediaScanner(
      importer.createMetadataHashcode,
      importer.DispositionChecker.createChecker(
          this.historyLoader,
          this.tracker),
      importer.DefaultDirectoryWatcher.create);

  /**
   * Handles importing of user media (e.g. photos, videos) from removable
   * devices.
   * @type {!importer.MediaImportHandler}
   */
  this.mediaImportHandler = new importer.MediaImportHandler(
      this.progressCenter,
      this.historyLoader,
      this.tracker);

  /**
   * String assets.
   * @type {Object<string>}
   */
  this.stringData = null;

  /**
   * Provides drive search to app launcher.
   * @private {LauncherSearch}
   */
  this.launcherSearch_ = new LauncherSearch();

  // Initialize handlers.
  chrome.fileBrowserHandler.onExecute.addListener(this.onExecute_.bind(this));
  chrome.runtime.onMessageExternal.addListener(
      this.onExternalMessageReceived_.bind(this));
  chrome.contextMenus.onClicked.addListener(
      this.onContextMenuClicked_.bind(this));

  // Initializa string and volume manager related stuffs.
  this.initializationPromise_.then(function(strings) {
    this.stringData = strings;
    this.initContextMenu_();

    this.fileOperationManager = new FileOperationManager();
    this.fileOperationHandler_ = new FileOperationHandler(
        this.fileOperationManager, this.progressCenter);
  }.bind(this));

  // Handle newly mounted FSP file systems. Workaround for crbug.com/456648.
  // TODO(mtomasz): Replace this hack with a proper solution.
  chrome.fileManagerPrivate.onMountCompleted.addListener(
      this.onMountCompleted_.bind(this));

  this.queue.run(function(callback) {
    this.initializationPromise_.then(callback);
  }.bind(this));
}

FileBrowserBackground.prototype.__proto__ = BackgroundBase.prototype;

/**
 * Register callback to be invoked after initialization.
 * If the initialization is already done, the callback is invoked immediately.
 *
 * @param {function()} callback Initialize callback to be registered.
 */
FileBrowserBackground.prototype.ready = function(callback) {
  this.initializationPromise_.then(callback);
};

/**
 * Opens the volume root (or opt directoryPath) in main UI.
 *
 * @param {!Event} event An event with the volumeId or
 *     devicePath.
 * @private
 */
FileBrowserBackground.prototype.handleViewEvent_ =
    function(event) {
  VolumeManager.getInstance()
      .then(
          /**
           * Retrieves the root file entry of the volume on the requested
           * device.
           * @param {!VolumeManager} volumeManager
           */
          function(volumeManager) {
            if (event.devicePath) {
              var volume = volumeManager.volumeInfoList.findByDevicePath(
                  event.devicePath);
              if (volume) {
                this.navigateToVolume_(volume, event.filePath);
              } else {
                console.error('Got view event with invalid volume id.');
              }
            } else if (event.volumeId) {
              this.navigateToVolumeWhenReady_(event.volumeId, event.filePath);
            } else {
              console.error('Got view event with no actionable destination.');
            }
          }.bind(this));
};

/**
 * Opens the volume root (or opt directoryPath) in main UI.
 *
 * @param {!string} volumeId ID of the volume to navigate to.
 * @param {!string=} opt_directoryPath Optional path to be opened.
 * @private
 */
FileBrowserBackground.prototype.navigateToVolumeWhenReady_ =
    function(volumeId, opt_directoryPath) {
  VolumeManager.getInstance()
      .then(
          /**
           * Retrieves the root file entry of the volume on the requested
           * device.
           * @param {!VolumeManager} volumeManager
           */
          function(volumeManager) {
            volumeManager.volumeInfoList.whenVolumeInfoReady(volumeId)
                .then(
                    function(volume) {
                      this.navigateToVolume_(volume, opt_directoryPath);
                    }.bind(this))
                .catch(
                    function(e) {
                      console.error(
                          'Unable to find volume for id: ' + volumeId +
                          '. Error: ' + e.message);
                    });
          }.bind(this));
};

/**
 * Opens the volume root (or opt directoryPath) in main UI.
 *
 * @param {!VolumeInfo} volume
 * @param {string=} opt_directoryPath Optional directory path to be opened.
 * @private
 */
FileBrowserBackground.prototype.navigateToVolume_ =
    function(volume, opt_directoryPath) {
  volume.resolveDisplayRoot()
      .then(
          /**
           * If a path was specified, retrieve that directory entry,
           * otherwise just return the unmodified root entry.
           * @param {DirectoryEntry} root
           * @return {!Promise<DirectoryEntry>}
           */
          function(root) {
            if (opt_directoryPath) {
              return new Promise(
                  root.getDirectory.bind(
                      root, opt_directoryPath, {create: false}));
            } else {
              return Promise.resolve(root);
            }
          })
      .then(
          /**
           * Launches app opened on {@code directory}.
           * @param {DirectoryEntry} directory
           */
          function(directory) {
            launchFileManager(
                {currentDirectoryURL: directory.toURL()},
                /* App ID */ undefined,
                LaunchType.FOCUS_SAME_OR_CREATE);
          });
};

/**
 * Prefix for the file manager window ID.
 * @type {string}
 * @const
 */
var FILES_ID_PREFIX = 'files#';

/**
 * Regexp matching a file manager window ID.
 * @type {RegExp}
 * @const
 */
var FILES_ID_PATTERN = new RegExp('^' + FILES_ID_PREFIX + '(\\d*)$');

/**
 * Prefix for the dialog ID.
 * @type {string}
 * @const
 */
var DIALOG_ID_PREFIX = 'dialog#';

/**
 * Value of the next file manager window ID.
 * @type {number}
 */
var nextFileManagerWindowID = 0;

/**
 * Value of the next file manager dialog ID.
 * @type {number}
 */
var nextFileManagerDialogID = 0;

/**
 * File manager window create options.
 * @type {Object}
 * @const
 */
var FILE_MANAGER_WINDOW_CREATE_OPTIONS = {
  bounds: {
    left: Math.round(window.screen.availWidth * 0.1),
    top: Math.round(window.screen.availHeight * 0.1),
    width: Math.round(window.screen.availWidth * 0.8),
    height: Math.round(window.screen.availHeight * 0.8)
  },
  frame: {
    color: '#1976d2'
  },
  minWidth: 480,
  minHeight: 300
};

/**
 * @param {Object=} opt_appState App state.
 * @param {number=} opt_id Window id.
 * @param {LaunchType=} opt_type Launch type. Default: ALWAYS_CREATE.
 * @param {function(string)=} opt_callback Completion callback with the App ID.
 */
function launchFileManager(opt_appState, opt_id, opt_type, opt_callback) {
  var type = opt_type || LaunchType.ALWAYS_CREATE;
  opt_appState =
      /**
       * @type {(undefined|
       *         {currentDirectoryURL: (string|undefined),
       *          selectionURL: (string|undefined)})}
       */
      (opt_appState);

  // Wait until all windows are created.
  window.background.queue.run(function(onTaskCompleted) {
    // Check if there is already a window with the same URL. If so, then
    // reuse it instead of opening a new one.
    if (type == LaunchType.FOCUS_SAME_OR_CREATE ||
        type == LaunchType.FOCUS_ANY_OR_CREATE) {
      if (opt_appState) {
        for (var key in window.background.appWindows) {
          if (!key.match(FILES_ID_PATTERN))
            continue;
          var contentWindow = window.background.appWindows[key].contentWindow;
          if (!contentWindow.appState)
            continue;
          // Different current directories.
          if (opt_appState.currentDirectoryURL !==
                  contentWindow.appState.currentDirectoryURL) {
            continue;
          }
          // Selection URL specified, and it is different.
          if (opt_appState.selectionURL &&
                  opt_appState.selectionURL !==
                  contentWindow.appState.selectionURL) {
            continue;
          }
          window.background.appWindows[key].focus();
          if (opt_callback)
            opt_callback(key);
          onTaskCompleted();
          return;
        }
      }
    }

    // Focus any window if none is focused. Try restored first.
    if (type == LaunchType.FOCUS_ANY_OR_CREATE) {
      // If there is already a focused window, then finish.
      for (var key in window.background.appWindows) {
        if (!key.match(FILES_ID_PATTERN))
          continue;

        // The isFocused() method should always be available, but in case
        // Files.app's failed on some error, wrap it with try catch.
        try {
          if (window.background.appWindows[key].contentWindow.isFocused()) {
            if (opt_callback)
              opt_callback(key);
            onTaskCompleted();
            return;
          }
        } catch (e) {
          console.error(e.message);
        }
      }
      // Try to focus the first non-minimized window.
      for (var key in window.background.appWindows) {
        if (!key.match(FILES_ID_PATTERN))
          continue;

        if (!window.background.appWindows[key].isMinimized()) {
          window.background.appWindows[key].focus();
          if (opt_callback)
            opt_callback(key);
          onTaskCompleted();
          return;
        }
      }
      // Restore and focus any window.
      for (var key in window.background.appWindows) {
        if (!key.match(FILES_ID_PATTERN))
          continue;

        window.background.appWindows[key].focus();
        if (opt_callback)
          opt_callback(key);
        onTaskCompleted();
        return;
      }
    }

    // Create a new instance in case of ALWAYS_CREATE type, or as a fallback
    // for other types.

    var id = opt_id || nextFileManagerWindowID;
    nextFileManagerWindowID = Math.max(nextFileManagerWindowID, id + 1);
    var appId = FILES_ID_PREFIX + id;

    var appWindow = new AppWindowWrapper(
        'main.html',
        appId,
        FILE_MANAGER_WINDOW_CREATE_OPTIONS);
    appWindow.launch(opt_appState || {}, false, function() {
      appWindow.rawAppWindow.focus();
      if (opt_callback)
        opt_callback(appId);
      onTaskCompleted();
    });
  });
}

/**
 * Registers dialog window to the background page.
 *
 * @param {!Window} dialogWindow Window of the dialog.
 */
function registerDialog(dialogWindow) {
  var id = DIALOG_ID_PREFIX + (nextFileManagerDialogID++);
  window.background.dialogs[id] = dialogWindow;
  dialogWindow.addEventListener('pagehide', function() {
    delete window.background.dialogs[id];
  });
}

/**
 * Executes a file browser task.
 *
 * @param {string} action Task id.
 * @param {Object} details Details object.
 * @private
 */
FileBrowserBackground.prototype.onExecute_ = function(action, details) {
  var appState = {
    params: {action: action},
    // It is not allowed to call getParent() here, since there may be
    // no permissions to access it at this stage. Therefore we are passing
    // the selectionURL only, and the currentDirectory will be resolved
    // later.
    selectionURL: details.entries[0].toURL()
  };

  // Every other action opens a Files app window.
  // For mounted devices just focus any Files.app window. The mounted
  // volume will appear on the navigation list.
  launchFileManager(
      appState,
      /* App ID */ undefined,
      LaunchType.FOCUS_SAME_OR_CREATE);
};

/**
 * Launches the app.
 * @private
 * @override
 */
FileBrowserBackground.prototype.onLaunched_ = function() {
  this.initializationPromise_.then(function() {
    if (nextFileManagerWindowID == 0) {
      // The app just launched. Remove window state records that are not needed
      // any more.
      chrome.storage.local.get(function(items) {
        for (var key in items) {
          if (items.hasOwnProperty(key)) {
            if (key.match(FILES_ID_PATTERN))
              chrome.storage.local.remove(key);
          }
        }
      });
    }
    launchFileManager(null, undefined, LaunchType.FOCUS_ANY_OR_CREATE);
  });
};

/** @const {string} */
var GPLUS_PHOTOS_APP_ID = 'efjnaogkjbogokcnohkmnjdojkikgobo';

/**
 * Handles a message received via chrome.runtime.sendMessageExternal.
 *
 * @param {*} message
 * @param {MessageSender} sender
 */
FileBrowserBackground.prototype.onExternalMessageReceived_ =
    function(message, sender) {
  if ('id' in sender && sender.id === GPLUS_PHOTOS_APP_ID) {
    importer.handlePhotosAppMessage(message);
  }
};

/**
 * Restarted the app, restore windows.
 * @private
 * @override
 */
FileBrowserBackground.prototype.onRestarted_ = function() {
  // Reopen file manager windows.
  chrome.storage.local.get(function(items) {
    for (var key in items) {
      if (items.hasOwnProperty(key)) {
        var match = key.match(FILES_ID_PATTERN);
        if (match) {
          var id = Number(match[1]);
          try {
            var appState = /** @type {Object} */ (JSON.parse(items[key]));
            launchFileManager(appState, id);
          } catch (e) {
            console.error('Corrupt launch data for ' + id);
          }
        }
      }
    }
  });
};

/**
 * Handles clicks on a custom item on the launcher context menu.
 * @param {!Object} info Event details.
 * @private
 */
FileBrowserBackground.prototype.onContextMenuClicked_ = function(info) {
  if (info.menuItemId == 'new-window') {
    // Find the focused window (if any) and use it's current url for the
    // new window. If not found, then launch with the default url.
    this.findFocusedWindow_().then(function(key) {
      if (!key) {
        launchFileManager(appState);
        return;
      }
      var appState = {
        // Do not clone the selection url, only the current directory.
        currentDirectoryURL: window.background.appWindows[key].
            contentWindow.appState.currentDirectoryURL
      };
      launchFileManager(appState);
    }).catch(function(error) {
      console.error(error.stack || error);
    });
  }
};

/**
 * Looks for a focused window.
 *
 * @return {!Promise.<?string>} Promise fulfilled with a key of the focused
 *     window, or null if not found.
 */
FileBrowserBackground.prototype.findFocusedWindow_ = function() {
  return new Promise(function(fulfill, reject) {
    for (var key in window.background.appWindows) {
      try {
        if (window.background.appWindows[key].contentWindow.isFocused()) {
          fulfill(key);
          return;
        }
      } catch (ignore) {
        // The isFocused method may not be defined during initialization.
        // Therefore, wrapped with a try-catch block.
      }
    }
    fulfill(null);
  });
};

/**
 * Handles mounted FSP volumes and fires Files app. This is a quick fix for
 * crbug.com/456648.
 * @param {!Object} event Event details.
 * @private
 */
FileBrowserBackground.prototype.onMountCompleted_ = function(event) {
  // If there is no focused window, then create a new one opened on the
  // mounted FSP volume.
  this.findFocusedWindow_().then(function(key) {
    if (key === null &&
        event.eventType === 'mount' &&
        (event.status === 'success' ||
         event.status === 'error_path_already_mounted') &&
        event.volumeMetadata.mountContext === 'user' &&
        event.volumeMetadata.volumeType ===
            VolumeManagerCommon.VolumeType.PROVIDED &&
        event.volumeMetadata.source === VolumeManagerCommon.Source.FILE) {
      this.navigateToVolumeWhenReady_(event.volumeMetadata.volumeId);
    }
  }.bind(this)).catch(function(error) {
    console.error(error.stack || error);
  });
};

/**
 * Initializes the context menu. Recreates if already exists.
 * @private
 */
FileBrowserBackground.prototype.initContextMenu_ = function() {
  try {
    // According to the spec [1], the callback is optional. But no callback
    // causes an error for some reason, so we call it with null-callback to
    // prevent the error. http://crbug.com/353877
    // Also, we read the runtime.lastError here not to output the message on the
    // console as an unchecked error.
    // - [1] https://developer.chrome.com/extensions/contextMenus#method-remove
    chrome.contextMenus.remove('new-window', function() {
      var ignore = chrome.runtime.lastError;
    });
  } catch (ignore) {
    // There is no way to detect if the context menu is already added, therefore
    // try to recreate it every time.
  }
  chrome.contextMenus.create({
    id: 'new-window',
    contexts: ['launcher'],
    title: str('NEW_WINDOW_BUTTON_LABEL')
  });
};

/**
 * Singleton instance of Background.
 * NOTE: This must come after the call to metrics.clearUserId.
 * @type {FileBrowserBackground}
 */
window.background = new FileBrowserBackground();
