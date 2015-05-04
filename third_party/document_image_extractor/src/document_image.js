// Copyright 2015  The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('image.collections.extension.DocumentImage');

goog.require('goog.asserts');
goog.require('image.collections.extension.DocumentFeature');

goog.scope(function() {
var DocumentFeature = image.collections.extension.DocumentFeature;



/**
 * A class representing a salient image in an HTML document.
 * @param {number} relevance
 * @param {string} url
 * @param {!goog.math.Size=} opt_size Natural size of image or null if image
 *     size is not available in the DOM tree.
 * @param {!goog.math.Size=} opt_displaySize Size at which the image is being
 *     shown. One of opt_size or opt_displaySize MUST be specified.
 * @extends {DocumentFeature}
 * @constructor
 */
image.collections.extension.DocumentImage = function(
    relevance, url, opt_size, opt_displaySize) {
  DocumentImage.base(this, 'constructor', relevance);

  /** @private {string} Absolute image url. */
  this.url_ = url;

  /** @private {!goog.math.Size|undefined} Image resolution in pixels. */
  this.size_ = opt_size;

  /** @private {!goog.math.Size|undefined} Displayed image resolution
   *                                       in pixels.
   */
  this.displaySize_ = opt_displaySize;

  goog.asserts.assert(goog.isDef(opt_size) || goog.isDef(opt_displaySize));
};
goog.inherits(image.collections.extension.DocumentImage, DocumentFeature);
var DocumentImage = image.collections.extension.DocumentImage;


/**
 * These attributes needs to be set for images defined in <meta> and <link>
 * elements (e.g. OpenGraph images) so that they can be used as salient image
 * candidates. They are computed by the DomController class.
 * @enum {string}
 */
DocumentImage.CustomAttribute = {
  WIDTH: 'data-google-stars-image-width',
  HEIGHT: 'data-google-stars-image-height'
};


/**
 * Returns the absolute image url.
 * @return {string}
 */
DocumentImage.prototype.getUrl = function() {
  return this.url_;
};


/**
 * Returns the image resolution in pixels.
 * @return {!goog.math.Size|undefined}
 */
DocumentImage.prototype.getSize = function() {
  return this.size_;
};


/**
 * Returns the shown image resolution in pixels.
 * @return {!goog.math.Size|undefined}
 */
DocumentImage.prototype.getDisplaySize = function() {
  return this.displaySize_;
};
});  // goog.scope
