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
  this.testImageDataUrl_ = null;
}

/**
 * @type {string} Data url of an image.
 */
MockThumbnailLoader.testImageDataUrl_ = null;

/**
 * Set data url of an image which is returned for testing.
 *
 * @param {string} dataUrl Data url of an image.
 */
MockThumbnailLoader.setTestImageDataUrl = function(dataUrl) {
  MockThumbnailLoader.testImageDataUrl_ = dataUrl;
};

/**
 * Load an image. (This mock implementation does not attach an image to the
 * box).
 *
 * @param {Element} box Box.
 * @param {ThumbnailLoader.FillMode} fillMode Fill mode.
 * @param {ThumbnailLoader.OptimizationMode=} opt_optimizationMode Optimization.
 * @param {function(Image, Object)=} opt_onSuccess Success callback.
 * @param {function()=} opt_onError Error callback.
 * @param {function()=} opt_onGeneric Callback which is called when generic
 *     image is used.
 */
MockThumbnailLoader.prototype.load = function(
    box, fillMode, opt_optimizationMode, opt_onSuccess, opt_onError,
    opt_onGeneric) {
  if (opt_onSuccess && MockThumbnailLoader.testImageDataUrl_) {
    var image = new Image();
    image.onload = function() {
      opt_onSuccess(image, null);
    };
    image.src = MockThumbnailLoader.testImageDataUrl_;
  }
};
