// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.chrome = {
  commandLinePrivate: {
    hasSwitch: function(name, callback) {
      callback(false);
    }
  },
  fileManagerPrivate: {
    getFileTasks: function(entries, callback) {
      setTimeout(callback.bind(null, [
        {
          taskId: 'handler-extension-id|app|any',
          isDefault: false,
          isGenericFileHandler: true
        }
      ]), 0);
    },
    executeTask: function(taskId, entries, onViewFiles) {
      onViewFiles('failed');
    }
  },
  runtime: {id: 'test-extension-id'}
};

window.metrics = {
  recordEnum: function() {}
};

loadTimeData.data = {
  NO_TASK_FOR_EXECUTABLE: 'NO_TASK_FOR_EXECUTABLE',
  NO_TASK_FOR_FILE_URL: 'NO_TASK_FOR_FILE_URL',
  NO_TASK_FOR_FILE: 'NO_TASK_FOR_FILE',
  NO_TASK_FOR_DMG: 'NO_TASK_FOR_DMG',
  NO_TASK_FOR_CRX: 'NO_TASK_FOR_CRX',
  NO_TASK_FOR_CRX_TITLE: 'NO_TASK_FOR_CRX_TITLE'
};

/**
 * Returns a mock file manager.
 * @return {!FileManager}
 */
function getMockFileManager() {
  return {
    volumeManager: {
      getDriveConnectionState: function() {
        return VolumeManagerCommon.DriveConnectionType.ONLINE;
      },
      getVolumeInfo: function() {
        return {
          volumeType: VolumeManagerCommon.VolumeType.DRIVE
        }
      }
    },
    ui: {
      alertDialog: {
        showHtml: function(title, text, onOk, onCancel, onShow) {}
      }
    },
    metadataModel: {},
    directoryModel: {}
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

    FileTasks.create(
        fileManager.volumeManager, fileManager.metadataModel,
        fileManager.directoryModel, fileManager.ui, entries, [null]).
      then(function(tasks) {
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

    FileTasks.create(
        fileManager.volumeManager, fileManager.metadataModel,
        fileManager.directoryModel, fileManager.ui, entries, mimeTypes)
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

    FileTasks.create(
        {
          getDriveConnectionState: function() {
            return VolumeManagerCommon.DriveConnectionType.ONLINE;
          }
        },
        {},
        {},
        {
          taskMenuButton: document.createElement('button'),
          fileContextMenu: {
            defaultActionMenuItem: document.createElement('div')
          },
          suggestAppsDialog: {
            showByExtensionAndMime: function(
                extension, mimeType, onDialogClosed) {
              assertEquals('.rtf', extension);
              assertEquals('application/rtf', mimeType);
              resolve();
            }
          }
        },
        [entry],
        ['application/rtf']).then(function(tasks) {
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

    FileTasks.create(
        {
          getDriveConnectionState: function() {
            return VolumeManagerCommon.DriveConnectionType.ONLINE;
          }
        },
        {},
        {},
        {
          taskMenuButton: document.createElement('button'),
          fileContextMenu: {
            defaultActionMenuItem: document.createElement('div')
          }
        },
        [entry],
        [null]).then(function(tasks) {
          tasks.openSuggestAppsDialog(function() {}, function() {}, resolve);
        });
  });

  reportPromise(onFailureIsCalled, callback);
}
