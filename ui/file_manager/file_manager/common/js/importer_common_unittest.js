// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {!MockVolumeManager} */
var volumeManager;

/** @type {!VolumeInfo} */
var cameraVolume;

/** @type {!VolumeInfo} */
var sdVolume;

/** @type {!VolumeInfo} */
var driveVolume;

/** @type {!MockFileEntry} */
var cameraFileEntry;

/** @type {!MockFileEntry} */
var sdFileEntry;

/** @type {!MockFileEntry} */
var driveFileEntry;

// Set up the test components.
function setUp() {
  // Sadly, boilerplate setup necessary to include test support classes.
  loadTimeData.data = {
    DRIVE_DIRECTORY_LABEL: 'My Drive',
    DOWNLOADS_DIRECTORY_LABEL: 'Downloads'
  };
  var cameraFileSystem = new MockFileSystem(
      'camera-fs', 'filesystem:camera-123');
  var sdFileSystem = new MockFileSystem(
      'sd-fs', 'filesystem:sd-123');
  cameraVolume = MockVolumeManager.createMockVolumeInfo(
          VolumeManagerCommon.VolumeType.MTP,
          'camera-fs',
          'Some Camera');
  sdVolume = MockVolumeManager.createMockVolumeInfo(
          VolumeManagerCommon.VolumeType.REMOVABLE,
          'sd-fs',
          'Some SD Card');
  volumeManager = new MockVolumeManager();
  volumeManager.volumeInfoList.push(cameraVolume);
  volumeManager.volumeInfoList.push(sdVolume);
  driveVolume = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE);
  cameraFileEntry = createFileEntry(cameraVolume, '/DCIM/poodles.jpg');
  sdFileEntry = createFileEntry(sdVolume, '/dcim/a-z/IMG1234.jpg');
  driveFileEntry = createFileEntry(driveVolume, '/someotherfile.jpg');
}

function testIsMediaEntry() {
  assertTrue(importer.isMediaEntry(cameraFileEntry));
  assertFalse(importer.isMediaEntry(driveFileEntry));
}

function testIsEligibleVolume() {
  assertTrue(importer.isEligibleVolume(cameraVolume));
  assertTrue(importer.isEligibleVolume(sdVolume));
  assertFalse(importer.isEligibleVolume(driveVolume));
}

function testIsEligibleEntry() {
  assertTrue(importer.isEligibleEntry(volumeManager, cameraFileEntry));
  assertTrue(importer.isEligibleEntry(volumeManager, sdFileEntry));
  assertFalse(importer.isEligibleEntry(volumeManager, driveFileEntry));
}

function testIsMediaDirectory() {
  ['/DCIM', '/DCIM/', '/dcim', '/dcim/' ].forEach(
      assertIsMediaDir);
  ['/blabbity/DCIM', '/blabbity/dcim', '/blabbity-blab'].forEach(
      assertIsNotMediaDir);
}

/** @param {string} path */
function assertIsMediaDir(path) {
  var dir = createDirectoryEntry(sdVolume, path);
  assertTrue(importer.isMediaDirectory(dir, volumeManager));
}

/** @param {string} path */
function assertIsNotMediaDir(path) {
  var dir = createDirectoryEntry(sdVolume, path);
  assertFalse(importer.isMediaDirectory(dir, volumeManager));
}

function createFileEntry(volume, path) {
  var entry = new MockFileEntry(
      volume.fileSystem,
      path, {
        size: 1234,
        modificationTime: new Date().toString()
      });
  // Ensure the file entry has a volumeID...necessary for lookups
  // via the VolumeManager.
  entry.volumeId = volume.volumeId;
  return entry;
}

function createDirectoryEntry(volume, path) {
  var entry = new MockDirectoryEntry(volume.fileSystem, path);
  // Ensure the file entry has a volumeID...necessary for lookups
  // via the VolumeManager.
  entry.volumeId = volume.volumeId;
  return entry;
}
