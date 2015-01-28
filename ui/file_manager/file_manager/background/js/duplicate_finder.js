// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Namespace
var importer = importer || {};

/**
 * Interface for import deduplicators.  A duplicate finder is linked to an
 * import destination, and will check whether files already exist in that import
 * destination.
 * @interface
 */
importer.DuplicateFinder = function() {};

/**
 * Checks whether the given file already exists in the import destination.
 * @param {!FileEntry} entry The file entry to check.
 * @return {!Promise<boolean>}
 */
importer.DuplicateFinder.prototype.checkDuplicate;

/**
 * A duplicate finder for Google Drive.
 * @constructor
 * @implements {importer.DuplicateFinder}
 * @struct
 */
importer.DriveDuplicateFinder = function() {};

/** @override */
importer.DriveDuplicateFinder.prototype.checkDuplicate = function(entry) {
  // TODO(kenobi): Implement content hash deduplication.
  return Promise.resolve(false);
};

/**
 * A duplicate finder for testing.  Allows the return value to be set.
 * @constructor
 * @implements {importer.DuplicateFinder}
 * @struct
 */
importer.TestDuplicateFinder = function() {
  /** @type {boolean} */
  this.returnValue = false;
};

/** @override */
importer.TestDuplicateFinder.prototype.checkDuplicate = function(entry) {
  return Promise.resolve(this.returnValue);
};
