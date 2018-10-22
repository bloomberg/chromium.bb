// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Mock thumbnail loader.
 *
 * @param {Entry} entry An entry.
 * @param {ThumbnailLoader.LoaderType=} opt_loaderType Loader type.
 * @param {Object=} opt_metadata Metadata.
 * @param {string=} opt_mediaType Media type.
 * @param {Array<ThumbnailLoader.LoadTarget>=} opt_loadTargets Load targets.
 * @param {number=} opt_priority Priority.
 */
function MockThumbnailLoader(entry, opt_loaderType, opt_metadata, opt_mediaType,
    opt_loadTargets, opt_priority) {
  this.entry_ = entry;
}

/**
 * Data url of test image.
 * @private {string}
 */
MockThumbnailLoader.testImageDataUrl = null;

/**
 * Width of test image.
 * @private {number}
 */
MockThumbnailLoader.testImageWidth = 0;

/**
 * Height of test image.
 * @private {number}
 */
MockThumbnailLoader.testImageHeight = 0;

/**
 * Error urls.
 * @private {Array<string>}
 */
MockThumbnailLoader.errorUrls = [];

/**
 * Loads thumbnail as data url.
 *
 * @return {!Promise<{data:string, width:number, height:number}>} A promise
 *     which is resolved with data url.
 */
MockThumbnailLoader.prototype.loadAsDataUrl = function() {
  if (MockThumbnailLoader.errorUrls.indexOf(this.entry_.toURL()) !== -1)
    throw new Error('Failed to load thumbnail.');

  return Promise.resolve({
    data: MockThumbnailLoader.testImageDataUrl,
    width: MockThumbnailLoader.testImageWidth,
    height: MockThumbnailLoader.testImageHeight
  });
};
