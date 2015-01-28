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
importer.DriveDuplicateFinder = function() {
  /** @private {Promise<string>} */
  this.driveIdPromise_ = null;
};

/** @override */
importer.DriveDuplicateFinder.prototype.checkDuplicate = function(entry) {
  return importer.DriveDuplicateFinder.computeHash_(entry)
      .then(this.findByHash_.bind(this))
      .then(
          /**
           * @param {!Array<string>} urls
           * @return {boolean}
           */
          function(urls) {
            return urls.length > 0;
          });
};

/**
 * Computes the content hash for the given file entry.
 * @param {!FileEntry} entry
 * @private
 */
importer.DriveDuplicateFinder.computeHash_ = function(entry) {
  return new Promise(
      function(resolve, reject) {
        chrome.fileManagerPrivate.computeChecksum(
            entry.toURL(),
            /** @param {string} result The content hash. */
            function(result) {
              if (chrome.runtime.lastError) {
                reject(chrome.runtime.lastError);
              } else {
                resolve(result);
              }
            });
      });
};

/**
 * Finds files with content hashes matching the given hash.
 * @param {string} hash The content hash of the file to find.
 * @return {!Promise<Array<string>>} The URLs of the found files.  If there are
 *     no matches, the list will be empty.
 * @private
 */
importer.DriveDuplicateFinder.prototype.findByHash_ = function(hash) {
  return this.getDriveId_()
      .then(importer.DriveDuplicateFinder.searchFilesByHash_.bind(null, hash));
};

/**
 * @return {!Promise<string>} ID of the user's Drive volume.
 * @private
 */
importer.DriveDuplicateFinder.prototype.getDriveId_ = function() {
  if (!this.driveIdPromise_) {
    this.driveIdPromise_ = VolumeManager.getInstance()
        .then(
            /**
             * @param {!VolumeManager} volumeManager
             * @return {string} ID of the user's Drive volume.
             */
            function(volumeManager) {
              return volumeManager.getCurrentProfileVolumeInfo(
                  VolumeManagerCommon.VolumeType.DRIVE).volumeId;
            });
  }
  return this.driveIdPromise_;
};

/**
 * A promise-based wrapper for chrome.fileManagerPrivate.searchFilesByHashes.
 * @param {string} hash The content hash to search for.
 * @param {string} volumeId The volume to search.
 * @return <!Promise<Array<string>>> A list of file URLs.
 */
importer.DriveDuplicateFinder.searchFilesByHash_ = function(hash, volumeId) {
  return new Promise(
      function(resolve, reject) {
        chrome.fileManagerPrivate.searchFilesByHashes(
            volumeId,
            [hash],
            /** @param {!Object<string, Array<string>>} urls */
            function(urls) {
              if (chrome.runtime.lastError) {
                reject(chrome.runtime.lastError);
              } else {
                resolve(urls[hash]);
              }
            });
      });
};
