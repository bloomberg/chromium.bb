// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Namespace
var importer = importer || {};

/**
 * importer.ImportHistory and importer.HistoryLoader test double.
 * ONE STOP SHOPPING!
 *
 * @constructor
 * @struct
 * @implements {importer.HistoryLoader}
 * @implements {importer.ImportHistory}
 */
importer.TestImportHistory = function() {
  /**
   * List of paths that should be considered imported.
   * @type {!Array.<string>}
   */
  this.importedPaths = {};
};

/** @override */
importer.TestImportHistory.prototype.getHistory =
    function() {
  return Promise.resolve(this);
};

/** @override */
importer.TestImportHistory.prototype.wasImported =
    function(entry, destination) {
  var path = entry.fullPath;
  return Promise.resolve(
      path in this.importedPaths &&
      this.importedPaths[path].indexOf(destination) > -1);
};

/** @override */
importer.TestImportHistory.prototype.markImported =
    function(entry, destination) {
  var path = entry.fullPath;
  if (path in this.importedPaths) {
    this.importedPaths[path].push(destination);
  } else {
    this.importedPaths[path] = [destination];
  }
  return Promise.resolve();
};

/** @override */
importer.TestImportHistory.prototype.addObserver = function() {};

/** @override */
importer.TestImportHistory.prototype.removeObserver = function() {};
