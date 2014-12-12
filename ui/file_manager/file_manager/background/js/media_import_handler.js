// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Namespace
var importer = importer || {};

/**
 * Handler for importing media from removable devices into the user's Drive.
 *
 * @constructor
 * @struct
 *
 * @param {!FileOperationManager} fileOperationManager
 * @param {!importer.MediaScanner} scanner
 */
importer.MediaImportHandler = function(fileOperationManager, scanner) {
  /** @private {!FileOperationManager} */
  this.fileOperationManager_ = fileOperationManager;

  /** @private {!importer.MediaScanner} */
  this.scanner_ = scanner;

  /**
   * If there is an active scan, this field will be set to a non-null value.
   * @type {?importer.ScanResult}
   */
  this.activeScanResult_ = null;
};

/**
 * @typedef {function():(!DirectoryEntry|!Promise<!DirectoryEntry>)}
 */
importer.MediaImportHandler.DestinationFactory;

/**
 * Import all media found in a given subdirectory tree.
 * @param {!DirectoryEntry} source The directory to import media from.
 * @param {!importer.MediaImportHandler.DestinationFactory=} opt_destination A
 *     function that returns the directory into which media will be imported.
 *     The function will be executed only when the import task actually runs.
 * @return {!importer.MediaImportHandler.ImportTask} The resulting import task.
 */
importer.MediaImportHandler.prototype.importMedia =
    function(source, opt_destination) {

  var scanResult = this.scanner_.scan([source]);
  this.activeScanResult_ = scanResult;
  scanResult.whenFinished()
      .then(
          function() {
            this.activeScanResult_ = null;
          }.bind(this));

  var destination = opt_destination ||
      importer.MediaImportHandler.defaultDestination.getImportDestination;
  return new importer.MediaImportHandler.ImportTask(
      this.fileOperationManager_,
      scanResult,
      source,
      destination);
};

/**
 * Note that this isn't an actual FileOperationManager.Task.  It currently uses
 * the FileOperationManager (and thus *spawns* an associated
 * FileOperationManager.CopyTask) but this is a temporary state of affairs.
 *
 * TODO(kenobi): Add a proper implementation that doesn't use
 * FileOperationManager, but instead actually performs the copy using the
 * fileManagerPrivate API directly.
 *
 * @constructor
 * @struct
 *
 * @param {!FileOperationManager} fileOperationManager
 * @param {!importer.ScanResult} scanResult
 * @param {!DirectoryEntry} source Source dir containing media for import.
 * @param {!importer.MediaImportHandler.DestinationFactory} destination A
 *     function that returns the directory into which media will be imported.
 */
importer.MediaImportHandler.ImportTask =
    function(fileOperationManager, scanResult, source, destination) {
  /** @private {!DirectoryEntry} */
  this.source_ = source;

  // Call fileOperationManager.requestTaskCancel to cancel this task.
  // TODO(kenobi): Add task cancellation.
  this.taskId_ = fileOperationManager.generateTaskId();

  Promise.all([
    destination(),
    scanResult.whenFinished()
  ]).then(
      function(args) {
        /** @type {!DirectoryEntry} */
        var destinationDir = args[0];

        /** @type {!importer.ScanResult} */
        var scanResult = args[1];

        fileOperationManager.paste(
            scanResult.getFileEntries(),
            destinationDir,
            /* isMove */ false,
            /* opt_taskId */ this.taskId_);
      }.bind(this));
};

/** @struct */
importer.MediaImportHandler.ImportTask.prototype = {
  /**
   * @return {string} The task ID.
   */
  get taskId() {
    return this.taskId_;
  }
};

/**
 * Namespace for a default import destination factory. The
 * defaultDestionation.getImportDestination function creates and returns the
 * directory /photos/YYYY-MM-DD in the user's Google Drive.  YYYY-MM-DD is the
 * current date.
 */
importer.MediaImportHandler.defaultDestination = {};

/**
 * Retrieves the user's drive root.
 * @return {!Promise<!DirectoryEntry>}
 * @private
 */
importer.MediaImportHandler.defaultDestination.getDriveRoot_ = function() {
  return VolumeManager.getInstance()
      .then(
          function(volumeManager) {
            var drive = volumeManager.getCurrentProfileVolumeInfo(
                VolumeManagerCommon.VolumeType.DRIVE);
            return drive.resolveDisplayRoot();
          });
};

/**
 * Fetches (creating if necessary) the import destination subdirectory.
 * @param {!DirectoryEntry} root The drive root.
 * @return {!Promise<!DirectoryEntry>}
 * @private
 */
importer.MediaImportHandler.defaultDestination.getOrCreateImportDestination_ =
    function(root) {
  /**
   * @param {string} name The name of the new directory.
   * @param {!DirectoryEntry} entry The parent directory.
   * @return {!Promise<!DirectoryEntry>} The created directory.
   */
  var mkdir_ = function(name, entry) {
    /** @const {Object} */
    var CREATE_OPTIONS = {
      create: true,
      exclusive: false
    };
    return new Promise(function(resolve, reject) {
      entry.getDirectory(name, CREATE_OPTIONS, resolve, reject);
    });
  };

  /**
   * @return {string} The current date, in YYYY-MM-DD format.
   */
  var getDateString = function() {
    var padAndConvert = function(i) {
      return (i < 10 ? '0' : '') + i.toString();
    };
    var date = new Date();
    var year = date.getFullYear().toString();
    var month = padAndConvert(date.getMonth());
    var day = padAndConvert(date.getDate());

    return year + '-' + month + '-' + day;
  };

  return Promise.resolve(root)
      .then(mkdir_.bind(this, 'photos'))
      .then(mkdir_.bind(this, getDateString()));
};

/**
 * Returns the destination directory for media imports.  Creates the
 * destination, if it doesn't exist.
 * @return {!Promise<!DirectoryEntry>}
 */
importer.MediaImportHandler.defaultDestination.getImportDestination =
    function() {
  var defaultDestination = importer.MediaImportHandler.defaultDestination;
  return defaultDestination.getDriveRoot_()
      .then(defaultDestination.getOrCreateImportDestination_);
};
