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
 * Mock of ImageEncoder. Since some test changes the behavior of ImageEncoder,
 * this is initialized in setUp().
 */
var ImageEncoder;

/**
 * Load time data.
 */
loadTimeData.data = {
  DRIVE_DIRECTORY_LABEL: '',
  DOWNLOADS_DIRECTORY_LABEL: ''
};

function setUp() {
  ImageEncoder = {
    encodeMetadata: function() {},
    getBlob: function() {}
  };
}

/**
 * Returns a mock of metadata model.
 * @private
 * @return {!MetadataModel}
 */
function getMockMetadataModel() {
  return {
    get: function(entries, names) {
      return Promise.resolve([
        {size: 200}
      ]);
    },
    notifyEntriesChanged: function() {
    }
  };
}

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
  var metadataModel = getMockMetadataModel();
  metadataModel.notifyEntriesChanged = function() {
    entryChanged = true;
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

/**
 * Tests for GalleryItem#saveToFile. In this test case, fileWriter.write fails
 * with an error.
 */
function testSaveToFileWriteFailCase(callback) {
  var fileSystem = new MockFileSystem('volumeId');
  fileSystem.populate(['/test.jpg']);
  var entry = fileSystem.entries['/test.jpg'];

  entry.createWriter = function(callback) {
    callback({
      write: function() {
        Promise.resolve().then(function() {
          this.onerror(new Error());
        }.bind(this));
      },
      truncate: function() {
        Promise.resolve().then(function() {
          this.onwriteend();
        }.bind(this));
      }
    });
  };

  var item = new Gallery.Item(
      entry,
      {isReadOnly: false},
      {size: 100},
      {},
      /* original */ true);
  reportPromise(
      new Promise(item.saveToFile.bind(
          item,
          {getLocationInfo: function() { return {}; }},
          getMockMetadataModel(),
          /* fallbackDir */ null,
          /* overwrite */ true,
          document.createElement('canvas'))).then(function(result) {
            assertFalse(result);
          }), callback);
}

/**
 * Tests for GalleryItem#saveToFile. In this test case, ImageEncoder.getBlob
 * fails with an error. This test case confirms that no write operation runs
 * when it fails to get a blob of new image.
 */
function testSaveToFileGetBlobFailCase(callback) {
  ImageEncoder.getBlob = function() {
    throw new Error();
  };

  var fileSystem = new MockFileSystem('volumeId');
  fileSystem.populate(['/test.jpg']);
  var entry = fileSystem.entries['/test.jpg'];

  var writeOperationRun = false;
  entry.createWriter = function(callback) {
    callback({
      write: function() {
        Promise.resolve().then(function() {
          writeOperationRun = true;
          this.onwriteend();
        }.bind(this));
      },
      truncate: function() {
        Promise.resolve().then(function() {
          writeOperationRun = true;
          this.onwriteend();
        }.bind(this));
      }
    });
  };

  var item = new Gallery.Item(
      entry,
      {isReadOnly: false},
      {size: 100},
      {},
      /* original */ true);
  reportPromise(
      new Promise(item.saveToFile.bind(
          item,
          {getLocationInfo: function() { return {}; }},
          getMockMetadataModel(),
          /* fallbackDir */ null,
          /* overwrite */ true,
          document.createElement('canvas'))).then(function(result) {
            assertFalse(result);
            assertFalse(writeOperationRun);
          }), callback);
}
