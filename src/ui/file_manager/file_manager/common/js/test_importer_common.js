// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shared cloud importer namespace
var importer = importer || {};

/**
 * Sets up a logger for use in unit tests.  The test logger doesn't attempt to
 * access chrome's sync file system.  Call this during setUp.
 * @return {!importer.TestLogger}
 */
importer.setupTestLogger = function() {
  var logger = new importer.TestLogger();
  importer.logger_ = logger;
  return logger;
};

/**
 * A {@code importer.Logger} for testing.  Just sends output to the console.
 *
 * @constructor
 * @implements {importer.Logger}
 * @struct
 * @final
 */
importer.TestLogger = function() {
  /** @public {!TestCallRecorder} */
  this.errorRecorder = new TestCallRecorder();

  /** @type {boolean} Should we print stuff to console */
  this.quiet_ = false;
};

/**
 * Causes logger to stop printing anything to console.
 * This can be useful when errors are expected in tests.
 */
importer.TestLogger.prototype.quiet = function() {
  this.quiet_ = true;
};

/** @override */
importer.TestLogger.prototype.info = function(content) {
  if (!this.quiet_) {
    console.log(content);
  }
};

/** @override */
importer.TestLogger.prototype.error = function(content) {
  this.errorRecorder.callback(content);
  if (!this.quiet_) {
    console.error(content);
    console.error(new Error('Error stack').stack);
  }
};

/** @override */
importer.TestLogger.prototype.catcher = function(context) {
  return function(error) {
    this.error('Caught promise error. Context: ' + context +
        ' Error: ' + error.message);
    if (!this.quiet_) {
      console.error(error.stack);
    }
  }.bind(this);
};
