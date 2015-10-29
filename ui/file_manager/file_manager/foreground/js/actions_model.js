// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @interface
 */
function Action() {
}

Action.prototype.execute = function() {
};

/**
 * @return {boolean}
 */
Action.prototype.canExecute = function() {
};

/**
 * @return {?string}
 */
Action.prototype.getTitle = function() {
};

/**
 * @param {!Entry} entry
 * @param {!FileManagerUI} ui
 * @param {!VolumeManagerWrapper} volumeManager
 * @implements {Action}
 * @constructor
 * @struct
 */
function DriveShareAction(entry, volumeManager, ui) {
  /**
   * @private {!Entry}
   * @const
   */
  this.entry_ = entry;

  /**
   * @private {!VolumeManagerWrapper}
   * @const
   */
  this.volumeManager_ = volumeManager;

  /**
   * @private {!FileManagerUI}
   * @const
   */
  this.ui_ = ui;
}

/**
 * @param {!Array<!Entry>} entries
 * @param {!FileManagerUI} ui
 * @param {!VolumeManagerWrapper} volumeManager
 * @return {DriveShareAction}
 */
DriveShareAction.create = function(entries, volumeManager, ui) {
  if (entries.length !== 1)
    return null;

  return new DriveShareAction(entries[0], volumeManager, ui);
};

/**
 * @override
 */
DriveShareAction.prototype.execute = function() {
  this.ui_.shareDialog.showEntry(this.entry_, function(result) {
    if (result == ShareDialog.Result.NETWORK_ERROR)
      this.ui_.errorDialog.show(str('SHARE_ERROR'), null, null, null);
  }.bind(this));
};

/**
 * @override
 */
DriveShareAction.prototype.canExecute = function() {
  return this.volumeManager_.getDriveConnectionState().type !==
      VolumeManagerCommon.DriveConnectionType.OFFLINE;
};

/**
 * @return {?string}
 */
DriveShareAction.prototype.getTitle = function() {
  return null;
};

/**
 * @param {!Array<!Entry>} entries
 * @param {!MetadataModel} metadataModel
 * @param {!DriveSyncHandler} driveSyncHandler
 * @param {!FileManagerUI} ui
 * @param {boolean} value
 * @param {function()} onExecute
 * @implements {Action}
 * @constructor
 * @struct
 */
function DriveToggleOfflineAction(entries, metadataModel, driveSyncHandler, ui,
    value, onExecute) {
  /**
   * @private {!Array<!Entry>}
   * @const
   */
  this.entries_ = entries;

  /**
   * @private {!MetadataModel}
   * @const
   */
  this.metadataModel_ = metadataModel;

  /**
   * @private {!DriveSyncHandler}
   * @const
   */
  this.driveSyncHandler_ = driveSyncHandler;

  /**
   * @private {!FileManagerUI}
   * @const
   */
  this.ui_ = ui;

  /**
   * @private {boolean}
   * @const
   */
  this.value_ = value;

  /**
   * @private {function()}
   * @const
   */
  this.onExecute_ = onExecute;
}

/**
 * @param {!Array<!Entry>} entries
 * @param {!MetadataModel} metadataModel
 * @param {!DriveSyncHandler} driveSyncHandler
 * @param {!FileManagerUI} ui
 * @param {boolean} value
 * @param {function()} onExecute
 * @return {DriveToggleOfflineAction}
 */
DriveToggleOfflineAction.create = function(entries, metadataModel,
    driveSyncHandler, ui, value, onExecute) {
  var directoryEntries = entries.filter(function(entry) {
    return entry.isDirectory;
  });
  if (directoryEntries.length > 0)
    return null;

  var actionableEntries = entries.filter(function(entry) {
    if (entry.isDirectory)
      return false;
    var metadata = metadataModel.getCache(
        [entry], ['hosted', 'pinned'])[0];
    if (metadata.hosted)
      return false;
    if (metadata.pinned === value)
      return false;
    return true;
  });

  if (actionableEntries.length === 0)
    return null;

  return new DriveToggleOfflineAction(actionableEntries, metadataModel,
      driveSyncHandler, ui, value, onExecute);
};

/**
 * @override
 */
DriveToggleOfflineAction.prototype.execute = function() {
  var entries = this.entries_;
  if (entries.length == 0)
    return;

  var currentEntry;
  var error = false;

  var steps = {
    // Pick an entry and pin it.
    start: function() {
      // Check if all the entries are pinned or not.
      if (entries.length === 0) {
        this.onExecute_();
        return;
      }
      currentEntry = entries.shift();
      chrome.fileManagerPrivate.pinDriveFile(
          currentEntry,
          this.value_,
          steps.entryPinned);
    }.bind(this),

    // Check the result of pinning.
    entryPinned: function() {
      error = !!chrome.runtime.lastError;
      if (error && this.value_) {
        this.metadataModel_.get([currentEntry], ['size']).then(
            function(results) {
              steps.showError(results[0].size);
            });
        return;
      }
      this.metadataModel_.notifyEntriesChanged([currentEntry]);
      this.metadataModel_.get([currentEntry], ['pinned']).then(steps.updateUI);
    }.bind(this),

    // Update the user interface according to the cache state.
    updateUI: function() {
      this.ui_.listContainer.currentView.updateListItemsMetadata(
          'external', [currentEntry]);
      if (!error)
        steps.start();
    }.bind(this),

    // Show an error.
    showError: function(size) {
      this.ui_.alertDialog.showHtml(
          str('DRIVE_OUT_OF_SPACE_HEADER'),
          strf('DRIVE_OUT_OF_SPACE_MESSAGE',
               unescape(currentEntry.name),
               util.bytesToString(size)),
          null, null, null);
    }.bind(this)
  };
  steps.start();

  if (this.value_ && this.driveSyncHandler_.isSyncSuppressed())
    this.driveSyncHandler_.showDisabledMobileSyncNotification();
};

/**
 * @override
 */
DriveToggleOfflineAction.prototype.canExecute = function() {
  return true;
};

/**
 * @return {?string}
 */
DriveToggleOfflineAction.prototype.getTitle = function() {
  return null;
};

/**
 * @param {!Entry} entry
 * @param {!FolderShortcutsDataModel} shortcutsModel
 * @param {function()} onExecute
 * @implements {Action}
 * @constructor
 * @struct
 */
function DriveCreateFolderShortcutAction(entry, shortcutsModel, onExecute) {
  /**
   * @private {!Entry}
   * @const
   */
  this.entry_ = entry;

  /**
   * @private {!FolderShortcutsDataModel}
   * @const
   */
  this.shortcutsModel_ = shortcutsModel;

  /**
   * @private {function()}
   * @const
   */
  this.onExecute_ = onExecute;
}

/**
 * @param {!Array<!Entry>} entries
 * @param {!VolumeManagerWrapper} volumeManager
 * @param {!FolderShortcutsDataModel} shortcutsModel
 * @param {function()} onExecute
 * @return {DriveCreateFolderShortcutAction}
 */
DriveCreateFolderShortcutAction.create = function(entries, volumeManager,
    shortcutsModel, onExecute) {
  if (entries.length !== 1 || entries[0].isFile)
    return null;
  var locationInfo = volumeManager.getLocationInfo(entries[0]);
  if (!locationInfo || locationInfo.isSpecialSearchRoot ||
      locationInfo.isRootEntry) {
    return null;
  }
  return new DriveCreateFolderShortcutAction(
      entries[0], shortcutsModel, onExecute);
};

/**
 * @override
 */
DriveCreateFolderShortcutAction.prototype.execute = function() {
  this.shortcutsModel_.add(this.entry_);
  this.onExecute_();
};

/**
 * @override
 */
DriveCreateFolderShortcutAction.prototype.canExecute = function() {
  return !this.shortcutsModel_.exists(this.entry_);
};

/**
 * @return {?string}
 */
DriveCreateFolderShortcutAction.prototype.getTitle = function() {
  return null;
};

/**
 * @param {!Entry} entry
 * @param {!FolderShortcutsDataModel} shortcutsModel
 * @param {function()} onExecute
 * @implements {Action}
 * @constructor
 * @struct
 */
function DriveRemoveFolderShortcutAction(entry, shortcutsModel, onExecute) {
  /**
   * @private {!Entry}
   * @const
   */
  this.entry_ = entry;

  /**
   * @private {!FolderShortcutsDataModel}
   * @const
   */
  this.shortcutsModel_ = shortcutsModel;

  /**
   * @private {function()}
   * @const
   */
  this.onExecute_ = onExecute;
}

/**
 * @param {!Array<!Entry>} entries
 * @param {!FolderShortcutsDataModel} shortcutsModel
 * @param {function()} onExecute
 * @return {DriveRemoveFolderShortcutAction}
 */
DriveRemoveFolderShortcutAction.create = function(entries, shortcutsModel,
    onExecute) {
  if (entries.length !== 1 || entries[0].isFile ||
      !shortcutsModel.exists(entries[0])) {
    return null;
  }
  return new DriveRemoveFolderShortcutAction(
      entries[0], shortcutsModel, onExecute);
};

/**
 * @override
 */
DriveRemoveFolderShortcutAction.prototype.execute = function() {
  this.shortcutsModel_.remove(this.entry_);
  this.onExecute_();
};

/**
 * @override
 */
DriveRemoveFolderShortcutAction.prototype.canExecute = function() {
  return this.shortcutsModel_.exists(this.entry_);
};

/**
 * @return {?string}
 */
DriveRemoveFolderShortcutAction.prototype.getTitle = function() {
  return null;
};

/**
 * @param {!Array<!Entry>} entries
 * @param {string} id
 * @param {?string} title
 * @param {function()} onExecute
 * @implements {Action}
 * @constructor
 * @struct
 */
function CustomAction(entries, id, title, onExecute) {
  /**
   * @private {!Array<!Entry>}
   * @const
   */
  this.entries_ = entries;

  /**
   * @private {string}
   * @const
   */
  this.id_ = id;

  /**
   * @private {?string}
   * @const
   */
  this.title_ = title;

  /**
   * @private {function()}
   * @const
   */
  this.onExecute_ = onExecute;
}

/**
 * @override
 */
CustomAction.prototype.execute = function() {
  chrome.fileManagerPrivate.executeCustomAction(this.entries_, this.id_,
      function() {
        if (chrome.runtime.lastError) {
          console.error('Failed to execute a custom action because of: ' +
            chrome.runtime.lastError.message);
        }
        this.onExecute_();
      }.bind(this));
};

/**
 * @override
 */
CustomAction.prototype.canExecute = function() {
  return true;  // Custom actions are always executable.
};

/**
 * @override
 */
CustomAction.prototype.getTitle = function() {
  return this.title_;
};

/**
 * Represents a set of actions for a set of entries.
 *
 * @param {!VolumeManagerWrapper} volumeManager
 * @param {!MetadataModel} metadataModel
 * @param {!FolderShortcutsDataModel} shortcutsModel
 * @param {!DriveSyncHandler} driveSyncHandler
 * @param {!FileManagerUI} ui
 * @param {!Array<!Entry>} entries
 * @constructor
 * @extends {cr.EventTarget}
 * @struct
 */
function ActionsModel(
    volumeManager, metadataModel, shortcutsModel, driveSyncHandler, ui,
    entries) {
  /**
   * @private {!VolumeManagerWrapper}
   * @const
   */
  this.volumeManager_ = volumeManager;

  /**
   * @private {!MetadataModel}
   * @const
   */
  this.metadataModel_ = metadataModel;

  /**
   * @private {!FolderShortcutsDataModel}
   * @const
   */
  this.shortcutsModel_ = shortcutsModel;

  /**
   * @private {!DriveSyncHandler}
   * @const
   */
  this.driveSyncHandler_ = driveSyncHandler;

  /**
   * @private {!FileManagerUI}
   * @const
   */
  this.ui_ = ui;

  /**
   * @private {!Array<!Entry>}
   * @const
   */
  this.entries_ = entries;

  /**
   * @private {!Object<!Action>}
   */
  this.actions_ = {};

  /**
   * @private {?function()}
   */
  this.initializePromiseReject_ = null;

  /**
   * @private {Promise}
   */
  this.initializePromise_ = null;

  /**
   * @private {boolean}
   */
  this.destroyed_ = false;
}

ActionsModel.prototype = {
  __proto__: cr.EventTarget.prototype
};

/**
 * List of common actions, used both internally and externally (custom actions).
 * Keep in sync with file_system_provider.idl.
 * @enum {string}
 */
ActionsModel.CommonActionId = {
  SHARE: 'SHARE',
  SAVE_FOR_OFFLINE: 'SAVE_FOR_OFFLINE',
  OFFLINE_NOT_NECESSARY: 'OFFLINE_NOT_NECESSARY'
};

/**
 * @enum {string}
 */
ActionsModel.InternalActionId = {
  CREATE_FOLDER_SHORTCUT: 'create-folder-shortcut',
  REMOVE_FOLDER_SHORTCUT: 'remove-folder-shortcut'
};

/**
 * @const {!Array<string>}
 */
ActionsModel.METADATA_PREFETCH_PROPERTY_NAMES = [
  'hosted',
  'pinned'
];

/**
 * @return {!Promise}
 */
ActionsModel.prototype.initialize = function() {
  if (this.initializePromise_)
    return this.initializePromise_;

  this.initializePromise_ = new Promise(function(fulfill, reject) {
    if (this.destroyed_) {
      reject();
      return;
    }
    this.initializePromiseReject_ = reject;

    // All entries are expected to be on the same volume. It's assumed, and not
    // checked.
    var volumeInfo = this.entries_.length &&
        this.volumeManager_.getVolumeInfo(this.entries_[0]);

    if (!this.entries_.length || !volumeInfo) {
      fulfill({});
      return;
    }

    var actions = {};
    switch (volumeInfo.volumeType) {
      // For Drive, actions are constructed directly in the Files app code.
      case VolumeManagerCommon.VolumeType.DRIVE:
        var shareAction = DriveShareAction.create(
            this.entries_, this.volumeManager_, this.ui_);
        if (shareAction)
          actions[ActionsModel.CommonActionId.SHARE] = shareAction;

        var saveForOfflineAction = DriveToggleOfflineAction.create(
            this.entries_, this.metadataModel_, this.driveSyncHandler_,
            this.ui_, true, this.invalidate_.bind(this));
        if (saveForOfflineAction) {
          actions[ActionsModel.CommonActionId.SAVE_FOR_OFFLINE] =
              saveForOfflineAction;
        }

        var offlineNotNecessaryAction = DriveToggleOfflineAction.create(
            this.entries_, this.metadataModel_, this.driveSyncHandler_,
            this.ui_, false, this.invalidate_.bind(this));
        if (offlineNotNecessaryAction) {
          actions[ActionsModel.CommonActionId.OFFLINE_NOT_NECESSARY] =
              offlineNotNecessaryAction;
        }

        var createFolderShortcutAction =
            DriveCreateFolderShortcutAction.create(this.entries_,
                this.volumeManager_, this.shortcutsModel_,
                this.invalidate_.bind(this));
        if (createFolderShortcutAction) {
          actions[ActionsModel.InternalActionId.CREATE_FOLDER_SHORTCUT] =
              createFolderShortcutAction;
        }

        var removeFolderShortcutAction =
            DriveRemoveFolderShortcutAction.create(this.entries_,
                this.shortcutsModel_, this.invalidate_.bind(this));
        if (removeFolderShortcutAction) {
          actions[ActionsModel.InternalActionId.REMOVE_FOLDER_SHORTCUT] =
              removeFolderShortcutAction;
        }
        fulfill(actions);
        break;

      // For FSP, fetch custom actions via an API.
      case VolumeManagerCommon.VolumeType.PROVIDED:
        chrome.fileManagerPrivate.getCustomActions(this.entries_,
            function(customActions) {
              if (chrome.runtime.lastError) {
                console.error('Failed to fetch custom actions because of: ' +
                    chrome.runtime.lastError.message);
              } else {
                customActions.forEach(function(action) {
                  actions[action.id] = new CustomAction(
                      this.entries_, action.id, action.title || null,
                      this.invalidate_.bind(this));
                }.bind(this));
              }
              fulfill(actions);
            }.bind(this));
        break;

      default:
        fulfill(actions);
    }
  }.bind(this)).then(function(actions) {
    this.actions_ = actions;
  }.bind(this));

  return this.initializePromise_;
};

/**
 * @return {!Object<!Action>}
 */
ActionsModel.prototype.getActions = function() {
  return this.actions_;
};

/**
 * @param {string} id
 * @return {Action}
 */
ActionsModel.prototype.getAction = function(id) {
  return this.actions_[id] || null;
};

/**
 * Destroys the model and cancels initialization if in progress.
 */
ActionsModel.prototype.destroy = function() {
  this.destroyed_ = true;
  if (this.initializePromiseReject_ !== null) {
    var reject = this.initializePromiseReject_;
    this.initializePromiseReject_ = null;
    reject();
  }
};

/**
 * Invalidates the current actions model by emitting an invalidation event.
 * The model has to be initialized again, as the list of actions might have
 * changed.
 *
 * @private
 */
ActionsModel.prototype.invalidate_ = function() {
  if (this.initializePromiseReject_ !== null) {
    var reject = this.initializePromiseReject_;
    this.initializePromiseReject_ = null;
    this.initializePromise_ = null;
    reject();
  }
  cr.dispatchSimpleEvent(this, 'invalidated', true);
};
