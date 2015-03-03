// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var model;
var fileSystem;
var item;

function setUp() {
  var metadataCache = new MockMetadataCache();
  model = new GalleryDataModel(
      metadataCache,
      /* Mock MetadataModel */{},
      /* Mock EntryListWatcher */{});
  fileSystem = new MockFileSystem('volumeId');
  item = new Gallery.Item(
      new MockEntry(fileSystem, '/test.jpg'),
      null,
      {media: {mimeType: 'image/jpeg'}},
      metadataCache,
      /* original */ true);
}

function testSaveItemOverwrite(callback) {
  // Mocking the saveToFile method.
  item.saveToFile = function(
      volumeManager,
      fallbackDir,
      overwrite,
      canvas,
      callback) {
    assertTrue(overwrite);
    callback(true);
  };
  model.push(item);
  reportPromise(
      model.saveItem({}, item, document.createElement('canvas'), true).
          then(function() { assertEquals(1, model.length); }),
      callback);
}

function testSaveItemNewFile(callback) {
  // Mocking the saveToFile method.
  item.saveToFile = function(
      volumeManager,
      fallbackDir,
      overwrite,
      canvas,
      callback) {
    assertFalse(overwrite);
    // Gallery item track new file.
    this.entry_ = new MockEntry(fileSystem, '/test (1).jpg');
    callback(true);
  };
  model.push(item);
  reportPromise(
      model.saveItem({}, item, document.createElement('canvas'), false).
          then(function() { assertEquals(2, model.length); }),
      callback);
}
