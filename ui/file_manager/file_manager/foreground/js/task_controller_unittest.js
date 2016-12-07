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

function testExecuteEntryTask(callback) {
  window.chrome.fileManagerPrivate = {
    getFileTasks: function(entries, callback) {
      setTimeout(callback.bind(null, [
        {taskId:'handler-extension-id|file|open', isDefault: false},
        {taskId:'handler-extension-id|file|play', isDefault: true}
      ]), 0);
    },
    onAppsUpdated: {
      addListener: function() {},
    },
  };

  var fileSystem = new MockFileSystem('volumeId');
  fileSystem.entries['/test.png'] =
      new MockFileEntry(fileSystem, '/test.png', {});
  var controller = new TaskController(
      DialogType.FULL_PAGE,
      {
        getDriveConnectionState: function() {
          return VolumeManagerCommon.DriveConnectionType.ONLINE;
        },
        getVolumeInfo: function() {
          return {
            volumeType: VolumeManagerCommon.VolumeType.DRIVE
          }
        }
      },
      {
        taskMenuButton: document.createElement('button'),
        fileContextMenu: {
          defaultActionMenuItem: document.createElement('div')
        }
      },
      new MockMetadataModel({}),
      {},
      new cr.EventTarget(),
      null);

  controller.executeEntryTask(fileSystem.entries['/test.png']);
  reportPromise(new Promise(function(fulfill) {
    chrome.fileManagerPrivate.executeTask = fulfill;
  }).then(function(info) {
    assertEquals("handler-extension-id|file|play", info);
  }), callback);
}
