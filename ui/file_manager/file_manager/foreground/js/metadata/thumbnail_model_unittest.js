// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var imageEntry = {
  name: 'image.jpg',
  toURL: function() { return 'filesystem://A'; }
};

var nonImageEntry = {
  name: 'note.txt',
  toURL: function() { return 'filesystem://B'; }
};

var fileSystemMetadata;
var contentMetadata;
var thumbnailModel;

function setUp() {
  fileSystemMetadata = {
    modificationTime: new Date(2015, 0, 1),
    present: true,
    dirty: false,
    thumbnailUrl: 'EXTERNAL_THUMBNAIL_URL',
    customIconUrl: 'CUSTOM_ICON_URL'
  };
  contentMetadata = {
    contentThumbnailUrl: 'CONTENT_THUMBNAIL_URL',
    contentThumbnailTransform: 'CONTENT_THUMBNAIL_TRANSFORM',
    contentImageTransform: 'CONTENT_IMAGE_TRANSFORM'
  };
  thumbnailModel = new ThumbnailModel(
      {get: function() { return Promise.resolve([fileSystemMetadata]); }},
      {get: function() { return Promise.resolve([contentMetadata]); }});
}

function testThumbnailModelGetBasic(callback) {
  reportPromise(thumbnailModel.get([imageEntry]).then(function(results) {
    assertEquals(1, results.length);
    assertEquals(
        new Date(2015, 0, 1).toString(),
        results[0].filesystem.modificationTime.toString());
    assertEquals('EXTERNAL_THUMBNAIL_URL', results[0].external.thumbnailUrl);
    assertEquals('CUSTOM_ICON_URL', results[0].external.customIconUrl);
    assertFalse(results[0].external.dirty);
    assertEquals('CONTENT_THUMBNAIL_URL', results[0].thumbnail.url);
    assertEquals('CONTENT_THUMBNAIL_TRANSFORM', results[0].thumbnail.transform);
    assertEquals('CONTENT_IMAGE_TRANSFORM', results[0].media.imageTransform);
  }), callback);
}

function testThumbnailModelGetNotPresent(callback) {
  fileSystemMetadata.present = false;
  reportPromise(thumbnailModel.get([imageEntry]).then(function(results) {
    assertEquals(1, results.length);
    assertEquals(
        new Date(2015, 0, 1).toString(),
        results[0].filesystem.modificationTime.toString());
    assertEquals('EXTERNAL_THUMBNAIL_URL', results[0].external.thumbnailUrl);
    assertEquals('CUSTOM_ICON_URL', results[0].external.customIconUrl);
    assertFalse(results[0].external.dirty);
    assertEquals(undefined, results[0].thumbnail.url);
    assertEquals(undefined, results[0].thumbnail.transform);
    assertEquals(undefined, results[0].media.imageTransform);
  }), callback);
}

function testThumbnailModelGetNonImage(callback) {
  reportPromise(thumbnailModel.get([nonImageEntry]).then(function(results) {
    assertEquals(1, results.length);
    assertEquals(
        new Date(2015, 0, 1).toString(),
        results[0].filesystem.modificationTime.toString());
    assertEquals('EXTERNAL_THUMBNAIL_URL', results[0].external.thumbnailUrl);
    assertEquals('CUSTOM_ICON_URL', results[0].external.customIconUrl);
    assertFalse(results[0].external.dirty);
    assertEquals(undefined, results[0].thumbnail.url);
    assertEquals(undefined, results[0].thumbnail.transform);
    assertEquals(undefined, results[0].media.imageTransform);
  }), callback);
}
