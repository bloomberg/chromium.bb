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

// Sadly, boilerplate setup necessary to include test support classes.
loadTimeData.data = {
  DRIVE_DIRECTORY_LABEL: 'My Drive',
  DOWNLOADS_DIRECTORY_LABEL: 'Downloads'
};

// Set up the test components.
function setUp() {
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
  volumeManager.volumeInfoList.add(cameraVolume);
  volumeManager.volumeInfoList.add(sdVolume);
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

function testResolver_Resolve(callback) {
  var resolver = new importer.Resolver();
  assertFalse(resolver.settled);
  resolver.resolve(1);
  resolver.promise.then(
      function(value) {
        assertTrue(resolver.settled);
        assertEquals(1, value);
      });

  reportPromise(resolver.promise, callback);
}

function testResolver_Reject(callback) {
  var resolver = new importer.Resolver();
  assertFalse(resolver.settled);
  resolver.reject('ouch');
  resolver.promise
      .then(callback.bind(null, true))
      .catch(
          function(error) {
            assertTrue(resolver.settled);
            assertEquals('ouch', error);
            callback(false);
          });
}

function testGetMachineId(callback) {
  var storage = new MockChromeStorageAPI();

  var promise = importer.getMachineId().then(
      function(firstMachineId) {
        assertTrue(100000 <= firstMachineId <= 9999999);
        importer.getMachineId().then(
            function(secondMachineId) {
              assertEquals(firstMachineId, secondMachineId);
            });
      });
  reportPromise(promise, callback);
}

function testHistoryFilename(callback) {
  var storage = new MockChromeStorageAPI();

  var promise = importer.getHistoryFilename().then(
      function(firstName) {
        assertTrue(!!firstName && firstName.length > 10);
        importer.getHistoryFilename().then(
            function(secondName) {
              assertEquals(firstName, secondName);
            });
      });

  reportPromise(promise, callback);
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
