// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.metrics = {
  recordEnum: function() {}
};

function MockMetadataModel(properties) {
  this.properties_ = properties;
}

MockMetadataModel.prototype.get = function() {
  return Promise.resolve([this.properties_]);
};

function setUp() {
  // Behavior of window.chrome depends on each test case. window.chrome should
  // be initialized properly inside each test function.
  window.chrome = {
    runtime: {
      id: 'test-extension-id',
      lastError: null
    }
  };

  cr.ui.decorate('command', cr.ui.Command);
}

function testDoEntryAction(callback) {
  window.chrome.fileManagerPrivate = {
    getFileTasks: function(entries, callback) {
      setTimeout(callback.bind(null, [
        {taskId:'handler-extension-id|file|open', isDefault: false},
        {taskId:'handler-extension-id|file|play', isDefault: true}
      ]), 0);
    }
  };

  var fileSystem = new MockFileSystem('volumeId');
  fileSystem.entries['/test.png'] =
      new MockFileEntry(fileSystem, '/test.png', {});
  var controller = new TaskController(
      DialogType.FULL_PAGE,
      {
        taskMenuButton: document.createElement('button'),
        fileContextMenu: {
          defaultActionMenuItem: document.createElement('div')
        }
      },
      new MockMetadataModel({}),
      new cr.EventTarget(),
      null,
      function() {
        return new FileTasks({
          volumeManager: {
            getDriveConnectionState: function() {
              return VolumeManagerCommon.DriveConnectionType.ONLINE;
            }
          },
          isOnDrive: function() {
            return true;
          },
          getMetadataModel: function() {}
        });
      });

  controller.doEntryAction(fileSystem.entries['/test.png']);
  reportPromise(new Promise(function(fulfill) {
    chrome.fileManagerPrivate.executeTask = fulfill;
  }).then(function(info) {
    assertEquals("handler-extension-id|file|play", info);
  }), callback);
}

/**
 * Test case for openSuggestAppsDialog with an entry which has external type of
 * metadata.
 */
function testOpenSuggestAppsDialogWithMetadata(callback) {
  var showByExtensionAndMimeIsCalled = new Promise(function(resolve, reject) {
    var fileSystem = new MockFileSystem('volumeId');
    var entry = new MockFileEntry(fileSystem, '/test.rtf');

    var controller = new TaskController(
        DialogType.FULL_PAGE,
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
        new MockMetadataModel({contentMimeType: 'application/rtf'}),
        new cr.EventTarget(),
        null,
        null);

    controller.openSuggestAppsDialog(
        entry, function() {}, function() {}, function() {});
  });

  reportPromise(showByExtensionAndMimeIsCalled, callback);
}

/**
 * Test case for openSuggestAppsDialog with an entry which doesn't have external
 * type of metadata.
 */
function testOpenSuggestAppsDialogWithoutMetadata(callback) {
  window.chrome.fileManagerPrivate = {
    getMimeType: function(url, callback) {
      callback('application/rtf');
    }
  };

  var showByExtensionAndMimeIsCalled = new Promise(function(resolve, reject) {
    var fileSystem = new MockFileSystem('volumeId');
    var entry = new MockFileEntry(fileSystem, '/test.rtf');

    var controller = new TaskController(
        DialogType.FULL_PAGE,
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
        new MockMetadataModel({}),
        new cr.EventTarget(),
        null,
        null);

    controller.openSuggestAppsDialog(
        entry, function() {}, function() {}, function() {});
  });

  reportPromise(showByExtensionAndMimeIsCalled, callback);
}

/**
 * Test case for openSuggestAppsDialog with an entry which doesn't have
 * extensiion. Since both extension and MIME type are required for
 * openSuggestAppsDialogopen, onFalure should be called for this test case.
 */
function testOpenSuggestAppsDialogFailure(callback) {
  var onFailureIsCalled = new Promise(function(resolve, reject) {
    var fileSystem = new MockFileSystem('volumeId');
    var entry = new MockFileEntry(fileSystem, '/test');

    var controller = new TaskController(
        DialogType.FULL_PAGE,
        {
          taskMenuButton: document.createElement('button'),
          fileContextMenu: {
            defaultActionMenuItem: document.createElement('div')
          }
        },
        new MockMetadataModel({contentMimeType: 'image/png'}),
        new cr.EventTarget(),
        null,
        null);

    controller.openSuggestAppsDialog(
        entry, function() {}, function() {}, resolve);
  });

  reportPromise(onFailureIsCalled, callback);
}
