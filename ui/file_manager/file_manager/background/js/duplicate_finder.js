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
 * A factory for producing duplicate finders.
 * @interface
 */
importer.DuplicateFinder.Factory = function() {};

/** @return {!importer.DuplicateFinder} */
importer.DuplicateFinder.Factory.prototype.create;

/**
 * A duplicate finder for Google Drive.
 *
 * @constructor
 * @implements {importer.DuplicateFinder}
 * @struct
 */
importer.DriveDuplicateFinder = function() {
  /** @private {Promise<string>} */
  this.driveIdPromise_ = null;

  /**
   * Aggregate time spent computing content hashes (in ms).
   * @private {number}
   */
  this.computeHashTime_ = 0;

  /**
   * Aggregate time spent performing content hash searches (in ms).
   * @private {number}
   */
  this.searchHashTime_ = 0;
};

/**
 * @typedef {{
 *   computeHashTime: number,
 *   searchHashTime: number
 * }}
 */
importer.DriveDuplicateFinder.Statistics;

/** @override */
importer.DriveDuplicateFinder.prototype.checkDuplicate = function(entry) {
  return this.computeHash_(entry)
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
importer.DriveDuplicateFinder.prototype.computeHash_ = function(entry) {
  return new Promise(
      /** @this {importer.DriveDuplicateFinder} */
      function(resolve, reject) {
        var startTime = new Date().getTime();
        chrome.fileManagerPrivate.computeChecksum(
            entry.toURL(),
            /** @param {string} result The content hash. */
            function(result) {
              var endTime = new Date().getTime();
              this.searchHashTime_ += endTime - startTime;
              if (chrome.runtime.lastError) {
                reject(chrome.runtime.lastError);
              } else {
                resolve(result);
              }
            });
      }.bind(this));
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
      .then(this.searchFilesByHash_.bind(this, hash));
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
importer.DriveDuplicateFinder.prototype.searchFilesByHash_ =
    function(hash, volumeId) {
  return new Promise(
      /** @this {importer.DriveDuplicateFinder} */
      function(resolve, reject) {
        var startTime = new Date().getTime();
        chrome.fileManagerPrivate.searchFilesByHashes(
            volumeId,
            [hash],
            /**
             * @param {!Object<string, Array<string>>} urls
             * @this {importer.DriveDuplicateFinder}
             */
            function(urls) {
              var endTime = new Date().getTime();
              this.searchHashTime_ += endTime - startTime;
              if (chrome.runtime.lastError) {
                reject(chrome.runtime.lastError);
              } else {
                resolve(urls[hash]);
              }
            }.bind(this));
      }.bind(this));
};

/** @return {!importer.DriveDuplicateFinder.Statistics} */
importer.DriveDuplicateFinder.prototype.getStatistics = function() {
  return {
    computeHashTime: this.computeHashTime_,
    searchHashTime: this.searchHashTime_
  };
};

/**
 * @constructor
 * @implements {importer.DuplicateFinder.Factory}
 */
importer.DriveDuplicateFinder.Factory = function() {};

/** @override */
importer.DriveDuplicateFinder.Factory.prototype.create = function() {
  return new importer.DriveDuplicateFinder();
};
