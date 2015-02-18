// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(yawano): Move all externs under ui/file_manager/externs.

/**
 * @constructor
 * @extends {Window}
 */
var BackgroundWindow = function() {};

/**
 * @type {FileBrowserBackground}
 */
BackgroundWindow.prototype.background;

/**
 * @param {Window} window
 */
BackgroundWindow.prototype.registerDialog = function(window) {};

/**
 * @param {Object=} opt_appState App state.
 * @param {number=} opt_id Window id.
 * @param {LaunchType=} opt_type Launch type. Default: ALWAYS_CREATE.
 * @param {function(string)=} opt_callback Completion callback with the App ID.
 */
BackgroundWindow.prototype.launchFileManager =
    function(opt_appState, opt_id, opt_type, opt_callback) {};


/**
 * @constructor
 * @extends {Event}
 */
var FileOperationProgressEvent = function() {};

/** @type {fileOperationUtil.EventRouter.EventType} */
FileOperationProgressEvent.prototype.reason;

/** @type {(fileOperationUtil.Error|undefined)} */
FileOperationProgressEvent.prototype.error;


/**
 * @constructor
 * @extends {Event}
 */
var EntriesChangedEvent = function() {};

/** @type {util.EntryChangedKind} */
EntriesChangedEvent.prototype.kind;

/** @type {Array.<!Entry>} */
EntriesChangedEvent.prototype.entries;

/**
 * @constructor
 * @extends {Event}
 * @struct
 * @suppress {checkStructDictInheritance}
 */
var DirectoryChangeEvent = function() {};

/** @type {DirectoryEntry} */
DirectoryChangeEvent.prototype.previousDirEntry;

/** @type {DirectoryEntry|Object} */
DirectoryChangeEvent.prototype.newDirEntry;

/** @type {boolean} */
DirectoryChangeEvent.prototype.volumeChanged;
