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

/** @type {!MockFileSystem} */
var fileSystem;

/** @type {!MockCopyTo} */
var mockCopier;

// Set up string assets.
loadTimeData.data = {
  CLOUD_IMPORT_ITEMS_REMAINING: '',
  DRIVE_DIRECTORY_LABEL: 'My Drive',
  DOWNLOADS_DIRECTORY_LABEL: 'Downloads'
};

function setUp() {
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
  mediaImporter = new importer.MediaImportHandler(
      progressCenter,
      importHistory);
}

function testImportMedia(callback) {
  var media = setupFileSystem([
    '/DCIM/photos0/IMG00001.jpg',
    '/DCIM/photos0/IMG00002.jpg',
    '/DCIM/photos0/IMG00003.jpg',
    '/DCIM/photos1/IMG00001.jpg',
    '/DCIM/photos1/IMG00002.jpg',
    '/DCIM/photos1/IMG00003.jpg'
  ]);

  var destinationFileSystem = new MockFileSystem('fake-destination');
  var destination = function() { return destinationFileSystem.root; };

  var scanResult = new TestScanResult(media);
  var importTask = mediaImporter.importFromScanResult(scanResult, destination);
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
          assertEquals(media.length, mockCopier.copiedFiles.length);
          mockCopier.copiedFiles.forEach(
            /** @param {!MockCopyTo.CopyInfo} copy */
            function(copy) {
              // Verify the copied file is one of the expected files.
              assertTrue(media.indexOf(copy.source) >= 0);
              // Verify that the files are being copied to the right locations.
              assertEquals(destination(), copy.destination);
            });
        }),
      callback);

  scanResult.finalize();
}

function testUpdatesHistoryAfterImport(callback) {
  var entries = setupFileSystem([
    '/DCIM/photos0/IMG00001.jpg',
    '/DCIM/photos1/IMG00003.jpg'
  ]);

  var destinationFileSystem = new MockFileSystem('fake-destination');
  var destination = function() { return destinationFileSystem.root; };

  var scanResult = new TestScanResult(entries);
  var importTask = mediaImporter.importFromScanResult(scanResult, destination);
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
    '/DCIM/photos1/IMG00001.jpg',
    '/DCIM/photos1/IMG00002.jpg',
    '/DCIM/photos1/IMG00003.jpg'
  ]);

  /** @const {number} */
  var EXPECTED_COPY_COUNT = 3;

  var destinationFileSystem = new MockFileSystem('fake-destination');
  var destination = function() { return destinationFileSystem.root; };

  var scanResult = new TestScanResult(media);
  var importTask = mediaImporter.importFromScanResult(scanResult, destination);
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

  reportPromise(
      whenImportCancelled.then(
        function() {
          assertEquals(EXPECTED_COPY_COUNT, mockCopier.copiedFiles.length);
          mockCopier.copiedFiles.forEach(
            /** @param {!MockCopyTo.CopyInfo} copy */
            function(copy) {
              // Verify the copied file is one of the expected files.
              assertTrue(media.indexOf(copy.source) >= 0);
              // Verify that the files are being copied to the right locations.
              assertEquals(destination(), copy.destination);
            });
        }),
      callback);

  // Simulate cancellation after the expected number of copies is done.
  var copyCount = 0;
  mockCopier.onCopy(
      /** @param {!MockCopyTo.CopyInfo} copy */
      function(copy) {
        mockCopier.doCopy(copy);
        if (++copyCount === EXPECTED_COPY_COUNT) {
          importTask.requestCancel();
        }
      });

  scanResult.finalize();
}

/**
 * @param {!Array.<string>} fileNames
 * @return {!Array.<!Entry>}
 */
function setupFileSystem(fileNames) {
  // Set up a filesystem with some files.
  fileSystem = new MockFileSystem('fake-media-volume');
  fileSystem.populate(fileNames);

  return fileNames.map(
      function(filename) {
        return fileSystem.entries[filename];
      });
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

  // Default copy callback just does the copy.
  this.copyCallback_ = this.doCopy.bind(this);
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

  this.copyCallback_({
    source: source,
    destination: parent,
    newName: newName
  });
};

/**
 * Set a callback to be called whenever #copyTo_ is called.  This can be used to
 * simulate errors, etc, during copying.  The default copy callback just calls
 * #doCopy.
 * @param {!function(!MockCopyTo.CopyInfo)} copyCallback
 */
MockCopyTo.prototype.onCopy = function(copyCallback) {
  this.copyCallback_ = copyCallback;
};

/**
 * Completes the given copy.  Call this in the callback passed to #onCopy, to
 * simulate the current copy operation completing successfully.
 * @param {!MockCopyTo.CopyInfo} copy
 */
MockCopyTo.prototype.doCopy = function(copy) {
  this.copiedFiles.push(copy);
  this.entryChangedCallback_(copy.source.toURL(),
      copy.destination);
  this.successCallback_();
};
