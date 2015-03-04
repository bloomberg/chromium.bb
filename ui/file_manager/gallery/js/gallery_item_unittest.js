// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Mock of ImageUtil.
 */
var ImageUtil = {
  getMetricName: function() {},
  metrics: {
    recordEnum: function() {},
    recordInterval: function() {},
    startInterval: function() {}
  }
};

/**
 * Mock of ImageEncoder
 */
var ImageEncoder = {
  encodeMetadata: function() {},
  getBlob: function() {}
};

/**
 * Load time data.
 */
loadTimeData.data = {
  DRIVE_DIRECTORY_LABEL: '',
  DOWNLOADS_DIRECTORY_LABEL: ''
};

/**
 * Tests for GalleryItem#saveToFile.
 */
function testSaveToFile(callback) {
  var fileSystem = new MockFileSystem('volumeId');
  fileSystem.populate(['/test.jpg']);
  var entry = fileSystem.entries['/test.jpg'];
  entry.createWriter = function(callback) {
    callback({
      write: function() {
        Promise.resolve().then(function() {
          this.onwriteend();
        }.bind(this));
      },
      truncate: function() {
        this.write();
      }
    });
  };
  var entryChanged = false;
  var metadataModel = {
    get: function(entries, names) {
      return Promise.resolve([
        {size: 200}
      ]);
    },
    notifyEntriesChanged: function() {
      entryChanged = true;
    }
  };
  var item = new Gallery.Item(
      entry,
      {isReadOnly: false},
      {size: 100},
      {},
      /* original */ true);
  assertEquals(100, item.getMetadataItem().size);
  assertFalse(entryChanged);
  reportPromise(
      new Promise(item.saveToFile.bind(
          item,
          {getLocationInfo: function() { return {}; }},
          metadataModel,
          /* fallbackDir */ null,
          /* overwrite */ true,
          document.createElement('canvas'))).then(function() {
            assertEquals(200, item.getMetadataItem().size);
            assertTrue(entryChanged);
          }), callback);
}
