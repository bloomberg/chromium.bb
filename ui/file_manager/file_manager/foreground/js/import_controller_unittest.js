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

function testExecute_opensDestination(callback) {
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

  /**
   * Sets up an import destination.
   * @return {!Promise<!DirectoryEntry>}
   */
  var makeDestination = function() {
    return new Promise(function(resolve, reject) {
      window.webkitRequestFileSystem(
          TEMPORARY,
          1024*1024,
          function(fs) {
            fs.root.getDirectory(
                'foo',
                {create:true},
                function(directoryEntry) {
                  resolve(directoryEntry);
                },
                reject);
          },
          reject);
    });
  };

  reportPromise(
      makeDestination().then(
          // Kicks off an import to the given destination, then verifies that
          // the import controller moved the user to the correct location.
          function(destination) {
            mediaImporter.setDestination(destination);

            controller.update();
            mediaScanner.finalizeScans();
            controller.update();
            controller.execute();

            return environment.whenCurrentDirectoryIsSet().then(
                function(directory) {
                  assertEquals(destination, directory);
                });
          }),
      callback);

}

/**
 * A stub that just provides interfaces from ImportTask that are required by
 * these tests.
 * @param {DirectoryEntry} destination
 * @constructor
 */
function TestImportTask(destination) {
  this.destination_ = destination;
}

/** @return {!Promise<DirectoryEntry>} */
TestImportTask.prototype.getDestination = function() {
  return Promise.resolve(this.destination_);
};

/**
 * Test import runner.
 *
 * @constructor
 */
function TestImportRunner() {
  /** @type {!Array.<!importer.ScanResult>} */
  this.imported = [];

  /** @type {DirectoryEntry} */
  this.destination_ = null;
}

/** @override */
TestImportRunner.prototype.importFromScanResult = function(scanResult) {
  this.imported.push(scanResult);
  return new TestImportTask(this.destination_);
};

/**
 * @param {!DirectoryEntry} destination
 */
TestImportRunner.prototype.setDestination = function(destination) {
  this.destination_ = destination;
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

  /** @private {function(!DirectoryEntry)} */
  this.directoryWasSet_;

  /** @private {!Promise<!DirectoryEntry>} */
  this.directoryWasSetPromise_ = new Promise(function(resolve, reject) {
    this.directoryWasSet_ = resolve;
  }.bind(this));
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
TestControllerEnvironment.prototype.setCurrentDirectory = function(entry) {
  this.directoryWasSet_(entry);
  this.directory_ = entry;
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
 * A promise that resolves when the current directory is set in the environment.
 * @return {!Promise<!DirectoryEntry>}
 */
TestControllerEnvironment.prototype.whenCurrentDirectoryIsSet = function() {
  return this.directoryWasSetPromise_;
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
