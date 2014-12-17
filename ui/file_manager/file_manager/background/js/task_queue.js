// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Namespace
var importer = importer || {};

/**
 * A queue of tasks.  Tasks (subclasses of TaskQueue.Task) can be pushed onto
 * the queue.  The queue becomes active whenever it is not empty, and it will
 * begin executing tasks one at a time.  The tasks are executed in a separate
 * asynchronous context.  As each task runs, it can send update notifications
 * which are relayed back to clients via callbacks.  When the queue runs of of
 * tasks, it goes back into an idle state.  Clients can set callbacks which will
 * be triggered whenever the queue transitions between the active and idle
 * states.
 *
 * @constructor
 * @struct
 */
importer.TaskQueue = function() {
  /** @private {!Array<!importer.TaskQueue.Task>} */
  this.tasks_ = [];

  /** @private {!Array<!function(string, !importer.TaskQueue.Task)>} */
  this.updateCallbacks_ = [];

  /** @private {?function()} */
  this.activeCallback_ = null;

  /** @private {?function()} */
  this.idleCallback_ = null;

  /** @private {boolean} */
  this.active_ = false;
};

/**
 * @enum {string}
 */
importer.TaskQueue.UpdateType = {
  PROGRESS: 'PROGRESS',
  SUCCESS: 'SUCCESS',
  ERROR: 'ERROR'
};

/**
 * @param {!importer.TaskQueue.Task} task
 */
importer.TaskQueue.prototype.queueTask = function(task) {
  // The Tasks that are pushed onto the queue aren't required to be inherently
  // asynchronous.  This code force task execution to occur asynchronously.
  Promise.resolve().then(function() {
    task.addObserver(this.onTaskUpdate_.bind(this));
    this.tasks_.push(task);
    // If more than one task is queued, then the queue is already running.
    if (this.tasks_.length === 1) {
      this.runPending_();
    }
  }.bind(this));
};

/**
 * Sets a callback to be triggered when a task updates.
 * @param {function(string, !importer.TaskQueue.Task)} callback
 */
importer.TaskQueue.prototype.addUpdateCallback = function(callback) {
  this.updateCallbacks_.push(callback);
};

/**
 * Sets a callback that is triggered each time the queue goes from an idle
 * (i.e. empty with no running tasks) to an active (i.e. having a running task)
 * state.
 * @param {function()} callback
 */
importer.TaskQueue.prototype.setActiveCallback = function(callback) {
  this.activeCallback_ = callback;
};

/**
 * Sets a callback that is triggered each time the queue goes from an active to
 * an idle state.  Also see #onActive.
 * @param {function()} callback
 */
importer.TaskQueue.prototype.setIdleCallback = function(callback) {
  this.idleCallback_ = callback;
};

/**
 * Sends out notifications when a task updates.  This is meant to be called by
 * the running tasks owned by this queue.
 * @param {!importer.TaskQueue.UpdateType} updateType
 * @param {!importer.TaskQueue.Task} task
 * @private
 */
importer.TaskQueue.prototype.onTaskUpdate_ = function(updateType, task) {
  // Send a task update to clients.
  this.updateCallbacks_.forEach(function(callback) {
    callback.call(null, updateType, task);
  });

  // If the task update is a terminal one, move on to the next task.
  var UpdateType = importer.TaskQueue.UpdateType;
  if (updateType === UpdateType.SUCCESS || updateType === UpdateType.ERROR) {
    // Assumption: the currently running task is at the head of the queue.
    console.assert(this.tasks_[0] === task,
        'Only tasks that are at the head of the queue should be active');
    // Remove the completed task from the queue.
    this.tasks_.shift();
    // Run the next thing in the queue.
    this.runPending_();
  }
};

/**
 * Wakes the task queue up and runs the next pending task, or makes the queue go
 * back to sleep if no tasks are pending.
 * @private
 */
importer.TaskQueue.prototype.runPending_ = function() {
  if (this.tasks_.length === 0) {
    // All done - go back to idle.
    this.active_ = false;
    if (this.idleCallback_)
      this.idleCallback_();
    return;
  }

  if (!this.active_) {
    // If the queue is currently idle, transition to active state.
    this.active_ = true;
    if (this.activeCallback_)
      this.activeCallback_();
  }

  var nextTask = this.tasks_[0];
  nextTask.run();
};

/**
 * Interface for any Task that is to run on the TaskQueue.
 * @interface
 */
importer.TaskQueue.Task = function() {};

/**
 * @typedef {function(!importer.TaskQueue.UpdateType, !importer.TaskQueue.Task)}
 */
importer.TaskQueue.Task.Observer;

/**
 * Sets the TaskQueue that will own this task.  The TaskQueue must call this
 * prior to enqueuing a Task.
 * @param {!importer.TaskQueue.Task.Observer} observer A callback that
 *     will be triggered each time the task has a status update.
 */
importer.TaskQueue.Task.prototype.addObserver;

/**
 * Performs the actual work of the Task.  Child classes should implement this.
 */
importer.TaskQueue.Task.prototype.run;

/**
 * Base class for importer tasks.
 * @constructor
 * @implements {importer.TaskQueue.Task}
 *
 * @param {string} taskId
 */
importer.TaskQueue.BaseTask = function(taskId) {
  /** @private {string} */
  this.taskId_ = taskId;
  /** @private {!Array<!importer.TaskQueue.Task.Observer>} */
  this.observers_ = [];
};

/** @struct */
importer.TaskQueue.BaseTask.prototype = {
  /**
   * @return {string} The task ID.
   */
  get taskId() {
    return this.taskId_;
  }
};

/** @override */
importer.TaskQueue.BaseTask.prototype.addObserver = function(observer) {
  this.observers_.push(observer);
};

/** @override */
importer.TaskQueue.BaseTask.prototype.run = function() {};

/**
 * @param {!importer.TaskQueue.UpdateType} updateType
 * @protected
 */
importer.TaskQueue.BaseTask.prototype.notify = function(updateType) {
  this.observers_.forEach(
      /** @param {!importer.TaskQueue.Task.Observer} callback */
      function(callback) {
        callback.call(null, updateType, this);
      }.bind(this));
};
