// Copyright 2015  The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('image.collections.extension.domextractor.DocumentVideo');

goog.require('image.collections.extension.domextractor.DocumentFeature');
goog.require('image.collections.extension.domextractor.DomUtils');

goog.scope(function() {
var DocumentFeature = image.collections.extension.domextractor.DocumentFeature;



/**
 * A class representing a salient video in an HTML document.
 * @param {number} relevance
 * @param {string} url
 * @param {!image.collections.extension.domextractor.Size} size
 * @extends {DocumentFeature}
 * @constructor
 * @suppress {undefinedNames}
 */
image.collections.extension.domextractor.DocumentVideo =
    function(relevance, url, size) {
  DocumentVideo.base(this, 'constructor', relevance);

  /** @private {string} Absolute video url. */
  this.url_ = url;

  /**
   * @private {!image.collections.extension.domextractor.Size} Video resolution
   *     in pixels
   */
  this.size_ = size;
};
image.collections.extension.domextractor.DomUtils.inherits(
    image.collections.extension.domextractor.DocumentVideo, DocumentFeature);
var DocumentVideo = image.collections.extension.domextractor.DocumentVideo;


/** @enum {string} */
DocumentVideo.CustomAttribute = {
  WIDTH: 'data-google-stars-video-width',
  HEIGHT: 'data-google-stars-video-height'
};


/**
 * Returns the absolute video url.
 * @return {string}
 */
DocumentVideo.prototype.getUrl = function() {
  return this.url_;
};


/**
 * Returns the video resolution in pixels.
 * @return {!image.collections.extension.domextractor.Size}
 */
DocumentVideo.prototype.getSize = function() {
  return this.size_;
};
});  // goog.scope
