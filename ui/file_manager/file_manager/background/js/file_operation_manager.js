// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {!VolumeManager} volumeManager
 * @constructor
 * @struct
 * @suppress {checkStructDictInheritance}
 * @extends {cr.EventTarget}
 */
function FileOperationManager(volumeManager) {
  /**
   * @private {!VolumeManager}
   * @const
   */
  this.volumeManager_ = volumeManager;

  /**
   * @private {!Array<!fileOperationUtil.Task>}
   */
  this.copyTasks_ = [];

  /**
   * @private {!Array<!fileOperationUtil.Task>}
   */
  this.deleteTasks_ = [];

  /**
   * @private {number}
   */
  this.taskIdCounter_ = 0;

  /**
   * @private {!fileOperationUtil.EventRouter}
   * @const
   */
  this.eventRouter_ = new fileOperationUtil.EventRouter();
}

/**
 * Adds an event listener for the tasks.
 * @param {string} type The name of the event.
 * @param {EventListenerType} handler The handler for the event.  This is called
 *     when the event is dispatched.
 * @override
 */
FileOperationManager.prototype.addEventListener = function(type, handler) {
  this.eventRouter_.addEventListener(type, handler);
};

/**
 * Removes an event listener for the tasks.
 * @param {string} type The name of the event.
 * @param {EventListenerType} handler The handler to be removed.
 * @override
 */
FileOperationManager.prototype.removeEventListener = function(type, handler) {
  this.eventRouter_.removeEventListener(type, handler);
};

/**
 * Says if there are any tasks in the queue.
 * @return {boolean} True, if there are any tasks.
 */
FileOperationManager.prototype.hasQueuedTasks = function() {
  return this.copyTasks_.length > 0 || this.deleteTasks_.length > 0;
};

/**
 * Completely clear out the copy queue, either because we encountered an error
 * or completed successfully.
 *
 * @private
 */
FileOperationManager.prototype.resetQueue_ = function() {
  this.copyTasks_ = [];
};

/**
 * Requests the specified task to be canceled.
 * @param {string} taskId ID of task to be canceled.
 */
FileOperationManager.prototype.requestTaskCancel = function(taskId) {
  var task = null;
  for (var i = 0; i < this.copyTasks_.length; i++) {
    task = this.copyTasks_[i];
    if (task.taskId !== taskId)
      continue;
    task.requestCancel();
    // If the task is not on progress, remove it immediately.
    if (i !== 0) {
      this.eventRouter_.sendProgressEvent(
          fileOperationUtil.EventRouter.EventType.CANCELED,
          task.getStatus(),
          task.taskId);
      this.copyTasks_.splice(i, 1);
    }
  }
  for (var i = 0; i < this.deleteTasks_.length; i++) {
    task = this.deleteTasks_[i];
    if (task.taskId !== taskId)
      continue;
    task.cancelRequested = true;
    // If the task is not on progress, remove it immediately.
    if (i !== 0) {
      this.eventRouter_.sendDeleteEvent(
          fileOperationUtil.EventRouter.EventType.CANCELED, task);
      this.deleteTasks_.splice(i, 1);
    }
  }
};

/**
 * Filters the entry in the same directory
 *
 * @param {Array.<Entry>} sourceEntries Entries of the source files.
 * @param {DirectoryEntry} targetEntry The destination entry of the target
 *     directory.
 * @param {boolean} isMove True if the operation is "move", otherwise (i.e.
 *     if the operation is "copy") false.
 * @return {Promise} Promise fulfilled with the filtered entry. This is not
 *     rejected.
 */
FileOperationManager.prototype.filterSameDirectoryEntry = function(
    sourceEntries, targetEntry, isMove) {
  if (!isMove)
    return Promise.resolve(sourceEntries);
  // Utility function to concat arrays.
  var compactArrays = function(arrays) {
    return arrays.filter(function(element) { return !!element; });
  };
  // Call processEntry for each item of entries.
  var processEntries = function(entries) {
    var promises = entries.map(processFileOrDirectoryEntries);
    return Promise.all(promises).then(compactArrays);
  };
  // Check all file entries and keeps only those need sharing operation.
  var processFileOrDirectoryEntries = function(entry) {
    return new Promise(function(resolve) {
      entry.getParent(function(inParentEntry) {
        if (!util.isSameEntry(inParentEntry, targetEntry))
          resolve(entry);
        else
          resolve(null);
      }, function(error) {
        console.error(error.stack || error);
        resolve(null);
      });
    });
  };
  return processEntries(sourceEntries);
};

/**
 * Kick off pasting.
 *
 * @param {Array.<Entry>} sourceEntries Entries of the source files.
 * @param {DirectoryEntry} targetEntry The destination entry of the target
 *     directory.
 * @param {boolean} isMove True if the operation is "move", otherwise (i.e.
 *     if the operation is "copy") false.
 * @param {string=} opt_taskId If the corresponding item has already created
 *     at another places, we need to specify the ID of the item. If the
 *     item is not created, FileOperationManager generates new ID.
 */
FileOperationManager.prototype.paste = function(
    sourceEntries, targetEntry, isMove, opt_taskId) {
  // Do nothing if sourceEntries is empty.
  if (sourceEntries.length === 0)
    return;

  this.filterSameDirectoryEntry(sourceEntries, targetEntry, isMove).then(
      function(entries) {
        if (entries.length === 0)
          return;
        this.queueCopy_(targetEntry, entries, isMove, opt_taskId);
  }.bind(this)).catch(function(error) {
    console.error(error.stack || error);
  });
};

/**
 * Initiate a file copy. When copying files, null can be specified as source
 * directory.
 *
 * @param {DirectoryEntry} targetDirEntry Target directory.
 * @param {Array.<Entry>} entries Entries to copy.
 * @param {boolean} isMove In case of move.
 * @param {string=} opt_taskId If the corresponding item has already created
 *     at another places, we need to specify the ID of the item. If the
 *     item is not created, FileOperationManager generates new ID.
 * @private
 */
FileOperationManager.prototype.queueCopy_ = function(
    targetDirEntry, entries, isMove, opt_taskId) {
  var task;
  var taskId = opt_taskId || this.generateTaskId();
  if (isMove) {
    // When moving between different volumes, moving is implemented as a copy
    // and delete. This is because moving between volumes is slow, and moveTo()
    // is not cancellable nor provides progress feedback.
    if (util.isSameFileSystem(entries[0].filesystem,
                              targetDirEntry.filesystem)) {
      task = new fileOperationUtil.MoveTask(taskId, entries, targetDirEntry);
    } else {
      task =
          new fileOperationUtil.CopyTask(taskId, entries, targetDirEntry, true);
    }
  } else {
    task =
        new fileOperationUtil.CopyTask(taskId, entries, targetDirEntry, false);
  }

  this.eventRouter_.sendProgressEvent(
      fileOperationUtil.EventRouter.EventType.BEGIN,
      task.getStatus(),
      task.taskId);
  task.initialize(function() {
    this.copyTasks_.push(task);
    if (this.copyTasks_.length === 1)
      this.serviceAllTasks_();
  }.bind(this));
};

/**
 * Service all pending tasks, as well as any that might appear during the
 * copy.
 *
 * @private
 */
FileOperationManager.prototype.serviceAllTasks_ = function() {
  if (!this.copyTasks_.length) {
    // All tasks have been serviced, clean up and exit.
    chrome.power.releaseKeepAwake();
    this.resetQueue_();
    return;
  }

  // Prevent the system from sleeping while copy is in progress.
  chrome.power.requestKeepAwake('system');

  var onTaskProgress = function() {
    this.eventRouter_.sendProgressEvent(
        fileOperationUtil.EventRouter.EventType.PROGRESS,
        this.copyTasks_[0].getStatus(),
        this.copyTasks_[0].taskId);
  }.bind(this);

  var onEntryChanged = function(kind, entry) {
    this.eventRouter_.sendEntryChangedEvent(kind, entry);
  }.bind(this);

  var onTaskError = function(err) {
    var task = this.copyTasks_.shift();
    var reason = err.data.name === util.FileError.ABORT_ERR ?
        fileOperationUtil.EventRouter.EventType.CANCELED :
        fileOperationUtil.EventRouter.EventType.ERROR;
    this.eventRouter_.sendProgressEvent(reason,
                                        task.getStatus(),
                                        task.taskId,
                                        err);
    this.serviceAllTasks_();
  }.bind(this);

  var onTaskSuccess = function() {
    // The task at the front of the queue is completed. Pop it from the queue.
    var task = this.copyTasks_.shift();
    this.eventRouter_.sendProgressEvent(
        fileOperationUtil.EventRouter.EventType.SUCCESS,
        task.getStatus(),
        task.taskId);
    this.serviceAllTasks_();
  }.bind(this);

  var nextTask = this.copyTasks_[0];
  this.eventRouter_.sendProgressEvent(
      fileOperationUtil.EventRouter.EventType.PROGRESS,
      nextTask.getStatus(),
      nextTask.taskId);
  nextTask.run(onEntryChanged, onTaskProgress, onTaskSuccess, onTaskError);
};

/**
 * Timeout before files are really deleted (to allow undo).
 */
FileOperationManager.DELETE_TIMEOUT = 30 * 1000;

/**
 * Schedules the files deletion.
 *
 * @param {Array.<Entry>} entries The entries.
 */
FileOperationManager.prototype.deleteEntries = function(entries) {
  // TODO(hirono): Make fileOperationUtil.DeleteTask.
  var task = Object.preventExtensions({
    entries: entries,
    taskId: this.generateTaskId(),
    entrySize: {},
    totalBytes: 0,
    processedBytes: 0,
    cancelRequested: false
  });

  // Obtains entry size and sum them up.
  var group = new AsyncUtil.Group();
  for (var i = 0; i < task.entries.length; i++) {
    group.add(function(entry, callback) {
      entry.getMetadata(function(metadata) {
        var index = task.entries.indexOf(entries);
        task.entrySize[entry.toURL()] = metadata.size;
        task.totalBytes += metadata.size;
        callback();
      }, function() {
        // Fail to obtain the metadata. Use fake value 1.
        task.entrySize[entry.toURL()] = 1;
        task.totalBytes += 1;
        callback();
      });
    }.bind(this, task.entries[i]));
  }

  // Add a delete task.
  group.run(function() {
    this.deleteTasks_.push(task);
    this.eventRouter_.sendDeleteEvent(
        fileOperationUtil.EventRouter.EventType.BEGIN, task);
    if (this.deleteTasks_.length === 1)
      this.serviceAllDeleteTasks_();
  }.bind(this));
};

/**
 * Service all pending delete tasks, as well as any that might appear during the
 * deletion.
 *
 * Must not be called if there is an in-flight delete task.
 *
 * @private
 */
FileOperationManager.prototype.serviceAllDeleteTasks_ = function() {
  this.serviceDeleteTask_(
      this.deleteTasks_[0],
      function() {
        this.deleteTasks_.shift();
        if (this.deleteTasks_.length)
          this.serviceAllDeleteTasks_();
      }.bind(this));
};

/**
 * Performs the deletion.
 *
 * @param {!Object} task The delete task (see deleteEntries function).
 * @param {function()} callback Callback run on task end.
 * @private
 */
FileOperationManager.prototype.serviceDeleteTask_ = function(task, callback) {
  var queue = new AsyncUtil.Queue();

  // Delete each entry.
  var error = null;
  var deleteOneEntry = function(inCallback) {
    if (!task.entries.length || task.cancelRequested || error) {
      inCallback();
      return;
    }
    this.eventRouter_.sendDeleteEvent(
        fileOperationUtil.EventRouter.EventType.PROGRESS, task);
    util.removeFileOrDirectory(
        task.entries[0],
        function() {
          this.eventRouter_.sendEntryChangedEvent(
              util.EntryChangedKind.DELETED, task.entries[0]);
          task.processedBytes += task.entrySize[task.entries[0].toURL()];
          task.entries.shift();
          deleteOneEntry(inCallback);
        }.bind(this),
        function(inError) {
          error = inError;
          inCallback();
        }.bind(this));
  }.bind(this);
  queue.run(deleteOneEntry);

  // Send an event and finish the async steps.
  queue.run(function(inCallback) {
    var EventType = fileOperationUtil.EventRouter.EventType;
    var reason;
    if (error)
      reason = EventType.ERROR;
    else if (task.cancelRequested)
      reason = EventType.CANCELED;
    else
      reason = EventType.SUCCESS;
    this.eventRouter_.sendDeleteEvent(reason, task);
    inCallback();
    callback();
  }.bind(this));
};

/**
 * Creates a zip file for the selection of files.
 *
 * @param {!DirectoryEntry} dirEntry The directory containing the selection.
 * @param {Array.<Entry>} selectionEntries The selected entries.
 */
FileOperationManager.prototype.zipSelection = function(
    dirEntry, selectionEntries) {
  var zipTask = new fileOperationUtil.ZipTask(
      this.generateTaskId(), selectionEntries, dirEntry, dirEntry);
  this.eventRouter_.sendProgressEvent(
      fileOperationUtil.EventRouter.EventType.BEGIN,
      zipTask.getStatus(),
      zipTask.taskId);
  zipTask.initialize(function() {
    this.copyTasks_.push(zipTask);
    if (this.copyTasks_.length == 1)
      this.serviceAllTasks_();
  }.bind(this));
};

/**
 * Generates new task ID.
 *
 * @return {string} New task ID.
 */
FileOperationManager.prototype.generateTaskId = function() {
  return 'file-operation-' + this.taskIdCounter_++;
};
