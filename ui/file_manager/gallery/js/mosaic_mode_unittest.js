// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Mocking load detached image function.
ThumbnailLoader.prototype.loadDetachedImage = function(call) {
  call(true);
};

function testBasic() {
  var fileSystem = new MockFileSystem('volumeId');
  var entry = new MockEntry(fileSystem, '/test.jpg');
  var item = new Gallery.Item(entry, {}, {}, {}, true);
  var model = new cr.ui.ArrayDataModel([item]);
  var selection = new cr.ui.ListSelectionModel();
  var mosaic = new Mosaic(
      document, null, model, selection, {getLocationInfo: function() {}});
  document.documentElement.appendChild(mosaic);
  mosaic.init();
  mosaic.layout();
  assertEquals(1, mosaic.querySelectorAll('.mosaic-tile').length);
  assertEquals(1, mosaic.querySelectorAll(
      '.mosaic-tile .load-target-file-entry').length);
}
