// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Namespace
var importer = importer || {};

/**
 * Interface providing access to information about active import processes.
 *
 * @interface
 */
importer.ImportRunner = function() {};

/**
 * Imports all media identified by scanResult.
 *
 * @param {!importer.ScanResult} scanResult
 * @param {!importer.MediaImportHandler.DestinationFactory=} opt_destination A
 *     function that returns the directory into which media will be imported.
 *     The function will be executed only when the import task actually runs.
 *
 * @return {!importer.MediaImportHandler.ImportTask} The resulting import task.
 */
importer.ImportRunner.prototype.importFromScanResult;

/**
 * Handler for importing media from removable devices into the user's Drive.
 *
 * @constructor
 * @implements {importer.ImportRunner}
 * @struct
 *
 * @param {!ProgressCenter} progressCenter
 * @param {!importer.HistoryLoader} historyLoader
 */
importer.MediaImportHandler = function(progressCenter, historyLoader) {
  /** @private {!ProgressCenter} */
  this.progressCenter_ = progressCenter;

  /** @private {!importer.HistoryLoader} */
  this.historyLoader_ = historyLoader;

  /** @private {!importer.TaskQueue} */
  this.queue_ = new importer.TaskQueue();

  /** @private {number} */
  this.nextTaskId_ = 0;
};

/**
 * @typedef {function():(!Promise<!DirectoryEntry>)}
 */
importer.MediaImportHandler.DestinationFactory;

/** @override */
importer.MediaImportHandler.prototype.importFromScanResult =
    function(scanResult, opt_destination) {

  var destination = opt_destination ||
      importer.MediaImportHandler.defaultDestination.getImportDestination;

  var task = new importer.MediaImportHandler.ImportTask(
      this.generateTaskId_(),
      this.historyLoader_,
      scanResult,
      destination);

  task.addObserver(this.onTaskProgress_.bind(this));

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
 * Sends updates to the ProgressCenter when an import is happening.
 *
 * @param {!importer.TaskQueue.UpdateType} updateType
 * @param {!importer.TaskQueue.Task} task
 * @private
 */
importer.MediaImportHandler.prototype.onTaskProgress_ =
    function(updateType, task) {
  var UpdateType = importer.TaskQueue.UpdateType;

  var item = this.progressCenter_.getItemById(task.taskId);
  if (!item) {
    item = new ProgressCenterItem();
    item.id = task.taskId;
    // TODO(kenobi): Might need a different progress item type here.
    item.type = ProgressItemType.COPY;
    item.message =
        strf('CLOUD_IMPORT_ITEMS_REMAINING', task.remainingFilesCount);
    item.progressMax = task.totalBytes;
    item.cancelCallback = function() {
      // TODO(kenobi): Deal with import cancellation.
    };
  }

  switch (updateType) {
    case UpdateType.PROGRESS:
      item.progressValue = task.processedBytes;
      item.message =
          strf('CLOUD_IMPORT_ITEMS_REMAINING', task.remainingFilesCount);
      break;
    case UpdateType.SUCCESS:
      item.state = ProgressItemState.COMPLETED;
      break;
    case UpdateType.ERROR:
      item.state = ProgressItemState.ERROR;
      break;
  }

  this.progressCenter_.updateItem(item);
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
 * @param {!importer.HistoryLoader} historyLoader
 * @param {!importer.ScanResult} scanResult
 * @param {!importer.MediaImportHandler.DestinationFactory} destination A
 *     function that returns the directory into which media will be imported.
 */
importer.MediaImportHandler.ImportTask = function(
    taskId,
    historyLoader,
    scanResult,
    destination) {

  importer.TaskQueue.BaseTask.call(this, taskId);
  /** @private {string} */
  this.taskId_ = taskId;

  /** @private {!importer.MediaImportHandler.DestinationFactory} */
  this.getDestination_ = destination;

  /** @private {!importer.ScanResult} */
  this.scanResult_ = scanResult;

  /** @private {!importer.HistoryLoader} */
  this.historyLoader_ = historyLoader;

  /** @private {DirectoryEntry} */
  this.destination_ = null;

  /** @private {number} */
  this.totalBytes_ = 0;

  /** @private {number} */
  this.processedBytes_ = 0;

  /** @private {number} */
  this.remainingFilesCount_ = 0;
};

/** @struct */
importer.MediaImportHandler.ImportTask.prototype = {
  /** @return {number} Number of imported bytes */
  get processedBytes() { return this.processedBytes_; },

  /** @return {number} Total number of bytes to import */
  get totalBytes() { return this.totalBytes_; },

  /** @return {number} Number of files left to import */
  get remainingFilesCount() { return this.remainingFilesCount_; }
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
  this.scanResult_.whenFinal()
      .then(this.initialize_.bind(this))
      .then(this.getDestination_.bind(this))
      .then(this.importTo_.bind(this));
};

/**
 * @private
 */
importer.MediaImportHandler.ImportTask.prototype.initialize_ = function() {
  this.remainingFilesCount_ = this.scanResult_.getFileEntries().length;
  this.totalBytes_ = this.scanResult_.getTotalBytes();
  this.notify(importer.TaskQueue.UpdateType.PROGRESS);
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

  // A count of the current number of processed bytes for this entry.
  var currentBytes = 0;

  var onProgress = function(sourceUrl, processedBytes) {
    // Update the running total, then send a progress update.
    this.processedBytes_ -= currentBytes;
    this.processedBytes_ += processedBytes;
    currentBytes = processedBytes;
    this.notify(importer.TaskQueue.UpdateType.PROGRESS);
  };

  var onEntryChanged = function(sourceUrl, destEntry) {
    this.processedBytes_ -= currentBytes;
    this.processedBytes_ += entry.size;
    this.onEntryChanged_(sourceUrl, destEntry);
    this.notify(importer.TaskQueue.UpdateType.PROGRESS);
  };

  /** @this {importer.MediaImportHandler.ImportTask} */
  var onComplete = function() {
    completionCallback();
    this.markAsCopied_(entry);
    this.notify(importer.TaskQueue.UpdateType.PROGRESS);
  };

  fileOperationUtil.copyTo(
      entry,
      this.destination_,
      entry.name,  // TODO(kenobi): account for duplicate filenames
      onEntryChanged.bind(this),
      onProgress.bind(this),
      onComplete.bind(this),
      this.onError_.bind(this));
};

/**
 * A callback to notify listeners when a file has been copied.
 * @param {string} sourceUrl
 * @param {Entry} destination
 */
importer.MediaImportHandler.ImportTask.prototype.onEntryChanged_ =
    function(sourceUrl, destination) {
  // TODO(kenobi): Add code to notify observers when entries are created.
};

/** @param {!FileEntry} entry */
importer.MediaImportHandler.ImportTask.prototype.markAsCopied_ =
    function(entry) {
  this.remainingFilesCount_--;
  var destinationUrl = this.destination_.toURL() + '/' + entry.name;
  this.historyLoader_.getHistory().then(
      /** @param {!importer.ImportHistory} history */
      function(history) {
        history.markCopied(
            entry,
            importer.Destination.GOOGLE_DRIVE,
            destinationUrl);
      });
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
