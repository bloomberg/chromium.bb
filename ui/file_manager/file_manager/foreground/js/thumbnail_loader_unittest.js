// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getLoadTarget(entry, metadata) {
  return new ThumbnailLoader(entry, null, metadata).getLoadTarget();
}

function testShouldUseMetadataThumbnail() {
  var mockFileSystem = new MockFileSystem('volumeId');
  var imageEntry = new MockEntry(mockFileSystem, '/test.jpg');
  var pdfEntry = new MockEntry(mockFileSystem, '/test.pdf');

  // Embed thumbnail is provided.
  assertEquals(
      ThumbnailLoader.LoadTarget.CONTENT_METADATA,
      getLoadTarget(imageEntry, {thumbnail: {url: 'url'}}));

  // Drive thumbnail is provided and the file is not cached locally.
  assertEquals(
      ThumbnailLoader.LoadTarget.EXTERNAL_METADATA,
      getLoadTarget(imageEntry, {external: {thumbnailUrl: 'url'}}));

  // Drive thumbnail is provided but the file is not synced.
  assertEquals(
      ThumbnailLoader.LoadTarget.FILE_ENTRY,
      getLoadTarget(
          imageEntry, {external: {thumbnailUrl: 'url', dirty: true}}));

  // Drive thumbnail is provided and it is not an image file.
  assertEquals(
      ThumbnailLoader.LoadTarget.EXTERNAL_METADATA,
      getLoadTarget(
          pdfEntry, {external: {thumbnailUrl: 'url', dirty: true}}));
}
