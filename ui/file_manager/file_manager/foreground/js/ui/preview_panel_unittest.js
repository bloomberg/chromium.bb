// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Mock ThumbnailLoader.FillMode
 */
var ThumbnailLoader = {
  FillMode: {
  }
};

/**
 * Mock FileGrid.
 */
var FileGrid = {
  ThumbnailQuality: {},
  decorateThumbnailBox: function(
      box,
      entry,
      metadataCache,
      volumeManager,
      historyLoader,
      fillMode,
      quality,
      animation,
      opt_callback) {
    var imageContainer = document.createElement('div');
    imageContainer.classList.add('img-container');
    box.appendChild(imageContainer);
    var image = new Image();
    image.width = 500;
    image.height = 500;
    if (opt_callback)
      opt_callback(image);
  }
};

function testPreviewThumbnail() {
  var element = document.createElement('div');
  var mockFileSystem = new MockFileSystem('volumeId');
  var mockEntry = new MockFileEntry(mockFileSystem, '/test.jpg');
  var thumbnailUI = new PreviewPanel.Thumbnails(element);
  thumbnailUI.selection = {entries: [mockEntry]};
  thumbnailUI.selection = {entries: [mockEntry]};
  // Multiple updates of selection do not create multiple popup images.
  assertEquals(1, element.querySelectorAll('.popup').length);
}
