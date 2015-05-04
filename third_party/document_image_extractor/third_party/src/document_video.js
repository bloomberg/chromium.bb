goog.provide('image.collections.extension.DocumentVideo');

goog.require('image.collections.extension.DocumentFeature');

goog.scope(function() {
var DocumentFeature = image.collections.extension.DocumentFeature;



/**
 * A class representing a salient video in an HTML document.
 * @param {number} relevance
 * @param {string} url
 * @param {!goog.math.Size} size
 * @extends {DocumentFeature}
 * @constructor
 */
image.collections.extension.DocumentVideo = function(relevance, url, size) {
  DocumentVideo.base(this, 'constructor', relevance);

  /** @private {string} Absolute video url. */
  this.url_ = url;

  /** @private {!goog.math.Size} Video resolution in pixels */
  this.size_ = size;
};
goog.inherits(image.collections.extension.DocumentVideo, DocumentFeature);
var DocumentVideo = image.collections.extension.DocumentVideo;


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
 * @return {!goog.math.Size}
 */
DocumentVideo.prototype.getSize = function() {
  return this.size_;
};
});  // goog.scope
