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
    CLOUD_IMPORT_SCANNING_BUTTON_LABEL: 'Scanning... ...!'
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

function testGetCommandUpdate_HiddenWhenDriveUnmounted() {
  var controller = createController(
      VolumeManagerCommon.VolumeType.MTP,
      'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos1/IMG00001.jpg'
      ],
      '/DCIM');

  environment.isDriveMounted = false;

  var response = controller.getCommandUpdate();
  assertFalse(response.visible);
  assertFalse(response.executable);

  mediaScanner.assertScanCount(0);
}

function testGetCommandUpdate_HiddenForNonMediaVolume() {
  var controller = createController(
      VolumeManagerCommon.VolumeType.MTP,
      'drive',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg'
      ],
      '/DCIM');

  environment.isDriveMounted = false;

  var response = controller.getCommandUpdate();
  assertFalse(response.visible);
  assertFalse(response.executable);

  mediaScanner.assertScanCount(0);
}

function testGetCommandUpdate_InitiatesScan() {
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

  var response = controller.getCommandUpdate();
  assertTrue(response.visible);
  assertFalse(response.executable);

  mediaScanner.assertScanCount(1);
}

function testDirectoryChange_InitiatesUpdate() {
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

  environment.directoryChangedListener_();
  commandUpdateRecorder.assertCallCount(1);
}

function testUnmountInvalidatesScans() {
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

  controller.getCommandUpdate();
  mediaScanner.assertScanCount(1);

  // Faux unmount the volume, then request an update again.
  // A fresh new scan should be started.
  environment.simulateUnmount();
  controller.getCommandUpdate();
  mediaScanner.assertScanCount(2);
}

function testGetCommandUpdate_CanExecuteAfterScanIsFinalized() {
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

  mediaScanner.fileEntries.push(
      new MockFileEntry(null, '/DCIM/photos0/IMG00001.jpg', {size: 0}));
  controller.getCommandUpdate();
  mediaScanner.finalizeScans();

  var response = controller.getCommandUpdate();
  assertTrue(response.visible);
  assertTrue(response.executable);
}

function testGetCommandUpdate_CannotExecuteEmptyScanResult() {
  var controller = createController(
      VolumeManagerCommon.VolumeType.MTP,
      'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.trader',
        '/DCIM/photos0/IMG00002.joes',
        '/DCIM/photos1/',
        '/DCIM/photos1/IMG00001.parking',
        '/DCIM/photos1/IMG00003.lots'
      ],
      '/DCIM');

  controller.getCommandUpdate();
  mediaScanner.finalizeScans();

  var response = controller.getCommandUpdate();
  assertTrue(response.visible);
  assertFalse(response.executable);
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

  controller.getCommandUpdate();
  mediaScanner.finalizeScans();
  controller.getCommandUpdate();
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

            controller.getCommandUpdate();
            mediaScanner.finalizeScans();
            controller.getCommandUpdate();
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
 * @param {number} expected
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
 *
 * @param {!VolumeInfo} volumeInfo
 * @param {!DirectoryEntry} directory
 */
TestControllerEnvironment = function(volumeInfo, directory) {
  /** @private {!VolumeInfo} */
  this.volumeInfo_ = volumeInfo;

  /** @private {!DirectoryEntry} */
  this.directory_ = directory;

  /** @private {function(string)} */
  this.volumeUnmountListener_;

  /** @private {function()} */
  this.directoryChangedListener_;

  /** @public {!DirectoryEntry} */
  this.selection = [];

  /** @public {boolean} */
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

/** @override */
TestControllerEnvironment.prototype.addVolumeUnmountListener =
    function(listener) {
  this.volumeUnmountListener_ = listener;
};

/** @override */
TestControllerEnvironment.prototype.addDirectoryChangedListener =
    function(listener) {
  this.directoryChangedListener_ = listener;
};

/**
 * Simulates an unmount event.
 */
TestControllerEnvironment.prototype.simulateUnmount = function() {
  this.volumeUnmountListener_(this.volumeInfo_.volumeId);
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
 * @param {string} volumeId
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
}

/**
 * @param {!VolumeManagerCommon.VolumeType} volumeType
 * @param {string} volumeId
 * @param {!Array.<string>} fileNames
 * @return {!VolumeInfo}
 */
function setupFileSystem(volumeType, volumeId, fileNames) {
  var volumeInfo = volumeManager.createVolumeInfo(
      volumeType, volumeId, 'A volume known as ' + volumeId);
  assertTrue(volumeInfo != null);
  volumeInfo.fileSystem.populate(fileNames);
  return volumeInfo;
}
