// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {!MockFileOperationManager} */
var fileOperationManager;

/** @type {!TestMediaScanner} */
var mediaScanner;

/** @type {!importer.MediaImportHandler} */
var mediaImporter;

/** @type {!VolumeInfo} */
var drive;

/**
 * @type {!Array<!Object>}
 */
var importedMedia = [];

function setUp() {
  // Set up string assets.
  loadTimeData.data = {
    DRIVE_DIRECTORY_LABEL: 'My Drive',
    DOWNLOADS_DIRECTORY_LABEL: 'Downloads'
  };

  fileOperationManager = new MockFileOperationManager();

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

  mediaScanner = new TestMediaScanner();
  mediaImporter = new importer.MediaImportHandler(
      fileOperationManager);
}

function testImportMedia(callback) {
  // Set up a filesystem with some files.
  var fileSystem = new MockFileSystem('fake-media-volume');
  var filenames = [
    '/DCIM/photos0/IMG00001.jpg',
    '/DCIM/photos0/IMG00002.jpg',
    '/DCIM/photos0/IMG00003.jpg',
    '/DCIM/photos1/IMG00001.jpg',
    '/DCIM/photos1/IMG00002.jpg',
    '/DCIM/photos1/IMG00003.jpg'
  ];
  fileSystem.populate(filenames);
  // Set up a fake destination for the import.
  var destinationFileSystem = new MockFileSystem('fake-destination');
  var destination = function() { return destinationFileSystem.root; };

  // Set up some fake media scan results.
  var media = filenames.map(function(filename) {
    return fileSystem.entries[filename];
  });

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
        /** @param {!Array<!FileEntry>} importedMedia */
        function(importedMedia) {
          assertEquals(media.length, importedMedia.length);
          importedMedia.forEach(
            /** @param {!FileEntry} imported */
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
