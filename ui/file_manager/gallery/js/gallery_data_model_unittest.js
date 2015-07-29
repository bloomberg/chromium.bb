// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var model;
var fileSystem;
var item;

/**
 * Mock thumbnail model.
 */
function ThumbnailModel() {
}

ThumbnailModel.prototype.get = function(entries) {
  return Promise.resolve(entries.map(function() {
    return {};
  }));
};

function setUp() {
  model = new GalleryDataModel(
      /* Mock MetadataModel */{
        get: function() {
          return Promise.resolve([{}]);
        }
      },
      /* Mock EntryListWatcher */{});
  fileSystem = new MockFileSystem('volumeId');
}

function testSaveItemOverwrite(callback) {
  var item = new Gallery.Item(
      new MockEntry(fileSystem, '/test.jpg'),
      null,
      /* metadataItem */ {},
      /* thumbnailMetadataItem */ {},
      /* original */ true);

  // Mocking the saveToFile method.
  item.saveToFile = function(
      volumeManager,
      metadataModel,
      fallbackDir,
      canvas,
      callback) {
    callback(true);
  };
  model.push(item);
  reportPromise(
      model.saveItem({}, item, document.createElement('canvas')).
          then(function() { assertEquals(1, model.length); }),
      callback);
}

function testSaveItemNewFile(callback) {
  var item = new Gallery.Item(
      new MockEntry(fileSystem, '/test.webp'),
      null,
      /* metadataItem */ {},
      /* thumbnailMetadataItem */ {},
      /* original */ true);

  // Mocking the saveToFile method. In this case, Gallery saves to a new file
  // since it cannot overwrite to webp image file.
  item.saveToFile = function(
      volumeManager,
      metadataModel,
      fallbackDir,
      canvas,
      callback) {
    // Gallery item track new file.
    this.entry_ = new MockEntry(fileSystem, '/test (1).png');
    callback(true);
  };
  model.push(item);
  reportPromise(
      model.saveItem({}, item, document.createElement('canvas')).
          then(function() { assertEquals(2, model.length); }),
      callback);
}
