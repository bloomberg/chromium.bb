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
  var fetchedMediaCleared = false;
  var metadataCache = {
    getLatest: function(entries, type, callback) {
      callback([{name: 'newMetadata'}]);
    },
    clear: function(entries, type) {
      fetchedMediaCleared = true;
    }
  };
  var item = new Gallery.Item(
      entry,
      {isReadOnly: false},
      {name: 'oldMetadata'},
      {},
      metadataCache,
      // Mock of MetadataModel.
      {
        get: function() {
          return Promise.resolve([{}]);
        },
        notifyEntriesChanged: function() {}
      },
      /* original */ true);
  assertEquals('oldMetadata', item.getMetadata().name);
  assertFalse(fetchedMediaCleared);
  reportPromise(
      new Promise(item.saveToFile.bind(
          item,
          {getLocationInfo: function() { return {}; }},
          null,
          true,
          document.createElement('canvas'))).then(function() {
            assertEquals('newMetadata', item.getMetadata().name);
            assertTrue(fetchedMediaCleared);
          }), callback);
}
