// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for async utility functions.
 */
var AsyncUtil = {};

/**
 * Asynchronous version of Array.forEach.
 * This executes a provided function callback once per array element, then
 * run completionCallback to notify the completion.
 * The callback can be an asynchronous function, but the execution is
 * sequentially done.
 *
 * @param {Array.<T>} array The array to be iterated.
 * @param {function(function(), T, number, Array.<T>} callback The iteration
 *     callback. The first argument is a callback to notify the completion of
 *     the iteration.
 * @param {function()} completionCallback Called when all iterations are
 *     completed.
 * @param {Object=} opt_thisObject Bound to callback if given.
 * @template T
 */
AsyncUtil.forEach = function(
    array, callback, completionCallback, opt_thisObject) {
  if (opt_thisObject)
    callback = callback.bind(opt_thisObject);

  var queue = new AsyncUtil.Queue();
  for (var i = 0; i < array.length; i++) {
    queue.run(function(element, index, iterationCompletionCallback) {
      callback(iterationCompletionCallback, element, index, array);
    }.bind(null, array[i], i));
  }
  queue.run(function(iterationCompletionCallback) {
    completionCallback();  // Don't pass iteration completion callback.
  });
};

/**
 * Creates a class for executing several asynchronous closures in a fifo queue.
 * Added tasks will be started in order they were added. Tasks are run
 * concurrently. At most, |limit| jobs will be run at the same time.
 *
 * @param {number} limit The number of jobs to run at the same time.
 * @constructor
 */
AsyncUtil.ConcurrentQueue = function(limit) {
  console.assert(limit > 0, '|limit| must be larger than 0');

  this.limit_ = limit;
  this.addedTasks_ = [];
  this.pendingTasks_ = [];
  this.isCancelled_ = false;

  Object.seal(this);
};

/**
 * @return {boolean} True when a task is running, otherwise false.
 */
AsyncUtil.ConcurrentQueue.prototype.isRunning = function() {
  return this.pendingTasks_.length !== 0;
};

/**
 * @return {number} Number of waiting tasks.
 */
AsyncUtil.ConcurrentQueue.prototype.getWaitingTasksCount = function() {
  return this.addedTasks_.length;
};

/**
 * @return {boolean} Number of running tasks.
 */
AsyncUtil.ConcurrentQueue.prototype.getRunningTasksCount = function() {
  return this.pendingTasks_.length;
};

/**
 * Enqueues a closure to be executed.
 * @param {function(function())} closure Closure with a completion
 *     callback to be executed.
 */
AsyncUtil.ConcurrentQueue.prototype.run = function(closure) {
  if (this.isCancelled_) {
    console.error('Queue is calcelled. Cannot add a new task.');
    return;
  }

  this.addedTasks_.push(closure);
  this.continue_();
};

/**
 * Cancels the queue. It removes all the not-run (yet) tasks. Note that this
 * does NOT stop tasks currently running.
 */
AsyncUtil.ConcurrentQueue.prototype.cancel = function() {
  this.isCancelled_ = true;
  this.addedTasks_ = [];
};

/**
 * @return {boolean} True when the queue have been requested to cancel or is
 *      already cancelled. Otherwise false.
 */
AsyncUtil.ConcurrentQueue.prototype.isCancelled = function() {
  return this.isCancelled_;
};

/**
 * Runs the next tasks if available.
 * @private
 */
AsyncUtil.ConcurrentQueue.prototype.continue_ = function() {
  if (this.addedTasks_.length === 0)
    return;

  console.assert(
      this.pendingTasks_.length <= this.limit_,
      'Too many jobs are running (' + this.pendingTasks_.length + ')');

  if (this.pendingTasks_.length >= this.limit_)
    return;

  // Run the next closure.
  var closure = this.addedTasks_.shift();
  this.pendingTasks_.push(closure);
  closure(this.onTaskFinished_.bind(this, closure));

  this.continue_();
};

/**
 * Called when a task is finished. Removes the tasks from pending task list.
 * @param {function()} closure Finished task, which has been bound in
 *     |continue_|.
 * @private
 */
AsyncUtil.ConcurrentQueue.prototype.onTaskFinished_ = function(closure) {
  var index = this.pendingTasks_.indexOf(closure);
  console.assert(index >= 0, 'Invalid task is finished');
  this.pendingTasks_.splice(index, 1);

  this.continue_();
};

/**
 * Creates a class for executing several asynchronous closures in a fifo queue.
 * Added tasks will be executed sequentially in order they were added.
 *
 * @constructor
 * @extends {AsyncUtil.ConcurrentQueue}
 */
AsyncUtil.Queue = function() {
  AsyncUtil.ConcurrentQueue.call(this, 1);
};

AsyncUtil.Queue.prototype.__proto__ = AsyncUtil.ConcurrentQueue.prototype;

/**
 * Creates a class for executing several asynchronous closures in a group in
 * a dependency order.
 *
 * @constructor
 */
AsyncUtil.Group = function() {
  this.addedTasks_ = {};
  this.pendingTasks_ = {};
  this.finishedTasks_ = {};
  this.completionCallbacks_ = [];
};

/**
 * Enqueues a closure to be executed after dependencies are completed.
 *
 * @param {function(function())} closure Closure with a completion callback to
 *     be executed.
 * @param {Array.<string>=} opt_dependencies Array of dependencies. If no
 *     dependencies, then the the closure will be executed immediately.
 * @param {string=} opt_name Task identifier. Specify to use in dependencies.
 */
AsyncUtil.Group.prototype.add = function(closure, opt_dependencies, opt_name) {
  var length = Object.keys(this.addedTasks_).length;
  var name = opt_name || ('(unnamed#' + (length + 1) + ')');

  var task = {
    closure: closure,
    dependencies: opt_dependencies || [],
    name: name
  };

  this.addedTasks_[name] = task;
  this.pendingTasks_[name] = task;
};

/**
 * Runs the enqueued closured in order of dependencies.
 *
 * @param {function()=} opt_onCompletion Completion callback.
 */
AsyncUtil.Group.prototype.run = function(opt_onCompletion) {
  if (opt_onCompletion)
    this.completionCallbacks_.push(opt_onCompletion);
  this.continue_();
};

/**
 * Runs enqueued pending tasks whose dependencies are completed.
 * @private
 */
AsyncUtil.Group.prototype.continue_ = function() {
  // If all of the added tasks have finished, then call completion callbacks.
  if (Object.keys(this.addedTasks_).length ==
      Object.keys(this.finishedTasks_).length) {
    for (var index = 0; index < this.completionCallbacks_.length; index++) {
      var callback = this.completionCallbacks_[index];
      callback();
    }
    this.completionCallbacks_ = [];
    return;
  }

  for (var name in this.pendingTasks_) {
    var task = this.pendingTasks_[name];
    var dependencyMissing = false;
    for (var index = 0; index < task.dependencies.length; index++) {
      var dependency = task.dependencies[index];
      // Check if the dependency has finished.
      if (!this.finishedTasks_[dependency])
        dependencyMissing = true;
    }
    // All dependences finished, therefore start the task.
    if (!dependencyMissing) {
      delete this.pendingTasks_[task.name];
      task.closure(this.finish_.bind(this, task));
    }
  }
};

/**
 * Finishes the passed task and continues executing enqueued closures.
 *
 * @param {Object} task Task object.
 * @private
 */
AsyncUtil.Group.prototype.finish_ = function(task) {
  this.finishedTasks_[task.name] = task;
  this.continue_();
};

/**
 * Samples calls so that they are not called too frequently.
 * The first call is always called immediately, and the following calls may
 * be skipped or delayed to keep each interval no less than |minInterval_|.
 *
 * @param {function()} closure Closure to be called.
 * @param {number=} opt_minInterval Minimum interval between each call in
 *     milliseconds. Default is 200 milliseconds.
 * @constructor
 */
AsyncUtil.RateLimiter = function(closure, opt_minInterval) {
  /**
   * @type {function()}
   * @private
   */
  this.closure_ = closure;

  /**
   * @type {number}
   * @private
   */
  this.minInterval_ = opt_minInterval || 200;

  /**
   * @type {number}
   * @private
   */
  this.scheduledRunsTimer_ = 0;

  /**
   * This variable remembers the last time the closure is called.
   * @type {number}
   * @private
   */
  this.lastRunTime_ = 0;

  Object.seal(this);
};

/**
 * Requests to run the closure.
 * Skips or delays calls so that the intervals between calls are no less than
 * |minInteval_| milliseconds.
 */
AsyncUtil.RateLimiter.prototype.run = function() {
  var now = Date.now();
  // If |minInterval| has not passed since the closure is run, skips or delays
  // this run.
  if (now - this.lastRunTime_ < this.minInterval_) {
    // Delays this run only when there is no scheduled run.
    // Otherwise, simply skip this run.
    if (!this.scheduledRunsTimer_) {
      this.scheduledRunsTimer_ = setTimeout(
          this.runImmediately.bind(this),
          this.lastRunTime_ + this.minInterval_ - now);
    }
    return;
  }

  // Otherwise, run immediately
  this.runImmediately();
};

/**
 * Calls the scheduled run immediately and cancels any scheduled calls.
 */
AsyncUtil.RateLimiter.prototype.runImmediately = function() {
  this.cancelScheduledRuns_();
  this.closure_();
  this.lastRunTime_ = Date.now();
};

/**
 * Cancels all scheduled runs (if any).
 * @private
 */
AsyncUtil.RateLimiter.prototype.cancelScheduledRuns_ = function() {
  if (this.scheduledRunsTimer_) {
    clearTimeout(this.scheduledRunsTimer_);
    this.scheduledRunsTimer_ = 0;
  }
};
