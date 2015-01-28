// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {!MockFileOperationManager} */
var progressCenter;

/** @type {!TestMediaScanner} */
var mediaScanner;

/** @type {!importer.MediaImportHandler} */
var mediaImporter;

/** @type {!importer.TestImportHistory} */
var importHistory;

/** @type {!VolumeInfo} */
var drive;

/** @type {!MockCopyTo} */
var mockCopier;

/** @type {!MockFileSystem} */
var destinationFileSystem;

/** @type {!importer.DuplicateFinder} */
var duplicateFinder;

// Set up string assets.
loadTimeData.data = {
  CLOUD_IMPORT_ITEMS_REMAINING: '',
  DRIVE_DIRECTORY_LABEL: 'My Drive',
  DOWNLOADS_DIRECTORY_LABEL: 'Downloads'
};

function setUp() {
  importer.setupTestLogger();

  progressCenter = new MockProgressCenter();

  // Replaces fileOperationUtil.copyTo with test function.
  mockCopier = new MockCopyTo();

  var volumeManager = new MockVolumeManager();
  drive = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE);
  // Create fake parented and non-parented roots.
  drive.fileSystem.populate([
    '/root/',
    '/other/'
  ]);

  MockVolumeManager.installMockSingleton(volumeManager);

  importHistory = new importer.TestImportHistory();
  mediaScanner = new TestMediaScanner();
  destinationFileSystem = new MockFileSystem(destinationFactory);
  duplicateFinder = new importer.TestDuplicateFinder();

  mediaImporter = new importer.MediaImportHandler(
      progressCenter, importHistory, duplicateFinder);
}

function testImportMedia(callback) {
  var media = setupFileSystem([
    '/DCIM/photos0/IMG00001.jpg',
    '/DCIM/photos0/IMG00002.jpg',
    '/DCIM/photos0/IMG00003.jpg',
    '/DCIM/photos1/IMG00004.jpg',
    '/DCIM/photos1/IMG00005.jpg',
    '/DCIM/photos1/IMG00006.jpg'
  ]);

  var scanResult = new TestScanResult(media);
  var importTask =
      mediaImporter.importFromScanResult(scanResult, destinationFactory);
  var whenImportDone = new Promise(
      function(resolve, reject) {
        importTask.addObserver(
            /**
             * @param {!importer.TaskQueue.UpdateType} updateType
             * @param {!importer.TaskQueue.Task} task
             */
            function(updateType, task) {
              switch (updateType) {
                case importer.TaskQueue.UpdateType.SUCCESS:
                  resolve();
                  break;
                case importer.TaskQueue.UpdateType.ERROR:
                  reject(new Error(importer.TaskQueue.UpdateType.ERROR));
                  break;
              }
            });
      });

  reportPromise(
      whenImportDone.then(
        function() {
          var copiedEntries = destinationFileSystem.root.getAllChildren();
          assertEquals(media.length, copiedEntries.length);
        }),
      callback);

  scanResult.finalize();
}

function testUpdatesHistoryAfterImport(callback) {
  var entries = setupFileSystem([
    '/DCIM/photos0/IMG00001.jpg',
    '/DCIM/photos1/IMG00003.jpg'
  ]);

  var scanResult = new TestScanResult(entries);
  var importTask =
      mediaImporter.importFromScanResult(scanResult, destinationFactory);
  var whenImportDone = new Promise(
      function(resolve, reject) {
        importTask.addObserver(
            /**
             * @param {!importer.TaskQueue.UpdateType} updateType
             * @param {!importer.TaskQueue.Task} task
             */
            function(updateType, task) {
              switch (updateType) {
                case importer.TaskQueue.UpdateType.SUCCESS:
                  resolve();
                  break;
                case importer.TaskQueue.UpdateType.ERROR:
                  reject(new Error(importer.TaskQueue.UpdateType.ERROR));
                  break;
              }
            });
      });

  reportPromise(
      whenImportDone.then(
        function() {
          mockCopier.copiedFiles.forEach(
              /** @param {!MockCopyTo.CopyInfo} copy */
              function(copy) {
                importHistory.assertCopied(
                    copy.source, importer.Destination.GOOGLE_DRIVE);
              });
        }),
      callback);

  scanResult.finalize();
}

// Tests that cancelling an import works properly.
function testImportCancellation(callback) {
  var media = setupFileSystem([
    '/DCIM/photos0/IMG00001.jpg',
    '/DCIM/photos0/IMG00002.jpg',
    '/DCIM/photos0/IMG00003.jpg',
    '/DCIM/photos1/IMG00004.jpg',
    '/DCIM/photos1/IMG00005.jpg',
    '/DCIM/photos1/IMG00006.jpg'
  ]);

  /** @const {number} */
  var EXPECTED_COPY_COUNT = 3;

  var scanResult = new TestScanResult(media);
  var importTask =
      mediaImporter.importFromScanResult(scanResult, destinationFactory);
  var whenImportCancelled = new Promise(
      function(resolve, reject) {
        importTask.addObserver(
            /**
             * @param {!importer.TaskQueue.UpdateType} updateType
             * @param {!importer.TaskQueue.Task} task
             */
            function(updateType, task) {
              if (updateType === importer.TaskQueue.UpdateType.CANCELED) {
                resolve();
              }
            });
      });

  // Simulate cancellation after the expected number of copies is done.
  var copyCount = 0;
  importTask.addObserver(function(updateType) {
    if (updateType ===
        importer.MediaImportHandler.ImportTask.UpdateType.ENTRY_CHANGED) {
      copyCount++;
      if (copyCount === EXPECTED_COPY_COUNT) {
        importTask.requestCancel();
      }
    }
  });

  reportPromise(
      whenImportCancelled.then(
        function() {
          var copiedEntries = destinationFileSystem.root.getAllChildren();
          assertEquals(EXPECTED_COPY_COUNT, copiedEntries.length);
        }),
      callback);

  scanResult.finalize();
}

function testImportWithDuplicates(callback) {
  var media = setupFileSystem([
    '/DCIM/photos0/IMG00001.jpg',
    '/DCIM/photos0/IMG00002.jpg',
    '/DCIM/photos0/IMG00003.jpg',
    '/DCIM/photos1/IMG00004.jpg',
    '/DCIM/photos1/IMG00005.jpg',
    '/DCIM/photos1/IMG00006.jpg'
  ]);

  /** @const {number} */
  var EXPECTED_COPY_COUNT = 3;

  var scanResult = new TestScanResult(media);
  var importTask =
      mediaImporter.importFromScanResult(scanResult, destinationFactory);
  var whenImportDone = new Promise(
      function(resolve, reject) {
        importTask.addObserver(
            /**
             * @param {!importer.TaskQueue.UpdateType} updateType
             * @param {!importer.TaskQueue.Task} task
             */
            function(updateType, task) {
              switch (updateType) {
                case importer.TaskQueue.UpdateType.SUCCESS:
                  resolve();
                  break;
                case importer.TaskQueue.UpdateType.ERROR:
                  reject(new Error(importer.TaskQueue.UpdateType.ERROR));
                  break;
              }
            });
      });

  // Simulate a known number of new imports followed by a bunch of duplicate
  // imports.
  var copyCount = 0;
  importTask.addObserver(function(updateType) {
    if (updateType ===
        importer.MediaImportHandler.ImportTask.UpdateType.ENTRY_CHANGED) {
      copyCount++;
      if (copyCount === EXPECTED_COPY_COUNT) {
        duplicateFinder.returnValue = true;
      }
    }
  });

  reportPromise(
      whenImportDone.then(
        function() {
          var copiedEntries = destinationFileSystem.root.getAllChildren();
          assertEquals(EXPECTED_COPY_COUNT, copiedEntries.length);
        }),
      callback);

  scanResult.finalize();
}

/**
 * @param {!Array.<string>} fileNames
 * @return {!Array.<!Entry>}
 */
function setupFileSystem(fileNames) {
  // Set up a filesystem with some files.
  var fileSystem = new MockFileSystem('fake-media-volume');
  fileSystem.populate(fileNames);

  return fileNames.map(
      function(filename) {
        return fileSystem.entries[filename];
      });
}

/** @return {!DirectoryEntry} The destination root, for testing. */
function destinationFactory() {
  return destinationFileSystem.root;
}

/**
 * Replaces fileOperationUtil.copyTo with some mock functionality for testing.
 * @constructor
 */
function MockCopyTo() {
  /** @type {!Array<!MockCopyTo.CopyInfo>} */
  this.copiedFiles = [];

  // Replace with test function.
  fileOperationUtil.copyTo = this.copyTo_.bind(this);

  this.entryChangedCallback_ = null;
  this.progressCallback_ = null;
  this.successCallback_ = null;
  this.errorCallback_ = null;
}

/**
 * @typedef {{
 *   source: source,
 *   destination: parent,
 *   newName: newName
 * }}
 */
MockCopyTo.CopyInfo;

/**
 * A mock to replace fileOperationUtil.copyTo.  See the original for details.
 * @param {Entry} source
 * @param {DirectoryEntry} parent
 * @param {string} newName
 * @param {function(string, Entry)} entryChangedCallback
 * @param {function(string, number)} progressCallback
 * @param {function(Entry)} successCallback
 * @param {function(DOMError)} errorCallback
 * @return {function()}
 */
MockCopyTo.prototype.copyTo_ = function(source, parent, newName,
    entryChangedCallback, progressCallback, successCallback, errorCallback) {
  this.entryChangedCallback_ = entryChangedCallback;
  this.progressCallback_ = progressCallback;
  this.successCallback_ = successCallback;
  this.errorCallback_ = errorCallback;

  // Log the copy, then copy the file.
  this.copiedFiles.push({
    source: source,
    destination: parent,
    newName: newName
  });
  source.copyTo(
      parent,
      newName,
      function(newEntry) {
        this.entryChangedCallback_(source.toURL(), parent);
        this.successCallback_(newEntry);
      }.bind(this),
      this.errorCallback_.bind(this));
};
