// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.chrome = {
  fileManagerPrivate: {
    getFileTasks: function(entries, callback) {
      setTimeout(callback.bind(null, [
        {taskId:'handler-extension-id|file|open', isDefault: false},
        {taskId:'handler-extension-id|file|play', isDefault: true}
      ]), 0);
    }
  },
  runtime: {id: 'test-extension-id'}
};

window.metrics = {
  recordEnum: function() {}
};

function testDoEntryAction(callback) {
  cr.ui.decorate('command', cr.ui.Command);
  var fileSystem = new MockFileSystem('volumeId');
  fileSystem.entries['/test.png'] =
      new MockFileEntry(fileSystem, '/test.png', {});
  var metadataCache = new MockMetadataCache();
  metadataCache.setForTest(fileSystem.entries['/test.png'], 'external', {});
  var controller = new TaskController(
      DialogType.FULL_PAGE,
      {
        taskMenuButton: document.createElement('button'),
        fileContextMenu: {
          defaultActionMenuItem: document.createElement('div')
        }
      },
      metadataCache,
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
          }
        });
      });

  controller.doEntryAction(fileSystem.entries['/test.png']);
  reportPromise(new Promise(function(fulfill) {
    chrome.fileManagerPrivate.executeTask = fulfill;
  }).then(function(info) {
    assertEquals("handler-extension-id|file|play", info);
  }), callback);
}
