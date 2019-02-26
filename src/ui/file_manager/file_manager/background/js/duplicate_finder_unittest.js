// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {!importer.DriveDuplicateFinder} */
var duplicateFinder;

/** @type {VolumeInfo} */
var drive;

/**
 * Map of file URL to hash code.
 * @type {!Object<string>}
 */
var hashes = {};

/**
 * Map of hash code to file URL.
 * @type {!Object<string>}
 */
var fileUrls = {};

/** @type {!MockFileSystem} */
var fileSystem;

/** @type {!importer.TestImportHistory} */
var testHistory;

/** @type {importer.DispositionChecker.CheckerFunction} */
var getDisposition;

window.metrics = {
  recordTime: function() {},
};

function setUp() {
  window.loadTimeData.getString = id => id;
  let mockChrome = {
    fileManagerPrivate: {
      /**
       * @param {!Entry} entry
       * @param {function(?string)} callback
       */
      computeChecksum: function(entry, callback) {
        callback(hashes[entry.toURL()] || null);
      },
      /**
       * @param {string} volumeId
       * @param {!Array<string>} hashes
       * @param {function(!Object<Array<string>>)} callback
       */
      searchFilesByHashes: function(volumeId, hashes, callback) {
        var result = {};
        hashes.forEach(
            /** @param {string} hash */
            function(hash) {
              result[hash] = fileUrls[hash] || [];
            });
        callback(result);
      },
    },
    runtime: {lastError: null},
  };

  installMockChrome(mockChrome);
  new MockCommandLinePrivate();
  // importer.setupTestLogger();
  fileSystem = new MockFileSystem('fake-filesystem');

  var volumeManager = new MockVolumeManager();
  drive = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE);
  assertTrue(drive != null);

  MockVolumeManager.installMockSingleton(volumeManager);

  testHistory = new importer.TestImportHistory();
  duplicateFinder = new importer.DriveDuplicateFinder();
  getDisposition = importer.DispositionChecker.createChecker(testHistory);
}

// Verifies the correct result when a duplicate exists.
function testCheckDuplicateTrue(callback) {
  var filePaths = ['/foo.txt'];
  var fileHashes = ['abc123'];
  var files = setupHashes(filePaths, fileHashes);

  reportPromise(
      duplicateFinder.isDuplicate(files[0])
          .then(
              function(isDuplicate) {
                assertTrue(isDuplicate);
              }),
      callback);
}

// Verifies the correct result when a duplicate doesn't exist.
function testCheckDuplicateFalse(callback) {
  var filePaths = ['/foo.txt'];
  var fileHashes = ['abc123'];
  var files = setupHashes(filePaths, fileHashes);

  // Make another file.
  var newFilePath = '/bar.txt';
  fileSystem.populate([newFilePath]);
  var newFile = /** @type {!FileEntry} */ (fileSystem.entries[newFilePath]);

  reportPromise(
      duplicateFinder.isDuplicate(newFile)
          .then(
              function(isDuplicate) {
                assertFalse(isDuplicate);
              }),
      callback);
}

function testDispositionChecker_ContentDupe(callback) {
  var filePaths = ['/foo.txt'];
  var fileHashes = ['abc123'];
  var files = setupHashes(filePaths, fileHashes);

  reportPromise(
      getDisposition(
          files[0], importer.Destination.GOOGLE_DRIVE,
          importer.ScanMode.CONTENT)
          .then(function(disposition) {
            assertEquals(importer.Disposition.CONTENT_DUPLICATE, disposition);
          }),
      callback);
}

function testDispositionChecker_HistoryDupe(callback) {
  var filePaths = ['/foo.txt'];
  var fileHashes = ['abc123'];
  var files = setupHashes(filePaths, fileHashes);

  testHistory.importedPaths['/foo.txt'] =
      [importer.Destination.GOOGLE_DRIVE];

  reportPromise(
      getDisposition(
          files[0], importer.Destination.GOOGLE_DRIVE,
          importer.ScanMode.CONTENT)
          .then(function(disposition) {
            assertEquals(importer.Disposition.HISTORY_DUPLICATE, disposition);
          }),
      callback);
}

function testDispositionChecker_Original(callback) {
  var filePaths = ['/foo.txt'];
  var fileHashes = ['abc123'];
  var files = setupHashes(filePaths, fileHashes);

  var newFilePath = '/bar.txt';
  fileSystem.populate([newFilePath]);
  var newFile = /** @type {!FileEntry} */ (fileSystem.entries[newFilePath]);

  reportPromise(
      getDisposition(
          newFile, importer.Destination.GOOGLE_DRIVE, importer.ScanMode.CONTENT)
          .then(function(disposition) {
            assertEquals(importer.Disposition.ORIGINAL, disposition);
          }),
      callback);
}

/**
 * @param {!Array<string>} filePaths
 * @param {!Array<string>} fileHashes
 * @return {!Array<!FileEntry>} Created files.
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
