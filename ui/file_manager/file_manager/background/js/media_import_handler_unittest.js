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

/**
 * @typedef {{
 *   source: source,
 *   destination: parent,
 *   newName: newName
 * }}
 */
var CopyCapture;

/**
 * @type {!Array<!CopyCapture>}
 */
var importedMedia = [];

function setUp() {
  // Set up string assets.
  loadTimeData.data = {
    DRIVE_DIRECTORY_LABEL: 'My Drive',
    DOWNLOADS_DIRECTORY_LABEL: 'Downloads'
  };

  progressCenter = new MockProgressCenter();

  // Replace with test function.
  fileOperationUtil.copyTo = function(source, parent, newName,
      entryChangedCallback, progressCallback, successCallback, errorCallback) {
    importedMedia.push({
      source: source,
      destination: parent,
      newName: newName
    });
    successCallback();
  };
  importedMedia = [];

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
                  resolve(importedMedia);
                  break;
                case importer.TaskQueue.UpdateType.ERROR:
                  reject(new Error(importer.TaskQueue.UpdateType.ERROR));
                  break;
              }
            });
      });

  reportPromise(
      whenImportDone.then(
        /** @param {!Array<!CopyCapture>} importedMedia */
        function(importedMedia) {
          assertEquals(media.length, importedMedia.length);
          importedMedia.forEach(
            /** @param {!CopyCapture} imported */
            function(imported) {
              // Verify the copied file is one of the expected files.
              assertTrue(media.indexOf(imported.source) >= 0);
              // Verify that the files are being copied to the right locations.
              assertEquals(destination(), imported.destination);
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
                  resolve(importedMedia);
                  break;
                case importer.TaskQueue.UpdateType.ERROR:
                  reject(new Error(importer.TaskQueue.UpdateType.ERROR));
                  break;
              }
            });
      });

  reportPromise(
      whenImportDone.then(
        /** @param {!Array<!CopyCapture>} importedMedia */
        function(importedMedia) {
          importedMedia.forEach(
              /** @param {!CopyCapture} */
              function(capture) {
                importHistory.assertCopied(
                    capture.source, importer.Destination.GOOGLE_DRIVE);
              });
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
  fileSystem = new MockFileSystem('fake-media-volume');
  fileSystem.populate(fileNames);

  return fileNames.map(
      function(filename) {
        return fileSystem.entries[filename];
      });
}
