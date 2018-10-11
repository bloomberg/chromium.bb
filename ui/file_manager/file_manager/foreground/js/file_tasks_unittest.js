// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.metrics = {
  recordEnum: function() {},
  recordSmallCount: function() {},
};

var mockTaskHistory = {
  getLastExecutedTime: function(id) {
    return 0;
  },
  recordTaskExecuted: function(id) {}
};

loadTimeData.data = {
  MORE_ACTIONS_BUTTON_LABEL: 'MORE_ACTIONS_BUTTON_LABEL',
  NO_TASK_FOR_EXECUTABLE: 'NO_TASK_FOR_EXECUTABLE',
  NO_TASK_FOR_FILE_URL: 'NO_TASK_FOR_FILE_URL',
  NO_TASK_FOR_FILE: 'NO_TASK_FOR_FILE',
  NO_TASK_FOR_DMG: 'NO_TASK_FOR_DMG',
  NO_TASK_FOR_CRX: 'NO_TASK_FOR_CRX',
  NO_TASK_FOR_CRX_TITLE: 'NO_TASK_FOR_CRX_TITLE',
  OPEN_WITH_BUTTON_LABEL: 'OPEN_WITH_BUTTON_LABEL',
  SHARE_BEFORE_OPEN_CROSTINI_TITLE: 'SHARE_BEFORE_OPEN_CROSTINI_TITLE',
  SHARE_BEFORE_OPEN_CROSTINI_SINGLE: 'SHARE_BEFORE_OPEN_CROSTINI_SINGLE',
  SHARE_BEFORE_OPEN_CROSTINI_MULTIPLE: 'SHARE_BEFORE_OPEN_CROSTINI_MULTIPLE',
  TASK_INSTALL_LINUX_PACKAGE: 'TASK_INSTALL_LINUX_PACKAGE',
  TASK_OPEN: 'TASK_OPEN',
  UNABLE_TO_OPEN_CROSTINI_TITLE: 'UNABLE_TO_OPEN_CROSTINI_TITLE',
  UNABLE_TO_OPEN_CROSTINI: 'UNABLE_TO_OPEN_CROSTINI',
};

function setUp() {
  window.chrome = {
    commandLinePrivate: {
      hasSwitch: function(name, callback) {
        callback(false);
      }
    },
    fileManagerPrivate: {
      getFileTasks: function(entries, callback) {
        setTimeout(
            callback.bind(null, [{
                            taskId: 'handler-extension-id|app|any',
                            isDefault: false,
                            isGenericFileHandler: true
                          }]),
            0);
      },
      executeTask: function(taskId, entries, onViewFiles) {
        onViewFiles('failed');
      }
    },
    runtime: {id: 'test-extension-id'}
  };
}

/**
 * Returns a mock file manager.
 * @return {!FileManager}
 */
function getMockFileManager() {
  return {
    volumeManager: {
      getLocationInfo: function(entry) {
        return {rootType: VolumeManagerCommon.RootType.DRIVE};
      },
      getDriveConnectionState: function() {
        return VolumeManagerCommon.DriveConnectionType.ONLINE;
      },
      getVolumeInfo: function(entry) {
        return {
          volumeType: VolumeManagerCommon.VolumeType.DRIVE
        };
      }
    },
    ui: {
      alertDialog: {showHtml: function(title, text, onOk, onCancel, onShow) {}}
    },
    metadataModel: {},
    directoryModel: {
      getCurrentRootType: function() {
        return null;
      }
    }
  };
}

/**
 * Returns a promise which is resolved when showHtml of alert dialog is called
 * with expected title and text.
 *
 * @param {!Array<!Entry>} entries Entries.
 * @param {string} expectedTitle An expected title.
 * @param {string} expectedText An expected text.
 * @return {!Promise}
 */
function showHtmlOfAlertDialogIsCalled(entries, expectedTitle, expectedText) {
  return new Promise(function(resolve, reject) {
    var fileManager = getMockFileManager();
    fileManager.ui.alertDialog.showHtml =
        function(title, text, onOk, onCancel, onShow) {
          assertEquals(expectedTitle, title);
          assertEquals(expectedText, text);
          resolve();
        };

    FileTasks
        .create(
            fileManager.volumeManager, fileManager.metadataModel,
            fileManager.directoryModel, fileManager.ui, entries, [null],
            mockTaskHistory)
        .then(function(tasks) {
          tasks.executeDefault();
        });
  });
}

/**
 * Returns a promise which is resolved when openSuggestAppsDialog is called.
 *
 * @param {!Array<!Entry>} entries Entries.
 * @param {!Array<?string>} mimeTypes Mime types.
 * @return {!Promise}
 */
function openSuggestAppsDialogIsCalled(entries, mimeTypes) {
  return new Promise(function(resolve, reject) {
    var fileManager = getMockFileManager();
    fileManager.ui.suggestAppsDialog = {
      showByExtensionAndMime: function(extension, mimeType, onDialogClosed) {
        resolve();
      }
    };

    FileTasks
        .create(
            fileManager.volumeManager, fileManager.metadataModel,
            fileManager.directoryModel, fileManager.ui, entries, mimeTypes,
            mockTaskHistory)
        .then(function(tasks) {
          tasks.executeDefault();
        });
  });
}

/**
 * Returns a promise which is resolved when task picker is shown.
 *
 * @param {!Array<!Entry>} entries Entries.
 * @param {!Array<?string>} mimeTypes Mime types.
 * @return {!Promise}
 */
function showDefaultTaskDialogCalled(entries, mimeTypes) {
  return new Promise(function(resolve, reject) {
    var fileManager = getMockFileManager();
    fileManager.ui.defaultTaskPicker = {
      showDefaultTaskDialog: function(
          title, message, items, defaultIdx, onSuccess) {
        resolve();
      }
    };

    FileTasks
        .create(
            fileManager.volumeManager, fileManager.metadataModel,
            fileManager.directoryModel, fileManager.ui, entries, mimeTypes,
            mockTaskHistory)
        .then(function(tasks) {
          tasks.executeDefault();
        });
  });
}

function testToOpenExeFile(callback) {
  var mockFileSystem = new MockFileSystem('volumeId');
  var mockEntry = new MockFileEntry(mockFileSystem, '/test.exe');

  reportPromise(showHtmlOfAlertDialogIsCalled(
      [mockEntry], 'test.exe', 'NO_TASK_FOR_EXECUTABLE'), callback);
}

function testToOpenDmgFile(callback) {
  var mockFileSystem = new MockFileSystem('volumeId');
  var mockEntry = new MockFileEntry(mockFileSystem, '/test.dmg');

  reportPromise(showHtmlOfAlertDialogIsCalled(
      [mockEntry], 'test.dmg', 'NO_TASK_FOR_DMG'), callback);
}

function testToOpenCrxFile(callback) {
  var mockFileSystem = new MockFileSystem('volumeId');
  var mockEntry = new MockFileEntry(mockFileSystem, '/test.crx');

  reportPromise(showHtmlOfAlertDialogIsCalled(
      [mockEntry], 'NO_TASK_FOR_CRX_TITLE', 'NO_TASK_FOR_CRX'), callback);
}

function testToOpenRtfFile(callback) {
  var mockFileSystem = new MockFileSystem('volumeId');
  var mockEntry = new MockFileEntry(mockFileSystem, '/test.rtf');

  reportPromise(openSuggestAppsDialogIsCalled(
      [mockEntry], ['application/rtf']), callback);
}

/**
 * Test case for openSuggestAppsDialog with an entry which has external type of
 * metadata.
 */
function testOpenSuggestAppsDialogWithMetadata(callback) {
  var showByExtensionAndMimeIsCalled = new Promise(function(resolve, reject) {
    var fileSystem = new MockFileSystem('volumeId');
    var entry = new MockFileEntry(fileSystem, '/test.rtf');
    var fileManager = getMockFileManager();

    FileTasks
        .create(
            fileManager.volumeManager, fileManager.metadataModel,
            fileManager.directoryModel, {
              taskMenuButton: document.createElement('button'),
              fileContextMenu:
                  {defaultActionMenuItem: document.createElement('div')},
              suggestAppsDialog: {
                showByExtensionAndMime: function(
                    extension, mimeType, onDialogClosed) {
                  assertEquals('.rtf', extension);
                  assertEquals('application/rtf', mimeType);
                  resolve();
                }
              }
            },
            [entry], ['application/rtf'], mockTaskHistory)
        .then(function(tasks) {
          tasks.openSuggestAppsDialog(
              function() {}, function() {}, function() {});
        });
  });

  reportPromise(showByExtensionAndMimeIsCalled, callback);
}

/**
 * Test case for openSuggestAppsDialog with an entry which doesn't have
 * extension. Since both extension and MIME type are required for
 * openSuggestAppsDialogopen, onFalure should be called for this test case.
 */
function testOpenSuggestAppsDialogFailure(callback) {
  var onFailureIsCalled = new Promise(function(resolve, reject) {
    var fileSystem = new MockFileSystem('volumeId');
    var entry = new MockFileEntry(fileSystem, '/test');
    var fileManager = getMockFileManager();

    FileTasks
        .create(
            fileManager.volumeManager, fileManager.metadataModel,
            fileManager.directoryModel, fileManager.ui, [entry], [null],
            mockTaskHistory)
        .then(function(tasks) {
          tasks.openSuggestAppsDialog(function() {}, function() {}, resolve);
        });
  });

  reportPromise(onFailureIsCalled, callback);
}

/**
 * Test case for opening task picker with an entry which doesn't have default
 * app but multiple apps that can open it.
 */
function testOpenTaskPicker(callback) {
  window.chrome.fileManagerPrivate.getFileTasks = function(entries, callback) {
    setTimeout(
        callback.bind(
            null,
            [
              {
                taskId: 'handler-extension-id1|app|any',
                isDefault: false,
                isGenericFileHandler: false,
                title: 'app 1',
              },
              {
                taskId: 'handler-extension-id2|app|any',
                isDefault: false,
                isGenericFileHandler: false,
                title: 'app 2',
              }
            ]),
        0);
  };

  var mockFileSystem = new MockFileSystem('volumeId');
  var mockEntry = new MockFileEntry(mockFileSystem, '/test.tiff');

  reportPromise(
      showDefaultTaskDialogCalled([mockEntry], ['image/tiff']), callback);
}

function testOpenWithMostRecentlyExecuted(callback) {
  const latestTaskId = 'handler-extension-most-recently-executed|app|any';
  const oldTaskId = 'handler-extension-executed-before|app|any';
  window.chrome.fileManagerPrivate.getFileTasks = function(entries, callback) {
    setTimeout(
        callback.bind(
            null,
            // File tasks is sorted by last executed time, latest first.
            [
              {
                taskId: latestTaskId,
                isDefault: false,
                isGenericFileHandler: false,
                title: 'app 1',
              },
              {
                taskId: oldTaskId,
                isDefault: false,
                isGenericFileHandler: false,
                title: 'app 2',
              },
              {
                taskId: 'handler-extension-never-executed|app|any',
                isDefault: false,
                isGenericFileHandler: false,
                title: 'app 3',
              },
            ]),
        0);
  };
  var taskHistory = {
    getLastExecutedTime: function(id) {
      if (id == oldTaskId)
        return 10000;
      else if (id == latestTaskId)
        return 20000;
      return 0;
    },
    recordTaskExecuted: function(taskId) {}
  };
  var executedTask = null;
  window.chrome.fileManagerPrivate.executeTask = function(
      taskId, entries, onViewFiles) {
    executedTask = taskId;
    onViewFiles('success');
  };

  var mockFileSystem = new MockFileSystem('volumeId');
  var mockEntry = new MockFileEntry(mockFileSystem, '/test.tiff');
  var entries = [mockEntry];
  var promise = new Promise(function(resolve, reject) {
    var fileManager = getMockFileManager();
    fileManager.ui.defaultTaskPicker = {
      showDefaultTaskDialog: function(
          title, message, items, defaultIdx, onSuccess) {
        failWithMessage('should not show task picker');
      }
    };

    FileTasks
        .create(
            fileManager.volumeManager, fileManager.metadataModel,
            fileManager.directoryModel, fileManager.ui, [mockEntry], [null],
            taskHistory)
        .then(function(tasks) {
          tasks.executeDefault();
          assertEquals(latestTaskId, executedTask);
          resolve();
        });
  });
  reportPromise(promise, callback);
}

function testOpenZipWithZipArchiver(callback) {
  var zipArchiverTaskId = 'dmboannefpncccogfdikhmhpmdnddgoe|app|open';

  chrome.commandLinePrivate.hasSwitch = function(name, callback) {
    if (name == 'enable-zip-archiver-unpacker') {
      // This flag used to exist and was used to switch between the "Zip
      // Unpacker" and "Zip Archiver" component extensions.
      failWithMessage('run zip archiver', 'zip archiver flags checked');
    }
    callback(false);
  };

  window.chrome.fileManagerPrivate.getFileTasks = function(entries, callback) {
    setTimeout(
        callback.bind(
            null,
            [
              {
                taskId: zipArchiverTaskId,
                isDefault: false,
                isGenericFileHandler: false,
                title: 'Zip Archiver',
              },
            ]),
        0);
  };
  // None of the tasks has ever been executed.
  var taskHistory = {
    getLastExecutedTime: function(id) {
      return 0;
    },
    recordTaskExecuted: function(taskId) {}
  };
  var executedTask = null;
  window.chrome.fileManagerPrivate.executeTask = function(
      taskId, entries, onViewFiles) {
    executedTask = taskId;
    onViewFiles('success');
  };

  var mockFileSystem = new MockFileSystem('volumeId');
  var mockEntry = new MockFileEntry(mockFileSystem, '/test.zip');
  var promise = new Promise(function(resolve, reject) {
    var fileManager = getMockFileManager();
    fileManager.ui.defaultTaskPicker = {
      showDefaultTaskDialog: function(
          title, message, items, defaultIdx, onSuccess) {
        failWithMessage('run zip archiver', 'default task picker was shown');
      }
    };

    FileTasks
        .create(
            fileManager.volumeManager, fileManager.metadataModel,
            fileManager.directoryModel, fileManager.ui, [mockEntry], [null],
            taskHistory)
        .then(function(tasks) {
          tasks.executeDefault();
          assertEquals(zipArchiverTaskId, executedTask);
          resolve();
        });
  });
  reportPromise(promise, callback);
}

function testOpenInstallLinuxPackageDialog(callback) {
  window.chrome.fileManagerPrivate.getFileTasks = function(entries, callback) {
    setTimeout(
        callback.bind(
            null,
            [
              {
                taskId: 'test-extension-id|file|install-linux-package',
                isDefault: false,
                isGenericFileHandler: false,
                title: '__MSG_INSTALL_LINUX_PACKAGE__',
              },
            ]),
        0);
  };

  var mockFileSystem = new MockFileSystem('volumeId');
  var mockEntry = new MockFileEntry(mockFileSystem, '/test.deb');
  var promise = new Promise(function(resolve, reject) {
    var fileManager = getMockFileManager();
    fileManager.ui.installLinuxPackageDialog = {
      showInstallLinuxPackageDialog: function(entry) {
        resolve();
      }
    };

    fileManager.volumeManager.getLocationInfo = function(entry) {
      return {rootType: VolumeManagerCommon.RootType.CROSTINI};
    };

    FileTasks
        .create(
            fileManager.volumeManager, fileManager.metadataModel,
            fileManager.directoryModel, fileManager.ui, [mockEntry], [null],
            mockTaskHistory)
        .then(function(tasks) {
          tasks.executeDefault();
        });
  });
  reportPromise(promise, callback);
}

function testMaybeShowCrostiniShareDialog() {
  const volumeManagerDownloads = {
    getLocationInfo: (entry) => {
      return {rootType: 'downloads'};
    }
  };
  const mockFileSystem = new MockFileSystem('downloads');
  const sharedDir = new MockDirectoryEntry(mockFileSystem, '/shared');
  const shared = new MockFileEntry(mockFileSystem, '/shared/file');
  Crostini.registerSharedPath(sharedDir, volumeManagerDownloads);
  const notShared1 = new MockFileEntry(mockFileSystem, '/notShared/file1');
  const notShared2 = new MockFileEntry(mockFileSystem, '/notShared/file2');
  const otherNotShared =
      new MockFileEntry(mockFileSystem, '/otherNotShared/file');

  const expect =
      (comment, entries, expectShareDialogShown, expectedTitle,
       expectedMessage) => {
        let showHtmlCalled = false;
        const showHtml = (title, message) => {
          showHtmlCalled = true;
          assertEquals(
              expectedTitle, title, 'crostini share dialog title: ' + comment);
          assertEquals(
              expectedMessage, message,
              'crostini share dialog message: ' + comment);
        };
        const fakeFilesTask = {
          entries_: entries,
          ui_: {
            alertDialog: {showHtml: showHtml},
            confirmDialog: {showHtml: showHtml},
          },
          sharePathWithCrostiniAndExecute_: () => {},
          volumeManager_: volumeManagerDownloads,
        };
        const crostiniTask = {taskId: '|crostini|'};
        const shareDialogShown =
            FileTasks.prototype.maybeShowCrostiniShareDialog_.call(
                fakeFilesTask, crostiniTask);
        assertEquals(
            expectShareDialogShown, shareDialogShown,
            'dialog shown: ' + comment);
        assertEquals(
            expectShareDialogShown, showHtmlCalled,
            'showHtml called:' + comment);
      };


  expect('No entries', [], false, '', '');

  Crostini.IS_CROSTINI_FILES_ENABLED = false;
  expect(
      'Single entry, crostini-files not enabled', [notShared1], true,
      'UNABLE_TO_OPEN_CROSTINI_TITLE', 'UNABLE_TO_OPEN_CROSTINI');

  Crostini.IS_CROSTINI_FILES_ENABLED = true;

  expect(
      'Single entry, not shared', [notShared1], true,
      'SHARE_BEFORE_OPEN_CROSTINI_TITLE', 'SHARE_BEFORE_OPEN_CROSTINI_SINGLE');

  expect('Single entry, shared', [shared], false, '', '');

  expect(
      '2 entries, not shared, same dir', [notShared1, notShared2], true,
      'SHARE_BEFORE_OPEN_CROSTINI_TITLE',
      'SHARE_BEFORE_OPEN_CROSTINI_MULTIPLE');

  expect(
      '2 entries, not shared, different dir', [notShared1, otherNotShared],
      true, 'UNABLE_TO_OPEN_CROSTINI_TITLE', 'UNABLE_TO_OPEN_CROSTINI');

  expect(
      '2 entries, 1 not shared, different dir, not shared first',
      [notShared1, shared], true, 'SHARE_BEFORE_OPEN_CROSTINI_TITLE',
      'SHARE_BEFORE_OPEN_CROSTINI_SINGLE');

  expect(
      '2 entries, 1 not shared, different dir, shared first',
      [shared, notShared1], true, 'SHARE_BEFORE_OPEN_CROSTINI_TITLE',
      'SHARE_BEFORE_OPEN_CROSTINI_SINGLE');

  expect(
      '3 entries, 2 not shared, different dir',
      [shared, notShared1, notShared2], true,
      'SHARE_BEFORE_OPEN_CROSTINI_TITLE',
      'SHARE_BEFORE_OPEN_CROSTINI_MULTIPLE');
}
