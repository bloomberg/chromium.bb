// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
 * @constructor
 * @extends {Event}
 */
var FileOperationProgressEvent = function() {};

/** @type {string} */
FileOperationProgressEvent.prototype.reason;

/** @type {(FileOperationManager.Error|undefined)} */
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
