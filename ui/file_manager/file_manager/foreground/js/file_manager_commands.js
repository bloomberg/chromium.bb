// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * TODO(dzvorygin): Here we use this hack, since 'hidden' is standard
 * attribute and we can't use it's setter as usual.
 * @param {boolean} value New value of hidden property.
 */
cr.ui.Command.prototype.setHidden = function(value) {
  this.__lookupSetter__('hidden').call(this, value);
};

/**
 * A command.
 * @interface
 */
var Command = function() {};

/**
 * Handles the execute event.
 * @param {Event} event Command event.
 * @param {FileManager} fileManager FileManager.
 */
Command.prototype.execute = function(event, fileManager) {};

/**
 * Handles the can execute event.
 * @param {Event} event Can execute event.
 * @param {FileManager} fileManager FileManager.
 */
Command.prototype.canExecute = function(event, fileManager) {};

/**
 * Utility for commands.
 */
var CommandUtil = {};

/**
 * Extracts entry on which command event was dispatched.
 *
 * @param {DirectoryTree|DirectoryItem|HTMLLIElement|cr.ui.List}
 *     element Directory to extract a path from.
 * @return {Entry} Entry of the found node.
 */
CommandUtil.getCommandEntry = function(element) {
  if (element instanceof DirectoryTree) {
    // element is a DirectoryTree.
    return element.selectedItem ? element.selectedItem.entry : null;
  } else if (element instanceof DirectoryItem ||
             element instanceof VolumeItem ||
             element instanceof ShortcutItem) {
    // element are sub items in DirectoryTree.
    return element.entry;
  } else if (element instanceof cr.ui.List) {
    // element is a normal List (eg. the file list on the right panel).
    var entry = element.selectedItem;
    // Check if it is Entry or not by checking for toURL().
    return entry && 'toURL' in entry ? entry : null;
  } else {
    console.warn('Unsupported element');
    return null;
  }
};

/**
 * Obtains an entry from the give navigation model item.
 * @param {NavigationModelItem} item Navigation model item.
 * @return {Entry} Related entry.
 * @private
 */
CommandUtil.getEntryFromNavigationModelItem_ = function(item) {
  if (item.isVolume)
    return item.volumeInfo.displayRoot;
  if (item.isShortcut)
    return item.entry;
  return null;
};

/**
 * Checks if command can be executed on drive.
 * @param {Event} event Command event to mark.
 * @param {FileManager} fileManager FileManager to use.
 */
CommandUtil.canExecuteEnabledOnDriveOnly = function(event, fileManager) {
  event.canExecute = fileManager.isOnDrive();
};

/**
 * Sets the command as visible only when the current volume is drive and it's
 * running as a normal app, not as a modal dialog.
 * @param {Event} event Command event to mark.
 * @param {FileManager} fileManager FileManager to use.
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
 * @param {Event} event Command event to mark.
 */
CommandUtil.canExecuteAlways = function(event) {
  event.canExecute = true;
};

/**
 * Returns a single selected/passed entry or null.
 * @param {Event} event Command event.
 * @param {FileManager} fileManager FileManager to use.
 * @return {FileEntry} The entry or null.
 */
CommandUtil.getSingleEntry = function(event, fileManager) {
  if (event.target.entry) {
    return event.target.entry;
  }
  var selection = fileManager.getSelection();
  if (selection.totalCount == 1) {
    return selection.entries[0];
  }
  return null;
};

/**
 * Obtains target entries that can be pinned from the selection.
 * If directories are included in the selection, it just returns an empty
 * array to avoid confusing because pinning directory is not supported
 * currently.
 *
 * @return {Array.<Entry>} Target entries.
 */
CommandUtil.getPinTargetEntries = function() {
  var hasDirectory = false;
  var results = fileManager.getSelection().entries.filter(function(entry) {
    hasDirectory = hasDirectory || entry.isDirectory;
    if (!entry || hasDirectory)
      return false;
    var metadata = fileManager.metadataCache_.getCached(entry, 'drive');
    if (!metadata || metadata.hosted)
      return false;
    entry.pinned = metadata.pinned;
    return true;
  });
  return hasDirectory ? [] : results;
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
    if (event.command.id === commandId)
      event.canExecute = document.queryCommandEnabled(event.command.id);
  });
};

/**
 * Default command.
 * @type {Command}
 */
CommandUtil.defaultCommand = {
  execute: function(event, fileManager) {
    fileManager.document.execCommand(event.command.id);
  },
  canExecute: function(event, fileManager) {
    event.canExecute = fileManager.document.queryCommandEnabled(
        event.command.id);
  }
};

/**
 * Creates the volume switch command with index.
 * @param {number} index Volume index from 1 to 9.
 * @return {Command} Volume switch command.
 */
CommandUtil.createVolumeSwitchCommand = function(index) {
  return {
    execute: function(event, fileManager) {
      fileManager.directoryTree.selectByIndex(index - 1);
    },
    canExecute: function(event, fileManager) {
      event.canExecute = index > 0 &&
          index <= fileManager.directoryTree.items.length;
    }
  };
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
  return selection.entries[0];
};

/**
 * Handle of the command events.
 * @param {FileManager} fileManager FileManager.
 * @constructor
 */
var CommandHandler = function(fileManager) {
  /**
   * FileManager.
   * @type {FileManager}
   * @private
   */
  this.fileManager_ = fileManager;

  /**
   * Command elements.
   * @type {Object.<string, cr.ui.Command>}
   * @private
   */
  this.commands_ = {};

  Object.seal(this);

  // Decorate command tags in the document.
  var commands = fileManager.document.querySelectorAll('command');
  for (var i = 0; i < commands.length; i++) {
    cr.ui.Command.decorate(commands[i]);
    this.commands_[commands[i].id] = commands[i];
  }

  // Register events.
  fileManager.document.addEventListener('command', this.onCommand_.bind(this));
  fileManager.document.addEventListener('canExecute',
                                        this.onCanExecute_.bind(this));
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
  // Do not handle commands, when a dialog is shown.
  if (this.fileManager_.document.querySelector('.cr-dialog-container.shown'))
    return true;

  return false;  // Do not ignore.
};

/**
 * Handles command events.
 * @param {Event} event Command event.
 * @private
 */
CommandHandler.prototype.onCommand_ = function(event) {
  if (this.shouldIgnoreEvents_())
    return;
  var handler = CommandHandler.COMMANDS_[event.command.id];
  handler.execute.call(this, event, this.fileManager_);
};

/**
 * Handles canExecute events.
 * @param {Event} event Can execute event.
 * @private
 */
CommandHandler.prototype.onCanExecute_ = function(event) {
  if (this.shouldIgnoreEvents_())
    return;
  var handler = CommandHandler.COMMANDS_[event.command.id];
  handler.canExecute.call(this, event, this.fileManager_);
};

/**
 * Commands.
 * @type {Object.<string, Command>}
 * @const
 * @private
 */
CommandHandler.COMMANDS_ = {};

/**
 * Unmounts external drive.
 * @type {Command}
 */
CommandHandler.COMMANDS_['unmount'] = {
  /**
   * @param {Event} event Command event.
   * @param {FileManager} fileManager The file manager instance.
   */
  execute: function(event, fileManager) {
    var root = CommandUtil.getCommandEntry(event.target);
    if (!root)
      return;
    var errorCallback = function() {
      fileManager.alert.showHtml('', str('UNMOUNT_FAILED'));
    };
    var volumeInfo = fileManager.volumeManager.getVolumeInfo(root);
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
   * @param {Event} event Command event.
   */
  canExecute: function(event, fileManager) {
    var root = CommandUtil.getCommandEntry(event.target);
    if (!root)
      return;
    var locationInfo = this.fileManager_.volumeManager.getLocationInfo(root);
    var rootType =
        locationInfo && locationInfo.isRootEntry && locationInfo.rootType;

    event.canExecute = (rootType == VolumeManagerCommon.RootType.ARCHIVE ||
                        rootType == VolumeManagerCommon.RootType.REMOVABLE ||
                        rootType == VolumeManagerCommon.RootType.PROVIDED);
    event.command.setHidden(!event.canExecute);

    switch (rootType) {
      case VolumeManagerCommon.RootType.ARCHIVE:
      case VolumeManagerCommon.RootType.PROVIDED:
        event.command.label = str('CLOSE_VOLUME_BUTTON_LABEL');
        break;
      case VolumeManagerCommon.RootType.REMOVABLE:
        event.command.label = str('UNMOUNT_DEVICE_BUTTON_LABEL');
        break;
    }
  }
};

/**
 * Formats external drive.
 * @type {Command}
 */
CommandHandler.COMMANDS_['format'] = {
  /**
   * @param {Event} event Command event.
   * @param {FileManager} fileManager The file manager instance.
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
      fileManager.confirm.show(
          loadTimeData.getString('FORMATTING_WARNING'),
          chrome.fileBrowserPrivate.formatVolume.bind(null,
                                                      volumeInfo.volumeId));
    }
  },
  /**
   * @param {Event} event Command event.
   * @param {FileManager} fileManager The file manager instance.
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
};

/**
 * Initiates new folder creation.
 * @type {Command}
 */
CommandHandler.COMMANDS_['new-folder'] = {
  execute: function(event, fileManager) {
    fileManager.createNewFolder();
  },
  canExecute: function(event, fileManager) {
    var directoryModel = fileManager.directoryModel;
    event.canExecute = !fileManager.isOnReadonlyDirectory() &&
                       !fileManager.isRenamingInProgress() &&
                       !directoryModel.isSearching() &&
                       !directoryModel.isScanning();
  }
};

/**
 * Initiates new window creation.
 * @type {Command}
 */
CommandHandler.COMMANDS_['new-window'] = {
  execute: function(event, fileManager) {
    chrome.fileBrowserPrivate.getProfiles(function(profiles,
                                                   currentId,
                                                   displayedId) {
      fileManager.backgroundPage.launchFileManager({
        currentDirectoryURL: fileManager.getCurrentDirectoryEntry() &&
            fileManager.getCurrentDirectoryEntry().toURL(),
        displayedId: currentId !== displayedId ? displayedId : undefined
      });
    });
  },
  canExecute: function(event, fileManager) {
    event.canExecute =
        fileManager.getCurrentDirectoryEntry() &&
        (fileManager.dialogType === DialogType.FULL_PAGE);
  }
};

/**
 * Toggles drive sync settings.
 * @type {Command}
 */
CommandHandler.COMMANDS_['drive-sync-settings'] = {
  execute: function(event, fileManager) {
    fileManager.toggleDriveSyncSettings();
  },
  canExecute: function(event, fileManager) {
    event.canExecute = fileManager.shouldShowDriveSettings();
    event.command.setHidden(!event.canExecute);
  }
};

/**
 * Toggles drive hosted settings.
 * @type {Command}
 */
CommandHandler.COMMANDS_['drive-hosted-settings'] = {
  execute: function(event, fileManager) {
    fileManager.toggleDriveHostedSettings();
  },
  canExecute: function(event, fileManager) {
    event.canExecute = fileManager.shouldShowDriveSettings();
    event.command.setHidden(!event.canExecute);
  }
};

/**
 * Deletes selected files.
 * @type {Command}
 */
CommandHandler.COMMANDS_['delete'] = {
  execute: function(event, fileManager) {
    var entries = fileManager.getSelection().entries;
    var message = entries.length == 1 ?
        strf('GALLERY_CONFIRM_DELETE_ONE', entries[0].name) :
        strf('GALLERY_CONFIRM_DELETE_SOME', entries.length);
    fileManager.ui.deleteConfirmDialog.show(message, function() {
      fileManager.fileOperationManager.deleteEntries(entries);
    });
  },
  canExecute: function(event, fileManager) {
    var selection = fileManager.getSelection();
    event.canExecute = !fileManager.isOnReadonlyDirectory() &&
                       selection &&
                       selection.totalCount > 0;
  }
};

/**
 * Pastes files from clipboard.
 * @type {Command}
 */
CommandHandler.COMMANDS_['paste'] = {
  execute: function(event, fileManager) {
    fileManager.document.execCommand(event.command.id);
  },
  canExecute: function(event, fileManager) {
    var fileTransferController = fileManager.fileTransferController;
    event.canExecute = (fileTransferController &&
        fileTransferController.queryPasteCommandEnabled());
    // Hide this command if only one folder is selected.
    event.command.setHidden(!!CommandUtil.getOnlyOneSelectedDirectory(
        fileManager.getSelection()));
  }
};

/**
 * Pastes files from clipboard into the selected folder.
 * @type {Command}
 */
CommandHandler.COMMANDS_['paste-into-folder'] = {
  execute: function(event, fileManager) {
    var selection = fileManager.getSelection();
    var dest = CommandUtil.getOnlyOneSelectedDirectory(selection);
    if (!dest) return;

    // This handler tweaks the Event object for 'paste' event so that
    // the FileTransferController can distinguish this 'paste-into-folder'
    // command and know the destination directory.
    var handler = function(inEvent) {
      inEvent.destDirectory = dest;
    };
    fileManager.document.addEventListener('paste', handler, true);
    fileManager.document.execCommand('paste');
    fileManager.document.removeEventListener('paste', handler, true);
  },
  canExecute: function(event, fileManager) {
    var fileTransferController = fileManager.fileTransferController;
    event.canExecute = (fileTransferController &&
        fileTransferController.queryPasteCommandEnabled());
    // Hide this command unless only one folder is selected.
    event.command.setHidden(!CommandUtil.getOnlyOneSelectedDirectory(
        fileManager.getSelection()));
  }
};

CommandHandler.COMMANDS_['cut'] = CommandUtil.defaultCommand;
CommandHandler.COMMANDS_['copy'] = CommandUtil.defaultCommand;

/**
 * Initiates file renaming.
 * @type {Command}
 */
CommandHandler.COMMANDS_['rename'] = {
  execute: function(event, fileManager) {
    fileManager.initiateRename();
  },
  canExecute: function(event, fileManager) {
    var selection = fileManager.getSelection();
    event.canExecute = !fileManager.isRenamingInProgress() &&
                       !fileManager.isOnReadonlyDirectory() &&
                       selection &&
                       selection.totalCount == 1;
  }
};

/**
 * Opens drive help.
 * @type {Command}
 */
CommandHandler.COMMANDS_['volume-help'] = {
  execute: function(event, fileManager) {
    if (fileManager.isOnDrive())
      util.visitURL(str('GOOGLE_DRIVE_HELP_URL'));
    else
      util.visitURL(str('FILES_APP_HELP_URL'));
  },
  canExecute: function(event, fileManager) {
    // Hides the help menu in modal dialog mode. It does not make much sense
    // because after all, users cannot view the help without closing, and
    // besides that the help page is about Files.app as an app, not about the
    // dialog mode itself. It can also lead to hard-to-fix bug crbug.com/339089.
    var hideHelp = DialogType.isModal(fileManager.dialogType);
    event.canExecute = !hideHelp;
    event.command.setHidden(hideHelp);
    fileManager.document_.getElementById('help-separator').hidden = hideHelp;
  },
};

/**
 * Opens drive buy-more-space url.
 * @type {Command}
 */
CommandHandler.COMMANDS_['drive-buy-more-space'] = {
  execute: function(event, fileManager) {
    util.visitURL(str('GOOGLE_DRIVE_BUY_STORAGE_URL'));
  },
  canExecute: CommandUtil.canExecuteVisibleOnDriveInNormalAppModeOnly
};

/**
 * Opens drive.google.com.
 * @type {Command}
 */
CommandHandler.COMMANDS_['drive-go-to-drive'] = {
  execute: function(event, fileManager) {
    util.visitURL(str('GOOGLE_DRIVE_ROOT_URL'));
  },
  canExecute: CommandUtil.canExecuteVisibleOnDriveInNormalAppModeOnly
};

/**
 * Displays open with dialog for current selection.
 * @type {Command}
 */
CommandHandler.COMMANDS_['open-with'] = {
  execute: function(event, fileManager) {
    var tasks = fileManager.getSelection().tasks;
    if (tasks) {
      tasks.showTaskPicker(fileManager.defaultTaskPicker,
          str('OPEN_WITH_BUTTON_LABEL'),
          '',
          function(task) {
            tasks.execute(task.taskId);
          });
    }
  },
  canExecute: function(event, fileManager) {
    var tasks = fileManager.getSelection().tasks;
    event.canExecute = tasks && tasks.size() > 1;
  }
};

/**
 * Focuses search input box.
 * @type {Command}
 */
CommandHandler.COMMANDS_['search'] = {
  execute: function(event, fileManager) {
    var element = fileManager.document.querySelector('#search-box input');
    element.focus();
    element.select();
  },
  canExecute: function(event, fileManager) {
    event.canExecute = !fileManager.isRenamingInProgress();
  }
};

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
CommandHandler.COMMANDS_['toggle-pinned'] = {
  execute: function(event, fileManager) {
    var pin = !event.command.checked;
    event.command.checked = pin;
    var entries = CommandUtil.getPinTargetEntries();
    var currentEntry;
    var error = false;
    var steps = {
      // Pick an entry and pin it.
      start: function() {
        // Check if all the entries are pinned or not.
        if (entries.length == 0)
          return;
        currentEntry = entries.shift();
        chrome.fileBrowserPrivate.pinDriveFile(
            currentEntry.toURL(),
            pin,
            steps.entryPinned);
      },

      // Check the result of pinning
      entryPinned: function() {
        // Convert to boolean.
        error = !!chrome.runtime.lastError;
        if (error && pin) {
          fileManager.metadataCache_.getOne(
              currentEntry, 'filesystem', steps.showError);
        }
        fileManager.metadataCache_.clear(currentEntry, 'drive');
        fileManager.metadataCache_.getOne(
            currentEntry, 'drive', steps.updateUI.bind(this));
      },

      // Update the user interface according to the cache state.
      updateUI: function(drive /* not used */) {
        fileManager.updateMetadataInUI_('drive', [currentEntry]);
        if (!error)
          steps.start();
      },

      // Show the error
      showError: function(filesystem) {
        fileManager.alert.showHtml(str('DRIVE_OUT_OF_SPACE_HEADER'),
                                   strf('DRIVE_OUT_OF_SPACE_MESSAGE',
                                        unescape(currentEntry.name),
                                        util.bytesToString(filesystem.size)));
      }
    };
    steps.start();
  },

  canExecute: function(event, fileManager) {
    var entries = CommandUtil.getPinTargetEntries();
    var checked = true;
    for (var i = 0; i < entries.length; i++) {
      checked = checked && entries[i].pinned;
    }
    if (entries.length > 0) {
      event.canExecute = true;
      event.command.setHidden(false);
      event.command.checked = checked;
    } else {
      event.canExecute = false;
      event.command.setHidden(true);
    }
  }
};

/**
 * Creates zip file for current selection.
 * @type {Command}
 */
CommandHandler.COMMANDS_['zip-selection'] = {
  execute: function(event, fileManager) {
    var dirEntry = fileManager.getCurrentDirectoryEntry();
    var selectionEntries = fileManager.getSelection().entries;
    fileManager.fileOperationManager_.zipSelection(dirEntry, selectionEntries);
  },
  canExecute: function(event, fileManager) {
    var dirEntry = fileManager.getCurrentDirectoryEntry();
    var selection = fileManager.getSelection();
    event.canExecute =
        dirEntry &&
        !fileManager.isOnReadonlyDirectory() &&
        !fileManager.isOnDrive() &&
        selection && selection.totalCount > 0;
  }
};

/**
 * Shows the share dialog for the current selection (single only).
 * @type {Command}
 */
CommandHandler.COMMANDS_['share'] = {
  execute: function(event, fileManager) {
    fileManager.shareSelection();
  },
  canExecute: function(event, fileManager) {
    var selection = fileManager.getSelection();
    var isDriveOffline =
        fileManager.volumeManager.getDriveConnectionState().type ===
            VolumeManagerCommon.DriveConnectionType.OFFLINE;
    event.canExecute = fileManager.isOnDrive() &&
        !isDriveOffline &&
        selection && selection.totalCount == 1;
    event.command.setHidden(!fileManager.isOnDrive());
  }
};

/**
 * Creates a shortcut of the selected folder (single only).
 * @type {Command}
 */
CommandHandler.COMMANDS_['create-folder-shortcut'] = {
  /**
   * @param {Event} event Command event.
   * @param {FileManager} fileManager The file manager instance.
   */
  execute: function(event, fileManager) {
    var entry = CommandUtil.getCommandEntry(event.target);
    if (entry)
      fileManager.createFolderShortcut(entry);
  },

  /**
   * @param {Event} event Command event.
   * @param {FileManager} fileManager The file manager instance.
   */
  canExecute: function(event, fileManager) {
    var entry = CommandUtil.getCommandEntry(event.target);
    var folderShortcutExists = entry &&
                               fileManager.folderShortcutExists(entry);

    var onlyOneFolderSelected = true;
    // Only on list, user can select multiple files. The command is enabled only
    // when a single file is selected.
    if (event.target instanceof cr.ui.List) {
      var items = event.target.selectedItems;
      onlyOneFolderSelected = (items.length == 1 && items[0].isDirectory);
    }

    var location = entry && fileManager.volumeManager.getLocationInfo(entry);
    var eligible = location && location.isEligibleForFolderShortcut;
    event.canExecute =
        eligible && onlyOneFolderSelected && !folderShortcutExists;
    event.command.setHidden(!eligible || !onlyOneFolderSelected);
  }
};

/**
 * Removes the folder shortcut.
 * @type {Command}
 */
CommandHandler.COMMANDS_['remove-folder-shortcut'] = {
  /**
   * @param {Event} event Command event.
   * @param {FileManager} fileManager The file manager instance.
   */
  execute: function(event, fileManager) {
    var entry = CommandUtil.getCommandEntry(event.target);
    if (entry)
      fileManager.removeFolderShortcut(entry);
  },

  /**
   * @param {Event} event Command event.
   * @param {FileManager} fileManager The file manager instance.
   */
  canExecute: function(event, fileManager) {
    var entry = CommandUtil.getCommandEntry(event.target);
    var location = entry && fileManager.volumeManager.getLocationInfo(entry);

    var eligible = location && location.isEligibleForFolderShortcut;
    var isShortcut = entry && fileManager.folderShortcutExists(entry);
    event.canExecute = isShortcut && eligible;
    event.command.setHidden(!event.canExecute);
  }
};

/**
 * Zoom in to the Files.app.
 * @type {Command}
 */
CommandHandler.COMMANDS_['zoom-in'] = {
  execute: function(event, fileManager) {
    chrome.fileBrowserPrivate.zoom('in');
  },
  canExecute: CommandUtil.canExecuteAlways
};

/**
 * Zoom out from the Files.app.
 * @type {Command}
 */
CommandHandler.COMMANDS_['zoom-out'] = {
  execute: function(event, fileManager) {
    chrome.fileBrowserPrivate.zoom('out');
  },
  canExecute: CommandUtil.canExecuteAlways
};

/**
 * Reset the zoom factor.
 * @type {Command}
 */
CommandHandler.COMMANDS_['zoom-reset'] = {
  execute: function(event, fileManager) {
    chrome.fileBrowserPrivate.zoom('reset');
  },
  canExecute: CommandUtil.canExecuteAlways
};

/**
 * Open inspector for foreground page.
 * @type {Command}
 */
CommandHandler.COMMANDS_['inspect-normal'] = {
  execute: function(event, fileManager) {
    chrome.fileBrowserPrivate.openInspector('normal');
  },
  canExecute: CommandUtil.canExecuteAlways
};

/**
 * Open inspector for foreground page and bring focus to the console.
 * @type {Command}
 */
CommandHandler.COMMANDS_['inspect-console'] = {
  execute: function(event, fileManager) {
    chrome.fileBrowserPrivate.openInspector('console');
  },
  canExecute: CommandUtil.canExecuteAlways
};

/**
 * Open inspector for foreground page in inspect element mode.
 * @type {Command}
 */
CommandHandler.COMMANDS_['inspect-element'] = {
  execute: function(event, fileManager) {
    chrome.fileBrowserPrivate.openInspector('element');
  },
  canExecute: CommandUtil.canExecuteAlways
};

/**
 * Open inspector for background page.
 * @type {Command}
 */
CommandHandler.COMMANDS_['inspect-background'] = {
  execute: function(event, fileManager) {
    chrome.fileBrowserPrivate.openInspector('background');
  },
  canExecute: CommandUtil.canExecuteAlways
};
