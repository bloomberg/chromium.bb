// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Namespace
var importer = importer || {};

/**
 * @typedef {{
 *   label_id: string,
 *   visible: boolean,
 *   executable: boolean
 * }}
 */
importer.UpdateResponse;

/** @enum {importer.UpdateResponse} */
importer.UpdateResponses = {
  HIDDEN: {
    label_id: 'CLOUD_IMPORT_BUTTON_LABEL',
    visible: false,
    executable: false
  },
  SCANNING: {
    label_id: 'CLOUD_IMPORT_SCANNING_BUTTON_LABEL',
    visible: true,
    executable: false
  },
  EXECUTABLE: {
    label_id: 'CLOUD_IMPORT_BUTTON_LABEL',
    visible: true,
    executable: true
  }
};

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
 * @param {function()} commandUpdateHandler
 */
importer.ImportController =
    function(environment, scanner, importRunner, commandUpdateHandler) {

  /** @private {!importer.ControllerEnvironment} */
  this.environment_ = environment;

  /** @private {!importer.ImportRunner} */
  this.importRunner_ = importRunner;

  /** @private {!importer.MediaScanner} */
  this.scanner_ = scanner;

  /** @private {function()} */
  this.updateCommands_ = commandUpdateHandler;

  /** @private {!importer.ScanObserver} */
  this.scanObserverBound_ = this.onScanEvent_.bind(this);

  this.scanner_.addObserver(this.scanObserverBound_);

  /** @private {!Object.<string, !importer.ScanResult>} */
  this.directoryScans_ = {};
};

/**
 * @param {!importer.ScanEvent} event Command event.
 * @param {importer.ScanResult} result
 */
importer.ImportController.prototype.onScanEvent_ = function(event, result) {
  // TODO(smckay): only do this if this is a directory scan.
  if (event === importer.ScanEvent.FINALIZED) {
    this.updateCommands_();
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
  this.importRunner_.importFromScanResult(result);
};

/**
 * @return {!importer.UpdateResponse} response
 */
importer.ImportController.prototype.update = function() {

  // If there is no Google Drive mount, Drive may be disabled
  // or the machine may be running in guest mode.
  if (!this.environment_.isGoogleDriveMounted()) {
    return importer.UpdateResponses.HIDDEN;
  }

  var entries = this.environment_.getSelection();

  // Enabled if user has a selection and it consists entirely of files
  // that:
  // 1) are of a recognized media type
  // 2) reside on a removable media device
  // 3) in the DCIM dir
  if (entries.length) {
    if (entries.every(
        importer.isEligibleEntry.bind(null, this.environment_))) {
      // TODO(smckay): Include entry count in label.
      return importer.UpdateResponses.EXECUTABLE;
    }
  } else if (this.isCurrentDirectoryScannable_()) {
    var scan = this.getCurrentDirectoryScan_();
    return scan.isFinal() ?
        importer.UpdateResponses.EXECUTABLE :
        importer.UpdateResponses.SCANNING;
  }

  return importer.UpdateResponses.HIDDEN;
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
  var url = directory.toURL();
  var scan = this.directoryScans_[url];
  if (!scan) {
    scan = this.scanner_.scan([directory]);
    // TODO(smckay): evict scans when a device is unmounted or changed.
    this.directoryScans_[url] = scan;
  }
  return scan;
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
 * Returns true if the Drive mount is present.
 * @return {boolean}
 */
importer.ControllerEnvironment.prototype.isGoogleDriveMounted;


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
