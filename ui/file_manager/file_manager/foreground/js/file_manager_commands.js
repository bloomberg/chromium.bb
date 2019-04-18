// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A command.
 * @interface
 */
const Command = function() {};

/**
 * Handles the execute event.
 * @param {!Event} event Command event.
 * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps.
 */
Command.prototype.execute = (event, fileManager) => {};

/**
 * Handles the can execute event.
 * @param {!Event} event Can execute event.
 * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps.
 */
Command.prototype.canExecute = (event, fileManager) => {};

/**
 * Utility for commands.
 */
const CommandUtil = {};

/**
 * Extracts entry on which command event was dispatched.
 *
 * @param {!CommandHandlerDeps} fileManager
 * @param {EventTarget} element Element which is the command event's target.
 * @return {Entry|FakeEntry} Entry of the found node.
 */
CommandUtil.getCommandEntry = (fileManager, element) => {
  const entries = CommandUtil.getCommandEntries(fileManager, element);
  return entries.length === 0 ? null : entries[0];
};

/**
 * Extracts entries on which command event was dispatched.
 *
 * @param {!CommandHandlerDeps} fileManager
 * @param {EventTarget} element Element which is the command event's target.
 * @return {!Array<!Entry>} Entries of the found node.
 */
CommandUtil.getCommandEntries = (fileManager, element) => {
  if (element && element.entry) {
    // DirectoryItem has "entry" attribute.
    return [element.entry];
  } else if (element.selectedItem && element.selectedItem.entry) {
    // DirectoryTree has the selected item.
    return [element.selectedItem.entry];
  } else if (element.selectedItems && element.selectedItems.length) {
    // File list (cr.ui.List).
    const entries = element.selectedItems;
    // Check if it is Entry or not by checking for toURL().
    return entries.filter(entry => ('toURL' in entry));
  } else if (fileManager.ui.actionbar.contains(/** @type {Node} */ (element))) {
    // Commands in the action bar can only act in the currently selected files.
    return fileManager.getSelection().entries;
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
 * @return {DirectoryEntry|FilesAppEntry} The extracted parent entry.
 */
CommandUtil.getParentEntry = (element, directoryModel) => {
  if (element && element.selectedItem && element.selectedItem.parentItem &&
      element.selectedItem.parentItem.entry) {
    // DirectoryTree has the selected item.
    return element.selectedItem.parentItem.entry;
  } else if (element.parentItem && element.parentItem.entry) {
    // DirectoryItem has parentItem.
    return element.parentItem.entry;
  } else if (element instanceof cr.ui.List) {
    return directoryModel ? directoryModel.getCurrentDirEntry() : null;
  } else {
    return null;
  }
};

/**
 * Returns VolumeInfo from the current target for commands, based on |element|.
 * It can be from directory tree (clicked item or selected item), or from file
 * list selected items; or null if can determine it.
 *
 * @param {EventTarget} element
 * @param {!CommandHandlerDeps} fileManager
 * @return {VolumeInfo}
 */
CommandUtil.getElementVolumeInfo = (element, fileManager) => {
  if (element.volumeInfo) {
    return element.volumeInfo;
  }
  const entry = CommandUtil.getCommandEntry(fileManager, element);
  return entry && fileManager.volumeManager.getVolumeInfo(entry);
};

/**
 * Sets the command as visible only when the current volume is drive and it's
 * running as a normal app, not as a modal dialog.
 * NOTE: This doesn't work for directory tree menu, because user can right-click
 * on any visible volume.
 * @param {!Event} event Command event to mark.
 * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
 */
CommandUtil.canExecuteVisibleOnDriveInNormalAppModeOnly =
    (event, fileManager) => {
      const enabled = fileManager.directoryModel.isOnDrive() &&
          !DialogType.isModal(fileManager.dialogType);
      event.canExecute = enabled;
      event.command.setHidden(!enabled);
    };

/**
 * Sets as the command as always enabled.
 * @param {!Event} event Command event to mark.
 */
CommandUtil.canExecuteAlways = event => {
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
CommandUtil.forceDefaultHandler = (node, commandId) => {
  const doc = node.ownerDocument;
  const command = /** @type {!cr.ui.Command} */ (
      doc.querySelector('command[id="' + commandId + '"]'));
  node.addEventListener('keydown', e => {
    if (command.matchesEvent(e)) {
      // Prevent cr.ui.CommandManager of handling it and leave it
      // for the default handler.
      e.stopPropagation();
    }
  });
  node.addEventListener('command', event => {
    if (event.command.id !== commandId) {
      return;
    }
    document.execCommand(event.command.id);
    event.cancelBubble = true;
  });
  node.addEventListener('canExecute', event => {
    if (event.command.id !== commandId) {
      return;
    }
    event.canExecute = document.queryCommandEnabled(event.command.id);
    event.command.setHidden(false);
  });
};

/**
 * Creates the volume switch command with index.
 * @param {number} index Volume index from 1 to 9.
 * @return {Command} Volume switch command.
 */
CommandUtil.createVolumeSwitchCommand = index => {
  return /** @type {Command} */ ({
    /**
     * @param {!Event} event Command event.
     * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
     */
    execute: function(event, fileManager) {
      fileManager.directoryTree.activateByIndex(index - 1);
    },
    /**
     * @param {!Event} event Command event.
     * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
     */
    canExecute: function(event, fileManager) {
      event.canExecute =
          index > 0 && index <= fileManager.directoryTree.items.length;
    }
  });
};

/**
 * Returns a directory entry when only one entry is selected and it is
 * directory. Otherwise, returns null.
 * @param {FileSelection} selection Instance of FileSelection.
 * @return {?DirectoryEntry} Directory entry which is selected alone.
 */
CommandUtil.getOnlyOneSelectedDirectory = selection => {
  if (!selection) {
    return null;
  }
  if (selection.totalCount !== 1) {
    return null;
  }
  if (!selection.entries[0].isDirectory) {
    return null;
  }
  return /** @type {!DirectoryEntry} */ (selection.entries[0]);
};

/**
 * Returns true if the given entry is the root entry of the volume.
 * @param {!VolumeManager} volumeManager
 * @param {(!Entry|!FakeEntry)} entry Entry or a fake entry.
 * @return {boolean} True if the entry is a root entry.
 */
CommandUtil.isRootEntry = (volumeManager, entry) => {
  if (!volumeManager || !entry) {
    return false;
  }

  const volumeInfo = volumeManager.getVolumeInfo(entry);
  return !!volumeInfo && util.isSameEntry(volumeInfo.displayRoot, entry);
};

/**
 * Returns true if the given event was triggered by the selection menu button.
 * @event {!Event} event Command event.
 * @return {boolean} Ture if the event was triggered by the selection menu
 * button.
 */
CommandUtil.isFromSelectionMenu = event => {
  return event.target.id == 'selection-menu-button';
};

/**
 * If entry is fake/invalid/root, we don't show menu items intended for regular
 * entries.
 * @param {!VolumeManager} volumeManager
 * @param {(!Entry|!FakeEntry)} entry Entry or a fake entry.
 * @return {boolean} True if we should show the menu items for regular entries.
 */
CommandUtil.shouldShowMenuItemsForEntry = (volumeManager, entry) => {
  // If the entry is fake entry, hide context menu entries.
  if (util.isFakeEntry(entry)) {
    return false;
  }

  // If the entry is not a valid entry, hide context menu entries.
  if (!volumeManager) {
    return false;
  }

  const volumeInfo = volumeManager.getVolumeInfo(entry);
  if (!volumeInfo) {
    return false;
  }

  // If the entry is root entry of its volume (but not a team drive root),
  // hide context menu entries.
  if (CommandUtil.isRootEntry(volumeManager, entry) &&
      !util.isTeamDriveRoot(entry)) {
    return false;
  }

  if (util.isTeamDrivesGrandRoot(entry)) {
    return false;
  }

  return true;
};

/**
 * If entry is MyFiles/Downloads, we don't allow cut/delete/rename.
 * @param {!VolumeManager} volumeManager
 * @param {(Entry|FakeEntry)} entry Entry or a fake entry.
 * @return {boolean}
 */
CommandUtil.isDownloads = (volumeManager, entry) => {
  if (!entry) {
    return false;
  }
  if (util.isFakeEntry(entry)) {
    return false;
  }

  // If the entry is not a valid entry.
  if (!volumeManager) {
    return false;
  }

  const volumeInfo = volumeManager.getVolumeInfo(entry);
  if (!volumeInfo) {
    return false;
  }

  if (util.isMyFilesVolumeEnabled() &&
      volumeInfo.volumeType === VolumeManagerCommon.RootType.DOWNLOADS &&
      entry.fullPath === '/Downloads') {
    return true;
  }
  return false;
};

/**
 * Returns whether all of the given entries have the given capability.
 *
 * @param {!Array<Entry>} entries List of entries to check capabilities for.
 * @param {!string} capability Name of the capability to check for.
 */
CommandUtil.hasCapability = (entries, capability) => {
  if (entries.length == 0) {
    return false;
  }

  // Check if the capability is true or undefined, but not false. A capability
  // can be undefined if the metadata is not fetched from the server yet (e.g.
  // if we create a new file in offline mode), or if there is a problem with the
  // cache and we don't have data yet. For this reason, we need to allow the
  // functionality even if it's not set.
  // TODO(crbug.com/849999): Store restrictions instead of capabilities.
  const metadata = fileManager.metadataModel.getCache(entries, [capability]);
  return metadata.length === entries.length &&
      metadata.every(item => item[capability] !== false);
};

/**
 * Handle of the command events.
 * @param {!CommandHandlerDeps} fileManager Classes |CommandHalder| depends.
 * @param {!FileSelectionHandler} selectionHandler
 * @constructor
 * @struct
 */
const CommandHandler = function(fileManager, selectionHandler) {
  /**
   * CommandHandlerDeps.
   * @type {!CommandHandlerDeps}
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
  const commands = fileManager.document.querySelectorAll('command');
  for (let i = 0; i < commands.length; i++) {
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
  selectionHandler.addEventListener(
      FileSelectionHandler.EventType.CHANGE_THROTTLED,
      this.updateAvailability.bind(this));
  fileManager.metadataModel.addEventListener(
      'update', this.updateAvailability.bind(this));

  chrome.commandLinePrivate.hasSwitch(
      'disable-zip-archiver-packer', disabled => {
        CommandHandler.IS_ZIP_ARCHIVER_PACKER_ENABLED_ = !disabled;
      });
};

/**
 * A flag that determines whether zip archiver - packer is enabled or no.
 * @type {boolean}
 * @private
 */
CommandHandler.IS_ZIP_ARCHIVER_PACKER_ENABLED_ = false;

/**
 * Supported disk file system types for renaming.
 * @type {!Array<!VolumeManagerCommon.FileSystemType>}
 * @const
 * @private
 */
CommandHandler.RENAME_DISK_FILE_SYSYTEM_SUPPORT_ = [
  VolumeManagerCommon.FileSystemType.EXFAT,
  VolumeManagerCommon.FileSystemType.VFAT
];

/**
 * Name of a command (for UMA).
 * @enum {string}
 * @const
 */
CommandHandler.MenuCommandsForUMA = {
  HELP: 'volume-help',
  DRIVE_HELP: 'volume-help-drive',
  DRIVE_BUY_MORE_SPACE: 'drive-buy-more-space',
  DRIVE_GO_TO_DRIVE: 'drive-go-to-drive',
  HIDDEN_FILES_SHOW: 'toggle-hidden-files-on',
  HIDDEN_FILES_HIDE: 'toggle-hidden-files-off',
  MOBILE_DATA_ON: 'drive-sync-settings-enabled',
  MOBILE_DATA_OFF: 'drive-sync-settings-disabled',
  DEPRECATED_SHOW_GOOGLE_DOCS_FILES_OFF: 'drive-hosted-settings-disabled',
  DEPRECATED_SHOW_GOOGLE_DOCS_FILES_ON: 'drive-hosted-settings-enabled',
  HIDDEN_ANDROID_FOLDERS_SHOW: 'toggle-hidden-android-folders-on',
  HIDDEN_ANDROID_FOLDERS_HIDE: 'toggle-hidden-android-folders-off',
  SHARE_WITH_LINUX: 'share-with-linux',
  MANAGE_LINUX_SHARING: 'manage-linux-sharing',
  MANAGE_LINUX_SHARING_TOAST: 'manage-linux-sharing-toast',
  MANAGE_LINUX_SHARING_TOAST_STARTUP: 'manage-linux-sharing-toast-startup',
  SHARE_WITH_PLUGIN_VM: 'share-with-plugin-vm',
  MANAGE_PLUGIN_VM_SHARING: 'manage-plugin-vm-sharing',
  MANAGE_PLUGIN_VM_SHARING_TOAST: 'manage-plugin-vm-sharing-toast',
  MANAGE_PLUGIN_VM_SHARING_TOAST_STARTUP:
      'manage-plugin-vm-sharing-toast-startup',
};

/**
 * Keep the order of this in sync with FileManagerMenuCommands in
 * tools/metrics/histograms/enums.xml.
 * The array indices will be recorded in UMA as enum values. The index for each
 * root type should never be renumbered nor reused in this array.
 *
 * @type {!Array<CommandHandler.MenuCommandsForUMA>}
 * @const
 */
CommandHandler.ValidMenuCommandsForUMA = [
  CommandHandler.MenuCommandsForUMA.HELP,
  CommandHandler.MenuCommandsForUMA.DRIVE_HELP,
  CommandHandler.MenuCommandsForUMA.DRIVE_BUY_MORE_SPACE,
  CommandHandler.MenuCommandsForUMA.DRIVE_GO_TO_DRIVE,
  CommandHandler.MenuCommandsForUMA.HIDDEN_FILES_SHOW,
  CommandHandler.MenuCommandsForUMA.HIDDEN_FILES_HIDE,
  CommandHandler.MenuCommandsForUMA.MOBILE_DATA_ON,
  CommandHandler.MenuCommandsForUMA.MOBILE_DATA_OFF,
  CommandHandler.MenuCommandsForUMA.DEPRECATED_SHOW_GOOGLE_DOCS_FILES_OFF,
  CommandHandler.MenuCommandsForUMA.DEPRECATED_SHOW_GOOGLE_DOCS_FILES_ON,
  CommandHandler.MenuCommandsForUMA.HIDDEN_ANDROID_FOLDERS_SHOW,
  CommandHandler.MenuCommandsForUMA.HIDDEN_ANDROID_FOLDERS_HIDE,
  CommandHandler.MenuCommandsForUMA.SHARE_WITH_LINUX,
  CommandHandler.MenuCommandsForUMA.MANAGE_LINUX_SHARING,
  CommandHandler.MenuCommandsForUMA.MANAGE_LINUX_SHARING_TOAST,
  CommandHandler.MenuCommandsForUMA.MANAGE_LINUX_SHARING_TOAST_STARTUP,
  CommandHandler.MenuCommandsForUMA.SHARE_WITH_PLUGIN_VM,
  CommandHandler.MenuCommandsForUMA.MANAGE_PLUGIN_VM_SHARING,
  CommandHandler.MenuCommandsForUMA.MANAGE_PLUGIN_VM_SHARING_TOAST,
  CommandHandler.MenuCommandsForUMA.MANAGE_PLUGIN_VM_SHARING_TOAST_STARTUP,
];
console.assert(
    Object.keys(CommandHandler.MenuCommandsForUMA).length ===
        CommandHandler.ValidMenuCommandsForUMA.length,
    'Members in ValidMenuCommandsForUMA do not match those in ' +
        'MenuCommandsForUMA.');

/**
 * Records the menu item as selected in UMA.
 * @param {CommandHandler.MenuCommandsForUMA} menuItem The selected menu item.
 */
CommandHandler.recordMenuItemSelected = menuItem => {
  metrics.recordEnum(
      'MenuItemSelected', menuItem, CommandHandler.ValidMenuCommandsForUMA);
};

/**
 * Updates the availability of all commands.
 */
CommandHandler.prototype.updateAvailability = function() {
  for (const id in this.commands_) {
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
  const dialogs =
      this.fileManager_.document.getElementsByClassName('cr-dialog-container');
  if (dialogs.length !== 0 && dialogs[0].classList.contains('shown')) {
    return true;
  }

  return false;  // Do not ignore.
};

/**
 * Handles command events.
 * @param {!Event} event Command event.
 * @private
 */
CommandHandler.prototype.onCommand_ = function(event) {
  if (this.shouldIgnoreEvents_()) {
    return;
  }
  const handler = CommandHandler.COMMANDS_[event.command.id];
  handler.execute.call(
      /** @type {Command} */ (handler), event, this.fileManager_);
};

/**
 * Handles canExecute events.
 * @param {!Event} event Can execute event.
 * @private
 */
CommandHandler.prototype.onCanExecute_ = function(event) {
  if (this.shouldIgnoreEvents_()) {
    return;
  }
  const handler = CommandHandler.COMMANDS_[event.command.id];
  handler.canExecute.call(
      /** @type {Command} */ (handler), event, this.fileManager_);
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
   * @param {!CommandHandlerDeps} fileManager The file manager instance.
   */
  execute: function(event, fileManager) {
    /** @param {VolumeManagerCommon.VolumeType=} opt_volumeType */
    const errorCallback = opt_volumeType => {
      if (opt_volumeType === VolumeManagerCommon.VolumeType.REMOVABLE) {
        fileManager.ui.alertDialog.showHtml(
            '', str('UNMOUNT_FAILED'), null, null, null);
      } else {
        fileManager.ui.alertDialog.showHtml(
            '', str('UNMOUNT_PROVIDED_FAILED'), null, null, null);
      }
    };

    const successCallback = () => {
      const msg = strf('A11Y_VOLUME_EJECT', label);
      fileManager.ui.speakA11yMessage(msg);
    };

    // Find volumes to unmount.
    let volumes = [];
    let label = '';
    const element = event.target;
    if (element instanceof EntryListItem) {
      // The element is a group of removable partitions.
      const entry = element.entry;
      if (!entry) {
        errorCallback();
        return;
      }
      // Add child partitions to the list of volumes to be unmounted.
      volumes = entry.getUIChildren().map(child => child.volumeInfo);
      label = entry.label || '';
    } else {
      // The element is a removable volume with no partitions.
      const volumeInfo = CommandUtil.getElementVolumeInfo(element, fileManager);
      if (!volumeInfo) {
        errorCallback();
        return;
      }
      volumes.push(volumeInfo);
      label = element.label || '';
    }

    // Eject volumes of which there may be multiple.
    for (let i = 0; i < volumes.length; i++) {
      fileManager.volumeManager.unmount(
          volumes[i], (i == volumes.length - 1) ? successCallback : () => {},
          errorCallback.bind(null, volumes[i].volumeType));
    }
  },
  /**
   * @param {!Event} event Command event.
   * @this {CommandHandler}
   */
  canExecute: function(event, fileManager) {
    const volumeInfo =
        CommandUtil.getElementVolumeInfo(event.target, fileManager);
    const entry = event.target.entry;
    if (!volumeInfo && !entry) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    const volumeType = (event.target instanceof EntryListItem) ?
        entry.rootType :
        volumeInfo.volumeType;
    event.canExecute =
        (volumeType === VolumeManagerCommon.VolumeType.ARCHIVE ||
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
   * @param {!CommandHandlerDeps} fileManager The file manager instance.
   */
  execute: function(event, fileManager) {
    const directoryModel = fileManager.directoryModel;
    let root = CommandUtil.getCommandEntry(fileManager, event.target);
    // If an entry is not found from the event target, use the current
    // directory. This can happen for the format button for unsupported and
    // unrecognized volumes.
    if (!root) {
      root = directoryModel.getCurrentDirEntry();
    }

    const volumeInfo = fileManager.volumeManager.getVolumeInfo(assert(root));
    if (volumeInfo) {
      fileManager.ui.confirmDialog.show(
          loadTimeData.getString('FORMATTING_WARNING'),
          chrome.fileManagerPrivate.formatVolume.bind(
              null, volumeInfo.volumeId),
          null, null);
    }
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager The file manager instance.
   */
  canExecute: function(event, fileManager) {
    const directoryModel = fileManager.directoryModel;
    let root = CommandUtil.getCommandEntry(fileManager, event.target);
    // |root| is null for unrecognized volumes. Enable format command for such
    // volumes.
    const isUnrecognizedVolume = (root == null);
    // See the comment in execute() for why doing this.
    if (!root) {
      root = directoryModel.getCurrentDirEntry();
    }
    const location = root && fileManager.volumeManager.getLocationInfo(root);
    const writable = location && !location.isReadOnly;
    const isRoot = location && location.isRootEntry;
    const removableRoot = location && isRoot &&
        location.rootType === VolumeManagerCommon.RootType.REMOVABLE;
    event.canExecute = removableRoot && (isUnrecognizedVolume || writable);
    event.command.setHidden(!removableRoot);
  }
});

/**
 * Initiates new folder creation.
 * @type {Command}
 */
CommandHandler.COMMANDS_['new-folder'] = (() => {
  /**
   * @constructor
   * @struct
   */
  const NewFolderCommand = function() {
    /**
     * Whether a new-folder is in progress.
     * @type {boolean}
     * @private
     */
    this.busy_ = false;
  };

  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  NewFolderCommand.prototype.execute = function(event, fileManager) {
    let targetDirectory;
    let executedFromDirectoryTree;

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

    const directoryModel = fileManager.directoryModel;
    const directoryTree = fileManager.ui.directoryTree;
    const listContainer = fileManager.ui.listContainer;
    this.busy_ = true;

    this.generateNewDirectoryName_(targetDirectory).then((newName) => {
      if (!executedFromDirectoryTree) {
        listContainer.startBatchUpdates();
      }

      return new Promise(
                 targetDirectory.getDirectory.bind(
                     targetDirectory, newName, {create: true, exclusive: true}))
          .then(
              (newDirectory) => {
                metrics.recordUserAction('CreateNewFolder');

                // Select new directory and start rename operation.
                if (executedFromDirectoryTree) {
                  directoryTree.updateAndSelectNewDirectory(
                      targetDirectory, newDirectory);
                  fileManager.directoryTreeNamingController.attachAndStart(
                      assert(fileManager.ui.directoryTree.selectedItem), false,
                      null);
                  this.busy_ = false;
                } else {
                  directoryModel.updateAndSelectNewDirectory(newDirectory)
                      .then(() => {
                        listContainer.endBatchUpdates();
                        fileManager.namingController.initiateRename();
                        this.busy_ = false;
                      })
                      .catch(error => {
                        listContainer.endBatchUpdates();
                        this.busy_ = false;
                        console.error(error);
                      });
                }
              },
              (error) => {
                if (!executedFromDirectoryTree) {
                  listContainer.endBatchUpdates();
                }

                this.busy_ = false;

                fileManager.ui.alertDialog.show(
                    strf(
                        'ERROR_CREATING_FOLDER', newName,
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
    const index = opt_index || 0;

    const defaultName = str('DEFAULT_NEW_FOLDER_NAME');
    const newName =
        index === 0 ? defaultName : defaultName + ' (' + index + ')';

    return new Promise(parentDirectory.getDirectory.bind(
                           parentDirectory, newName, {create: false}))
        .then(newEntry => {
          return this.generateNewDirectoryName_(parentDirectory, index + 1);
        })
        .catch(() => {
          return newName;
        });
  };

  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  NewFolderCommand.prototype.canExecute = function(event, fileManager) {
    if (event.target instanceof DirectoryItem ||
        event.target instanceof DirectoryTree) {
      const entry = CommandUtil.getCommandEntry(fileManager, event.target);
      if (!entry || util.isFakeEntry(entry) ||
          util.isTeamDrivesGrandRoot(entry)) {
        event.canExecute = false;
        event.command.setHidden(true);
        return;
      }

      const locationInfo = fileManager.volumeManager.getLocationInfo(entry);
      event.canExecute = locationInfo && !locationInfo.isReadOnly &&
          CommandUtil.hasCapability([entry], 'canAddChildren');
      event.command.setHidden(false);
    } else {
      const directoryModel = fileManager.directoryModel;
      const directoryEntry = fileManager.getCurrentDirectoryEntry();
      event.canExecute = !fileManager.directoryModel.isReadOnly() &&
          !fileManager.namingController.isRenamingInProgress() &&
          !directoryModel.isSearching() &&
          CommandUtil.hasCapability([directoryEntry], 'canAddChildren');
      event.command.setHidden(false);
    }
    if (this.busy_) {
      event.canExecute = false;
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
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    fileManager.backgroundPage.launcher.launchFileManager({
      currentDirectoryURL: fileManager.getCurrentDirectoryEntry() &&
          fileManager.getCurrentDirectoryEntry().toURL()
    });
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    event.canExecute = fileManager.getCurrentDirectoryEntry() &&
        (fileManager.dialogType === DialogType.FULL_PAGE);
  }
});

CommandHandler.COMMANDS_['select-all'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    fileManager.directoryModel.getFileListSelection().setCheckSelectMode(true);
    fileManager.directoryModel.getFileListSelection().selectAll();
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    // Check we are not inside an input element (e.g. the search box).
    const inputElementActive =
        document.activeElement instanceof HTMLInputElement ||
        document.activeElement instanceof HTMLTextAreaElement ||
        document.activeElement.tagName.toLowerCase() === 'cr-input';
    event.canExecute = !inputElementActive &&
        fileManager.directoryModel.getFileList().length > 0;
  }
});

CommandHandler.COMMANDS_['toggle-hidden-files'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    const visible = !fileManager.fileFilter.isHiddenFilesVisible();
    fileManager.fileFilter.setHiddenFilesVisible(visible);
    event.command.checked = visible;  // Checkmark for "Show hidden files".
    CommandHandler.recordMenuItemSelected(
        visible ? CommandHandler.MenuCommandsForUMA.HIDDEN_FILES_SHOW :
                  CommandHandler.MenuCommandsForUMA.HIDDEN_FILES_HIDE);
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: CommandUtil.canExecuteAlways
});

/**
 * Toggles visibility of top-level Android folders which are not visible by
 * default.
 * @type {Command}
 */
CommandHandler.COMMANDS_['toggle-hidden-android-folders'] =
    /** @type {Command} */ ({
      /**
       * @param {!Event} event Command event.
       * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
       */
      execute: function(event, fileManager) {
        const visible = !fileManager.fileFilter.isAllAndroidFoldersVisible();
        fileManager.fileFilter.setAllAndroidFoldersVisible(visible);
        event.command.checked = visible;
        CommandHandler.recordMenuItemSelected(
            visible ?
                CommandHandler.MenuCommandsForUMA.HIDDEN_ANDROID_FOLDERS_SHOW :
                CommandHandler.MenuCommandsForUMA.HIDDEN_ANDROID_FOLDERS_HIDE);
      },
      /**
       * @param {!Event} event Command event.
       * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
       */
      canExecute: function(event, fileManager) {
        const hasAndroidFilesVolumeInfo =
            !!fileManager.volumeManager.getCurrentProfileVolumeInfo(
                VolumeManagerCommon.VolumeType.ANDROID_FILES);
        const currentRootType = fileManager.directoryModel.getCurrentRootType();
        const isInMyFiles =
            currentRootType == VolumeManagerCommon.RootType.MY_FILES ||
            currentRootType == VolumeManagerCommon.RootType.DOWNLOADS ||
            currentRootType == VolumeManagerCommon.RootType.CROSTINI ||
            currentRootType == VolumeManagerCommon.RootType.ANDROID_FILES;
        event.canExecute = hasAndroidFilesVolumeInfo && isInMyFiles;
        event.command.setHidden(!event.canExecute);
        event.command.checked =
            fileManager.fileFilter.isAllAndroidFoldersVisible();
      }
    });

/**
 * Toggles drive sync settings.
 * @type {Command}
 */
CommandHandler.COMMANDS_['drive-sync-settings'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    // If checked, the sync is disabled.
    const nowCellularDisabled =
        fileManager.ui.gearMenu.syncButton.hasAttribute('checked');
    const changeInfo = {cellularDisabled: !nowCellularDisabled};
    chrome.fileManagerPrivate.setPreferences(changeInfo);
    CommandHandler.recordMenuItemSelected(
        nowCellularDisabled ?
            CommandHandler.MenuCommandsForUMA.MOBILE_DATA_OFF :
            CommandHandler.MenuCommandsForUMA.MOBILE_DATA_ON);
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    event.canExecute = fileManager.directoryModel.isOnDrive() &&
        fileManager.volumeManager.getDriveConnectionState()
            .hasCellularNetworkAccess;
    event.command.setHidden(!event.canExecute);
  }
});

/**
 * Deletes selected files.
 * @type {Command}
 */
CommandHandler.COMMANDS_['delete'] = (() => {
  /**
   * @constructor
   * @implements {Command}
   */
  const DeleteCommand = function() {};

  DeleteCommand.prototype = {
    /**
     * @param {!Event} event Command event.
     * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
     */
    execute: function(event, fileManager) {
      const entries = CommandUtil.getCommandEntries(fileManager, event.target);

      // Execute might be called without a call of canExecute method,
      // e.g. called directly from code. Double check here not to delete
      // undeletable entries.
      if (!entries.every(CommandUtil.shouldShowMenuItemsForEntry.bind(
              null, fileManager.volumeManager)) ||
          this.containsReadOnlyEntry_(entries, fileManager)) {
        return;
      }

      const message = entries.length === 1 ?
          strf('GALLERY_CONFIRM_DELETE_ONE', entries[0].name) :
          strf('GALLERY_CONFIRM_DELETE_SOME', entries.length);

      fileManager.ui.deleteConfirmDialog.show(message, () => {
        fileManager.fileOperationManager.deleteEntries(entries);
      }, null, null);
    },

    /**
     * @param {!Event} event Command event.
     * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
     */
    canExecute: function(event, fileManager) {
      const entries = CommandUtil.getCommandEntries(fileManager, event.target);

      // If entries contain fake or root entry, hide delete option.
      if (!entries.every(CommandUtil.shouldShowMenuItemsForEntry.bind(
              null, fileManager.volumeManager))) {
        event.canExecute = false;
        event.command.setHidden(true);
        return;
      }

      event.canExecute = entries.length > 0 &&
          !this.containsReadOnlyEntry_(entries, fileManager) &&
          !fileManager.directoryModel.isReadOnly() &&
          CommandUtil.hasCapability(entries, 'canDelete');
      event.command.setHidden(false);
    },

    /**
     * Returns True if any entry belongs to a read-only volume or is
     * MyFiles>Downloads.
     * @param {!Array<!Entry>} entries
     * @param {!CommandHandlerDeps} fileManager
     * @return {boolean} True if entries contain read only entry.
     */
    containsReadOnlyEntry_: function(entries, fileManager) {
      return entries.some(entry => {
        const locationInfo = fileManager.volumeManager.getLocationInfo(entry);
        return (locationInfo && locationInfo.isReadOnly) ||
            CommandUtil.isDownloads(fileManager.volumeManager, entry);
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
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    fileManager.document.execCommand(event.command.id);
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    const fileTransferController = fileManager.fileTransferController;

    event.canExecute = !!fileTransferController &&
        fileTransferController.queryPasteCommandEnabled(
            fileManager.directoryModel.getCurrentDirEntry());

    // Hide this command if only one folder is selected.
    event.command.setHidden(
        !!CommandUtil.getOnlyOneSelectedDirectory(fileManager.getSelection()));
  }
});

/**
 * Pastes files from clipboard. This is basically same as 'paste'.
 * This command is used for always showing the Paste command to gear menu.
 * @type {Command}
 */
CommandHandler.COMMANDS_['paste-into-current-folder'] =
    /** @type {Command} */ ({
      /**
       * @param {!Event} event Command event.
       * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
       */
      execute: function(event, fileManager) {
        fileManager.document.execCommand('paste');
      },
      /**
       * @param {!Event} event Command event.
       * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
       */
      canExecute: function(event, fileManager) {
        const fileTransferController = fileManager.fileTransferController;

        event.canExecute = !!fileTransferController &&
            fileTransferController.queryPasteCommandEnabled(
                fileManager.directoryModel.getCurrentDirEntry());
      }
    });

/**
 * Pastes files from clipboard into the selected folder.
 * @type {Command}
 */
CommandHandler.COMMANDS_['paste-into-folder'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
    if (entries.length !== 1 || !entries[0].isDirectory ||
        !CommandUtil.shouldShowMenuItemsForEntry(
            fileManager.volumeManager, entries[0])) {
      return;
    }

    // This handler tweaks the Event object for 'paste' event so that
    // the FileTransferController can distinguish this 'paste-into-folder'
    // command and know the destination directory.
    const handler = inEvent => {
      inEvent.destDirectory = entries[0];
    };
    fileManager.document.addEventListener('paste', handler, true);
    fileManager.document.execCommand('paste');
    fileManager.document.removeEventListener('paste', handler, true);
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);

    // Show this item only when one directory is selected.
    if (entries.length !== 1 || !entries[0].isDirectory ||
        !CommandUtil.shouldShowMenuItemsForEntry(
            fileManager.volumeManager, entries[0])) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    const fileTransferController = fileManager.fileTransferController;
    const directoryEntry = /** @type {DirectoryEntry|FakeEntry} */ (entries[0]);
    event.canExecute = !!fileTransferController &&
        fileTransferController.queryPasteCommandEnabled(directoryEntry);
    event.command.setHidden(false);
  }
});

/**
 * Cut/Copy command.
 * @type {Command}
 * @private
 */
CommandHandler.cutCopyCommand_ = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    // Cancel check-select-mode on cut/copy.  Any further selection of a dir
    // should start a new selection rather than add to the existing selection.
    fileManager.directoryModel.getFileListSelection().setCheckSelectMode(false);
    fileManager.document.execCommand(event.command.id);
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    event.canExecute =
        fileManager.document.queryCommandEnabled(event.command.id);
  }
});

CommandHandler.COMMANDS_['cut'] = CommandHandler.cutCopyCommand_;
CommandHandler.COMMANDS_['copy'] = CommandHandler.cutCopyCommand_;

/**
 * Initiates file renaming.
 * @type {Command}
 */
CommandHandler.COMMANDS_['rename'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    const entry = CommandUtil.getCommandEntry(fileManager, event.target);
    if (CommandUtil.isDownloads(fileManager.volumeManager, entry)) {
      return;
    }
    if (event.target instanceof DirectoryTree ||
        event.target instanceof DirectoryItem) {
      let isRemovableRoot = false;
      let volumeInfo = null;
      if (entry) {
        volumeInfo = fileManager.volumeManager.getVolumeInfo(entry);
        // Checks whether the target is actually external drive or just a folder
        // inside the drive.
        if (volumeInfo &&
            CommandUtil.isRootEntry(fileManager.volumeManager, entry)) {
          isRemovableRoot = true;
        }
      }

      if (event.target instanceof DirectoryTree) {
        const directoryTree = event.target;
        assert(fileManager.directoryTreeNamingController)
            .attachAndStart(
                assert(directoryTree.selectedItem), isRemovableRoot,
                volumeInfo);
      } else if (event.target instanceof DirectoryItem) {
        const directoryItem = event.target;
        assert(fileManager.directoryTreeNamingController)
            .attachAndStart(directoryItem, isRemovableRoot, volumeInfo);
      }
    } else {
      fileManager.namingController.initiateRename();
    }
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    // Check if it is removable drive
    if ((() => {
          const root = CommandUtil.getCommandEntry(fileManager, event.target);
          // |root| is null for unrecognized volumes. Do not enable rename
          // command for such volumes because they need to be formatted prior to
          // rename.
          if (!root ||
              !CommandUtil.isRootEntry(fileManager.volumeManager, root)) {
            return false;
          }
          const volumeInfo = fileManager.volumeManager.getVolumeInfo(root);
          const location = fileManager.volumeManager.getLocationInfo(root);
          if (!volumeInfo || !location) {
            event.command.setHidden(true);
            event.canExecute = false;
            return true;
          }
          const writable = !location.isReadOnly;
          const removable =
              location.rootType === VolumeManagerCommon.RootType.REMOVABLE;
          event.canExecute = removable && writable &&
              CommandHandler.RENAME_DISK_FILE_SYSYTEM_SUPPORT_.indexOf(
                  volumeInfo.diskFileSystemType) > -1;
          event.command.setHidden(!removable);
          return removable;
        })()) {
      return;
    }

    // Check if it is file or folder
    const renameTarget = CommandUtil.isFromSelectionMenu(event) ?
        fileManager.ui.listContainer.currentList :
        event.target;
    const entries = CommandUtil.getCommandEntries(fileManager, renameTarget);
    if (entries.length === 0 ||
        !CommandUtil.shouldShowMenuItemsForEntry(
            fileManager.volumeManager, entries[0]) ||
        entries.some(
            CommandUtil.isDownloads.bind(null, fileManager.volumeManager))) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    const parentEntry =
        CommandUtil.getParentEntry(renameTarget, fileManager.directoryModel);
    const locationInfo = parentEntry ?
        fileManager.volumeManager.getLocationInfo(parentEntry) :
        null;
    const volumeIsNotReadOnly = !!locationInfo && !locationInfo.isReadOnly;
    event.canExecute = entries.length === 1 && volumeIsNotReadOnly &&
        CommandUtil.hasCapability(entries, 'canRename');
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
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    if (fileManager.directoryModel.isOnDrive()) {
      util.visitURL(str('GOOGLE_DRIVE_HELP_URL'));
      CommandHandler.recordMenuItemSelected(
          CommandHandler.MenuCommandsForUMA.DRIVE_HELP);
    } else {
      util.visitURL(str('FILES_APP_HELP_URL'));
      CommandHandler.recordMenuItemSelected(
          CommandHandler.MenuCommandsForUMA.HELP);
    }
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    // Hides the help menu in modal dialog mode. It does not make much sense
    // because after all, users cannot view the help without closing, and
    // besides that the help page is about the Files app as an app, not about
    // the dialog mode itself. It can also lead to hard-to-fix bug
    // crbug.com/339089.
    const hideHelp = DialogType.isModal(fileManager.dialogType);
    event.canExecute = !hideHelp;
    event.command.setHidden(hideHelp);
  }
});

/**
 * Opens the send feedback window with pre-populated content.
 */
CommandHandler.COMMANDS_['send-feedback'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    let message = {
      categoryTag: 'chromeos-files-app',
      requestFeedback: true,
      feedbackInfo: {
        description: '',
      },
    };

    const kFeedbackExtensionId = 'gfdkimpbcpahaombhbimeihdjnejgicl';
    // On ChromiumOS the feedback extension is not installed, so we just log
    // that filing feedback has failed.
    chrome.runtime.sendMessage(kFeedbackExtensionId, message, (response) => {
      if (chrome.runtime.lastError) {
        console.log(
            'Failed to send feedback: ' + chrome.runtime.lastError.message);
      }
    });
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    // Launching the feedback tool is always possible.
    event.canExecute = true;
  }
});

/**
 * Opens drive buy-more-space url.
 * @type {Command}
 */
CommandHandler.COMMANDS_['drive-buy-more-space'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    util.visitURL(str('GOOGLE_DRIVE_BUY_STORAGE_URL'));
    CommandHandler.recordMenuItemSelected(
        CommandHandler.MenuCommandsForUMA.DRIVE_BUY_MORE_SPACE);
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
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    util.visitURL(str('GOOGLE_DRIVE_ROOT_URL'));
    CommandHandler.recordMenuItemSelected(
        CommandHandler.MenuCommandsForUMA.DRIVE_GO_TO_DRIVE);
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
    const canExecute = fileManager.taskController.canExecuteDefaultTask();
    event.canExecute = canExecute;
    event.command.setHidden(!canExecute);
  }
});

/**
 * Displays "open with" dialog for current selection.
 * @type {Command}
 */
CommandHandler.COMMANDS_['open-with'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    fileManager.taskController.getFileTasks()
        .then(tasks => {
          tasks.showTaskPicker(
              fileManager.ui.defaultTaskPicker, str('OPEN_WITH_BUTTON_LABEL'),
              '', task => {
                tasks.execute(task);
              }, FileTasks.TaskPickerType.OpenWith);
        })
        .catch(error => {
          if (error) {
            console.error(error.stack || error);
          }
        });
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    const canExecute = fileManager.taskController.canExecuteOpenActions();
    event.canExecute = canExecute;
    event.command.setHidden(!canExecute);
  }
});

/**
 * Displays "More actions" dialog for current selection.
 * @type {Command}
 */
CommandHandler.COMMANDS_['more-actions'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    fileManager.taskController.getFileTasks()
        .then(tasks => {
          tasks.showTaskPicker(
              fileManager.ui.defaultTaskPicker,
              str('MORE_ACTIONS_BUTTON_LABEL'), '', task => {
                tasks.execute(task);
              }, FileTasks.TaskPickerType.MoreActions);
        })
        .catch(error => {
          if (error) {
            console.error(error.stack || error);
          }
        });
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    const canExecute = fileManager.taskController.canExecuteMoreActions();
    event.canExecute = canExecute;
    event.command.setHidden(!canExecute);
  }
});

/**
 * Displays any available (child) sub menu for current selection.
 * @type {Command}
 */
CommandHandler.COMMANDS_['show-submenu'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    fileManager.ui.shareMenuButton.showSubMenu();
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    const canExecute = fileManager.taskController.canExecuteShowOverflow();
    event.canExecute = canExecute;
    event.command.setHidden(!canExecute);
  }
});

/**
 * Displays QuickView for current selection.
 * @type {Command}
 */
CommandHandler.COMMANDS_['get-info'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager fileManager to use.
   */
  execute: function(event, fileManager) {
    // 'get-info' command is executed by 'command' event handler in
    // QuickViewController.
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    // QuickViewModel refers the file selection instead of event target.
    const entries = fileManager.getSelection().entries;
    if (entries.length === 0) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    event.canExecute = entries.length === 1;
    event.command.setHidden(false);
  }
});

/**
 * Focuses search input box.
 * @type {Command}
 */
CommandHandler.COMMANDS_['search'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    // Cancel item selection.
    fileManager.directoryModel.clearSelection();

    // Focus and unhide the search box.
    const element = fileManager.document.querySelector('#search-box cr-input');
    element.hidden = false;
    (/** @type {!CrInputElement} */ (element)).select();
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
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
   * @param {!CommandHandlerDeps} fileManager
   */
  execute: function(event, fileManager) {
    const actionsModel =
        fileManager.actionsController.getActionsModelFor(event.target);
    const saveForOfflineAction = actionsModel ?
        actionsModel.getAction(ActionsModel.CommonActionId.SAVE_FOR_OFFLINE) :
        null;
    const offlineNotNeededAction = actionsModel ?
        actionsModel.getAction(
            ActionsModel.CommonActionId.OFFLINE_NOT_NECESSARY) :
        null;
    // Saving for offline has a priority if both actions are available.
    const action = saveForOfflineAction || offlineNotNeededAction;
    if (action) {
      action.execute();
    }
  },

  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    const actionsModel =
        fileManager.actionsController.getActionsModelFor(event.target);
    const saveForOfflineAction = actionsModel ?
        actionsModel.getAction(ActionsModel.CommonActionId.SAVE_FOR_OFFLINE) :
        null;
    const offlineNotNeededAction = actionsModel ?
        actionsModel.getAction(
            ActionsModel.CommonActionId.OFFLINE_NOT_NECESSARY) :
        null;
    const action = saveForOfflineAction || offlineNotNeededAction;

    event.canExecute = action && action.canExecute();
    // If model is not computed yet, then keep the previous visibility to avoid
    // flickering.
    if (actionsModel) {
      event.command.setHidden(actionsModel && !action);
      event.command.checked = !!offlineNotNeededAction && !saveForOfflineAction;
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
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    const dirEntry = fileManager.getCurrentDirectoryEntry();
    if (!dirEntry ||
        !fileManager.getSelection().entries.every(
            CommandUtil.shouldShowMenuItemsForEntry.bind(
                null, fileManager.volumeManager))) {
      return;
    }

    if (CommandHandler.IS_ZIP_ARCHIVER_PACKER_ENABLED_) {
      fileManager.taskController.getFileTasks()
          .then(tasks => {
            if (fileManager.directoryModel.isOnDrive() ||
                fileManager.directoryModel.isOnMTP()) {
              tasks.execute(/** @type {chrome.fileManagerPrivate.FileTask} */ (
                  {taskId: FileTasks.ZIP_ARCHIVER_ZIP_USING_TMP_TASK_ID}));
            } else {
              tasks.execute(/** @type {chrome.fileManagerPrivate.FileTask} */ (
                  {taskId: FileTasks.ZIP_ARCHIVER_ZIP_TASK_ID}));
            }
          })
          .catch(error => {
            if (error) {
              console.error(error.stack || error);
            }
          });
    } else {
      const selectionEntries = fileManager.getSelection().entries;
      fileManager.fileOperationManager.zipSelection(
          selectionEntries, /** @type {!DirectoryEntry} */ (dirEntry));
    }
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    const dirEntry = fileManager.getCurrentDirectoryEntry();
    const selection = fileManager.getSelection();

    if (!selection.entries.every(CommandUtil.shouldShowMenuItemsForEntry.bind(
            null, fileManager.volumeManager))) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    event.command.setHidden(false);
    const isOnEligibleLocation =
        CommandHandler.IS_ZIP_ARCHIVER_PACKER_ENABLED_ ?
        true :
        !fileManager.directoryModel.isOnDrive() &&
            !fileManager.directoryModel.isOnMTP();

    event.canExecute = dirEntry && !fileManager.directoryModel.isReadOnly() &&
        isOnEligibleLocation && selection && selection.totalCount > 0;
  }
});

/**
 * Shows the share dialog for the current selection (single only).
 * @type {Command}
 */
CommandHandler.COMMANDS_['share'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    // To toolbar buttons are always related to the file list, even though the
    // focus is on the navigation list. This assumption will break once we add
    // Share to the context menu on the navigation list. crbug.com/530418
    const actionsModel =
        fileManager.actionsController.getActionsModelForContext(
            ActionsController.Context.FILE_LIST);
    const action = actionsModel ?
        actionsModel.getAction(ActionsModel.CommonActionId.SHARE) :
        null;
    if (action) {
      action.execute();
    }
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    const actionsModel =
        fileManager.actionsController.getActionsModelForContext(
            ActionsController.Context.FILE_LIST);
    const action = actionsModel ?
        actionsModel.getAction(ActionsModel.CommonActionId.SHARE) :
        null;
    event.canExecute = action && action.canExecute();
    // If model is not computed yet, then keep the previous visibility to avoid
    // flickering.
    if (actionsModel) {
      event.command.setHidden(actionsModel && !action);
    }
  }
});

/**
 * Opens the file in Drive for the user to manage sharing permissions etc.
 * @type {Command}
 */
CommandHandler.COMMANDS_['manage-in-drive'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager The file manager instance.
   */
  execute: function(event, fileManager) {
    const actionsModel =
        fileManager.actionsController.getActionsModelFor(event.target);
    const action = actionsModel ?
        actionsModel.getAction(ActionsModel.InternalActionId.MANAGE_IN_DRIVE) :
        null;
    if (action) {
      action.execute();
    }
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    const actionsModel =
        fileManager.actionsController.getActionsModelFor(event.target);
    const action = actionsModel ?
        actionsModel.getAction(ActionsModel.InternalActionId.MANAGE_IN_DRIVE) :
        null;
    event.canExecute = action && action.canExecute();
    if (actionsModel) {
      event.command.setHidden(!action);
    }
  }
});

/**
 * Shares the selected (single only) directory with the default crostini VM.
 * @type {Command}
 */
CommandHandler.COMMANDS_['share-with-linux'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    const entry = CommandUtil.getCommandEntry(fileManager, event.target);
    if (!entry || !entry.isDirectory) {
      return;
    }
    const dir = /** @type {!DirectoryEntry} */ (entry);
    const info = fileManager.volumeManager.getLocationInfo(dir);
    if (!info) {
      return;
    }
    function share() {
      // Always persist shares via right-click > Share with Linux.
      chrome.fileManagerPrivate.sharePathsWithCrostini(
          constants.DEFAULT_CROSTINI_VM, [dir], true /* persist */, () => {
            if (chrome.runtime.lastError) {
              console.error(
                  'Error sharing with linux: ' +
                  chrome.runtime.lastError.message);
            }
          });
      // Register the share and show the 'Manage Linux sharing' toast
      // immediately, since the container may take 10s or more to start.
      fileManager.crostini.registerSharedPath(
          constants.DEFAULT_CROSTINI_VM, dir);
      fileManager.ui.toast.show(str('FOLDER_SHARED_WITH_CROSTINI'), {
        text: str('MANAGE_LINUX_SHARING_BUTTON_LABEL'),
        callback: () => {
          chrome.fileManagerPrivate.openSettingsSubpage('crostini/sharedPaths');
          CommandHandler.recordMenuItemSelected(
              CommandHandler.MenuCommandsForUMA.MANAGE_LINUX_SHARING_TOAST);
        }
      });
    }
    // Show a confirmation dialog if we are sharing the root of a volume.
    // Non-Drive volume roots are always '/'.
    if (dir.fullPath == '/') {
      fileManager.ui_.confirmDialog.showHtml(
          strf('SHARE_ROOT_FOLDER_WITH_CROSTINI_TITLE'),
          strf('SHARE_ROOT_FOLDER_WITH_CROSTINI', info.volumeInfo.label), share,
          () => {});
    } else if (
        info.isRootEntry &&
        (info.rootType == VolumeManagerCommon.RootType.DRIVE ||
         info.rootType == VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT ||
         info.rootType ==
             VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT)) {
      // Only show the dialog for My Drive, Shared Drives Grand Root and
      // Computers Grand Root.  Do not show for roots of a single Shared Drive
      // or Computer.
      fileManager.ui_.confirmDialog.showHtml(
          strf('SHARE_ROOT_FOLDER_WITH_CROSTINI_TITLE'),
          strf('SHARE_ROOT_FOLDER_WITH_CROSTINI_DRIVE'), share, () => {});
    } else {
      // This is not a root, share it without confirmation dialog.
      share();
    }
    CommandHandler.recordMenuItemSelected(
        CommandHandler.MenuCommandsForUMA.SHARE_WITH_LINUX);
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    // Must be single directory not already shared.
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
    event.canExecute = entries.length === 1 && entries[0].isDirectory &&
        !fileManager.crostini.isPathShared(
            constants.DEFAULT_CROSTINI_VM, entries[0]) &&
        fileManager.crostini.canSharePath(
            constants.DEFAULT_CROSTINI_VM, entries[0], true /* persist */);
    event.command.setHidden(!event.canExecute);
  }
});

/**
 * Shares the selected (single only) directory with the Plugin VM.
 * @type {Command}
 */
CommandHandler.COMMANDS_['share-with-plugin-vm'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    const entry = CommandUtil.getCommandEntry(fileManager, event.target);
    if (!entry || !entry.isDirectory) {
      return;
    }
    const dir = /** @type {!DirectoryEntry} */ (entry);
    const info = fileManager.volumeManager.getLocationInfo(dir);
    if (!info) {
      return;
    }
    function share() {
      // Always persist shares via right-click > Share with PluginVM.
      chrome.fileManagerPrivate.sharePathsWithCrostini(
          constants.PLUGIN_VM, [dir], true /* persist */, () => {
            if (chrome.runtime.lastError) {
              console.error(
                  'Error sharing with Plugin VM: ' +
                  chrome.runtime.lastError.message);
            }
          });
      // Register the share and show the 'Manage PluginVM sharing' toast
      // immediately, since the container may take 10s or more to start.
      fileManager.crostini.registerSharedPath(constants.PLUGIN_VM, dir);
      fileManager.ui.toast.show(str('FOLDER_SHARED_WITH_CROSTINI'), {
        text: str('MANAGE_LINUX_SHARING_BUTTON_LABEL'),
        callback: () => {
          chrome.fileManagerPrivate.openSettingsSubpage('pluginvm/sharedPaths');
          CommandHandler.recordMenuItemSelected(
              CommandHandler.MenuCommandsForUMA.MANAGE_PLUGIN_VM_SHARING_TOAST);
        }
      });
    }
    // Show a confirmation dialog if we are sharing the root of a volume.
    // Non-Drive volume roots are always '/'.
    if (dir.fullPath == '/') {
      fileManager.ui_.confirmDialog.showHtml(
          strf('SHARE_ROOT_FOLDER_WITH_CROSTINI_TITLE'),
          strf('SHARE_ROOT_FOLDER_WITH_CROSTINI', info.volumeInfo.label), share,
          () => {});
    } else if (
        info.isRootEntry &&
        (info.rootType == VolumeManagerCommon.RootType.DRIVE ||
         info.rootType == VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT ||
         info.rootType ==
             VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT)) {
      // Only show the dialog for My Drive, Shared Drives Grand Root and
      // Computers Grand Root.  Do not show for roots of a single Shared Drive
      // or Computer.
      fileManager.ui_.confirmDialog.showHtml(
          strf('SHARE_ROOT_FOLDER_WITH_CROSTINI_TITLE'),
          strf('SHARE_ROOT_FOLDER_WITH_CROSTINI_DRIVE'), share, () => {});
    } else {
      // This is not a root, share it without confirmation dialog.
      share();
    }
    CommandHandler.recordMenuItemSelected(
        CommandHandler.MenuCommandsForUMA.SHARE_WITH_PLUGIN_VM);
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    // Must be single directory subfolder of Downloads not already shared.
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
    event.canExecute = entries.length === 1 && entries[0].isDirectory &&
        !fileManager.crostini.isPathShared(constants.PLUGIN_VM, entries[0]) &&
        fileManager.crostini.canSharePath(
            constants.PLUGIN_VM, entries[0], true /* persist */);
    event.command.setHidden(!event.canExecute);
  }
});

/**
 * Link to settings page from gear menu.  Allows the user to manage files and
 * folders shared with the crostini container.
 * @type {Command}
 */
CommandHandler.COMMANDS_['manage-linux-sharing-gear'] =
    /** @type {Command} */ ({
      /**
       * @param {!Event} event Command event.
       * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
       */
      execute: function(event, fileManager) {
        chrome.fileManagerPrivate.openSettingsSubpage('crostini/sharedPaths');
        CommandHandler.recordMenuItemSelected(
            CommandHandler.MenuCommandsForUMA.MANAGE_LINUX_SHARING);
      },
      /**
       * @param {!Event} event Command event.
       * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
       */
      canExecute: function(event, fileManager) {
        event.canExecute =
            fileManager.crostini.isEnabled(constants.DEFAULT_CROSTINI_VM);
        event.command.setHidden(!event.canExecute);
      }
    });

/**
 * Link to settings page from file context menus (not gear menu).  Allows
 * the user to manage files and folders shared with the crostini container.
 * @type {Command}
 */
CommandHandler.COMMANDS_['manage-linux-sharing'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    chrome.fileManagerPrivate.openSettingsSubpage('crostini/sharedPaths');
    CommandHandler.recordMenuItemSelected(
        CommandHandler.MenuCommandsForUMA.MANAGE_LINUX_SHARING);
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
    event.canExecute = entries.length === 1 && entries[0].isDirectory &&
        fileManager.crostini.isPathShared(
            constants.DEFAULT_CROSTINI_VM, entries[0]);
    event.command.setHidden(!event.canExecute);
  }
});

/**
 * Link to settings page from gear menu.  Allows the user to manage files and
 * folders shared with the Plugin VM.
 * @type {Command}
 */
CommandHandler.COMMANDS_['manage-plugin-vm-sharing-gear'] =
    /** @type {Command} */ ({
      /**
       * @param {!Event} event Command event.
       * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
       */
      execute: function(event, fileManager) {
        chrome.fileManagerPrivate.openSettingsSubpage('pluginvm/sharedPaths');
        CommandHandler.recordMenuItemSelected(
            CommandHandler.MenuCommandsForUMA.MANAGE_PLUGIN_VM_SHARING);
      },
      /**
       * @param {!Event} event Command event.
       * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
       */
      canExecute: function(event, fileManager) {
        event.canExecute = fileManager.crostini.isEnabled(constants.PLUGIN_VM);
        event.command.setHidden(!event.canExecute);
      }
    });

/**
 * Link to settings page from file context menus (not gear menu).  Allows
 * the user to manage files and folders shared with the Plugin VM.
 * @type {Command}
 */
CommandHandler.COMMANDS_['manage-plugin-vm-sharing'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    chrome.fileManagerPrivate.openSettingsSubpage('pluginvm/sharedPaths');
    CommandHandler.recordMenuItemSelected(
        CommandHandler.MenuCommandsForUMA.MANAGE_PLUGIN_VM_SHARING);
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
    event.canExecute = entries.length === 1 && entries[0].isDirectory &&
        fileManager.crostini.isPathShared(constants.PLUGIN_VM, entries[0]);
    event.command.setHidden(!event.canExecute);
  }
});

/**
 * Creates a shortcut of the selected folder (single only).
 * @type {Command}
 */
CommandHandler.COMMANDS_['create-folder-shortcut'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager The file manager instance.
   */
  execute: function(event, fileManager) {
    const actionsModel =
        fileManager.actionsController.getActionsModelFor(event.target);
    const action = actionsModel ?
        actionsModel.getAction(
            ActionsModel.InternalActionId.CREATE_FOLDER_SHORTCUT) :
        null;
    if (action) {
      action.execute();
    }
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    const actionsModel =
        fileManager.actionsController.getActionsModelFor(event.target);
    const action = actionsModel ?
        actionsModel.getAction(
            ActionsModel.InternalActionId.CREATE_FOLDER_SHORTCUT) :
        null;
    event.canExecute = action && action.canExecute();
    if (actionsModel) {
      event.command.setHidden(!action);
    }
  }
});

/**
 * Removes the folder shortcut.
 * @type {Command}
 */
CommandHandler.COMMANDS_['remove-folder-shortcut'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager The file manager instance.
   */
  execute: function(event, fileManager) {
    const actionsModel =
        fileManager.actionsController.getActionsModelFor(event.target);
    const action = actionsModel ?
        actionsModel.getAction(
            ActionsModel.InternalActionId.REMOVE_FOLDER_SHORTCUT) :
        null;
    if (action) {
      action.execute();
    }
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    const actionsModel =
        fileManager.actionsController.getActionsModelFor(event.target);
    const action = actionsModel ?
        actionsModel.getAction(
            ActionsModel.InternalActionId.REMOVE_FOLDER_SHORTCUT) :
        null;
    event.canExecute = action && action.canExecute();
    if (actionsModel) {
      event.command.setHidden(!action);
    }
  }
});

/**
 * Zoom in to the Files app.
 * @type {Command}
 */
CommandHandler.COMMANDS_['zoom-in'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    chrome.fileManagerPrivate.zoom('in');
  },
  canExecute: CommandUtil.canExecuteAlways
});

/**
 * Zoom out from the Files app.
 * @type {Command}
 */
CommandHandler.COMMANDS_['zoom-out'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
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
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
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
    if (fileManager.directoryModel.getFileList()) {
      fileManager.directoryModel.getFileList().sort('name', 'asc');
    }
  },
  canExecute: CommandUtil.canExecuteAlways
});

/**
 * Sort the file list by size (in descending order).
 * @type {Command}
 */
CommandHandler.COMMANDS_['sort-by-size'] = /** @type {Command} */ ({
  execute: function(event, fileManager) {
    if (fileManager.directoryModel.getFileList()) {
      fileManager.directoryModel.getFileList().sort('size', 'desc');
    }
  },
  canExecute: CommandUtil.canExecuteAlways
});

/**
 * Sort the file list by type (in ascending order).
 * @type {Command}
 */
CommandHandler.COMMANDS_['sort-by-type'] = /** @type {Command} */ ({
  execute: function(event, fileManager) {
    if (fileManager.directoryModel.getFileList()) {
      fileManager.directoryModel.getFileList().sort('type', 'asc');
    }
  },
  canExecute: CommandUtil.canExecuteAlways
});

/**
 * Sort the file list by date-modified (in descending order).
 * @type {Command}
 */
CommandHandler.COMMANDS_['sort-by-date'] = /** @type {Command} */ ({
  execute: function(event, fileManager) {
    if (fileManager.directoryModel.getFileList()) {
      fileManager.directoryModel.getFileList().sort('modificationTime', 'desc');
    }
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
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
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
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
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
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
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
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
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
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    fileManager.ui.suggestAppsDialog.showProviders((result, itemId) => {
      // If a new provider is installed, then launch it so the configuration
      // dialog is shown (if it's available).
      if (result === SuggestAppsDialog.Result.SUCCESS) {
        fileManager.providersModel.requestMount(assert(itemId));
      }
    });
  },
  canExecute: function(event, fileManager) {
    event.canExecute = fileManager.dialogType === DialogType.FULL_PAGE;
    event.command.setHidden(!event.canExecute);
  }
});

/**
 * Opens the gear menu.
 * @type {Command}
 */
CommandHandler.COMMANDS_['open-gear-menu'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    fileManager.ui.gearButton.showMenu(true);
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    event.canExecute = CommandUtil.canExecuteAlways;
  }
});

/**
 * Focus the first button visible on action bar (at the top).
 * @type {Command}
 */
CommandHandler.COMMANDS_['focus-action-bar'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    fileManager.ui.actionbar.querySelector('button:not([hidden])').focus();
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: CommandUtil.canExecuteAlways
});

/**
 * Handle back button.
 * @type {Command}
 */
CommandHandler.COMMANDS_['browser-back'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    // TODO(fukino): It should be better to minimize Files app only when there
    // is no back stack, and otherwise use BrowserBack for history navigation.
    // https://crbug.com/624100.
    const currentWindow = chrome.app.window.current();
    if (currentWindow) {
      currentWindow.minimize();
    }
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    event.canExecute = CommandUtil.canExecuteAlways;
  }
});

/**
 * Configures the currently selected volume.
 */
CommandHandler.COMMANDS_['configure'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    const volumeInfo =
        CommandUtil.getElementVolumeInfo(event.target, fileManager);
    if (volumeInfo && volumeInfo.configurable) {
      fileManager.volumeManager.configure(volumeInfo);
    }
  },
  canExecute: function(event, fileManager) {
    const volumeInfo =
        CommandUtil.getElementVolumeInfo(event.target, fileManager);
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
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    fileManager.directoryModel.rescan(true /* refresh */);
    fileManager.spinnerController.blink();
  },
  canExecute: function(event, fileManager) {
    const currentDirEntry = fileManager.directoryModel.getCurrentDirEntry();
    const volumeInfo = currentDirEntry &&
        fileManager.volumeManager.getVolumeInfo(currentDirEntry);
    event.canExecute = volumeInfo && !volumeInfo.watchable;
    event.command.setHidden(
        !event.canExecute ||
        fileManager.directoryModel.getFileListSelection().getCheckSelectMode());
  }
});

/**
 * Refreshes the currently selected directory.
 */
CommandHandler.COMMANDS_['set-wallpaper'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    const entry = fileManager.getSelection().entries[0];
    new Promise((resolve, reject) => {
      entry.file(resolve, reject);
    })
        .then(blob => {
          const fileReader = new FileReader();
          return new Promise((resolve, reject) => {
            fileReader.onload = () => {
              resolve(fileReader.result);
            };
            fileReader.onerror = () => {
              reject(fileReader.error);
            };
            fileReader.readAsArrayBuffer(blob);
          });
        })
        .then((/** @type {!ArrayBuffer} */ arrayBuffer) => {
          return new Promise((resolve, reject) => {
            chrome.wallpaper.setWallpaper(
                {
                  data: arrayBuffer,
                  layout: chrome.wallpaper.WallpaperLayout.CENTER_CROPPED,
                  filename: 'wallpaper'
                },
                () => {
                  if (chrome.runtime.lastError) {
                    reject(chrome.runtime.lastError);
                  } else {
                    resolve(null);
                  }
                });
          });
        })
        .catch(() => {
          fileManager.ui.alertDialog.showHtml(
              '', str('ERROR_INVALID_WALLPAPER'), null, null, null);
        });
  },
  canExecute: function(event, fileManager) {
    const entries = fileManager.getSelection().entries;
    if (entries.length === 0) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }
    const type = FileType.getType(entries[0]);
    if (entries.length !== 1 || type.type !== 'image') {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    event.canExecute = type.subtype === 'JPEG' || type.subtype === 'PNG';
    event.command.setHidden(false);
  }
});

/**
 * Opens settings/storage sub page.
 * @type {Command}
 */
CommandHandler.COMMANDS_['volume-storage'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    chrome.fileManagerPrivate.openSettingsSubpage('storage');
  },
  canExecute: CommandUtil.canExecuteAlways
});

/**
 * Opens "providers menu" to allow users to install new providers/FSPs.
 * @type {Command}
 */
CommandHandler.COMMANDS_['new-service'] = /** @type {Command} */ ({
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  execute: function(event, fileManager) {
    fileManager.ui.gearButton.showSubMenu();
  },
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
   */
  canExecute: function(event, fileManager) {
    event.canExecute =
        (fileManager.dialogType === DialogType.FULL_PAGE &&
         !chrome.extension.inIncognitoContext);
  }
});
