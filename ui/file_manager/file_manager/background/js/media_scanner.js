// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Recursively scans through a list of given files and directories, and creates
 * a list of media files.
 *
 * @constructor
 */
function MediaScanner() {}

/**
 * Scans a list of directory and file entries, returning image and video files.
 * @param {!Array<!Entry>} entries A list of file and directory entries.  File
 *     entries are added directly to the media list; directory entries are
 *     recursively traversed to find files, which are added to the media list.
 * @return {!Promise<!Array<!FileEntry>>}
 */
MediaScanner.prototype.scan = function(entries) {
  /**
   * Returns files and directories found under the given Entry.
   * @param {!Entry} entry
   * @return {!Promise<!Array<!Entry>>}
   */
  var scanRecurse = function(entry) {
    return new Promise(function(resolve, reject) {
      fileOperationUtil.resolveRecursively(entry, resolve, reject);
    });
  };

  /**
   * Flattens a nested list of Entries.
   * @param {!Array<!Array<!Entry>>} array
   * @return {!Array<!Entry>}
   */
  var flatten = function(array) {
    return array.reduce(function(prev, curr) {
      return prev.concat(curr);
    }, []);
  };

  /**
   * Filters non-image and non-video files out of the given list.
   * @param {!Array<!Entry>} array
   * @return {!Array<!FileEntry>}
   */
  var filter = function(array) {
    return array.filter(FileType.isImageOrVideo);
  };

  return Promise.all(entries.map(scanRecurse))
      .then(flatten)
      .then(filter);
};
