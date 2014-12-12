// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {!MockFileOperationManager}
 */
var fileOperationManager;

/**
 * @type {!TestMediaScanner}
 */
var mediaScanner;

/**
 * @type {!importer.MediaImportHandler}
 */
var mediaImporter;

/**
 * @type {!VolumeInfo}
 */
var drive;

function setUp() {
  // Set up string assets.
  loadTimeData.data = {
    DRIVE_DIRECTORY_LABEL: 'My Drive',
    DOWNLOADS_DIRECTORY_LABEL: 'Downloads'
  };

  fileOperationManager = new MockFileOperationManager();

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
      fileOperationManager,
      mediaScanner);
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
  mediaScanner.fileEntries = media;

  // Verify the results when the import operation is kicked off.
  reportPromise(
      // Looks like we're using a "paste" operation to copy imported files.
      fileOperationManager.whenPasteCalled().then(
          function(args) {
            // Verify that the task ID is correct.
            assertEquals(importTask.taskId, args.opt_taskId);
            // Verify that we're copying, not moving, files.
            assertFalse(args.isMove);
            // Verify that the sources are correct.
            assertFileEntryListEquals(media, args.sourceEntries);
            // Verify that the destination is correct.
            assertEquals(destinationFileSystem.root, args.targetEntry);
          }),
      callback);
  // Kick off an import
  var importTask = mediaImporter.importMedia(fileSystem.root, destination);
}
