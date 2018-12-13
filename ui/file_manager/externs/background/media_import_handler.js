// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Namespace
var importer;

/*
 * Classes MediaImportHandler needs are not currently defined in externs to
 * allow for Closure compilation of its media import unittest.
 *
 * Rectify this situation by forward declaring needed classes, and defining
 * their constructor signatures, to enable Closure type checks in all users
 * of the media import handler class, including unittests.
 */

/**
 * Define TaskQueue constructor.
 * @constructor
 * @struct
 */
importer.TaskQueue = function() {};

/**
 * Declare TaskQueue.Task interface.
 * @interface
 */
importer.TaskQueue.Task = function() {};

/**
 * Define TaskQueue.BaseTask constructor.
 * @constructor
 * @implements {importer.TaskQueue.Task}
 * @param {string} taskId
 */
importer.TaskQueue.BaseTask = function(taskId) {};

/**
 * Declare DispositionChecker class.
 * @struct
 */
importer.DispositionChecker;

/**
 * Define a function type that returns a Promise that resolves the content
 * disposition of an entry.
 *
 * @typedef {function(!FileEntry, !importer.Destination, !importer.ScanMode):
 *     !Promise<!importer.Disposition>}
 */
importer.DispositionChecker.CheckerFunction;

/**
 * Define MediaImportHandler constructor: handler for importing media from
 * removable devices into the user's Drive.
 *
 * @constructor
 * @implements {importer.ImportRunner}
 * @struct
 * @param {!ProgressCenter} progressCenter
 * @param {!importer.HistoryLoader} historyLoader
 * @param {!importer.DispositionChecker.CheckerFunction} dispositionChecker
 * @param {!DriveSyncHandler} driveSyncHandler
 */
importer.MediaImportHandler = function(
    progressCenter, historyLoader, dispositionChecker, driveSyncHandler) {};

/**
 * Define importer.ImportRunner.importFromScanResult override.
 * @override
 * @return {!importer.MediaImportHandler.ImportTask} The media import task.
 */
importer.MediaImportHandler.prototype.importFromScanResult;

/**
 * Define MediaImportHandler.ImportTask.
 *
 * Note that this isn't an actual FileOperationManager.Task.  It currently uses
 * the FileOperationManager (and thus *spawns* an associated
 * FileOperationManager.CopyTask) but this is a temporary state of affairs.
 *
 * @constructor
 * @extends {importer.TaskQueue.BaseTask}
 * @struct
 * @param {string} taskId
 * @param {!importer.HistoryLoader} historyLoader
 * @param {!importer.ScanResult} scanResult
 * @param {!Promise<!DirectoryEntry>} directoryPromise
 * @param {!importer.Destination} destination The logical destination.
 * @param {!importer.DispositionChecker.CheckerFunction} dispositionChecker
 */
importer.MediaImportHandler.ImportTask = function(
    taskId, historyLoader, scanResult, directoryPromise, destination,
    dispositionChecker) {};

/** @struct */
importer.MediaImportHandler.ImportTask.prototype = {
  /** @return {!Promise} Resolves when task
      is complete, or cancelled, rejects on error. */
  get whenFinished() {}
};

/**
 * Requests cancellation of this task: an update will be sent to all observers
 * of this task when the task is actually cancelled.
 */
importer.MediaImportHandler.ImportTask.prototype.requestCancel = function() {};
