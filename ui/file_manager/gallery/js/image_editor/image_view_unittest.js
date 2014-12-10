// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testImageView() {
  assertTrue(!!ImageView);

  // Item has full size cache.
  var itemWithFullCache = new Gallery.Item(null, null, {}, null, false);
  itemWithFullCache.contentImage = document.createElement('canvas');
  assertEquals(
      ImageView.LoadTarget.CACHED_MAIN_IMAGE,
      ImageView.getLoadTarget(itemWithFullCache, new ImageView.Effect.None()));

  // Item has screen size cache.
  var itemWithScreenCache = new Gallery.Item(null, null, {}, null, false);
  itemWithScreenCache.screenImage = document.createElement('canvas');
  assertEquals(
      ImageView.LoadTarget.CACHED_THUMBNAIL,
      ImageView.getLoadTarget(
          itemWithScreenCache, new ImageView.Effect.None()));

  // Item with content thumbnail.
  var itemWithContentThumbnail = new Gallery.Item(
      null, null, {thumbnail: {url: 'url'}}, null, false);
  assertEquals(
      ImageView.LoadTarget.THUMBNAIL,
      ImageView.getLoadTarget(
          itemWithContentThumbnail, new ImageView.Effect.None()));

  // Item with external thumbnail.
  var itemWithExternalThumbnail = new Gallery.Item(
      null, null, {external: {thumbnailUrl: 'url'}}, null, false);
  assertEquals(
      ImageView.LoadTarget.THUMBNAIL,
      ImageView.getLoadTarget(
          itemWithExternalThumbnail, new ImageView.Effect.None()));

  // Item with external thumbnail shown by slide effect.
  var itemWithExternalThumbnailSlide = new Gallery.Item(
      null, null, {external: {thumbnailUrl: 'url'}}, null, false);
  assertEquals(
      ImageView.LoadTarget.THUMBNAIL,
      ImageView.getLoadTarget(
          itemWithExternalThumbnailSlide, new ImageView.Effect.Slide(1)));

  // Item with external thumbnail shown by zoom effect.
  var itemWithExternalThumbnailZoom = new Gallery.Item(
      null, null, {external: {thumbnailUrl: 'url'}}, null, false);
  assertEquals(
      ImageView.LoadTarget.MAIN_IMAGE,
      ImageView.getLoadTarget(
          itemWithExternalThumbnailZoom,
          new ImageView.Effect.Zoom(0, 0, null)));

  // Item without cache/thumbnail.
  var itemWithoutCacheOrThumbnail = new Gallery.Item(
      null, null, {}, null, false);
  assertEquals(
      ImageView.LoadTarget.MAIN_IMAGE,
      ImageView.getLoadTarget(
          itemWithoutCacheOrThumbnail, new ImageView.Effect.None));
}
