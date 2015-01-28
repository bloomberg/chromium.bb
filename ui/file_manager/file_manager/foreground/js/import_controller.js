// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Namespace
var importer = importer || {};

/** @enum {string} */
importer.ResponseId = {
  HIDDEN: 'hidden',
  SCANNING: 'scanning',
  NO_MEDIA: 'no_media',
  EXECUTABLE: 'executable'
};

/**
 * @typedef {{
 *   id: !importer.ResponseId,
 *   label: string,
 *   visible: boolean,
 *   executable: boolean,
 *   coreIcon: string
 * }}
 */
importer.CommandUpdate;

/**
 * Class that orchestrates background activity and UI changes on
 * behalf of Cloud Import.
 *
 * @constructor
 * @struct
 *
 * @param {!importer.ControllerEnvironment} environment The class providing
 *     access to runtime environmental information, like the current directory,
 *     volume lookup and so-on.
 * @param {!importer.MediaScanner} scanner
 * @param {!importer.ImportRunner} importRunner
 * @param {!importer.CommandWidget} commandWidget
 */
importer.ImportController =
    function(environment, scanner, importRunner, commandWidget) {

  /** @private {!importer.ControllerEnvironment} */
  this.environment_ = environment;

  /** @private {!importer.ImportRunner} */
  this.importRunner_ = importRunner;

  /** @private {!importer.MediaScanner} */
  this.scanner_ = scanner;

  /** @private {!importer.CommandWidget} */
  this.commandWidget_ = commandWidget;

  /**
   * A cache of scans by volumeId, directory URL.
   * Currently only scans of directories are cached.
   * @private {!Object.<string, !Object.<string, !importer.ScanResult>>}
   */
  this.cachedScans_ = {};

  var listener = this.onScanEvent_.bind(this);
  this.scanner_.addObserver(listener);
  // Remove the observer when the foreground window is closed.
  window.addEventListener(
      'pagehide',
      function() {
        this.scanner_.removeObserver(listener);
      }.bind(this));

  this.environment_.addVolumeUnmountListener(
      this.onVolumeUnmounted_.bind(this));

  this.environment_.addDirectoryChangedListener(
      this.onDirectoryChanged_.bind(this));

  this.commandWidget_.addExecuteListener(
      this.execute.bind(this));
};

/**
 * @param {!importer.ScanEvent} event Command event.
 * @param {importer.ScanResult} result
 *
 * @private
 */
importer.ImportController.prototype.onScanEvent_ = function(event, result) {
  if (event === importer.ScanEvent.INVALIDATED) {
    for (var key in this.cachedScans_) {
      for (var url in this.cachedScans_[key]) {
        if (this.cachedScans_[key][url].isInvalidated()) {
          delete this.cachedScans_[key][url];
        }
      }
    }
  }
  if (event === importer.ScanEvent.FINALIZED ||
      event === importer.ScanEvent.INVALIDATED) {
    this.pushUpdate_();
  }
};

/**
 * Executes import against the current directory. Should only
 * be called when the current directory has been validated
 * by calling "update" on this class.
 */
importer.ImportController.prototype.execute = function() {
  metrics.recordEnum('CloudImport.UserAction', 'IMPORT_INITIATED');
  var result = this.getScanForImport_();
  var importTask = this.importRunner_.importFromScanResult(result);
};

/**
 * Push an update to the command widget.
 * @private
 */
importer.ImportController.prototype.pushUpdate_ = function() {
  this.commandWidget_.update(this.getCommandUpdate());
};

/**
 * Returns an update describing the state of the CommandWidget.
 *
 * @return {!importer.CommandUpdate} response
 */
importer.ImportController.prototype.getCommandUpdate = function() {
  // If there is no Google Drive mount, Drive may be disabled
  // or the machine may be running in guest mode.
  if (this.environment_.isGoogleDriveMounted()) {
    var entries = this.environment_.getSelection();

    // Enabled if user has a selection and it consists entirely of files
    // that:
    // 1) are of a recognized media type
    // 2) reside on a removable media device
    // 3) in the DCIM directory
    if (entries.length) {
      if (entries.every(
          importer.isEligibleEntry.bind(null, this.environment_))) {
        return importer.ImportController.createUpdate_(
            importer.ResponseId.EXECUTABLE, entries.length);
      }
    } else if (this.isCurrentDirectoryScannable_()) {
      var scan = this.getCurrentDirectoryScan_();
      if (scan.isFinal()) {
        if (scan.getFileEntries().length === 0) {
          return importer.ImportController.createUpdate_(
              importer.ResponseId.NO_MEDIA);
        } else {
          return importer.ImportController.createUpdate_(
              importer.ResponseId.EXECUTABLE,
              scan.getFileEntries().length);
        }
      } else {
        return importer.ImportController.createUpdate_(
            importer.ResponseId.SCANNING);
      }
    }
  }

  return importer.ImportController.createUpdate_(
      importer.ResponseId.HIDDEN);
};

/**
 * @param {importer.ResponseId} responseId
 * @param {number=} opt_fileCount
 *
 * @return {!importer.CommandUpdate}
 * @private
 */
importer.ImportController.createUpdate_ =
    function(responseId, opt_fileCount) {
  switch(responseId) {
    case importer.ResponseId.HIDDEN:
      return {
        id: responseId,
        visible: false,
        executable: false,
        label: '** SHOULD NOT BE VISIBLE **',
        coreIcon: 'cloud-off'
      };
    case importer.ResponseId.SCANNING:
      return {
        id: responseId,
        visible: true,
        executable: false,
        label: str('CLOUD_IMPORT_SCANNING_BUTTON_LABEL'),
        coreIcon: 'autorenew'
      };
    case importer.ResponseId.NO_MEDIA:
      return {
        id: responseId,
        visible: true,
        executable: false,
        label: str('CLOUD_IMPORT_EMPTY_SCAN_BUTTON_LABEL'),
        coreIcon: 'cloud-done'
      };
    case importer.ResponseId.EXECUTABLE:
      return {
        id: responseId,
        label: strf('CLOUD_IMPORT_BUTTON_LABEL', opt_fileCount),
        visible: true,
        executable: true,
        coreIcon: 'cloud-upload'
      };
    default:
      assertNotReached('Unrecognized response id: ' + responseId);
  }
};

/**
 * @return {boolean} true if the current directory is scan eligible.
 * @private
 */
importer.ImportController.prototype.isCurrentDirectoryScannable_ =
    function() {
  var directory = this.environment_.getCurrentDirectory();
  return !!directory &&
      importer.isMediaDirectory(directory, this.environment_);
};

/**
 * Get or create scan for the current directory or file selection.
 *
 * @return {!importer.ScanResult} A scan result object that may be
 *     actively scanning.
 * @private
 */
importer.ImportController.prototype.getScanForImport_ = function() {
  var entries = this.environment_.getSelection();

  if (entries.length) {
    if (entries.every(
        importer.isEligibleEntry.bind(null, this.environment_))) {
      return this.scanner_.scan(entries);
    }
  } else {
    return this.getCurrentDirectoryScan_();
  }
};

/**
 * Get or create scan for the current directory.
 *
 * @return {!importer.ScanResult} A scan result object that may be
 *     actively scanning.
 * @private
 */
importer.ImportController.prototype.getCurrentDirectoryScan_ = function() {
  console.assert(this.isCurrentDirectoryScannable_());
  var directory = this.environment_.getCurrentDirectory();
  var volumeId = this.environment_.getVolumeInfo(directory).volumeId;

  // Lazily initialize the cache for volumeId.
  if (!this.cachedScans_.hasOwnProperty(volumeId)) {
    this.cachedScans_[volumeId] = {};
  }

  var url = directory.toURL();
  var scan = this.cachedScans_[volumeId][url];
  if (!scan) {
    scan = this.scanner_.scan([directory]);
    this.cachedScans_[volumeId][url] = scan;
  }
  assert(!scan.isInvalidated());
  return scan;
};

/**
 * @param {string} volumeId
 * @private
 */
importer.ImportController.prototype.onVolumeUnmounted_ = function(volumeId) {
  // Forget all scans related to the unmounted volume volume.
  if (this.cachedScans_.hasOwnProperty(volumeId)) {
    delete this.cachedScans_[volumeId];
  }
  this.pushUpdate_();
};

/** @private */
importer.ImportController.prototype.onDirectoryChanged_ = function() {
  this.pushUpdate_();
};

/**
 * Interface abstracting away the concrete file manager available
 * to commands. By hiding file manager we make it easy to test
 * ImportController.
 *
 * @interface
 * @extends {VolumeManagerCommon.VolumeInfoProvider}
 */
importer.ControllerEnvironment = function() {};

/**
 * Returns the current file selection, if any. May be empty.
 * @return {!Array.<!Entry>}
 */
importer.ControllerEnvironment.prototype.getSelection;

/**
 * Returns the directory entry for the current directory.
 * @return {!DirectoryEntry}
 */
importer.ControllerEnvironment.prototype.getCurrentDirectory;

/**
 * @param {!DirectoryEntry} entry
 */
importer.ControllerEnvironment.prototype.setCurrentDirectory;

/**
 * Returns true if the Drive mount is present.
 * @return {boolean}
 */
importer.ControllerEnvironment.prototype.isGoogleDriveMounted;

/**
 * Installs an 'unmount' listener. Listener is called with
 * the corresponding volume id when a volume is unmounted.
 * @param {function(string)} listener
 */
importer.ControllerEnvironment.prototype.addVolumeUnmountListener;

/**
 * Installs an 'directory-changed' listener. Listener is called when
 * the directory changed.
 * @param {function()} listener
 */
importer.ControllerEnvironment.prototype.addDirectoryChangedListener;

/**
 * Class providing access to various pieces of information in the
 * FileManager environment, like the current directory, volumeinfo lookup
 * By hiding file manager we make it easy to test importer.ImportController.
 *
 * @constructor
 * @implements {importer.ControllerEnvironment}
 *
 * @param {!FileManager} fileManager
 */
importer.RuntimeControllerEnvironment = function(fileManager) {
  /** @private {!FileManager} */
  this.fileManager_ = fileManager;
};

/** @override */
importer.RuntimeControllerEnvironment.prototype.getSelection =
    function() {
  return this.fileManager_.getSelection().entries;
};

/** @override */
importer.RuntimeControllerEnvironment.prototype.getCurrentDirectory =
    function() {
  return /** @type {!DirectoryEntry} */ (
      this.fileManager_.getCurrentDirectoryEntry());
};

/** @override */
importer.RuntimeControllerEnvironment.prototype.setCurrentDirectory =
    function(entry) {
  this.fileManager_.directoryModel.activateDirectoryEntry(entry);
};

/** @override */
importer.RuntimeControllerEnvironment.prototype.getVolumeInfo =
    function(entry) {
  return this.fileManager_.volumeManager.getVolumeInfo(entry);
};

/** @override */
importer.RuntimeControllerEnvironment.prototype.isGoogleDriveMounted =
    function() {
  var drive = this.fileManager_.volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE);
  return !!drive;
};

/** @override */
importer.RuntimeControllerEnvironment.prototype.addVolumeUnmountListener =
    function(listener) {
  // TODO(smckay): remove listeners when the page is torn down.
  chrome.fileManagerPrivate.onMountCompleted.addListener(
      /**
       * @param {!MountCompletedEvent} event
       * @this {importer.RuntimeControllerEnvironment}
       */
      function(event) {
        if (event.eventType === 'unmount') {
          listener(event.volumeMetadata.volumeId);
        }
      });
};

/** @override */
importer.RuntimeControllerEnvironment.prototype.addDirectoryChangedListener =
    function(listener) {
  // TODO(smckay): remove listeners when the page is torn down.
  this.fileManager_.directoryModel.addEventListener(
      'directory-changed',
      listener);
};

/**
 * Class that adapts from the new non-command button to the old
 * command style interface.
 *
 * @interface
 */
importer.CommandWidget = function() {};

/**
 * Install a listener that get's called when the user wants to initiate
 * import.
 *
 * @param {function()} listener
 */
importer.CommandWidget.prototype.addExecuteListener;

/**
 * @param {!importer.CommandUpdate} update
 */
importer.CommandWidget.prototype.update;

/**
 * Runtime implementation of CommandWidget.
 *
 * @constructor
 * @implements {importer.CommandWidget}
 * @struct
 */
importer.RuntimeCommandWidget = function() {
  /** @private {Element} */
  this.buttonElement_ = document.querySelector('#cloud-import-button');

  this.buttonElement_.onclick = this.notifyExecuteListener_.bind(this);

  /** @private {Element} */
  this.iconElement_ = document.querySelector('#cloud-import-button core-icon');

  /** @private {function()} */
  this.listener_;
};

/** @override */
importer.RuntimeCommandWidget.prototype.addExecuteListener =
    function(listener) {
  console.assert(!this.listener_);
  this.listener_ = listener;
};

/** @private */
importer.RuntimeCommandWidget.prototype.notifyExecuteListener_ = function() {
  console.assert(!!this.listener_);
  this.listener_();
};

/** @override */
importer.RuntimeCommandWidget.prototype.update = function(update) {
    this.buttonElement_.setAttribute('title', update.label);
    this.buttonElement_.disabled = !update.executable;
    this.buttonElement_.style.display  = update.visible ? 'block' : 'none';
    this.iconElement_.setAttribute('icon', update.coreIcon);
};
