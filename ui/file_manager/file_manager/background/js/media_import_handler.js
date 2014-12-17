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

  /** @private {!importer.TaskQueue} */
  this.queue_ = new importer.TaskQueue();

  /** @private {!importer.MediaScanner} */
  this.scanner_ = scanner;

  /**
   * If there is an active scan, this field will be set to a non-null value.
   * @type {?importer.ScanResult}
   */
  this.activeScanResult_ = null;

  /** @private {number} */
  this.nextTaskId_ = 0;
};

/**
 * @typedef {function():(!Promise<!DirectoryEntry>)}
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

  var task = new importer.MediaImportHandler.ImportTask(
      this.generateTaskId_(),
      this.fileOperationManager_,
      scanResult,
      source,
      destination);

  this.queue_.queueTask(task);

  return task;
};

/**
 * Generates unique task IDs.
 * @private
 */
importer.MediaImportHandler.prototype.generateTaskId_ = function() {
  return 'media-import' + this.nextTaskId_++;
};

/**
 * Note that this isn't an actual FileOperationManager.Task.  It currently uses
 * the FileOperationManager (and thus *spawns* an associated
 * FileOperationManager.CopyTask) but this is a temporary state of affairs.
 *
 * TODO(kenobi): Add a proper implementation that doesn't use
 * FileOperationManager, but instead actually performs the copy using the
 * fileManagerPrivate API directly.
 * TODO(kenobi): Add task cancellation.
 *
 * @constructor
 * @extends {importer.TaskQueue.BaseTask}
 * @struct
 *
 * @param {string} taskId
 * @param {!FileOperationManager} fileOperationManager
 * @param {!importer.ScanResult} scanResult
 * @param {!DirectoryEntry} source Source dir containing media for import.
 * @param {!importer.MediaImportHandler.DestinationFactory} destination A
 *     function that returns the directory into which media will be imported.
 */
importer.MediaImportHandler.ImportTask =
    function(taskId, fileOperationManager, scanResult, source, destination) {
  importer.TaskQueue.BaseTask.call(this, taskId);

  /** @private {!DirectoryEntry} */
  this.source_ = source;

  /** @private {string} */
  this.taskId_ = taskId;

  /** @private {!importer.MediaImportHandler.DestinationFactory} */
  this.getDestination_ = destination;

  /** @private {!importer.ScanResult} */
  this.scanResult_ = scanResult;

  /** @private {!FileOperationManager} */
  this.fileOperationManager_ = fileOperationManager;

  /** @private {DirectoryEntry} */
  this.destination_ = null;
};

/**
 * Extends importer.TaskQueue.Task
 */
importer.MediaImportHandler.ImportTask.prototype.__proto__ =
    importer.TaskQueue.BaseTask.prototype;

/** @override */
importer.MediaImportHandler.ImportTask.prototype.run = function() {
  // Wait for the scan to finish, then get the destination entry, then start the
  // import.
  this.scanResult_.whenFinished()
      .then(this.getDestination_.bind(this))
      .then(this.importTo_.bind(this));
};

/**
 * Initiates an import to the given location.  This should only be called once
 * the scan result indicates that it is ready.
 * @param {!DirectoryEntry} destination
 * @private
 */
importer.MediaImportHandler.ImportTask.prototype.importTo_ =
    function(destination) {
  this.destination_ = destination;
  AsyncUtil.forEach(
      this.scanResult_.getFileEntries(),
      this.importOne_.bind(this),
      this.onSuccess_.bind(this));
};

/**
 * @param {function()} completionCallback Called after this operation is
 *     complete.
 * @param {!FileEntry} entry The entry to import.
 * @param {number} index The entry's index in the scan results.
 * @private
 */
importer.MediaImportHandler.ImportTask.prototype.importOne_ =
    function(completionCallback, entry, index) {
  // TODO(kenobi): Check for cancellation.
  fileOperationUtil.copyTo(
      entry,
      this.destination_,
      entry.name,  // TODO(kenobi): account for duplicate filename
      this.onEntryChanged_.bind(this),
      this.onProgress_.bind(this),
      completionCallback,
      this.onError_.bind(this));
};

/**
 * A callback to notify listeners when a file has been copied.
 * @param {Entry} source
 * @param {Entry} destination
 */
importer.MediaImportHandler.ImportTask.prototype.onEntryChanged_ =
    function(source, destination) {
  // TODO(kenobi): Add code to notify observers when entries are created.
};

/**
 * @param {Entry} entry
 * @param {number} processedBytes
 * @private
 */
importer.MediaImportHandler.ImportTask.prototype.onProgress_ =
    function(entry, processedBytes) {
  this.notify(importer.TaskQueue.UpdateType.PROGRESS);
};

/** @private */
importer.MediaImportHandler.ImportTask.prototype.onSuccess_ = function() {
  this.notify(importer.TaskQueue.UpdateType.SUCCESS);
};

/**
 * @param {DOMError} error
 * @private
 */
importer.MediaImportHandler.ImportTask.prototype.onError_ = function(error) {
  this.notify(importer.TaskQueue.UpdateType.ERROR);
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

/**
 * Sends events for progress updates and creation of file entries.
 *
 * TODO: File entry-related events might need to be handled via callback and not
 * events - see crbug.com/358491
 *
 * @constructor
 * @extends {cr.EventTarget}
 */
importer.MediaImportHandler.EventRouter = function() {
};

/**
 * Extends cr.EventTarget.
 */
importer.MediaImportHandler.EventRouter.prototype.__proto__ =
    cr.EventTarget.prototype;

/**
 * @param {!importer.MediaImportHandler.ImportTask} task
 */
importer.MediaImportHandler.EventRouter.prototype.sendUpdate = function(task) {
};

/**
 * @param {!FileEntry} entry The new entry.
 */
importer.MediaImportHandler.EventRouter.prototype.sendEntryCreated =
    function(entry) {
};
