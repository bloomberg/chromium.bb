// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.chrome = {
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
    executeTask: function(taskId, urls, onViewFiles) {
      onViewFiles('failed');
    }
  },
  runtime: {id: 'test-extension-id'}
};

window.metrics = {
  recordEnum: function() {}
};

loadTimeData.data = {
  NO_ACTION_FOR_EXECUTABLE: 'NO_ACTION_FOR_EXECUTABLE',
  NO_ACTION_FOR_FILE_URL: 'NO_ACTION_FOR_FILE_URL',
  NO_ACTION_FOR_DMG: 'NO_ACTION_FOR_DMG',
  NO_ACTION_FOR_CRX: 'NO_ACTION_FOR_CRX',
  NO_ACTION_FOR_CRX_TITLE: 'NO_ACTION_FOR_CRX_TITLE'
};

/**
 * Returns a mock file manager.
 * @return {!FileManager}
 */
function getMockFileManager() {
  return {
    isOnDrive: function() {
      return false;
    },
    volumeManager: {
      getDriveConnectionState: function() {
        return VolumeManagerCommon.DriveConnectionType.ONLINE;
      }
    },
    ui: {
      alertDialog: {
        showHtml: function(title, text, onOk, onCancel, onShow) {}
      }
    },
    taskController: {
      openSuggestAppsDialog: function(
          entry, onSuccess, onCancelled, onFailure) {}
    },
    getMetadataModel: function() {}
  };
}

/**
 * Returns a promise which is resolved when showHtml of alert dialog is called
 * with expected title and text.
 *
 * @param {!Array.<!Entry>} entries Entries.
 * @param {string} expectedTitle An expected title.
 * @param {string} expectedText An expected text.
 * @return {!Promise}
 */
function showHtmlOfAlertDialogIsCalled(
    entries, expectedTitle, expectedText) {
  return new Promise(function(resolve, reject) {
    var fileManager = getMockFileManager();
    fileManager.ui.alertDialog.showHtml =
        function(title, text, onOk, onCancel, onShow) {
          assertEquals(expectedTitle, title);
          assertEquals(expectedText, text);
          resolve();
        };

    var fileTasks = new FileTasks(fileManager);
    fileTasks.init(entries).then(function() {
      fileTasks.executeDefault();
    });
  });
}

/**
 * Returns a promise which is resolved when openSuggestAppsDialog is called.
 *
 * @param {!Array.<!Entry>} entries Entries.
 * @return {!Promise}
 */
function openSuggestAppsDialogIsCalled(entries) {
  return new Promise(function(resolve, reject) {
    var fileManager = getMockFileManager();
    fileManager.taskController.openSuggestAppsDialog =
        function(entry, onSuccess, onCancelled, onFailure) {
          resolve();
        };

    var fileTasks = new FileTasks(fileManager);
    fileTasks.init(entries).then(function() {
      fileTasks.executeDefault();
    });
  });
}

function testToOpenExeFile(callback) {
  var mockFileSystem = new MockFileSystem('volumeId');
  var mockEntry = new MockFileEntry(mockFileSystem, '/test.exe');

  reportPromise(showHtmlOfAlertDialogIsCalled(
      [mockEntry], 'test.exe', 'NO_ACTION_FOR_EXECUTABLE'), callback);
}

function testToOpenDmgFile(callback) {
  var mockFileSystem = new MockFileSystem('volumeId');
  var mockEntry = new MockFileEntry(mockFileSystem, '/test.dmg');

  reportPromise(showHtmlOfAlertDialogIsCalled(
      [mockEntry], 'test.dmg', 'NO_ACTION_FOR_DMG'), callback);
}

function testToOpenCrxFile(callback) {
  var mockFileSystem = new MockFileSystem('volumeId');
  var mockEntry = new MockFileEntry(mockFileSystem, '/test.crx');

  reportPromise(showHtmlOfAlertDialogIsCalled(
      [mockEntry], 'NO_ACTION_FOR_CRX_TITLE', 'NO_ACTION_FOR_CRX'), callback);
}

function testToOpenRtfFile(callback) {
  var mockFileSystem = new MockFileSystem('volumeId');
  var mockEntry = new MockFileEntry(mockFileSystem, '/test.rtf');

  reportPromise(openSuggestAppsDialogIsCalled([mockEntry]), callback);
}
