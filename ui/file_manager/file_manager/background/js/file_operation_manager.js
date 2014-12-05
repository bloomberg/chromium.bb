// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @extends {cr.EventTarget}
 */
function FileOperationManager() {
  this.copyTasks_ = [];
  this.deleteTasks_ = [];
  this.taskIdCounter_ = 0;
  this.eventRouter_ = new FileOperationManager.EventRouter();

  Object.seal(this);
}

/**
 * Manages Event dispatching.
 * Currently this can send three types of events: "copy-progress",
 * "copy-operation-completed" and "delete".
 *
 * TODO(hidehiko): Reorganize the event dispatching mechanism.
 * @constructor
 * @extends {cr.EventTarget}
 */
FileOperationManager.EventRouter = function() {
  this.pendingDeletedEntries_ = [];
  this.pendingCreatedEntries_ = [];
  this.entryChangedEventRateLimiter_ = new AsyncUtil.RateLimiter(
      this.dispatchEntryChangedEvent_.bind(this), 500);
};

/**
 * Extends cr.EventTarget.
 */
FileOperationManager.EventRouter.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Dispatches a simple "copy-progress" event with reason and current
 * FileOperationManager status. If it is an ERROR event, error should be set.
 *
 * @param {string} reason Event type. One of "BEGIN", "PROGRESS", "SUCCESS",
 *     "ERROR" or "CANCELLED". TODO(hidehiko): Use enum.
 * @param {Object} status Current FileOperationManager's status. See also
 *     FileOperationManager.Task.getStatus().
 * @param {string} taskId ID of task related with the event.
 * @param {fileOperationUtil.Error=} opt_error The info for the error. This
 *     should be set iff the reason is "ERROR".
 */
FileOperationManager.EventRouter.prototype.sendProgressEvent = function(
    reason, status, taskId, opt_error) {
  // Before finishing operation, dispatch pending entries-changed events.
  if (reason === 'SUCCESS' || reason === 'CANCELED')
    this.entryChangedEventRateLimiter_.runImmediately();

  var event = /** @type {FileOperationProgressEvent} */
      (new Event('copy-progress'));
  event.reason = reason;
  event.status = status;
  event.taskId = taskId;
  if (opt_error)
    event.error = opt_error;
  this.dispatchEvent(event);
};

/**
 * Stores changed (created or deleted) entry temporarily, and maybe dispatch
 * entries-changed event with stored entries.
 * @param {util.EntryChangedKind} kind The enum to represent if the entry is
 *     created or deleted.
 * @param {Entry} entry The changed entry.
 */
FileOperationManager.EventRouter.prototype.sendEntryChangedEvent = function(
    kind, entry) {
  if (kind === util.EntryChangedKind.DELETED)
    this.pendingDeletedEntries_.push(entry);
  if (kind === util.EntryChangedKind.CREATED)
    this.pendingCreatedEntries_.push(entry);

  this.entryChangedEventRateLimiter_.run();
};

/**
 * Dispatches an event to notify that entries are changed (created or deleted).
 * @private
 */
FileOperationManager.EventRouter.prototype.dispatchEntryChangedEvent_ =
    function() {
  if (this.pendingDeletedEntries_.length > 0) {
    var event = new Event('entries-changed');
    event.kind = util.EntryChangedKind.DELETED;
    event.entries = this.pendingDeletedEntries_;
    this.dispatchEvent(event);
    this.pendingDeletedEntries_ = [];
  }
  if (this.pendingCreatedEntries_.length > 0) {
    var event = new Event('entries-changed');
    event.kind = util.EntryChangedKind.CREATED;
    event.entries = this.pendingCreatedEntries_;
    this.dispatchEvent(event);
    this.pendingCreatedEntries_ = [];
  }
};

/**
 * Dispatches an event to notify entries are changed for delete task.
 *
 * @param {string} reason Event type. One of "BEGIN", "PROGRESS", "SUCCESS",
 *     or "ERROR". TODO(hidehiko): Use enum.
 * @param {!Object} task Delete task related with the event.
 */
FileOperationManager.EventRouter.prototype.sendDeleteEvent = function(
    reason, task) {
  var event = /** @type {FileOperationProgressEvent} */ (new Event('delete'));
  event.reason = reason;
  event.taskId = task.taskId;
  event.entries = task.entries;
  event.totalBytes = task.totalBytes;
  event.processedBytes = task.processedBytes;
  this.dispatchEvent(event);
};

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
      this.eventRouter_.sendProgressEvent('CANCELED',
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
      this.eventRouter_.sendDeleteEvent('CANCELED', task);
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

  this.eventRouter_.sendProgressEvent('BEGIN', task.getStatus(), task.taskId);
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
    this.eventRouter_.sendProgressEvent('PROGRESS',
                                        this.copyTasks_[0].getStatus(),
                                        this.copyTasks_[0].taskId);
  }.bind(this);

  var onEntryChanged = function(kind, entry) {
    this.eventRouter_.sendEntryChangedEvent(kind, entry);
  }.bind(this);

  var onTaskError = function(err) {
    var task = this.copyTasks_.shift();
    var reason = err.data.name === util.FileError.ABORT_ERR ?
        'CANCELED' : 'ERROR';
    this.eventRouter_.sendProgressEvent(reason,
                                        task.getStatus(),
                                        task.taskId,
                                        err);
    this.serviceAllTasks_();
  }.bind(this);

  var onTaskSuccess = function() {
    // The task at the front of the queue is completed. Pop it from the queue.
    var task = this.copyTasks_.shift();
    this.eventRouter_.sendProgressEvent('SUCCESS',
                                        task.getStatus(),
                                        task.taskId);
    this.serviceAllTasks_();
  }.bind(this);

  var nextTask = this.copyTasks_[0];
  this.eventRouter_.sendProgressEvent('PROGRESS',
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
  var task = Object.seal({
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
    this.eventRouter_.sendDeleteEvent('BEGIN', task);
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
 * @param {Object} task The delete task (see deleteEntries function).
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
    this.eventRouter_.sendDeleteEvent('PROGRESS', task);
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
    var reason;
    if (error)
      reason = 'ERROR';
    else if (task.cancelRequested)
      reason = 'CANCELED';
    else
      reason = 'SUCCESS';
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
  this.eventRouter_.sendProgressEvent('BEGIN',
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
