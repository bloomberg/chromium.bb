// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Mock class for DirectoryModel.
 * @constructor
 * @extends {DirectoryModel}
 */
function MockDirectoryModel() {
  /**
   * @private {!MockFileFilter}
   */
  this.fileFilter_ = new MockFileFilter();

  /**
   * @private {MockDirectoryEntry}
   */
  this.currentEntry_ = null;

  /** @private {FilesAppDirEntry} */
  this.myFiles_ = null;
}

/**
 * MockDirectoryModel inherits cr.EventTarget.
 */
MockDirectoryModel.prototype = {__proto__: cr.EventTarget.prototype};

/**
 * Returns a file filter.
 * @return {FileFilter} A file filter.
 */
MockDirectoryModel.prototype.getFileFilter = function() {
  return this.fileFilter_;
};

/**
 * @return {MockDirectoryEntry}
 */
MockDirectoryModel.prototype.getCurrentDirEntry = function() {
  return this.currentEntry_;
};

/**
 * @param {MockDirectoryEntry} entry
 * @return {Promise}
 */
MockDirectoryModel.prototype.navigateToMockEntry = function(entry) {
  return new Promise(function(resolve, reject) {
    var event = new Event('directory-changed');
    event.previousDirEntry = this.currentEntry_;
    event.newDirEntry = entry;
    event.volumeChanged =
        this.currentEntry_ && util.isSameEntry(this.currentEntry_, entry);
    this.currentEntry_ = entry;
    this.dispatchEvent(event);
    resolve();
  }.bind(this));
};

/**
 * @param {FilesAppDirEntry} myFilesEntry
 * @override
 */
MockDirectoryModel.prototype.setMyFiles = function(myFilesEntry) {
  this.myFiles_ = myFilesEntry;
};

/**
 * Mock class for FileFilter.
 * @constructor
 * @extends {FileFilter}
 */
function MockFileFilter() {}

/**
 * MockFileFilter extends cr.EventTarget.
 */
MockFileFilter.prototype = {__proto__: cr.EventTarget.prototype};

/**
 * Current implementation always returns true.
 * @param {Entry} entry File entry.
 * @return {boolean} True if the file should be shown, false otherwise.
 */
MockFileFilter.prototype.filter = function(entry) {
  return true;
};
