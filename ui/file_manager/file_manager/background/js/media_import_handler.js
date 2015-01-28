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
 * @typedef {function():(!Promise<!DirectoryEntry>)}
 */
importer.ImportRunner.DestinationFactory;

/**
 * Imports all media identified by scanResult.
 *
 * @param {!importer.ScanResult} scanResult
 * @param {!importer.ImportRunner.DestinationFactory=} opt_destination A
 *     function that returns the directory into which media will be imported.
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
 * @param {!importer.DuplicateFinder} duplicateFinder
 */
importer.MediaImportHandler =
    function(progressCenter, historyLoader, duplicateFinder) {
  /** @private {!ProgressCenter} */
  this.progressCenter_ = progressCenter;

  /** @private {!importer.HistoryLoader} */
  this.historyLoader_ = historyLoader;

  /** @private {!importer.TaskQueue} */
  this.queue_ = new importer.TaskQueue();

  /** @private {!importer.DuplicateFinder} */
  this.duplicateFinder_ = duplicateFinder;

  /** @private {number} */
  this.nextTaskId_ = 0;
};

/** @override */
importer.MediaImportHandler.prototype.importFromScanResult =
    function(scanResult, opt_destination) {
  var destination = opt_destination ||
      importer.MediaImportHandler.defaultDestination.getImportDestination;

  var task = new importer.MediaImportHandler.ImportTask(
      this.generateTaskId_(),
      this.historyLoader_,
      scanResult,
      destination,
      this.duplicateFinder_);

  task.addObserver(this.onTaskProgress_.bind(this, task));

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
 * @param {!importer.TaskQueue.Task} task
 * @param {string} updateType
 * @private
 */
importer.MediaImportHandler.prototype.onTaskProgress_ =
    function(task, updateType) {
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
      task.requestCancel();
    };
  }

  switch (updateType) {
    case UpdateType.PROGRESS:
      item.progressValue = task.processedBytes;
      item.message =
          strf('CLOUD_IMPORT_ITEMS_REMAINING', task.remainingFilesCount);
      break;
    case UpdateType.SUCCESS:
      item.message = '';
      item.progressValue = item.progressMax;
      item.state = ProgressItemState.COMPLETED;
      break;
    case UpdateType.ERROR:
      item.message = '';
      item.progressValue = item.progressMax;
      item.state = ProgressItemState.ERROR;
      break;
    case UpdateType.CANCELED:
      item.message = '';
      item.state = ProgressItemState.CANCELED;
      break;
  }

  this.progressCenter_.updateItem(item);
};

/**
 * Note that this isn't an actual FileOperationManager.Task.  It currently uses
 * the FileOperationManager (and thus *spawns* an associated
 * FileOperationManager.CopyTask) but this is a temporary state of affairs.
 *
 * @constructor
 * @extends {importer.TaskQueue.BaseTask}
 * @struct
 *
 * @param {string} taskId
 * @param {!importer.HistoryLoader} historyLoader
 * @param {!importer.ScanResult} scanResult
 * @param {!importer.ImportRunner.DestinationFactory} destinationFactory A
 *     function that returns the directory into which media will be imported.
  * @param {!importer.DuplicateFinder} duplicateFinder A duplicate-finder linked
  *     to the import destination, that will be used to deduplicate imports.
 */
importer.MediaImportHandler.ImportTask = function(
    taskId,
    historyLoader,
    scanResult,
    destinationFactory,
    duplicateFinder) {

  importer.TaskQueue.BaseTask.call(this, taskId);
  /** @private {string} */
  this.taskId_ = taskId;

  /** @private {!importer.ImportRunner.DestinationFactory} */
  this.destinationFactory_ = destinationFactory;

  /** @private {!importer.DuplicateFinder} */
  this.deduplicator_ = duplicateFinder;

  /** @private {!importer.ScanResult} */
  this.scanResult_ = scanResult;

  /** @private {!importer.HistoryLoader} */
  this.historyLoader_ = historyLoader;

  /** @private {Promise<!DirectoryEntry>} */
  this.destinationPromise_ = null;

  /** @private {number} */
  this.totalBytes_ = 0;

  /** @private {number} */
  this.processedBytes_ = 0;

  /** @private {number} */
  this.remainingFilesCount_ = 0;

  /** @private {?function()} */
  this.cancelCallback_ = null;

  /** @private {boolean} Indicates whether this task was canceled. */
  this.canceled_ = false;
};

/**
 * Update types that are specific to ImportTask.  Clients can add Observers to
 * ImportTask to listen for these kinds of updates.
 * @enum {string}
 */
importer.MediaImportHandler.ImportTask.UpdateType = {
  ENTRY_CHANGED: 'ENTRY_CHANGED'
};

/**
 * Auxilliary info for ENTRY_CHANGED notifications.
 * @typedef {{
 *   sourceUrl: string,
 *   destination: !Entry
 * }}
 */
importer.MediaImportHandler.ImportTask.EntryChangedInfo;

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
      .then(this.getDestination.bind(this))
      .then(this.importTo_.bind(this))
      .catch(importer.getLogger().catcher('import-task-chain'));
};

/**
 * Request cancellation of this task.  An update will be sent to observers once
 * the task is actually cancelled.
 */
importer.MediaImportHandler.ImportTask.prototype.requestCancel = function() {
  this.canceled_ = true;
  if (this.cancelCallback_) {
    // Reset the callback before calling it, as the callback might do anything
    // (including calling #requestCancel again).
    var cancelCallback = this.cancelCallback_;
    this.cancelCallback_ = null;
    cancelCallback();
  }
};

/** @private */
importer.MediaImportHandler.ImportTask.prototype.initialize_ = function() {
  this.remainingFilesCount_ = this.scanResult_.getFileEntries().length;
  this.totalBytes_ = this.scanResult_.getTotalBytes();
  this.notify(importer.TaskQueue.UpdateType.PROGRESS);
};

/**
 * Returns a DirectoryEntry for the destination.  The destination is guaranteed
 * to have been created when the promise resolves.
 * @return {!Promise<!DirectoryEntry>}
 */
importer.MediaImportHandler.ImportTask.prototype.getDestination =
    function() {
  if (!this.destinationPromise_) {
    this.destinationPromise_ = this.destinationFactory_();
  }
  return this.destinationPromise_;
};

/**
 * Initiates an import to the given location.  This should only be called once
 * the scan result indicates that it is ready.
 * @param {!DirectoryEntry} destination
 * @private
 */
importer.MediaImportHandler.ImportTask.prototype.importTo_ =
    function(destination) {
  AsyncUtil.forEach(
      this.scanResult_.getFileEntries(),
      this.importOne_.bind(this, destination),
      this.onSuccess_.bind(this));
};

/**
 * @param {!DirectoryEntry} destination
 * @param {function()} completionCallback Called after this operation is
 *     complete.
 * @param {!FileEntry} entry The entry to import.
 * @private
 */
importer.MediaImportHandler.ImportTask.prototype.importOne_ =
    function(destination, completionCallback, entry) {
  if (this.canceled_) {
    this.notify(importer.TaskQueue.UpdateType.CANCELED);
    return;
  }

  this.deduplicator_.checkDuplicate(entry)
      .then(
          /** @param {boolean} isDuplicate */
          function(isDuplicate) {
            if (isDuplicate) {
              // If the given file is a duplicate, don't import it again.  Just
              // update the progress indicator.
              // TODO(kenobi): Update import history to mark the dupe as sync'd.
              this.processedBytes_ += entry.size;
              this.notify(importer.TaskQueue.UpdateType.PROGRESS);
              return Promise.resolve();
            } else {
              return this.copy_(entry, destination);
            }
          }.bind(this))
      .then(completionCallback);
};

/**
 * @param {!FileEntry} entry The file to copy.
 * @param {!DirectoryEntry} destination The destination directory.
 * @return {!Promise<!FileEntry>} Resolves to the destination file when the copy
 *     is complete.
 * @private
 */
importer.MediaImportHandler.ImportTask.prototype.copy_ =
    function(entry, destination) {
  // A count of the current number of processed bytes for this entry.
  var currentBytes = 0;

  var resolver = new importer.Resolver();

  /**
   * Updates the task when the copy code reports progress.
   * @param {string} sourceUrl
   * @param {number} processedBytes
   * @this {importer.MediaImportHandler.ImportTask}
   */
  var onProgress = function(sourceUrl, processedBytes) {
    // Update the running total, then send a progress update.
    this.processedBytes_ -= currentBytes;
    this.processedBytes_ += processedBytes;
    currentBytes = processedBytes;
    this.notify(importer.TaskQueue.UpdateType.PROGRESS);
  };

  /**
   * Updates the task when the new file has been created.
   * @param {string} sourceUrl
   * @param {Entry} destinationEntry
   * @this {importer.MediaImportHandler.ImportTask}
   */
  var onEntryChanged = function(sourceUrl, destinationEntry) {
    this.processedBytes_ -= currentBytes;
    this.processedBytes_ += entry.size;
    destinationEntry.size = entry.size;
    this.notify(
        importer.MediaImportHandler.ImportTask.UpdateType.ENTRY_CHANGED,
        {
          sourceUrl: sourceUrl,
          destination: destinationEntry
        });
    this.notify(importer.TaskQueue.UpdateType.PROGRESS);
  };

  /**
   * @param {Entry} destinationEntry The new destination entry.
   * @this {importer.MediaImportHandler.ImportTask}
   */
  var onComplete = function(destinationEntry) {
    this.cancelCallback_ = null;
    this.markAsCopied_(entry, destination);
    this.notify(importer.TaskQueue.UpdateType.PROGRESS);
    resolver.resolve(destinationEntry);
  };

  /** @this {importer.MediaImportHandler.ImportTask} */
  var onError = function(error) {
    this.cancelCallback_ = null;
    this.onError_(error);
    resolver.reject(error);
  };

  this.cancelCallback_ = fileOperationUtil.copyTo(
      entry,
      destination,
      entry.name,  // TODO(kenobi): account for duplicate filenames
      onEntryChanged.bind(this),
      onProgress.bind(this),
      onComplete.bind(this),
      onError.bind(this));

  return resolver.promise;
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

/**
 * @param {!FileEntry} entry
 * @param {!DirectoryEntry} destination
 */
importer.MediaImportHandler.ImportTask.prototype.markAsCopied_ =
    function(entry, destination) {
  this.remainingFilesCount_--;
  var destinationUrl = destination.toURL() + '/' + entry.name;
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
 * defaultDestination.getImportDestination function creates and returns the
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
    // Months are 0-based, but days aren't.
    var month = padAndConvert(date.getMonth()) + 1;
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
      .then(defaultDestination.getOrCreateImportDestination_)
      .catch(importer.getLogger().catcher('import-destination-provision'));
};
