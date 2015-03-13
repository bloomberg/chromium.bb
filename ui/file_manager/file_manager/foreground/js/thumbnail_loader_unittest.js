// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getLoadTarget(entry, metadata) {
  return new ThumbnailLoader(entry, null, metadata).getLoadTarget();
}

/**
 * Generates a data url of a sample image for testing.
 *
 * @param {number} width Width.
 * @param {number} height Height.
 * @return {string} Data url of a sample image.
 */
function generateSampleImageDataUrl(width, height) {
  var canvas = document.createElement('canvas');
  canvas.width = width;
  canvas.height = height;

  var context = canvas.getContext('2d');
  context.fillStyle = 'black';
  context.fillRect(0, 0, width / 2, height / 2);
  context.fillRect(width / 2, height / 2, width / 2, height / 2);

  return canvas.toDataURL('image/png');
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

  // Drive thumbnail is provided but the file is cached locally.
  assertEquals(
      ThumbnailLoader.LoadTarget.FILE_ENTRY,
      getLoadTarget(
          imageEntry, {external: {thumbnailUrl: 'url', present: true}}));

  // Drive thumbnail is provided and it is not an image file.
  assertEquals(
      ThumbnailLoader.LoadTarget.EXTERNAL_METADATA,
      getLoadTarget(
          pdfEntry, {external: {thumbnailUrl: 'url', present: true}}));
}

function testLoadAsDataUrlFromImageClient(callback) {
  ImageLoaderClient.getInstance = function() {
    return {
      load: function(url, callback, opt_option) {
        callback({
          status: 'success', data: 'imageDataUrl', width: 32, height: 32});
      }
    };
  };

  var fileSystem = new MockFileSystem('volume-id');
  var entry = new MockEntry(fileSystem, '/Test1.jpg');
  var thumbnailLoader = new ThumbnailLoader(entry);
  reportPromise(thumbnailLoader.loadAsDataUrl().then(function(result) {
    assertEquals('imageDataUrl', result.data);
  }), callback);
}

function testLoadAsDataUrlFromExifThumbnail(callback) {
  ImageLoaderClient.getInstance = function() {
    return {
      load: function(url, callback, opt_option) {
        // Assert that data url is passed.
        assertTrue(/^data:/i.test(url));
        callback({status: 'success', data: url, width: 32, height: 32});
      }
    };
  };

  var metadata = {
    thumbnail: {
      url: generateSampleImageDataUrl(32, 32)
    }
  };

  var fileSystem = new MockFileSystem('volume-id');
  var entry = new MockEntry(fileSystem, '/Test1.jpg');
  var thumbnailLoader = new ThumbnailLoader(entry, undefined, metadata);
  reportPromise(thumbnailLoader.loadAsDataUrl().then(function(result) {
    assertEquals(metadata.thumbnail.url, result.data);
  }), callback);
}

function testLoadAsDataUrlFromExifThumbnailRotate(callback) {
  ImageLoaderClient.getInstance = function() {
    return {
      load: function(url, callback, opt_option) {
        // Assert that data url is passed.
        assertTrue(/^data:/i.test(url));
        callback({status: 'success', data: url, width: 64, height: 32});
      }
    };
  };

  var metadata = {
    thumbnail: {
      url: generateSampleImageDataUrl(64, 32),
      transform: {
        rotate90: 1,
        scaleX: 1,
        scaleY: -1,
      }
    }
  };

  var fileSystem = new MockFileSystem('volume-id');
  var entry = new MockEntry(fileSystem, '/Test1.jpg');
  var thumbnailLoader = new ThumbnailLoader(entry, undefined, metadata);
  reportPromise(thumbnailLoader.loadAsDataUrl().then(function(result) {
    assertEquals(32, result.width);
    assertEquals(64, result.height);
    // For test image, transformed image should become equal to the following
    // generated sample image.
    assertEquals(generateSampleImageDataUrl(32, 64), result.data);
  }), callback);
}

function testLoadAsDataUrlFromExternal(callback) {
  var externalThumbnailUrl = 'https://external-thumbnail-url/';
  var externalThumbnailDataUrl = generateSampleImageDataUrl(32, 32);

  ImageLoaderClient.getInstance = function() {
    return {
      load: function(url, callback, opt_option) {
        assertEquals(externalThumbnailUrl, url);
        callback({status: 'success', data: externalThumbnailDataUrl,
          width: 32, height: 32});
      }
    };
  };

  var metadata = {
    external: {
      thumbnailUrl: externalThumbnailUrl
    }
  };

  var fileSystem = new MockFileSystem('volume-id');
  var entry = new MockEntry(fileSystem, '/Test1.jpg');
  var thumbnailLoader = new ThumbnailLoader(entry, undefined, metadata);
  reportPromise(thumbnailLoader.loadAsDataUrl().then(function(result) {
    assertEquals(externalThumbnailDataUrl, result.data);
  }), callback);
}
