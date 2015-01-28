// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shared cloud importer namespace
var importer = importer || {};

/**
 * Sets up a logger for use in unit tests.  The test logger doesn't attempt to
 * access chrome's sync file system.  Call this during setUp.
 */
importer.setupTestLogger = function() {
  importer.logger_ = new importer.TestLogger();
};

/**
 * A {@code importer.Logger} for testing.  Just sends output to the console.
 *
 * @constructor
 * @implements {importer.Logger}
 * @struct
 * @final
 */
importer.TestLogger = function() {};

/** @override */
importer.TestLogger.prototype.info = function(content) {
  console.log(content);
};

/** @override */
importer.TestLogger.prototype.error = function(content) {
  console.error(content);
  console.error(new Error('Error stack').stack);
};

/** @override */
importer.TestLogger.prototype.catcher = function(context) {
  return function(error) {
    this.error('Caught promise error. Context: ' + context +
        ' Error: ' + error.message);
    console.error(error.stack);
  }.bind(this);
};
