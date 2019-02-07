// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Mock metrics.
 * @type {!Object}
 */
window.metrics = {
  recordEnum: function() {},
};

/**
 * Mock chrome APIs.
 * @type {!Object}
 */
var mockChrome;

// Set up test components.
function setUp() {
  // Mock LoadTimeData strings.
  window.loadTimeData.getBoolean = key => false;
  window.loadTimeData.getString = id => id;

  // Mock chrome APIs.
  mockChrome = {
    commandLinePrivate: {
      hasSwitch: function(name, callback) {
        callback(false);
      },
    },
    runtime: {
      id: 'test-extension-id',
      lastError: null,
    },
    storage: {
      onChanged: {
        addListener: function(callback) {},
      },
      local: {
        get: function(key, callback) {
          callback({});
        },
        set: function(value) {},
      },
    },
  };

  setupFileManagerPrivate();
  installMockChrome(mockChrome);

  // Install cr.ui <command> elements on the page.
  document.body.innerHTML = [
    '<command id="default-task">',
    '<command id="open-with">',
    '<command id="more-actions">',
    '<command id="show-submenu">',
  ].join('');

  // Initialize cr.ui.Command with the <command>s.
  cr.ui.decorate('command', cr.ui.Command);
}

/**
 * Returns a task controller.
 * @param {!FileSelectionHandler} fileSelectionHandler
 * @return {!TaskController}
 */
function createTaskController(fileSelectionHandler) {
  var taskController = new TaskController(
      DialogType.FULL_PAGE,
      /** @type {!VolumeManager} */ ({
        getLocationInfo: function(entry) {
          return VolumeManagerCommon.RootType.DRIVE;
        },
        getDriveConnectionState: function() {
          return VolumeManagerCommon.DriveConnectionType.ONLINE;
        },
        getVolumeInfo: function() {
          return {
            volumeType: VolumeManagerCommon.VolumeType.DRIVE,
          };
        },
      }),
      /** @type {!FileManagerUI} */ ({
        taskMenuButton: document.createElement('button'),
        shareMenuButton: {
          menu: document.createElement('div'),
        },
        fileContextMenu: {
          defaultActionMenuItem: document.createElement('div'),
        },
        shareSubMenu: document.createElement('div'),
        speakA11yMessage: text => {},
      }),
      new MockMetadataModel({}),
      /** @type {!DirectoryModel} */ ({
        getCurrentRootType: () => null,
      }),
      fileSelectionHandler,
      /** @type {!MetadataUpdateController} */ ({}),
      /** @type {!NamingController} */ ({}), createCrostiniForTest());

  return taskController;
}

/**
 * Mock FileSelectionHandler.
 * @extends {FileSelectionHandler}
 */
class FakeFileSelectionHandler {
  constructor() {
    this.selection = /** @type {!FileSelection} */ ({});
    this.updateSelection([], []);
    this.eventTarget_ = new cr.EventTarget();
  }
  computeAdditionalCallback() {}
  updateSelection(entries, mimeTypes) {
    this.selection = /** @type {!FileSelection} */ ({
      entries: entries,
      mimeTypes: mimeTypes,
      computeAdditional: (metadataModel) => {
        this.computeAdditionalCallback();
        return new Promise((resolve) => {
          resolve();
        });
      },
    });
  }

  addEventListener(...args) {
    return this.eventTarget_.addEventListener(...args);
  }
}

/**
 * Setup test case fileManagerPrivate.
 */
function setupFileManagerPrivate() {
  mockChrome.fileManagerPrivate = {
    getFileTaskCalledCount_: 0,
    getFileTasks: function(entries, callback) {
      mockChrome.fileManagerPrivate.getFileTaskCalledCount_++;
      const fileTasks = ([
        /** @type {!chrome.fileManagerPrivate.FileTask} */ ({
          taskId: 'handler-extension-id|file|open',
          isDefault: false,
        }),
        /** @type {!chrome.fileManagerPrivate.FileTask} */ ({
          taskId: 'handler-extension-id|file|play',
          isDefault: true,
        }),
      ]);
      setTimeout(callback.bind(null, fileTasks), 0);
    },
    onAppsUpdated: {
      addListener: function() {},
    },
  };
}

/**
 * Tests that executeEntryTask() runs the expected task.
 */
function testExecuteEntryTask(callback) {
  var selectionHandler = new FakeFileSelectionHandler();

  var fileSystem = new MockFileSystem('volumeId');
  fileSystem.entries['/test.png'] = new MockFileEntry(fileSystem, '/test.png');
  var taskController = createTaskController(selectionHandler);

  var testEntry = /** @type {FileEntry} */ (fileSystem.entries['/test.png']);
  taskController.executeEntryTask(testEntry);

  reportPromise(
      new Promise(function(resolve) {
        mockChrome.fileManagerPrivate.executeTask = resolve;
      }).then(function(taskId) {
        assertEquals('handler-extension-id|file|play', taskId);
      }),
      callback);
}

/**
 * Tests that getFileTasks() does not call .fileManagerPrivate.getFileTasks()
 * multiple times when the selected entries are not changed.
 */
function testGetFileTasksShouldNotBeCalledMultipleTimes(callback) {
  var selectionHandler = new FakeFileSelectionHandler();

  var fileSystem = new MockFileSystem('volumeId');
  selectionHandler.updateSelection(
      [new MockFileEntry(fileSystem, '/test.png')], ['image/png']);
  var taskController = createTaskController(selectionHandler);

  assert(mockChrome.fileManagerPrivate.getFileTaskCalledCount_ === 0);

  taskController.getFileTasks()
      .then(function(tasks) {
        assert(mockChrome.fileManagerPrivate.getFileTaskCalledCount_ === 1);
        assert(util.isSameEntries(
            tasks.entries, selectionHandler.selection.entries));
        // Make oldSelection.entries !== newSelection.entries
        selectionHandler.updateSelection(
            [new MockFileEntry(fileSystem, '/test.png')], ['image/png']);
        return taskController.getFileTasks();
      })
      .then(function(tasks) {
        assert(mockChrome.fileManagerPrivate.getFileTaskCalledCount_ === 1);
        assert(util.isSameEntries(
            tasks.entries, selectionHandler.selection.entries));
        callback();
      })
      .catch(function(error) {
        assertNotReached(error.toString());
        callback();
      });
}

/**
 * Tests that getFileTasks() should always return the promise whose FileTasks
 * correspond to FileSelectionHandler.selection at the time getFileTasks() is
 * called.
 */
function testGetFileTasksShouldNotReturnObsoletePromise(callback) {
  var selectionHandler = new FakeFileSelectionHandler();

  var fileSystem = new MockFileSystem('volumeId');
  selectionHandler.updateSelection(
      [new MockFileEntry(fileSystem, '/test.png')], ['image/png']);
  var taskController = createTaskController(selectionHandler);

  taskController.getFileTasks()
      .then(function(tasks) {
        assert(util.isSameEntries(
            tasks.entries, selectionHandler.selection.entries));
        selectionHandler.updateSelection(
            [new MockFileEntry(fileSystem, '/testtest.jpg')], ['image/jpeg']);
        return taskController.getFileTasks();
      })
      .then(function(tasks) {
        assert(util.isSameEntries(
            tasks.entries, selectionHandler.selection.entries));
        callback();
      })
      .catch(function(error) {
        assertNotReached(error.toString());
        callback();
      });
}

/**
 * Tests that changing the file selection during a getFileTasks() call causes
 * the getFileTasks() promise to reject.
 */
function testGetFileTasksShouldNotCacheRejectedPromise(callback) {
  var selectionHandler = new FakeFileSelectionHandler();

  var fileSystem = new MockFileSystem('volumeId');
  selectionHandler.updateSelection(
      [new MockFileEntry(fileSystem, '/test.png')], ['image/png']);
  var taskController = createTaskController(selectionHandler);

  // Setup the selection handler computeAdditionalCallback to change the file
  // selection during the getFileTasks() call.
  selectionHandler.computeAdditionalCallback = function() {
    selectionHandler.updateSelection(
        [new MockFileEntry(fileSystem, '/test.png')], ['image/png']);
  };

  taskController.getFileTasks().then(
      function(tasks) {
        assertNotReached('Fail: getFileTasks promise should be rejected');
        callback();
      },
      function() {
        // Clears the selection handler computeAdditionalCallback so that the
        // promise won't be rejected during the getFileTasks() call.
        selectionHandler.computeAdditionalCallback = function() {};

        taskController.getFileTasks().then(
            function(tasks) {
              assert(util.isSameEntries(
                  tasks.entries, selectionHandler.selection.entries));
              callback();
            },
            function() {
              assertNotReached('Fail: getFileTasks promise was rejected');
              callback();
            });
      });
}
