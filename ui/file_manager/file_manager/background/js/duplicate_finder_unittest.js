// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {!importer.DuplicateFinder} */
var duplicateFinder;

/** @type {!VolumeInfo} */
var drive;

/**
 * Map of file URL to hash code.
 * @type {!Object<string, string>}
 */
var hashes = {};

/**
 * Map of hash code to file URL.
 * @type {!Object<string, string>}
 */
var fileUrls = {};

/** @type {!MockFileSystem} */
var fileSystem;

// Set up string assets.
loadTimeData.data = {
  CLOUD_IMPORT_ITEMS_REMAINING: '',
  DRIVE_DIRECTORY_LABEL: 'My Drive',
  DOWNLOADS_DIRECTORY_LABEL: 'Downloads'
};

function setUp() {
  // importer.setupTestLogger();
  fileSystem = new MockFileSystem('fake-filesystem');

  var volumeManager = new MockVolumeManager();
  drive = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE);
  MockVolumeManager.installMockSingleton(volumeManager);

  chrome = {
    fileManagerPrivate: {
      /**
       * @param {string} url
       * @param {function(?string)} callback
       */
      computeChecksum: function(url, callback) {
        callback(hashes[url] || null);
      },
      /**
       * @param {string} volumeId
       * @param {!Array<string>} hashes
       * @param {function(!Object<string, Array<string>>)} callback
       */
      searchFilesByHashes: function(volumeId, hashes, callback) {
        var result = {};
        hashes.forEach(
            /** @param {string} hash */
            function(hash) {
              result[hash] = fileUrls[hash] || [];
            });
        callback(result);
      }
    },
    runtime: {
      lastError: null
    }
  };

  duplicateFinder = new importer.DriveDuplicateFinder();
}

// Verifies the correct result when a duplicate exists.
function testCheckDuplicateTrue(callback) {
  var filePaths = ['/foo.txt'];
  var fileHashes = ['abc123'];
  var files = setupHashes(filePaths, fileHashes);

  reportPromise(
      duplicateFinder.checkDuplicate(files[0])
          .then(
              function(isDuplicate) {
                assertTrue(isDuplicate);
              }),
      callback);
};

// Verifies the correct result when a duplicate doesn't exist.
function testCheckDuplicateFalse(callback) {
  var filePaths = ['/foo.txt'];
  var fileHashes = ['abc123'];
  var files = setupHashes(filePaths, fileHashes);

  // Make another file.
  var newFilePath = '/bar.txt';
  fileSystem.populate([newFilePath]);
  var newFile = fileSystem.entries[newFilePath];

  reportPromise(
      duplicateFinder.checkDuplicate(newFile)
          .then(
              function(isDuplicate) {
                assertFalse(isDuplicate);
              }),
      callback);
};

/**
 * @param {!Array.<string>} filePaths
 * @param {!Array.<string>} fileHashes
 * @return {!Array.<!FileEntry>} Created files.
 */
function setupHashes(filePaths, fileHashes) {
  // Set up a filesystem with some files.
  fileSystem.populate(filePaths);

  var files = filePaths.map(
      function(filename) {
        return fileSystem.entries[filename];
      });

  files.forEach(function(file, index) {
    hashes[file.toURL()] = fileHashes[index];
    fileUrls[fileHashes[index]] = file.toURL();
  });

  return files;
}
