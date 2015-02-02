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

/** @type {!importer.TestCommandWidget} */
var widget;

/**
 * @enum {string}
 */
var MESSAGES = {
  CLOUD_IMPORT_BUTTON_LABEL: 'Import it!',
  CLOUD_IMPORT_ACTIVE_IMPORT_BUTTON_LABEL: 'Already importing!',
  CLOUD_IMPORT_EMPTY_SCAN_BUTTON_LABEL: 'No new media',
  CLOUD_IMPORT_INSUFFICIENT_SPACE_BUTTON_LABEL: 'Not enough space!',
  CLOUD_IMPORT_SCANNING_BUTTON_LABEL: 'Scanning... ...!',
  DRIVE_DIRECTORY_LABEL: 'My Drive',
  DOWNLOADS_DIRECTORY_LABEL: 'Downloads'
};

// Set up string assets.
loadTimeData.data = MESSAGES;

function setUp() {
  // Stub out metrics support.
  metrics = {
    recordEnum: function() {}
  };

  widget = new importer.TestCommandWidget();

  volumeManager = new MockVolumeManager();
  MockVolumeManager.installMockSingleton(volumeManager);

  destinationVolume = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS);

  mediaScanner = new TestMediaScanner();
  mediaImporter = new TestImportRunner();
}

function testGetCommandUpdate_HiddenWhenDriveUnmounted(callback) {
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
  var promise = controller.getCommandUpdate().then(
      function(response) {
        assertFalse(response.visible);
        assertFalse(response.executable);

        mediaScanner.assertScanCount(0);
      });

  reportPromise(promise, callback);
}

function testGetCommandUpdate_HiddenForNonMediaVolume(callback) {
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

  var promise = controller.getCommandUpdate().then(
      function(response) {
        assertFalse(response.visible);
        assertFalse(response.executable);

        mediaScanner.assertScanCount(0);
      });

  reportPromise(promise, callback);
}

function testGetCommandUpdate_InitiatesScan(callback) {
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

  var promise = controller.getCommandUpdate().then(
      function(response) {
        assertTrue(response.visible);
        assertFalse(response.executable);
        assertEquals(
            response.label,
            MESSAGES.CLOUD_IMPORT_SCANNING_BUTTON_LABEL);
        mediaScanner.assertScanCount(1);
      });

  reportPromise(promise, callback);
}

function testGetCommandUpdate_CanExecuteAfterScanIsFinalized(callback) {
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

  var fileSystem = new MockFileSystem('testFs');
  mediaScanner.fileEntries.push(
      new MockFileEntry(fileSystem, '/DCIM/photos0/IMG00001.jpg', {size: 0}));

  environment.directoryChangedListener_();  // initiates a scan.
  var promise = widget.updatePromise.then(
      function() {
        widget.resetPromise();
        mediaScanner.finalizeScans();
        return widget.updatePromise;
      }).then(
          function(response) {
            assertTrue(response.visible);
            assertTrue(response.executable);
            assertEquals(
                response.label,
                MESSAGES.CLOUD_IMPORT_BUTTON_LABEL);
          });

  reportPromise(promise, callback);
}

function testGetCommandUpdate_DisabledForInsufficientLocalStorage(callback) {
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

  var fileSystem = new MockFileSystem('testFs');
  mediaScanner.fileEntries.push(
      new MockFileEntry(
          fileSystem,
          '/DCIM/photos0/IMG00001.jpg',
          {size: 1000000}));

  environment.freeStorageSpace = 100;
  environment.directoryChangedListener_();  // initiates a scan.
  var promise = widget.updatePromise.then(
      function() {
        widget.resetPromise();
        mediaScanner.finalizeScans();
        return widget.updatePromise;
      }).then(
          function(response) {
            assertTrue(response.visible);
            assertFalse(response.executable);
            assertEquals(
                response.label,
                MESSAGES.CLOUD_IMPORT_INSUFFICIENT_SPACE_BUTTON_LABEL);
          });

  reportPromise(promise, callback);
}

function testGetCommandUpdate_CannotExecuteEmptyScanResult(callback) {
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

  var promise = controller.getCommandUpdate().then(
      function() {
        mediaScanner.finalizeScans();

        return controller.getCommandUpdate().then(
            function(response) {
              assertTrue(response.visible);
              assertFalse(response.executable);
              assertEquals(
                  response.label,
                  MESSAGES.CLOUD_IMPORT_EMPTY_SCAN_BUTTON_LABEL);
            });
      });

  reportPromise(promise, callback);
}

function testGetCommandUpdate_DisabledWhileImporting(callback) {
  var controller = createController(
      VolumeManagerCommon.VolumeType.MTP,
      'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
        '/DCIM/photos0/IMG00002.jpg',
      ],
      '/DCIM');

// First we need to force the controller into a scanning state.
environment.directoryChangedListener_();

var promise = widget.updatePromise.then(
    function() {
      widget.resetPromise();
      widget.executeListener();
      mediaImporter.assertImportsStarted(1);
      // return the reset promise so as to allow execution
      // to complete before the test is finished...even though
      // we're not waiting on anything in particular.
      return controller.getCommandUpdate();
    }).then(
        function(response) {
          assertTrue(response.visible);
          assertFalse(response.executable);
          assertEquals(
              response.label,
              MESSAGES.CLOUD_IMPORT_ACTIVE_IMPORT_BUTTON_LABEL);
        });

  reportPromise(promise, callback);
}

function testClick_StartsImport(callback) {
  var controller = createController(
      VolumeManagerCommon.VolumeType.MTP,
      'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
        '/DCIM/photos0/IMG00002.jpg',
      ],
      '/DCIM');

  // First we need to force the controller into a scanning state.
  environment.directoryChangedListener_();

  reportPromise(
      widget.updatePromise.then(
          function() {
            widget.resetPromise();
            widget.executeListener();
            mediaImporter.assertImportsStarted(1);
            // return the reset promise so as to allow execution
            // to complete before the test is finished...even though
            // we're not waiting on anything in particular.
            return widget.updatePromise;
          }),
      callback);
}

function testVolumeUnmount_InvalidatesScans(callback) {
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
  var promise = widget.updatePromise.then(
          function() {
            // Reset the promise so we can wait on a second widget update.
            widget.resetPromise();

            // Faux unmount the volume, then request an update again.
            // A fresh new scan should be started.
            environment.simulateUnmount();

            // Return the new promise, so subsequent "thens" only
            // fire once the widget has been updated again.
            return widget.updatePromise;
          }).then(
            function() {
              mediaScanner.assertScanCount(2);
            });

  reportPromise(promise, callback);
}

function testDirectoryChange_TriggersUpdate(callback) {
  var controller = createController(
      VolumeManagerCommon.VolumeType.MTP,
      'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
      ],
      '/DCIM');

  environment.directoryChangedListener_();
  reportPromise(widget.updatePromise, callback);
}

function testSelectionChange_TriggersUpdate(callback) {
  var controller = createController(
      VolumeManagerCommon.VolumeType.MTP,
      'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
      ],
      '/DCIM');

  environment.selectionChangedListener_();
  reportPromise(widget.updatePromise, callback);
}

function testVolumeUnmount_TriggersUpdate(callback) {
  var controller = createController(
      VolumeManagerCommon.VolumeType.MTP,
      'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
      ],
      '/DCIM');

  // Faux unmount the volume, then request an update again.
  // A fresh new scan should be started.
  environment.simulateUnmount();
  reportPromise(widget.updatePromise, callback);
}

function testFinalizeScans_TriggersUpdate(callback) {
  var controller = createController(
      VolumeManagerCommon.VolumeType.MTP,
      'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
      ],
      '/DCIM');

  var fileSystem = new MockFileSystem('testFs');
  // ensure there is some content in the scan so the code that depends
  // on this state doesn't croak which it finds it missing.
  mediaScanner.fileEntries.push(
      new MockFileEntry(fileSystem, '/DCIM/photos0/IMG00001.jpg', {size: 0}));

  environment.directoryChangedListener_();  // initiates a scan.
  mediaScanner.finalizeScans();

  reportPromise(widget.updatePromise, callback);
}

function testExecuteImport_TriggersUpdate(callback) {
  var controller = createController(
      VolumeManagerCommon.VolumeType.MTP,
      'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
        '/DCIM/photos0/IMG00002.jpg',
      ],
      '/DCIM');

  // First we need to force the controller into a scanning state.
  environment.directoryChangedListener_();
  mediaScanner.finalizeScans();

  var promise = widget.updatePromise.then(
      function() {
        widget.resetPromise();
        widget.executeListener();
        // By returning the new update promise, "reportPromise" will
        // hang unless an update occurs.
        return widget.updatePromise;
      });

  reportPromise(promise, callback);
}

function testFinishImport_TriggersUpdate(callback) {
  var controller = createController(
      VolumeManagerCommon.VolumeType.MTP,
      'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
        '/DCIM/photos0/IMG00002.jpg',
      ],
      '/DCIM');

  // First we need to force the controller into a scanning state.
  environment.directoryChangedListener_();
  mediaScanner.finalizeScans();

  var promise = widget.updatePromise.then(
      function() {
        widget.resetPromise();
        widget.executeListener();
        // By returning the new update promise, "reportPromise" will
        // hang unless an update occurs.
        return widget.updatePromise;
      }).then(
          function() {
            widget.resetPromise();
            mediaImporter.finishImportTasks();
            return widget.updatePromise;
          });

  reportPromise(promise, callback);
}

function testCancelImport_TriggersUpdate(callback) {
  var controller = createController(
      VolumeManagerCommon.VolumeType.MTP,
      'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
        '/DCIM/photos0/IMG00002.jpg',
      ],
      '/DCIM');

  // First we need to force the controller into a scanning state.
  environment.directoryChangedListener_();
  mediaScanner.finalizeScans();

  var promise = widget.updatePromise.then(
      function() {
        widget.resetPromise();
        widget.executeListener();
        // By returning the new update promise, "reportPromise" will
        // hang unless an update occurs.
        return widget.updatePromise;
      }).then(
          function() {
            widget.resetPromise();
            mediaImporter.cancelImportTasks();
            return widget.updatePromise;
          });

  reportPromise(promise, callback);
}

/**
 * A stub that just provides interfaces from ImportTask that are required by
 * these tests.
 * @param {DirectoryEntry} destination
 * @constructor
 */
function TestImportTask(destination) {
  this.destination_ = destination;

  /** @private {!importer.Resolver} */
  this.finishedResolver_ = new importer.Resolver();

  /** @public {!Promise} */
  this.whenFinished = this.finishedResolver_.promise;
}

/** @return {!Promise<DirectoryEntry>} */
TestImportTask.prototype.finish = function() {
  this.finishedResolver_.resolve();
};

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

  /** @type {!Array.<!TestImportTask>} */
  this.tasks_ = [];

  /** @type {DirectoryEntry} */
  this.destination_ = null;
}

/** @override */
TestImportRunner.prototype.importFromScanResult =
    function(scanResult, destination) {
  this.imported.push(scanResult);
  var task = new TestImportTask(this.destination_);
  this.tasks_.push(task);
  return task;
};

/**
 * @param {!DirectoryEntry} destination
 */
TestImportRunner.prototype.setDestination = function(destination) {
  this.destination_ = destination;
};

/**
 * @param {!DirectoryEntry} destination
 */
TestImportRunner.prototype.finishImportTasks = function() {
  this.tasks_.forEach(
      function(task) {
        task.finish();
      });
};

/**
 * @param {!DirectoryEntry} destination
 */
TestImportRunner.prototype.cancelImportTasks = function() {
  // No diff to us.
  this.finishImportTasks();
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

  /** @private {function()} */
  this.selectionChangedListener_;

  /** @public {!Entry} */
  this.selection = [];

  /** @public {boolean} */
  this.isDriveMounted = true;

  /** @private {function(!DirectoryEntry)} */
  this.directoryWasSet_;

  /** @private {number} */
  this.freeStorageSpace = 123456789;  // bytes

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
TestControllerEnvironment.prototype.getFreeStorageSpace =
    function() {
  return Promise.resolve(this.freeStorageSpace);
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

/** @override */
TestControllerEnvironment.prototype.addSelectionChangedListener =
    function(listener) {
  this.selectionChangedListener_ = listener;
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
 * Test implementation of importer.CommandWidget.
 *
 * @constructor
 * @implements {importer.CommandWidget}
 * @struct
 */
importer.TestCommandWidget = function() {
  /** @public {function()} */
  this.executeListener;

  /** @private {!importer.Resolver.<!importer.CommandUpdate>} */
  this.updateResolver_ = new importer.Resolver();

  /** @public {!importer.Resolver.<!importer.CommandUpdate>} */
  this.updatePromise = this.updateResolver_.promise;
};

/** Resets the widget */
importer.TestCommandWidget.prototype.resetPromise = function() {
  this.updateResolver_ = new importer.Resolver();
  this.updatePromise = this.updateResolver_.promise;
};

/** @override */
importer.TestCommandWidget.prototype.addExecuteListener = function(listener) {
  this.executeListener = listener;
};

/** @override */
importer.TestCommandWidget.prototype.update = function(update) {
  assertFalse(this.updateResolver_.settled, 'Should not have been settled.');
  this.updateResolver_.resolve(update);
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
      widget);
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
