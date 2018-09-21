// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {FileManager}
 */
var fileManager;

/**
 * Indicates if the DOM and scripts have been already loaded.
 * TODO(noel): is this variable used anywhere?
 * @type {boolean}
 */
var pageLoaded = false;

/**
 * Initializes the File Manager UI and starts the File Manager dialog. Called
 * by main.html after the DOM and all scripts have been loaded.
 * @param event DOMContentLoaded event.
 */
function init(event) {
  fileManager.initializeUI(document.body).then(function() {
    util.testSendMessage('ready');
    metrics.recordInterval('Load.Total');
    fileManager.tracker.send(metrics.Management.WINDOW_CREATED);
  });
}

// Creates the File Manager object. Note that the DOM, or any external
// scripts, may not be ready yet.
fileManager = new FileManager();

// Initialize the core stuff, which doesn't require access to the DOM or
// to external scripts.
fileManager.initializeCore();

// Final initialization performed after DOM and all scripts have loaded.
document.addEventListener('DOMContentLoaded', init);

metrics.recordInterval('Load.Script');  // Must be the last line.
