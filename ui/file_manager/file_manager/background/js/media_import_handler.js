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
 * @param {!importer.Destination} destination
 * @param {!Promise<!DirectoryEntry>} directoryPromise
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
 * @param {!importer.DispositionChecker.CheckerFunction} dispositionChecker
 * @param {!analytics.Tracker} tracker
 */
importer.MediaImportHandler = function(progressCenter, historyLoader,
                                       dispositionChecker, tracker) {
  /** @private {!ProgressCenter} */
  this.progressCenter_ = progressCenter;

  /** @private {!importer.HistoryLoader} */
  this.historyLoader_ = historyLoader;

  /** @private {!importer.TaskQueue} */
  this.queue_ = new importer.TaskQueue();

  // Prevent the system from sleeping while imports are active.
  this.queue_.setActiveCallback(function() {
    chrome.power.requestKeepAwake('system');
  });
  this.queue_.setIdleCallback(function() {
    chrome.power.releaseKeepAwake();
  });

  /** @private {!analytics.Tracker} */
  this.tracker_ = tracker;

  /** @private {number} */
  this.nextTaskId_ = 0;

  /** @private {!importer.DispositionChecker.CheckerFunction} */
  this.getDisposition_ = dispositionChecker;
};

// The name of the Drive property used to tag imported files.  Used to look up
// the property later.
importer.MediaImportHandler.IMPORTS_TAG_KEY = 'cloud-import';

// The value of the Drive property used to tag imported files.  Cloud import
// only imports 'media' right now - change this to an enum if other types of
// files start being imported.
importer.MediaImportHandler.IMPORTS_TAG_VALUE = 'media';

/** @override */
importer.MediaImportHandler.prototype.importFromScanResult =
    function(scanResult, destination, directoryPromise) {

  var task = new importer.MediaImportHandler.ImportTask(
      this.generateTaskId_(),
      this.historyLoader_,
      scanResult,
      directoryPromise,
      destination,
      this.getDisposition_,
      this.tracker_);

  task.addObserver(this.onTaskProgress_.bind(this, task));
  task.addObserver(this.onFileImported_.bind(this, task));

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
    item.progressMax = task.totalBytes;
    item.cancelCallback = function() {
      task.requestCancel();
    };
  }

  switch (updateType) {
    case UpdateType.PROGRESS:
      item.message =
          strf('CLOUD_IMPORT_ITEMS_REMAINING', task.remainingFilesCount);
      item.progressValue = task.processedBytes;
      item.state = ProgressItemState.PROGRESSING;
      break;
    case UpdateType.COMPLETE:
      item.message = '';
      item.progressValue = item.progressMax;
      item.state = ProgressItemState.COMPLETED;
      break;
    case UpdateType.ERROR:
      item.message =
          strf('CLOUD_IMPORT_ITEMS_REMAINING', task.remainingFilesCount);
      item.progressValue = task.processedBytes;
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
 * Tags newly-imported files with a Drive property.
 * @param {!importer.TaskQueue.Task} task
 * @param {string} updateType
 * @param {Object=} updateInfo
 */
importer.MediaImportHandler.prototype.onFileImported_ =
    function(task, updateType, updateInfo) {
  if (updateType !==
      importer.MediaImportHandler.ImportTask.UpdateType.ENTRY_CHANGED) {
    return;
  }
  // Update info must exist for ENTRY_CHANGED notifications.
  console.assert(updateInfo && updateInfo.destination);
  var info =
      /** @type {!importer.MediaImportHandler.ImportTask.EntryChangedInfo} */ (
          updateInfo);

  // Tag the import with a private drive property.
  chrome.fileManagerPrivate.setEntryTag(
      info.destination,
      'private',  // Scoped to just this app.
      importer.MediaImportHandler.IMPORTS_TAG_KEY,
      importer.MediaImportHandler.IMPORTS_TAG_VALUE,
      function() {
        if (chrome.runtime.lastError) {
          console.error('Unable to tag imported media: ' +
              chrome.runtime.lastError.message);
        }
      });
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
 * @param {!Promise<!DirectoryEntry>} directoryPromise
 * @param {!importer.Destination} destination The logical destination.
 * @param {!importer.DispositionChecker.CheckerFunction} dispositionChecker
 * @param {!analytics.Tracker} tracker
 */
importer.MediaImportHandler.ImportTask = function(
    taskId,
    historyLoader,
    scanResult,
    directoryPromise,
    destination,
    dispositionChecker,
    tracker) {

  importer.TaskQueue.BaseTask.call(this, taskId);
  /** @private {string} */
  this.taskId_ = taskId;

  /** @private {!importer.Destination} */
  this.destination_ = destination;

  /** @private {!Promise<!DirectoryEntry>} */
  this.directoryPromise_ = directoryPromise;

  /** @private {!importer.ScanResult} */
  this.scanResult_ = scanResult;

  /** @private {!importer.HistoryLoader} */
  this.historyLoader_ = historyLoader;

  /** @private {!analytics.Tracker} */
  this.tracker_ = tracker;

  /** @private {number} */
  this.totalBytes_ = 0;

  /** @private {number} */
  this.processedBytes_ = 0;

  /**
   * Number of duplicate files found by the content hash check.
   * @private {number} */
  this.duplicateFilesCount_ = 0;

  /** @private {number} */
  this.remainingFilesCount_ = 0;

  /** @private {?function()} */
  this.cancelCallback_ = null;

  /** @private {boolean} Indicates whether this task was canceled. */
  this.canceled_ = false;

  /** @private {number} */
  this.errorCount_ = 0;

  /** @private {!importer.DispositionChecker.CheckerFunction} */
  this.getDisposition_ = dispositionChecker;
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
      .then(this.importScanEntries_.bind(this))
      .then(this.markDuplicatesImported_.bind(this))
      .then(this.onSuccess_.bind(this))
      .catch(importer.getLogger().catcher('import-task-run'));
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
  var stats = this.scanResult_.getStatistics();
  this.remainingFilesCount_ = stats.newFileCount;
  this.totalBytes_ = stats.sizeBytes;
  this.notify(importer.TaskQueue.UpdateType.PROGRESS);

  this.tracker_.send(metrics.ImportEvents.STARTED);
};

/**
 * Initiates an import to the given location.  This should only be called once
 * the scan result indicates that it is ready.
 *
 * @private
 */
importer.MediaImportHandler.ImportTask.prototype.importScanEntries_ =
    function() {
  var resolver = new importer.Resolver();
  this.directoryPromise_.then(
      /** @this {importer.MediaImportHandler.ImportTask} */
      function(destinationDirectory) {
        AsyncUtil.forEach(
            this.scanResult_.getFileEntries(),
            this.importOne_.bind(this, destinationDirectory),
            resolver.resolve,
            resolver);
      }.bind(this));
  return resolver.promise;
};

/**
 * Marks all duplicate entries as imported.
 *
 * @private
 */
importer.MediaImportHandler.ImportTask.prototype.markDuplicatesImported_ =
    function() {
  this.historyLoader_.getHistory().then(
      /**
       * @param {!importer.ImportHistory} history
       * @this {importer.MediaImportHandler.ImportTask}
       */
      function(history) {
        this.scanResult_.getDuplicateFileEntries().forEach(
            /**
             * @param {!FileEntry} entry
             * @this {importer.MediaImportHandler.ImportTask}
             */
            function(entry) {
              history.markImported(entry, this.destination_);
            }.bind(this));
      }.bind(this))
      .catch(importer.getLogger().catcher('import-task-mark-dupes-imported'));
};

/**
 * Imports one file. If the file already exist in Drive, marks as imported.
 *
 * @param {!DirectoryEntry} destinationDirectory
 * @param {function()} completionCallback Called after this operation is
 *     complete.
 * @param {!FileEntry} entry The entry to import.
 * @private
 */
importer.MediaImportHandler.ImportTask.prototype.importOne_ =
    function(destinationDirectory, completionCallback, entry) {
  if (this.canceled_) {
    this.notify(importer.TaskQueue.UpdateType.CANCELED);
    this.tracker_.send(metrics.ImportEvents.IMPORT_CANCELLED);
    this.sendImportStats_();
    return;
  }

  this.getDisposition_(entry, importer.Destination.GOOGLE_DRIVE,
                       importer.ScanMode.CONTENT)
      .then(
          /**
           * @param {!importer.Disposition} disposition The disposition
           *     of the entry. Either some sort of dupe, or an original.
           * @return {!Promise}
           * @this {importer.DefaultMediaScanner}
           */
          function(disposition) {
            if (disposition === importer.Disposition.ORIGINAL) {
              return this.copy_(entry, destinationDirectory);
            }
            this.duplicateFilesCount_++;
            this.markAsImported_(entry);
          }.bind(this))
      // Regardless of the result of this copy, push on to the next file.
      .then(completionCallback)
      .catch(
          /** @param {*} error */
          function(error) {
            importer.getLogger().catcher('import-task-import-one')(error);
            completionCallback();
          });
};

/**
 * @param {!FileEntry} entry The file to copy.
 * @param {!DirectoryEntry} destinationDirectory The destination directory.
 * @return {!Promise<FileEntry>} Resolves to the destination file when the copy
 *     is complete.  The FileEntry may be null if the import was cancelled.  The
 *     promise will reject if an error occurs.
 * @private
 */
importer.MediaImportHandler.ImportTask.prototype.copy_ =
    function(entry, destinationDirectory) {
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
    this.markAsCopied_(entry, /** @type {!FileEntry} */ (destinationEntry));
    this.notify(importer.TaskQueue.UpdateType.PROGRESS);
    resolver.resolve(destinationEntry);
  };

  /** @this {importer.MediaImportHandler.ImportTask} */
  var onError = function(error) {
    this.cancelCallback_ = null;
    // Log the bytes as processed in spite of the error.  This ensures
    // completion of the progress bar.
    this.processedBytes_ -= currentBytes;
    this.processedBytes_ += entry.size;
    if (error.name === util.FileError.ABORT_ERR) {
      // Task cancellations result in the error callback being triggered with an
      // ABORT_ERR, but we want to ignore these errors.
      this.notify(importer.TaskQueue.UpdateType.PROGRESS);
      resolver.resolve(null);
    } else {
      this.errorCount_++;
      this.notify(importer.TaskQueue.UpdateType.ERROR);
      resolver.reject(error);
    }
  };

  fileOperationUtil.deduplicatePath(destinationDirectory, entry.name)
      .then(
          /**
           * Performs the copy using the given deduped filename.
           * @param {string} destinationFilename
           * @this {importer.MediaImportHandler.ImportTask}
           */
          function(destinationFilename) {
            this.cancelCallback_ = fileOperationUtil.copyTo(
                entry,
                destinationDirectory,
                destinationFilename,
                onEntryChanged.bind(this),
                onProgress.bind(this),
                onComplete.bind(this),
                onError.bind(this));
          }.bind(this),
          resolver.reject)
      .catch(importer.getLogger().catcher('import-task-copy'));

  return resolver.promise;
};

/**
 * @param {!FileEntry} entry
 * @param {!FileEntry} destinationEntry
 */
importer.MediaImportHandler.ImportTask.prototype.markAsCopied_ =
    function(entry, destinationEntry) {
  this.remainingFilesCount_--;
  this.historyLoader_.getHistory().then(
      /**
       * @param {!importer.ImportHistory} history
       * @this {importer.MediaImportHandler.ImportTask}
       */
      function(history) {
        history.markCopied(
            entry,
            this.destination_,
            destinationEntry.toURL());
      }.bind(this))
      .catch(importer.getLogger().catcher('import-task-mark-as-copied'));
};

/**
 * @param {!FileEntry} entry
 * @private
 */
importer.MediaImportHandler.ImportTask.prototype.markAsImported_ =
    function(entry) {
  this.remainingFilesCount_--;
  this.historyLoader_.getHistory().then(
      /** @param {!importer.ImportHistory} history */
      function(history) {
        history.markImported(entry, this.destination_);
      }.bind(this))
      .catch(importer.getLogger().catcher('import-task-mark-as-imported'));
};

/** @private */
importer.MediaImportHandler.ImportTask.prototype.onSuccess_ = function() {
  this.notify(importer.TaskQueue.UpdateType.COMPLETE);
  this.sendImportStats_();
};

/**
 * Sends import statistics to analytics.
 */
importer.MediaImportHandler.ImportTask.prototype.sendImportStats_ =
    function() {

  var scanStats = this.scanResult_.getStatistics();

  this.tracker_.send(
      metrics.ImportEvents.MEGABYTES_IMPORTED.value(
          // Make megabytes of our bytes.
          Math.floor(this.processedBytes_ / (1024 * 1024))));

  this.tracker_.send(
      metrics.ImportEvents.FILES_IMPORTED.value(
          // Substract the remaining files, in case the task was cancelled.
          scanStats.newFileCount - this.remainingFilesCount_));

  if (this.errorCount_ > 0) {
    this.tracker_.send(metrics.ImportEvents.ERRORS.value(this.errorCount_));
  }

  // Finally we want to report on the number of duplicates
  // that were identified during scanning.
  var totalDeduped = 0;
  // The scan is run without content duplicate check.
  // Instead, report the number of duplicated files found at import.
  assert(scanStats.duplicates[importer.Disposition.CONTENT_DUPLICATE] === 0);
  scanStats.duplicates[importer.Disposition.CONTENT_DUPLICATE] =
      this.duplicateFilesCount_;
  Object.keys(scanStats.duplicates).forEach(
      /**
       * @param {!importer.Disposition} disposition
       * @this {importer.MediaImportHandler.ImportTask}
       */
      function(disposition) {
        var count = scanStats.duplicates[disposition];
        totalDeduped += count;
        this.tracker_.send(
            metrics.ImportEvents.FILES_DEDUPLICATED
                .label(disposition)
                .value(count));
      }.bind(this));

  this.tracker_.send(
      metrics.ImportEvents.FILES_DEDUPLICATED
          .label('all-duplicates')
          .value(totalDeduped));
};
