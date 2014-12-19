// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {!MockVolumeManager} */
var volumeManager;

/** @type {!TestMediaScanner} */
var mediaScanner;

/** @type {!TestImportRunner} */
var mediaImporter;

/** @type {!TestControllerEnvironment} */
var environment;

/** @type {!Object} */
var commandEvent;

/** @type {!VolumeInfo} */
var sourceVolume;

/** @type {!VolumeInfo} */
var destinationVolume;

/** @type {!TestCallRecorder} */
var commandUpdateRecorder;

function setUp() {

  // Set up string assets.
  loadTimeData.data = {
    CLOUD_IMPORT_BUTTON_LABEL: 'Import it!',
    CLOUD_IMPORT_INSUFFICIENT_SPACE_BUTTON_LABEL: 'Not enough space!',
    CLOUD_IMPORT_SCANNING_BUTTON_LABEL: 'Scanning... ...!',
  };

  // Stub out metrics support.
  metrics = {
    recordEnum: function() {}
  };

  // Set up string assets.
  loadTimeData.data = {
    DRIVE_DIRECTORY_LABEL: 'My Drive',
    DOWNLOADS_DIRECTORY_LABEL: 'Downloads'
  };

  commandUpdateRecorder = new TestCallRecorder();

  volumeManager = new MockVolumeManager();
  MockVolumeManager.installMockSingleton(volumeManager);

  destinationVolume = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS);

  mediaScanner = new TestMediaScanner();
  mediaImporter = new TestImportRunner();
}

function testUpdate_InitiatesScan() {
  var controller = createController(
      VolumeManagerCommon.VolumeType.MTP,
      'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
        '/DCIM/photos0/IMG00002.jpg',
        '/DCIM/photos1/',
        '/DCIM/photos1/IMG00001.jpg',
        '/DCIM/photos1/IMG00003.jpg'
      ],
      '/DCIM');

  var response = controller.update(commandEvent);
  assertEquals(importer.UpdateResponses.SCANNING, response);

  mediaScanner.assertScanCount(1);
}

function testUpdate_CanExecuteAfterScanIsFinalized() {
  var controller = createController(
      VolumeManagerCommon.VolumeType.MTP,
      'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
        '/DCIM/photos0/IMG00002.jpg',
        '/DCIM/photos1/',
        '/DCIM/photos1/IMG00001.jpg',
        '/DCIM/photos1/IMG00003.jpg'
      ],
      '/DCIM');

  controller.update(commandEvent);
  mediaScanner.finalizeScans();
  var response = controller.update(commandEvent);
  assertEquals(importer.UpdateResponses.EXECUTABLE, response);
}

function testExecute_StartsImport() {
  var controller = createController(
      VolumeManagerCommon.VolumeType.MTP,
      'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
        '/DCIM/photos0/IMG00002.jpg',
        '/DCIM/photos1/',
        '/DCIM/photos1/IMG00001.jpg',
        '/DCIM/photos1/IMG00003.jpg'
      ],
      '/DCIM');

  controller.update();
  mediaScanner.finalizeScans();
  controller.update();
  controller.execute();
  mediaImporter.assertImportsStarted(1);
}

/**
 * Test import runner.
 *
 * @constructor
 */
function TestImportRunner() {
  /** @type {!Array.<!importer.ScanResult>} */
  this.imported = [];
}

/** @override */
TestImportRunner.prototype.importFromScanResult =
    function(scanResult, opt_destination) {
  this.imported.push(scanResult);
};

/**
 * @param {number}
 */
TestImportRunner.prototype.assertImportsStarted = function(expected) {
  assertEquals(expected, this.imported.length);
};

/**
 * Interface abstracting away the concrete file manager available
 * to commands. By hiding file manager we make it easy to test
 * importer.ImportController.
 *
 * @constructor
 * @implements {importer.CommandInput}
 */
TestControllerEnvironment = function(volumeInfo, directory) {
  /** @private {!volumeInfo} */
  this.volumeInfo_ = volumeInfo;

  /** @private {!DirectoryEntry} */
  this.directory_ = directory;

  /** @private {!DirectoryEntry} */
  this.selection = [];

  /** @private {boolean} */
  this.isDriveMounted = true;
};

/** @override */
TestControllerEnvironment.prototype.getSelection =
    function() {
  return this.selection;
};

/** @override */
TestControllerEnvironment.prototype.getCurrentDirectory =
    function() {
  return this.directory_;
};

/** @override */
TestControllerEnvironment.prototype.getVolumeInfo =
    function(entry) {
  return this.volumeInfo_;
};

/** @override */
TestControllerEnvironment.prototype.isGoogleDriveMounted =
    function() {
  return this.isDriveMounted;
};

/**
 * @param {!VolumeManagerCommon.VolumeType} volumeType
 * @param {!Array.<string>} fileNames
 * @param {string} currentDirectory
 * @return {!importer.ImportControler}
 */
function createController(volumeType, volumeId, fileNames, currentDirectory) {
  sourceVolume = setupFileSystem(
      volumeType,
      volumeId,
      fileNames);

  environment = new TestControllerEnvironment(
      sourceVolume,
      sourceVolume.fileSystem.entries[currentDirectory]);

  return new importer.ImportController(
      environment,
      mediaScanner,
      mediaImporter,
      commandUpdateRecorder.callback);
};

/**
 * @param {!VolumeManagerCommon.VolumeType} volumeType
 * @param {!Array.<string>} fileNames
 * @return {!VolumeInfo}
 */
function setupFileSystem(volumeType, volumdId, fileNames) {
  var volumeInfo = volumeManager.createVolumeInfo(
      volumeType, volumdId, 'A volume known as ' + volumdId);
  assertTrue(volumeInfo != null);
  volumeInfo.fileSystem.populate(fileNames);
  return volumeInfo;
}
