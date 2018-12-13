// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @const {!Event} */
var EMPTY_EVENT = new Event('directory-changed');

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

/** @type {!DirectoryEntry} */
var nonDcimDirectory;

/**
 * Mock metrics.
 * @type {!Object}
 */
window.metrics = {
  recordSmallCount: function() {},
  recordUserAction: function() {},
  recordValue: function() {},
  recordBoolean: function() {},
};

// Set up the test components.
function setUp() {
  window.loadTimeData.getString = id => id;
  window.loadTimeData.data = {};

  new MockChromeStorageAPI();
  new MockCommandLinePrivate();

  widget = new importer.TestCommandWidget();

  const testFileSystem = new MockFileSystem('testFs');
  nonDcimDirectory = new MockDirectoryEntry(testFileSystem, '/jellybeans/');

  volumeManager = new MockVolumeManager();
  MockVolumeManager.installMockSingleton(volumeManager);

  const downloads = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS);
  assert(downloads);
  destinationVolume = downloads;

  mediaScanner = new TestMediaScanner();
  mediaImporter = new TestImportRunner();
}

function testClickMainToStartImport(callback) {
  reportPromise(startImport(importer.ClickSource.MAIN), callback);
}

function testClickPanelToStartImport(callback) {
  reportPromise(startImport(importer.ClickSource.IMPORT), callback);
}

function testClickCancel(callback) {
  var promise = startImport(importer.ClickSource.IMPORT).then(function(task) {
    widget.click(importer.ClickSource.CANCEL);
    return task.whenCanceled;
  });

  reportPromise(promise, callback);
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

  var dcim = environment.getCurrentDirectory();
  assert(dcim);

  environment.directoryChangedListener(EMPTY_EVENT);
  var promise = widget.updateResolver.promise.then(
      function() {
        // Reset the promise so we can wait on a second widget update.
        widget.resetPromises();
        environment.setCurrentDirectory(nonDcimDirectory);
        environment.simulateUnmount();

        dcim = /** @type {!DirectoryEntry} */ (dcim);
        environment.setCurrentDirectory(dcim);
        environment.directoryChangedListener(EMPTY_EVENT);
        // Return the new promise, so subsequent "thens" only
        // fire once the widget has been updated again.
        return widget.updateResolver.promise;
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

  environment.directoryChangedListener(EMPTY_EVENT);
  reportPromise(widget.updateResolver.promise, callback);
}

function testDirectoryChange_CancelsScan(callback) {
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

  environment.directoryChangedListener(EMPTY_EVENT);
  var promise = widget.updateResolver.promise.then(
      function() {
        // Reset the promise so we can wait on a second widget update.
        widget.resetPromises();
        environment.setCurrentDirectory(nonDcimDirectory);
        environment.directoryChangedListener(EMPTY_EVENT);
      }).then(
          function() {
            mediaScanner.assertScanCount(1);
            mediaScanner.assertLastScanCanceled();
          });

  reportPromise(promise, callback);
}

function testWindowClose_CancelsScan(callback) {
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

  environment.directoryChangedListener(EMPTY_EVENT);
  var promise = widget.updateResolver.promise.then(
      function() {
        // Reset the promise so we can wait on a second widget update.
        widget.resetPromises();
        environment.windowCloseListener();
      }).then(
          function() {
            mediaScanner.assertScanCount(1);
            mediaScanner.assertLastScanCanceled();
          });

  reportPromise(promise, callback);
}

function testDirectoryChange_DetailsPanelVisibility_InitialChangeDir(callback) {
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
  var event = new Event('directory-changed');
  event.newDirEntry = new MockDirectoryEntry(fileSystem, '/DCIM/');

  // Ensure there is some content in the scan so the code that depends
  // on this state doesn't croak which it finds it missing.
  mediaScanner.fileEntries.push(new MockFileEntry(
      fileSystem, '/DCIM/photos0/IMG00001.jpg', getDefaultMetadata()));

  // Make controller enter a scanning state.
  environment.directoryChangedListener(event);
  assertFalse(widget.detailsVisible);

  var promise = widget.updateResolver.promise.then(function() {
    // "scanning..."
    assertFalse(widget.detailsVisible);
    widget.resetPromises();
    mediaScanner.finalizeScans();
    return widget.updateResolver.promise;
  }).then(function() {
    // "ready to update"
    // Details should pop up.
    assertTrue(widget.detailsVisible);
  });

  reportPromise(promise, callback);
}

function testDirectoryChange_DetailsPanelVisibility_SubsequentChangeDir() {
  var controller = createController(
      VolumeManagerCommon.VolumeType.MTP,
      'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
      ],
      '/DCIM');

  var event = new Event('directory-changed');
  event.newDirEntry = new MockDirectoryEntry(
      new MockFileSystem('testFs'),
      '/DCIM/');

  // Any previous dir at all will skip the new window logic.
  event.previousDirEntry = event.newDirEntry;

  environment.directoryChangedListener(event);
  assertFalse(widget.detailsVisible);
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

  var fileSystem = new MockFileSystem('testFs');

  // Ensure there is some content in the scan so the code that depends
  // on this state doesn't croak which it finds it missing.
  environment.selection.push(new MockFileEntry(
      fileSystem, '/DCIM/photos0/IMG00001.jpg', getDefaultMetadata()));

  environment.selectionChangedListener();
  mediaScanner.finalizeScans();
  reportPromise(widget.updateResolver.promise, callback);
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

  // Ensure there is some content in the scan so the code that depends
  // on this state doesn't croak which it finds it missing.
  mediaScanner.fileEntries.push(new MockFileEntry(
      fileSystem, '/DCIM/photos0/IMG00001.jpg', getDefaultMetadata()));

  environment.directoryChangedListener(EMPTY_EVENT);  // initiates a scan.
  widget.resetPromises();
  mediaScanner.finalizeScans();

  reportPromise(widget.updateResolver.promise, callback);
}

function testClickDestination_ShowsRootPriorToImport(callback) {
  var controller = createController(
      VolumeManagerCommon.VolumeType.MTP,
      'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
      ],
      '/DCIM');

  widget.click(importer.ClickSource.DESTINATION);

  reportPromise(environment.showImportRootResolver.promise, callback);
}

function testClickDestination_ShowsDestinationAfterImportStarted(callback) {
  var promise = startImport(importer.ClickSource.MAIN)
      .then(
          function() {
            return mediaImporter.importResolver.promise.then(
                function() {
                  widget.click(importer.ClickSource.DESTINATION);
                  return environment.showImportDestinationResolver.promise;
                });
          });

  reportPromise(promise, callback);
}

function startImport(clickSource) {
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

  var fileSystem = new MockFileSystem('testFs');

  // Ensure there is some content in the scan so the code that depends
  // on this state doesn't croak which it finds it missing.
  mediaScanner.fileEntries.push(new MockFileEntry(
      fileSystem, '/DCIM/photos0/IMG00001.jpg', getDefaultMetadata()));

  // First we need to force the controller into a scanning state.
  environment.directoryChangedListener(EMPTY_EVENT);

  return widget.updateResolver.promise.then(
      function() {
        widget.resetPromises();
        mediaScanner.finalizeScans();
        return widget.updateResolver.promise.then(
            function() {
              widget.resetPromises();
              widget.click(clickSource);
              return mediaImporter.importResolver.promise;
            });
      });
}

/**
 * A stub that just provides interfaces from ImportTask that are required by
 * these tests.
 *
 * @constructor
 *
 * @param {!importer.ScanResult} scan
 * @param {!importer.Destination} destination
 * @param {!Promise<!DirectoryEntry>} destinationDirectory
 */
function TestImportTask(scan, destination, destinationDirectory) {
  /** @public {!importer.ScanResult} */
  this.scan = scan;

  /** @type {!importer.Destination} */
  this.destination = destination;

  /** @type {!Promise<!DirectoryEntry>} */
  this.destinationDirectory = destinationDirectory;

  /** @private {!importer.Resolver} */
  this.finishedResolver_ = new importer.Resolver();

  /** @private {!importer.Resolver} */
  this.canceledResolver_ = new importer.Resolver();

  /** @public {!Promise} */
  this.whenFinished = this.finishedResolver_.promise;

  /** @public {!Promise} */
  this.whenCanceled = this.canceledResolver_.promise;
}

TestImportTask.prototype.finish = function() {
  this.finishedResolver_.resolve();
};

TestImportTask.prototype.requestCancel = function() {
  this.canceledResolver_.resolve();
};

/**
 * Test import runner.
 *
 * @constructor
 * @implements {importer.ImportRunner}
 */
function TestImportRunner() {
  /** @public {!Array<!importer.ScanResult>} */
  this.imported = [];

  /**
   * Resolves when import is started.
   * @public {!importer.Resolver.<!TestImportTask>}
   */
  this.importResolver = new importer.Resolver();

  /** @private {!Array<!TestImportTask>} */
  this.tasks_ = [];
}

/** @override */
TestImportRunner.prototype.importFromScanResult = function(
    scan, destination, destinationDirectory) {
  this.imported.push(scan);
  var task = new TestImportTask(scan, destination, destinationDirectory);
  this.tasks_.push(task);
  this.importResolver.resolve(task);
  return this.toMediaImportTask_(task);
};

/**
 * Returns |task| as importer.MediaImportHandler.ImportTask type.
 * @param {!Object} task
 * @return {!importer.MediaImportHandler.ImportTask}
 * @private
 */
TestImportRunner.prototype.toMediaImportTask_ = function(task) {
  return /** @type {!importer.MediaImportHandler.ImportTask} */ (task);
};

TestImportRunner.prototype.finishImportTasks = function() {
  this.tasks_.forEach((task) => task.finish());
};

TestImportRunner.prototype.cancelImportTasks = function() {
  this.finishImportTasks();
};

/**
 * Interface abstracting away the concrete file manager available
 * to commands. By hiding file manager we make it easy to test
 * importer.ImportController.
 *
 * @constructor
 * @implements {importer.ControllerEnvironment}
 *
 * @param {!VolumeManager} volumeManager
 * @param {!VolumeInfo} volumeInfo
 * @param {!DirectoryEntry} directory
 */
function TestControllerEnvironment(volumeManager, volumeInfo, directory) {
  /** @private {!VolumeManager} */
  this.volumeManager = volumeManager;

  /** @private {!VolumeInfo} */
  this.volumeInfo_ = volumeInfo;

  /** @private {!DirectoryEntry} */
  this.directory_ = directory;

  /** @public {function()} */
  this.windowCloseListener;

  /** @public {function(string)} */
  this.volumeUnmountListener;

  /** @public {function(!Event)} */
  this.directoryChangedListener;

  /** @public {function()} */
  this.selectionChangedListener;

  /** @public {!Array<!Entry>} */
  this.selection = [];

  /** @public {boolean} */
  this.isDriveMounted = true;

  /** @public {number} */
  this.freeStorageSpace = 123456789;  // bytes

  /** @public {!importer.Resolver} */
  this.showImportRootResolver = new importer.Resolver();

  /** @public {!importer.Resolver} */
  this.showImportDestinationResolver = new importer.Resolver();
}

/** @override */
TestControllerEnvironment.prototype.getSelection = function() {
  return this.selection;
};

/** @override */
TestControllerEnvironment.prototype.getCurrentDirectory = function() {
  return this.directory_;
};

/** @override */
TestControllerEnvironment.prototype.setCurrentDirectory = function(entry) {
  this.directory_ = entry;
};

/** @override */
TestControllerEnvironment.prototype.getVolumeInfo = function(entry) {
  return this.volumeInfo_;
};

/** @override */
TestControllerEnvironment.prototype.isGoogleDriveMounted = function() {
  return this.isDriveMounted;
};

/** @override */
TestControllerEnvironment.prototype.getFreeStorageSpace = function() {
  return Promise.resolve(this.freeStorageSpace);
};

/** @override */
TestControllerEnvironment.prototype.addWindowCloseListener =
    function(listener) {
  this.windowCloseListener = listener;
};

/** @override */
TestControllerEnvironment.prototype.addVolumeUnmountListener =
    function(listener) {
  this.volumeUnmountListener = listener;
};

/** @override */
TestControllerEnvironment.prototype.addDirectoryChangedListener =
    function(listener) {
  this.directoryChangedListener = listener;
};

/** @override */
TestControllerEnvironment.prototype.addSelectionChangedListener =
    function(listener) {
  this.selectionChangedListener = listener;
};

/** @override */
TestControllerEnvironment.prototype.getImportDestination = function(date) {
  const fileSystem = new MockFileSystem('testFs');
  const directoryEntry = new MockDirectoryEntry(fileSystem, '/abc/123');
  return Promise.resolve(directoryEntry);
};

/** @override */
TestControllerEnvironment.prototype.showImportDestination = function() {
  this.showImportDestinationResolver.resolve();
  return Promise.resolve(true);
};

/** @override */
TestControllerEnvironment.prototype.showImportRoot = function() {
  this.showImportRootResolver.resolve();
  return Promise.resolve(true);
};

/**
 * Simulates an unmount event.
 */
TestControllerEnvironment.prototype.simulateUnmount = function() {
  this.volumeUnmountListener(this.volumeInfo_.volumeId);
};

/**
 * Test implementation of importer.CommandWidget.
 *
 * @constructor
 * @implements {importer.CommandWidget}
 * @struct
 */
importer.TestCommandWidget = function() {
  /** @public {function(importer.ClickSource<string>)} */
  this.clickListener;

  /** @public {!importer.Resolver} */
  this.updateResolver = new importer.Resolver();

  /** @public {!importer.Resolver} */
  this.toggleDetailsResolver = new importer.Resolver();

  /** @public {boolean} */
  this.detailsVisible = false;
};

/** Resets the widget */
importer.TestCommandWidget.prototype.resetPromises = function() {
  this.updateResolver = new importer.Resolver();
  this.toggleDetailsResolver = new importer.Resolver();
};

/** @override */
importer.TestCommandWidget.prototype.addClickListener = function(listener) {
  this.clickListener = listener;
};

/**
 * Fires faux click.
 * @param  {!importer.ClickSource} source
 */
importer.TestCommandWidget.prototype.click = function(source) {
  this.clickListener(source);
};

/** @override */
importer.TestCommandWidget.prototype.update = function(
    activityState, opt_scan, opt_destinationSizeBytes) {
  assertFalse(
      this.updateResolver.settled,
      'Update promise should not have been settled.');
  this.updateResolver.resolve(activityState);
};

importer.TestCommandWidget.prototype.updateDetails = function(scan) {};

importer.TestCommandWidget.prototype.performMainButtonRippleAnimation =
    function() {};

/** @override */
importer.TestCommandWidget.prototype.toggleDetails = function() {
  assertFalse(
      this.toggleDetailsResolver.settled,
      'Toggle details promise should not have been settled.');
  this.setDetailsVisible(!this.detailsVisible);
  this.toggleDetailsResolver.resolve();
};

/** @override */
importer.TestCommandWidget.prototype.setDetailsVisible = function(visible) {
  this.detailsVisible = visible;
};

/** @override */
importer.TestCommandWidget.prototype.setDetailsBannerVisible = function(
    visible) {};

/**
 * @param {!VolumeManagerCommon.VolumeType} volumeType
 * @param {string} volumeId
 * @param {!Array<string>} fileNames
 * @param {string} currentDirectory
 * @return {!importer.ImportController}
 */
function createController(volumeType, volumeId, fileNames, currentDirectory) {
  sourceVolume = setupFileSystem(volumeType, volumeId, fileNames);

  environment = new TestControllerEnvironment(
      volumeManager, sourceVolume,
      sourceVolume.fileSystem.entries[currentDirectory]);

  return new importer.ImportController(
      environment, mediaScanner, mediaImporter, widget);
}

/**
 * @param {!VolumeManagerCommon.VolumeType} volumeType
 * @param {string} volumeId
 * @param {!Array<string>} fileNames
 * @return {!VolumeInfo}
 */
function setupFileSystem(volumeType, volumeId, fileNames) {
  var volumeInfo = volumeManager.createVolumeInfo(
      volumeType, volumeId, 'A volume known as ' + volumeId);
  assertTrue(volumeInfo != null);
  var mockFileSystem = /** @type {!MockFileSystem} */ (volumeInfo.fileSystem);
  mockFileSystem.populate(fileNames);
  return volumeInfo;
}

/**
 * @return {!Metadata}
 */
function getDefaultMetadata() {
  return /** @type {!Metadata} */ ({size: 0});
}
