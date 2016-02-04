// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Sets 'hidden' property of a cr.ui.Command instance and dispatches
 * 'hiddenChange' event manually so that associated cr.ui.MenuItem can handle
 * the event.
 * TODO(fukino): Remove this workaround when crbug.com/481941 is fixed.
 *
 * @param {boolean} value New value of hidden property.
 */
cr.ui.Command.prototype.setHidden = function(value) {
  if (value === this.hidden)
    return;

  var oldValue = this.hidden;
  this.hidden = value;
  cr.dispatchPropertyChange(this, 'hidden', value, oldValue);
};

/**
 * A command.
 * @interface
 */
var Command = function() {};

/**
 * Handles the execute event.
 * @param {!Event} event Command event.
 * @param {!FileManager} fileManager FileManager.
 */
Command.prototype.execute = function(event, fileManager) {};

/**
 * Handles the can execute event.
 * @param {!Event} event Can execute event.
 * @param {!FileManager} fileManager FileManager.
 */
Command.prototype.canExecute = function(event, fileManager) {};

/**
 * Utility for commands.
 */
var CommandUtil = {};

/**
 * Extracts entry on which command event was dispatched.
 *
 * @param {EventTarget} element Element which is the command event's target.
 * @return {Entry} Entry of the found node.
 */
CommandUtil.getCommandEntry = function(element) {
  var entries = CommandUtil.getCommandEntries(element);
  return entries.length === 0 ? null : entries[0];
};

/**
 * Extracts entries on which command event was dispatched.
 *
 * @param {EventTarget} element Element which is the command event's target.
 * @return {!Array<!Entry>} Entries of the found node.
 */
CommandUtil.getCommandEntries = function(element) {
  if (element instanceof DirectoryTree) {
    // element is a DirectoryTree.
    return element.selectedItem ? [element.selectedItem.entry] : [];
  } else if (element instanceof DirectoryItem ||
             element instanceof ShortcutItem) {
    // element are sub items in DirectoryTree.
    return element.entry ? [element.entry] : [];
  } else if (element instanceof cr.ui.List) {
    // element is a normal List (eg. the file list on the right panel).
    var entries = element.selectedItems;
    // Check if it is Entry or not by checking for toURL().
    return entries.some(function(entry) { return !('toURL' in entry); }) ?
        [] : entries;
  } else {
    return [];
  }
};

/**
 * Extracts a directory which contains entries on which command event was
 * dispatched.
 *
 * @param {EventTarget} element Element which is the command event's target.
 * @param {DirectoryModel} directoryModel
 * @return {DirectoryEntry|FakeEntry} The extracted parent entry.
 */
CommandUtil.getParentEntry = function(element, directoryModel) {
  if (element instanceof DirectoryTree) {
    if (!element.selectedItem)
      return null;
    var parentItem = element.selectedItem.parentItem;
    return parentItem ? parentItem.entry : null;
  } else if (element instanceof DirectoryItem ||
             element instanceof ShortcutItem) {
    return element.parentItem ? element.parentItem.entry : null;
  } else if (element instanceof cr.ui.List) {
    return directoryModel ? directoryModel.getCurrentDirEntry() : null;
  } else {
    return null;
  }
};

/**
 * @param {EventTarget} element
 * @param {!FileManager} fileManager
 * @return {VolumeInfo}
 */
CommandUtil.getElementVolumeInfo = function(element, fileManager) {
  if (element instanceof DirectoryTree && element.selectedItem)
    return CommandUtil.getElementVolumeInfo(element.selectedItem, fileManager);
  if (element instanceof VolumeItem)
    return element.volumeInfo;
  if (element instanceof ShortcutItem) {
    return element.entry && fileManager.volumeManager.getVolumeInfo(
        element.entry);
  }
  return null;
};

/**
 * @param {!FileManager} fileManager
 * @return {VolumeInfo}
 */
CommandUtil.getCurrentVolumeInfo = function(fileManager) {
  var currentDirEntry = fileManager.directoryModel.getCurrentDirEntry();
  return currentDirEntry ? fileManager.volumeManager.getVolumeInfo(
      currentDirEntry) : null;
};

/**
 * Obtains an entry from the give navigation model item.
 * @param {!NavigationModelItem} item Navigation model item.
 * @return {Entry} Related entry.
 * @private
 */
CommandUtil.getEntryFromNavigationModelItem_ = function(item) {
  switch (item.type) {
    case NavigationModelItemType.VOLUME:
      return /** @type {!NavigationModelVolumeItem} */ (
          item).volumeInfo.displayRoot;
    case NavigationModelItemType.SHORTCUT:
      return /** @type {!NavigationModelShortcutItem} */ (item).entry;
  }
  return null;
};

/**
 * Checks if command can be executed on drive.
 * @param {!Event} event Command event to mark.
 * @param {!FileManager} fileManager FileManager to use.
 */
CommandUtil.canExecuteEnabledOnDriveOnly = function(event, fileManager) {
  event.canExecute = fileManager.isOnDrive();
};

/**
 * Sets the command as visible only when the current volume is drive and it's
 * running as a normal app, not as a modal dialog.
 * @param {!Event} event Command event to mark.
 * @param {!FileManager} fileManager FileManager to use.
 */
CommandUtil.canExecuteVisibleOnDriveInNormalAppModeOnly =
    function(event, fileManager) {
  var enabled = fileManager.isOnDrive() &&
      !DialogType.isModal(fileManager.dialogType);
  event.canExecute = enabled;
  event.command.setHidden(!enabled);
};

/**
 * Sets as the command as always enabled.
 * @param {!Event} event Command event to mark.
 */
CommandUtil.canExecuteAlways = function(event) {
  event.canExecute = true;
};

/**
 * Sets the default handler for the commandId and prevents handling
 * the keydown events for this command. Not doing that breaks relationship
 * of original keyboard event and the command. WebKit would handle it
 * differently in some cases.
 * @param {Node} node to register command handler on.
 * @param {string} commandId Command id to respond to.
 */
CommandUtil.forceDefaultHandler = function(node, commandId) {
  var doc = node.ownerDocument;
  var command = doc.querySelector('command[id="' + commandId + '"]');
  node.addEventListener('keydown', function(e) {
    if (command.matchesEvent(e)) {
      // Prevent cr.ui.CommandManager of handling it and leave it
      // for the default handler.
      e.stopPropagation();
    }
  });
  node.addEventListener('command', function(event) {
    if (event.command.id !== commandId)
      return;
    document.execCommand(event.command.id);
    event.cancelBubble = true;
  });
  node.addEventListener('canExecute', function(event) {
    if (event.command.id !== commandId)
      return;
    event.canExecute = document.queryCommandEnabled(event.command.id);
    event.command.setHidden(false);
  });
};

/**
 * Default command.
 * @type {Command}
 */
CommandUtil.defaultCommand = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    fileManager.document.execCommand(event.command.id);
  },
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  canExecute: function(event, fileManager) {
    event.canExecute = fileManager.document.queryCommandEnabled(
        event.command.id);
  }
});

/**
 * Creates the volume switch command with index.
 * @param {number} index Volume index from 1 to 9.
 * @return {Command} Volume switch command.
 */
CommandUtil.createVolumeSwitchCommand = function(index) {
  return /** @type {Command} */ ({
    /**
     * @param {!Event} event Command event.
     * @param {!FileManager} fileManager FileManager to use.
     */
    execute: function(event, fileManager) {
      fileManager.directoryTree.activateByIndex(index - 1);
    },
    /**
     * @param {!Event} event Command event.
     * @param {!FileManager} fileManager FileManager to use.
     */
    canExecute: function(event, fileManager) {
      event.canExecute = index > 0 &&
          index <= fileManager.directoryTree.items.length;
    }
  });
};

/**
 * Returns a directory entry when only one entry is selected and it is
 * directory. Otherwise, returns null.
 * @param {FileSelection} selection Instance of FileSelection.
 * @return {?DirectoryEntry} Directory entry which is selected alone.
 */
CommandUtil.getOnlyOneSelectedDirectory = function(selection) {
  if (!selection)
    return null;
  if (selection.totalCount !== 1)
    return null;
  if (!selection.entries[0].isDirectory)
    return null;
  return /** @type {!DirectoryEntry} */(selection.entries[0]);
};

/**
 * If entry is fake or root entry, we don't show menu item for it.
 * @param {VolumeManagerWrapper} volumeManager
 * @param {(!Entry|!FakeEntry)} entry Entry or a fake entry.
 * @return {boolean} True if we should show menu item for the entry.
 */
CommandUtil.shouldShowMenuItemForEntry = function(volumeManager, entry) {
  if (!volumeManager || util.isFakeEntry(entry))
    return false;

  var volumeInfo = volumeManager.getVolumeInfo(entry);
  if (!volumeInfo)
    return false;

  return volumeInfo.displayRoot !== entry;
};

/**
 * Handle of the command events.
 * @param {!FileManager} fileManager FileManager.
 * @constructor
 * @struct
 */
var CommandHandler = function(fileManager) {
  /**
   * FileManager.
   * @type {!FileManager}
   * @private
   */
  this.fileManager_ = fileManager;

  /**
   * Command elements.
   * @type {Object<cr.ui.Command>}
   * @private
   */
  this.commands_ = {};

  // Decorate command tags in the document.
  var commands = fileManager.document.querySelectorAll('command');
  for (var i = 0; i < commands.length; i++) {
    cr.ui.Command.decorate(commands[i]);
    this.commands_[commands[i].id] = commands[i];
  }

  // Register events.
  fileManager.document.addEventListener('command', this.onCommand_.bind(this));
  fileManager.document.addEventListener(
      'canExecute', this.onCanExecute_.bind(this));
  fileManager.directoryModel.addEventListener(
      'directory-change', this.updateAvailability.bind(this));
  fileManager.volumeManager.addEventListener(
      'drive-connection-changed', this.updateAvailability.bind(this));
};

/**
 * Updates the availability of all commands.
 */
CommandHandler.prototype.updateAvailability = function() {
  for (var id in this.commands_) {
    this.commands_[id].canExecuteChange();
  }
};

/**
 * Checks if the handler should ignore the current event, eg. since there is
 * a popup dialog currently opened.
 *
 * @return {boolean} True if the event should be ignored, false otherwise.
 * @private
 */
CommandHandler.prototype.shouldIgnoreEvents_ = function() {
  // Do not handle commands, when a dialog is shown. Do not use querySelector
  // as it's much slower, and this method is executed often.
  var dialogs = this.fileManager_.document.getElementsByClassName(
      'cr-dialog-container');
  if (dialogs.length !== 0 && dialogs[0].classList.contains('shown'))
    return true;

  return false;  // Do not ignore.
};

/**
 * Handles command events.
 * @param {!Event} event Command event.
 * @private
 */
CommandHandler.prototype.onCommand_ = function(event) {
  if (this.shouldIgnoreEvents_())
    return;
  var handler = CommandHandler.COMMANDS_[event.command.id];
  handler.execute.call(/** @type {Command} */ (handler), event,
                       this.fileManager_);
};

/**
 * Handles canExecute events.
 * @param {!Event} event Can execute event.
 * @private
 */
CommandHandler.prototype.onCanExecute_ = function(event) {
  if (this.shouldIgnoreEvents_())
    return;
  var handler = CommandHandler.COMMANDS_[event.command.id];
  handler.canExecute.call(/** @type {Command} */ (handler), event,
                          this.fileManager_);
};

/**
 * Commands.
 * @type {Object<Command>}
 * @const
 * @private
 */
CommandHandler.COMMANDS_ = {};

/**
 * Unmounts external drive.
 * @type {Command}
 */
CommandHandler.COMMANDS_['unmount'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager The file manager instance.
   */
  execute: function(event, fileManager) {
    var errorCallback = function() {
      fileManager.ui.alertDialog.showHtml(
          '', str('UNMOUNT_FAILED'), null, null, null);
    };

    var volumeInfo =
        CommandUtil.getElementVolumeInfo(event.target, fileManager) ||
        CommandUtil.getCurrentVolumeInfo(fileManager);
    if (!volumeInfo) {
      errorCallback();
      return;
    }

    fileManager.volumeManager_.unmount(
        volumeInfo,
        function() {},
        errorCallback);
  },
  /**
   * @param {!Event} event Command event.
   * @this {CommandHandler}
   */
  canExecute: function(event, fileManager) {
    var volumeInfo =
        CommandUtil.getElementVolumeInfo(event.target, fileManager) ||
        CommandUtil.getCurrentVolumeInfo(fileManager);
    if (!volumeInfo) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    var volumeType = volumeInfo.volumeType;
    event.canExecute = (
        volumeType === VolumeManagerCommon.VolumeType.ARCHIVE ||
        volumeType === VolumeManagerCommon.VolumeType.REMOVABLE ||
        volumeType === VolumeManagerCommon.VolumeType.PROVIDED);
    event.command.setHidden(!event.canExecute);

    switch (volumeType) {
      case VolumeManagerCommon.VolumeType.ARCHIVE:
      case VolumeManagerCommon.VolumeType.PROVIDED:
        event.command.label = str('CLOSE_VOLUME_BUTTON_LABEL');
        break;
      case VolumeManagerCommon.VolumeType.REMOVABLE:
        event.command.label = str('UNMOUNT_DEVICE_BUTTON_LABEL');
        break;
    }
  }
});

/**
 * Formats external drive.
 * @type {Command}
 */
CommandHandler.COMMANDS_['format'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager The file manager instance.
   */
  execute: function(event, fileManager) {
    var directoryModel = fileManager.directoryModel;
    var root = CommandUtil.getCommandEntry(event.target);
    // If an entry is not found from the event target, use the current
    // directory. This can happen for the format button for unsupported and
    // unrecognized volumes.
    if (!root)
      root = directoryModel.getCurrentDirEntry();

    var volumeInfo = fileManager.volumeManager.getVolumeInfo(root);
    if (volumeInfo) {
      fileManager.ui.confirmDialog.show(
          loadTimeData.getString('FORMATTING_WARNING'),
          chrome.fileManagerPrivate.formatVolume.bind(null,
                                                      volumeInfo.volumeId),
          null, null);
    }
  },
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager The file manager instance.
   */
  canExecute: function(event, fileManager) {
    var directoryModel = fileManager.directoryModel;
    var root = CommandUtil.getCommandEntry(event.target);
    // |root| is null for unrecognized volumes. Regard such volumes as writable
    // so that the format command is enabled.
    var isReadOnly = root && fileManager.isOnReadonlyDirectory();
    // See the comment in execute() for why doing this.
    if (!root)
      root = directoryModel.getCurrentDirEntry();
    var location = root && fileManager.volumeManager.getLocationInfo(root);
    var removable = location && location.rootType ===
        VolumeManagerCommon.RootType.REMOVABLE;
    event.canExecute = removable && !isReadOnly;
    event.command.setHidden(!removable);
  }
});

/**
 * Initiates new folder creation.
 * @type {Command}
 */
CommandHandler.COMMANDS_['new-folder'] = (function() {
  /**
   * @constructor
   * @struct
   */
  var NewFolderCommand = function() {};

  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  NewFolderCommand.prototype.execute = function(event, fileManager) {
    var targetDirectory;
    var executedFromDirectoryTree;

    if (event.target instanceof DirectoryTree) {
      targetDirectory = event.target.selectedItem.entry;
      executedFromDirectoryTree = true;
    } else if (event.target instanceof DirectoryItem) {
      targetDirectory = event.target.entry;
      executedFromDirectoryTree = true;
    } else {
      targetDirectory = fileManager.directoryModel.getCurrentDirEntry();
      executedFromDirectoryTree = false;
    }

    var directoryModel = fileManager.directoryModel;
    var directoryTree = fileManager.ui.directoryTree;
    var listContainer = fileManager.ui.listContainer;

    this.generateNewDirectoryName_(targetDirectory).then(function(newName) {
      if (!executedFromDirectoryTree)
        listContainer.startBatchUpdates();

      return new Promise(targetDirectory.getDirectory.bind(targetDirectory,
          newName,
          {create: true, exclusive: true})).then(function(newDirectory) {
            metrics.recordUserAction('CreateNewFolder');

            // Select new directory and start rename operation.
            if (executedFromDirectoryTree) {
              directoryTree.updateAndSelectNewDirectory(
                  targetDirectory, newDirectory);
              fileManager.getDirectoryTreeNamingController().attachAndStart(
                  assert(fileManager.ui.directoryTree.selectedItem));
            } else {
              directoryModel.updateAndSelectNewDirectory(
                  newDirectory).then(function() {
                listContainer.endBatchUpdates();
                fileManager.namingController.initiateRename();
              }, function() {
                listContainer.endBatchUpdates();
              });
            }
          }, function(error) {
            if (!executedFromDirectoryTree)
              listContainer.endBatchUpdates();

            fileManager.ui.alertDialog.show(
                strf('ERROR_CREATING_FOLDER',
                     newName,
                     util.getFileErrorString(error.name)),
                null, null);
          });
    });
  };

  /**
   * Generates new directory name.
   * @param {!DirectoryEntry} parentDirectory
   * @param {number=} opt_index
   * @private
   */
  NewFolderCommand.prototype.generateNewDirectoryName_ = function(
      parentDirectory, opt_index) {
    var index = opt_index || 0;

    var defaultName = str('DEFAULT_NEW_FOLDER_NAME');
    var newName = index === 0 ? defaultName :
        defaultName + ' (' + index + ')';

    return new Promise(parentDirectory.getDirectory.bind(
        parentDirectory, newName, {create: false})).then(function(newEntry) {
      return this.generateNewDirectoryName_(parentDirectory, index + 1);
    }.bind(this)).catch(function() {
      return newName;
    });
  };

  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  NewFolderCommand.prototype.canExecute = function(event, fileManager) {
    if (event.target instanceof DirectoryItem ||
        event.target instanceof DirectoryTree) {
      var entry = CommandUtil.getCommandEntry(event.target);
      if (!entry || !CommandUtil.shouldShowMenuItemForEntry(
          fileManager.volumeManager, entry)) {
        event.canExecute = false;
        event.command.setHidden(true);
        return;
      }

      var locationInfo = fileManager.volumeManager.getLocationInfo(entry);
      event.canExecute = locationInfo && !locationInfo.isReadOnly;
      event.command.setHidden(false);
    } else {
      var directoryModel = fileManager.directoryModel;
      event.canExecute = !fileManager.isOnReadonlyDirectory() &&
                         !fileManager.namingController.isRenamingInProgress() &&
                         !directoryModel.isSearching() &&
                         !directoryModel.isScanning();
      event.command.setHidden(false);
    }
  };

  return new NewFolderCommand();
})();

/**
 * Initiates new window creation.
 * @type {Command}
 */
CommandHandler.COMMANDS_['new-window'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    fileManager.backgroundPage.launchFileManager({
      currentDirectoryURL: fileManager.getCurrentDirectoryEntry() &&
          fileManager.getCurrentDirectoryEntry().toURL()
    });
  },
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  canExecute: function(event, fileManager) {
    event.canExecute =
        fileManager.getCurrentDirectoryEntry() &&
        (fileManager.dialogType === DialogType.FULL_PAGE);
  }
});

CommandHandler.COMMANDS_['toggle-hidden-files'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    var isFilterHiddenOn = !fileManager.fileFilter.isFilterHiddenOn();
    fileManager.fileFilter.setFilterHidden(isFilterHiddenOn);
    event.command.checked = /* is show hidden files */!isFilterHiddenOn;
  },
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  canExecute: CommandUtil.canExecuteAlways
});

/**
 * Toggles drive sync settings.
 * @type {Command}
 */
CommandHandler.COMMANDS_['drive-sync-settings'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    // If checked, the sync is disabled.
    var nowCellularDisabled =
        fileManager.ui.gearMenu.syncButton.hasAttribute('checked');
    var changeInfo = {cellularDisabled: !nowCellularDisabled};
    chrome.fileManagerPrivate.setPreferences(changeInfo);
  },
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  canExecute: function(event, fileManager) {
    event.canExecute = fileManager.shouldShowDriveSettings() &&
        fileManager.volumeManager.getDriveConnectionState().
        hasCellularNetworkAccess;
    event.command.setHidden(!event.canExecute);
  }
});

/**
 * Toggles drive hosted settings.
 * @type {Command}
 */
CommandHandler.COMMANDS_['drive-hosted-settings'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    // If checked, showing drive hosted files is enabled.
    var nowHostedFilesEnabled =
        fileManager.ui.gearMenu.hostedButton.hasAttribute('checked');
    var nowHostedFilesDisabled = !nowHostedFilesEnabled;
    /*
    var changeInfo = {hostedFilesDisabled: !nowHostedFilesDisabled};
    */
    var changeInfo = {};
    changeInfo['hostedFilesDisabled'] = !nowHostedFilesDisabled;
    chrome.fileManagerPrivate.setPreferences(changeInfo);
  },
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  canExecute: function(event, fileManager) {
    event.canExecute = fileManager.shouldShowDriveSettings();
    event.command.setHidden(!event.canExecute);
  }
});

/**
 * Deletes selected files.
 * @type {Command}
 */
CommandHandler.COMMANDS_['delete'] = (function() {
  /**
   * @constructor
   * @implements {Command}
   */
  var DeleteCommand = function() {};

  DeleteCommand.prototype = {
    /**
     * @param {!Event} event Command event.
     * @param {!FileManager} fileManager FileManager to use.
     */
    execute: function(event, fileManager) {
      var entries = CommandUtil.getCommandEntries(event.target);

      // Execute might be called without a call of canExecute method,
      // e.g. called directly from code. Double check here not to delete
      // undeletable entries.
      if (!entries.every(CommandUtil.shouldShowMenuItemForEntry.bind(
              null, fileManager.volumeManager)) ||
          this.containsReadOnlyEntry_(entries, fileManager))
        return;

      var message = entries.length === 1 ?
          strf('GALLERY_CONFIRM_DELETE_ONE', entries[0].name) :
          strf('GALLERY_CONFIRM_DELETE_SOME', entries.length);

      fileManager.ui.deleteConfirmDialog.show(message, function() {
        fileManager.fileOperationManager.deleteEntries(entries);
      }, null, null);
    },

    /**
     * @param {!Event} event Command event.
     * @param {!FileManager} fileManager FileManager to use.
     */
    canExecute: function(event, fileManager) {
      var entries = CommandUtil.getCommandEntries(event.target);

      // If entries contain fake or root entry, hide delete option.
      if (!entries.every(CommandUtil.shouldShowMenuItemForEntry.bind(
              null, fileManager.volumeManager))) {
        event.canExecute = false;
        event.command.setHidden(true);
        return;
      }

      event.canExecute = entries.length > 0 &&
          !this.containsReadOnlyEntry_(entries, fileManager);
      event.command.setHidden(false);
    },

    /**
     * @param {!Array<!Entry>} entries
     * @param {!FileManager} fileManager
     * @return {boolean} True if entries contain read only entry.
     */
    containsReadOnlyEntry_: function(entries, fileManager) {
      return entries.some(function(entry) {
        var locationInfo = fileManager.volumeManager.getLocationInfo(entry);
        return locationInfo && locationInfo.isReadOnly;
      });
    }
  };

  return new DeleteCommand();
})();

/**
 * Pastes files from clipboard.
 * @type {Command}
 */
CommandHandler.COMMANDS_['paste'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    fileManager.document.execCommand(event.command.id);
  },
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  canExecute: function(event, fileManager) {
    var fileTransferController = fileManager.fileTransferController;

    event.canExecute = !!fileTransferController &&
        fileTransferController.queryPasteCommandEnabled(
            fileManager.directoryModel.getCurrentDirEntry());

    // Hide this command if only one folder is selected.
    event.command.setHidden(!!CommandUtil.getOnlyOneSelectedDirectory(
        fileManager.getSelection()));
  }
});

/**
 * Pastes files from clipboard into the selected folder.
 * @type {Command}
 */
CommandHandler.COMMANDS_['paste-into-folder'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    var entries = CommandUtil.getCommandEntries(event.target);
    if (entries.length !== 1 || !entries[0].isDirectory ||
        !CommandUtil.shouldShowMenuItemForEntry(
            fileManager.volumeManager, entries[0])) {
      return;
    }

    // This handler tweaks the Event object for 'paste' event so that
    // the FileTransferController can distinguish this 'paste-into-folder'
    // command and know the destination directory.
    var handler = function(inEvent) {
      inEvent.destDirectory = entries[0];
    };
    fileManager.document.addEventListener('paste', handler, true);
    fileManager.document.execCommand('paste');
    fileManager.document.removeEventListener('paste', handler, true);
  },
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  canExecute: function(event, fileManager) {
    var entries = CommandUtil.getCommandEntries(event.target);

    // Show this item only when one directory is selected.
    if (entries.length !== 1 || !entries[0].isDirectory ||
        !CommandUtil.shouldShowMenuItemForEntry(
            fileManager.volumeManager, entries[0])) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    var fileTransferController = fileManager.fileTransferController;
    var directoryEntry = /** @type {DirectoryEntry|FakeEntry} */ (entries[0]);
    event.canExecute = !!fileTransferController &&
        fileTransferController.queryPasteCommandEnabled(directoryEntry);
    event.command.setHidden(false);
  }
});

CommandHandler.COMMANDS_['cut'] = CommandUtil.defaultCommand;
CommandHandler.COMMANDS_['copy'] = CommandUtil.defaultCommand;

/**
 * Initiates file renaming.
 * @type {Command}
 */
CommandHandler.COMMANDS_['rename'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    if (event.target instanceof DirectoryTree) {
      var directoryTree = event.target;
      assert(fileManager.getDirectoryTreeNamingController()).attachAndStart(
          assert(directoryTree.selectedItem));
    } else if (event.target instanceof DirectoryItem) {
      var directoryItem = event.target;
      assert(fileManager.getDirectoryTreeNamingController()).attachAndStart(
          directoryItem);
    } else {
      fileManager.namingController.initiateRename();
    }
  },
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  canExecute: function(event, fileManager) {
    var entries = CommandUtil.getCommandEntries(event.target);
    if (entries.length === 0 ||
        !CommandUtil.shouldShowMenuItemForEntry(
            fileManager.volumeManager, entries[0])) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    var parentEntry =
        CommandUtil.getParentEntry(event.target, fileManager.directoryModel);
    var locationInfo = parentEntry ?
        fileManager.volumeManager.getLocationInfo(parentEntry) : null;
    event.canExecute =
        entries.length === 1 && !!locationInfo && !locationInfo.isReadOnly;
    event.command.setHidden(false);
  }
});

/**
 * Opens drive help.
 * @type {Command}
 */
CommandHandler.COMMANDS_['volume-help'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    if (fileManager.isOnDrive())
      util.visitURL(str('GOOGLE_DRIVE_HELP_URL'));
    else
      util.visitURL(str('FILES_APP_HELP_URL'));
  },
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  canExecute: function(event, fileManager) {
    // Hides the help menu in modal dialog mode. It does not make much sense
    // because after all, users cannot view the help without closing, and
    // besides that the help page is about Files.app as an app, not about the
    // dialog mode itself. It can also lead to hard-to-fix bug crbug.com/339089.
    var hideHelp = DialogType.isModal(fileManager.dialogType);
    event.canExecute = !hideHelp;
    event.command.setHidden(hideHelp);
    fileManager.document_.getElementById('help-separator').hidden = hideHelp;
  }
});

/**
 * Opens drive buy-more-space url.
 * @type {Command}
 */
CommandHandler.COMMANDS_['drive-buy-more-space'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    util.visitURL(str('GOOGLE_DRIVE_BUY_STORAGE_URL'));
  },
  canExecute: CommandUtil.canExecuteVisibleOnDriveInNormalAppModeOnly
});

/**
 * Opens drive.google.com.
 * @type {Command}
 */
CommandHandler.COMMANDS_['drive-go-to-drive'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    util.visitURL(str('GOOGLE_DRIVE_ROOT_URL'));
  },
  canExecute: CommandUtil.canExecuteVisibleOnDriveInNormalAppModeOnly
});

/**
 * Opens a file with default task.
 * @type {Command}
 */
CommandHandler.COMMANDS_['default-task'] = /** @type {Command} */ ({
  execute: function(event, fileManager) {
    fileManager.taskController.executeDefaultTask();
  },
  canExecute: function(event, fileManager) {
    var canExecute = fileManager.taskController.canExecuteDefaultTask();
    event.canExecute = canExecute;
    event.command.setHidden(!canExecute);
  }
});

/**
 * Displays open with dialog for current selection.
 * @type {Command}
 */
CommandHandler.COMMANDS_['open-with'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    fileManager.taskController.getFileTasks().then(function(tasks) {
      tasks.showTaskPicker(fileManager.ui.defaultTaskPicker,
          str('OPEN_WITH_BUTTON_LABEL'),
          '',
          function(task) {
            tasks.execute(task.taskId);
          },
          false);
    })
    .catch(function(error) {
      if (error)
        console.error(error.stack || error);
    });
  },
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  canExecute: function(event, fileManager) {
    var canExecute = fileManager.taskController.canExecuteOpenWith();
    event.canExecute = canExecute;
    event.command.setHidden(!canExecute);
  }
});

/**
 * Focuses search input box.
 * @type {Command}
 */
CommandHandler.COMMANDS_['search'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    // Cancel item selection.
    fileManager.directoryModel.clearSelection();

    // Focus the search box.
    var element = fileManager.document.querySelector('#search-box input');
    element.focus();
    element.select();
  },
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  canExecute: function(event, fileManager) {
    event.canExecute = !fileManager.namingController.isRenamingInProgress();
  }
});

/**
 * Activates the n-th volume.
 * @type {Command}
 */
CommandHandler.COMMANDS_['volume-switch-1'] =
    CommandUtil.createVolumeSwitchCommand(1);
CommandHandler.COMMANDS_['volume-switch-2'] =
    CommandUtil.createVolumeSwitchCommand(2);
CommandHandler.COMMANDS_['volume-switch-3'] =
    CommandUtil.createVolumeSwitchCommand(3);
CommandHandler.COMMANDS_['volume-switch-4'] =
    CommandUtil.createVolumeSwitchCommand(4);
CommandHandler.COMMANDS_['volume-switch-5'] =
    CommandUtil.createVolumeSwitchCommand(5);
CommandHandler.COMMANDS_['volume-switch-6'] =
    CommandUtil.createVolumeSwitchCommand(6);
CommandHandler.COMMANDS_['volume-switch-7'] =
    CommandUtil.createVolumeSwitchCommand(7);
CommandHandler.COMMANDS_['volume-switch-8'] =
    CommandUtil.createVolumeSwitchCommand(8);
CommandHandler.COMMANDS_['volume-switch-9'] =
    CommandUtil.createVolumeSwitchCommand(9);

/**
 * Flips 'available offline' flag on the file.
 * @type {Command}
 */
CommandHandler.COMMANDS_['toggle-pinned'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event
   * @param {!FileManager} fileManager
   */
  execute: function(event, fileManager) {
    var actionsModel = fileManager.actionsController.getActionsModelFor(
        event.target);
    var saveForOfflineAction = actionsModel ? actionsModel.getAction(
        ActionsModel.CommonActionId.SAVE_FOR_OFFLINE) : null;
    var offlineNotNeededAction = actionsModel ? actionsModel.getAction(
        ActionsModel.CommonActionId.OFFLINE_NOT_NECESSARY) : null;
    // Saving for offline has a priority if both actions are available.
    var action = saveForOfflineAction || offlineNotNeededAction;
    if (action)
      action.execute();
  },

  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  canExecute: function(event, fileManager) {
    var actionsModel = fileManager.actionsController.getActionsModelFor(
        event.target);
    var saveForOfflineAction = actionsModel ? actionsModel.getAction(
        ActionsModel.CommonActionId.SAVE_FOR_OFFLINE) : null;
    var offlineNotNeededAction = actionsModel ? actionsModel.getAction(
        ActionsModel.CommonActionId.OFFLINE_NOT_NECESSARY) : null;
    var action = saveForOfflineAction || offlineNotNeededAction;

    event.canExecute = action && action.canExecute();
    // If model is not computed yet, then keep the previous visibility to avoid
    // flickering.
    if (actionsModel) {
      event.command.setHidden(actionsModel && !action);
      event.command.checked = !!offlineNotNeededAction;
    }
  }
});

/**
 * Creates zip file for current selection.
 * @type {Command}
 */
CommandHandler.COMMANDS_['zip-selection'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    var dirEntry = fileManager.getCurrentDirectoryEntry();
    if (!dirEntry)
      return;
    var selectionEntries = fileManager.getSelection().entries;
    fileManager.fileOperationManager_.zipSelection(
        /** @type {!DirectoryEntry} */ (dirEntry), selectionEntries);
  },
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  canExecute: function(event, fileManager) {
    var dirEntry = fileManager.getCurrentDirectoryEntry();
    var selection = fileManager.getSelection();
    event.canExecute =
        dirEntry &&
        !fileManager.isOnReadonlyDirectory() &&
        !fileManager.isOnDrive() &&
        !fileManager.isOnMTP() &&
        selection && selection.totalCount > 0;
  }
});

/**
 * Shows the share dialog for the current selection (single only).
 * @type {Command}
 */
CommandHandler.COMMANDS_['share'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    // To toolbar buttons are always related to the file list, even though the
    // focus is on the navigation list. This assumption will break once we add
    // Share to the context menu on the navigation list. crbug.com/530418
    var actionsModel = fileManager.actionsController.getActionsModelForContext(
        ActionsController.Context.FILE_LIST);
    var action = actionsModel ? actionsModel.getAction(
        ActionsModel.CommonActionId.SHARE) : null;
    if (action)
      action.execute();
  },
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  canExecute: function(event, fileManager) {
    var actionsModel = fileManager.actionsController.getActionsModelForContext(
        ActionsController.Context.FILE_LIST);
    var action = actionsModel ? actionsModel.getAction(
        ActionsModel.CommonActionId.SHARE) : null;
    event.canExecute = action && action.canExecute();
    // If model is not computed yet, then keep the previous visibility to avoid
    // flickering.
    if (actionsModel)
      event.command.setHidden(actionsModel && !action);
  }
});

/**
 * Creates a shortcut of the selected folder (single only).
 * @type {Command}
 */
CommandHandler.COMMANDS_['create-folder-shortcut'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager The file manager instance.
   */
  execute: function(event, fileManager) {
    var actionsModel = fileManager.actionsController.getActionsModelFor(
        event.target);
    var action = actionsModel ? actionsModel.getAction(
        ActionsModel.InternalActionId.CREATE_FOLDER_SHORTCUT) : null;
    if (action)
      action.execute();
  },
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  canExecute: function(event, fileManager) {
    var actionsModel = fileManager.actionsController.getActionsModelFor(
        event.target);
    var action = actionsModel ? actionsModel.getAction(
        ActionsModel.InternalActionId.CREATE_FOLDER_SHORTCUT) : null;
    event.canExecute = action && action.canExecute();
    if (actionsModel)
      event.command.setHidden(!action);
  }
});

/**
 * Removes the folder shortcut.
 * @type {Command}
 */
CommandHandler.COMMANDS_['remove-folder-shortcut'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager The file manager instance.
   */
  execute: function(event, fileManager) {
    var actionsModel = fileManager.actionsController.getActionsModelFor(
        event.target);
    var action = actionsModel ? actionsModel.getAction(
        ActionsModel.InternalActionId.REMOVE_FOLDER_SHORTCUT) : null;
    if (action)
      action.execute();
  },
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  canExecute: function(event, fileManager) {
    var actionsModel = fileManager.actionsController.getActionsModelFor(
        event.target);
    var action = actionsModel ? actionsModel.getAction(
        ActionsModel.InternalActionId.REMOVE_FOLDER_SHORTCUT) : null;
    event.canExecute = action && action.canExecute();
    if (actionsModel)
      event.command.setHidden(!action);
  }
});

/**
 * Zoom in to the Files.app.
 * @type {Command}
 */
CommandHandler.COMMANDS_['zoom-in'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    chrome.fileManagerPrivate.zoom('in');
  },
  canExecute: CommandUtil.canExecuteAlways
});

/**
 * Zoom out from the Files.app.
 * @type {Command}
 */
CommandHandler.COMMANDS_['zoom-out'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    chrome.fileManagerPrivate.zoom('out');
  },
  canExecute: CommandUtil.canExecuteAlways
});

/**
 * Reset the zoom factor.
 * @type {Command}
 */
CommandHandler.COMMANDS_['zoom-reset'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    chrome.fileManagerPrivate.zoom('reset');
  },
  canExecute: CommandUtil.canExecuteAlways
});

/**
 * Sort the file list by name (in ascending order).
 * @type {Command}
 */
CommandHandler.COMMANDS_['sort-by-name'] = /** @type {Command} */ ({
  execute: function(event, fileManager) {
    if (fileManager.directoryModel.getFileList())
      fileManager.directoryModel.getFileList().sort('name', 'asc');
  },
  canExecute: CommandUtil.canExecuteAlways
});

/**
 * Sort the file list by size (in descending order).
 * @type {Command}
 */
CommandHandler.COMMANDS_['sort-by-size'] = /** @type {Command} */ ({
  execute: function(event, fileManager) {
    if (fileManager.directoryModel.getFileList())
      fileManager.directoryModel.getFileList().sort('size', 'desc');
  },
  canExecute: CommandUtil.canExecuteAlways
});

/**
 * Sort the file list by type (in ascending order).
 * @type {Command}
 */
CommandHandler.COMMANDS_['sort-by-type'] = /** @type {Command} */ ({
  execute: function(event, fileManager) {
    if (fileManager.directoryModel.getFileList())
      fileManager.directoryModel.getFileList().sort('type', 'asc');
  },
  canExecute: CommandUtil.canExecuteAlways
});

/**
 * Sort the file list by date-modified (in descending order).
 * @type {Command}
 */
CommandHandler.COMMANDS_['sort-by-date'] = /** @type {Command} */ ({
  execute: function(event, fileManager) {
    if (fileManager.directoryModel.getFileList())
      fileManager.directoryModel.getFileList().sort('modificationTime', 'desc');
  },
  canExecute: CommandUtil.canExecuteAlways
});

/**
 * Open inspector for foreground page.
 * @type {Command}
 */
CommandHandler.COMMANDS_['inspect-normal'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    chrome.fileManagerPrivate.openInspector('normal');
  },
  canExecute: CommandUtil.canExecuteAlways
});

/**
 * Open inspector for foreground page and bring focus to the console.
 * @type {Command}
 */
CommandHandler.COMMANDS_['inspect-console'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    chrome.fileManagerPrivate.openInspector('console');
  },
  canExecute: CommandUtil.canExecuteAlways
});

/**
 * Open inspector for foreground page in inspect element mode.
 * @type {Command}
 */
CommandHandler.COMMANDS_['inspect-element'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    chrome.fileManagerPrivate.openInspector('element');
  },
  canExecute: CommandUtil.canExecuteAlways
});

/**
 * Open inspector for background page.
 * @type {Command}
 */
CommandHandler.COMMANDS_['inspect-background'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    chrome.fileManagerPrivate.openInspector('background');
  },
  canExecute: CommandUtil.canExecuteAlways
});

/**
 * Shows a suggest dialog with new services to be added to the left nav.
 * @type {Command}
 */
CommandHandler.COMMANDS_['install-new-extension'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    fileManager.ui.suggestAppsDialog.showProviders(
        function(result, itemId) {
          // If a new provider is installed, then launch it so the configuration
          // dialog is shown (if it's available).
          if (result === SuggestAppsDialog.Result.SUCCESS)
            fileManager.providersModel.requestMount(assert(itemId));
        });
  },
  canExecute: function(event, fileManager) {
    event.canExecute = fileManager.dialogType === DialogType.FULL_PAGE;
    event.command.setHidden(!event.canExecute);
  }
});

/**
 * Configures the currently selected volume.
 */
CommandHandler.COMMANDS_['configure'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    var volumeInfo =
        CommandUtil.getElementVolumeInfo(event.target, fileManager) ||
        CommandUtil.getCurrentVolumeInfo(fileManager);
    if (volumeInfo && volumeInfo.configurable)
      fileManager.volumeManager.configure(volumeInfo);
  },
  canExecute: function(event, fileManager) {
    var volumeInfo =
        CommandUtil.getElementVolumeInfo(event.target, fileManager) ||
        CommandUtil.getCurrentVolumeInfo(fileManager);
    event.canExecute = volumeInfo && volumeInfo.configurable;
    event.command.setHidden(!event.canExecute);
  }
});

/**
 * Refreshes the currently selected directory.
 */
CommandHandler.COMMANDS_['refresh'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!FileManager} fileManager FileManager to use.
   */
  execute: function(event, fileManager) {
    fileManager.directoryModel.rescan(true /* refresh */);
    fileManager.spinnerController.blink();
  },
  canExecute: function(event, fileManager) {
    var currentDirEntry = fileManager.directoryModel.getCurrentDirEntry();
    var volumeInfo = currentDirEntry &&
        fileManager.volumeManager.getVolumeInfo(currentDirEntry);
    event.canExecute = volumeInfo && !volumeInfo.watchable;
    event.command.setHidden(!event.canExecute ||
        fileManager.directoryModel.getFileListSelection().getCheckSelectMode());
  }
});
