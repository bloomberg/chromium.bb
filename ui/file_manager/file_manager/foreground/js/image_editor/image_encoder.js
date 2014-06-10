// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * A namespace class for image encoding functions. All methods are static.
 */
function ImageEncoder() {}

/**
 * @type {Array.<Object>}
 */
ImageEncoder.metadataEncoders = {};

/**
 * @param {function(new:ImageEncoder.MetadataEncoder)} constructor
 *     // TODO(JSDOC).
 * @param {string} mimeType  // TODO(JSDOC).
 */
ImageEncoder.registerMetadataEncoder = function(constructor, mimeType) {
  ImageEncoder.metadataEncoders[mimeType] = constructor;
};

/**
 * Create a metadata encoder.
 *
 * The encoder will own and modify a copy of the original metadata.
 *
 * @param {Object} metadata Original metadata.
 * @return {ImageEncoder.MetadataEncoder} Created metadata encoder.
 */
ImageEncoder.createMetadataEncoder = function(metadata) {
  var constructor =
      (metadata && ImageEncoder.metadataEncoders[metadata.mimeType]) ||
      ImageEncoder.MetadataEncoder;
  return new constructor(metadata);
};


/**
 * Create a metadata encoder object holding a copy of metadata
 * modified according to the properties of the supplied image.
 *
 * @param {Object} metadata Original metadata.
 * @param {HTMLCanvasElement} canvas Canvas to use for metadata.
 * @param {number} quality Encoding quality (defaults to 1).
 * @return {ImageEncoder.MetadataEncoder} Encoder with encoded metadata.
 */
ImageEncoder.encodeMetadata = function(metadata, canvas, quality) {
  var encoder = ImageEncoder.createMetadataEncoder(metadata);
  encoder.setImageData(canvas);
  encoder.setThumbnailData(ImageEncoder.createThumbnail(canvas), quality || 1);
  return encoder;
};


/**
 * Return a blob with the encoded image with metadata inserted.
 * @param {HTMLCanvasElement} canvas The canvas with the image to be encoded.
 * @param {ImageEncoder.MetadataEncoder} metadataEncoder Encoder to use.
 * @param {number} quality (0..1], Encoding quality, defaults to 0.9.
 * @return {Blob} encoded data.
 */
ImageEncoder.getBlob = function(canvas, metadataEncoder, quality) {
  // Contrary to what one might think 1.0 is not a good default. Opening and
  // saving an typical photo taken with consumer camera increases its file size
  // by 50-100%.
  // Experiments show that 0.9 is much better. It shrinks some photos a bit,
  // keeps others about the same size, but does not visibly lower the quality.
  quality = quality || 0.9;

  ImageUtil.trace.resetTimer('dataurl');
  // WebKit does not support canvas.toBlob yet so canvas.toDataURL is
  // the only way to use the Chrome built-in image encoder.
  var dataURL =
      canvas.toDataURL(metadataEncoder.getMetadata().mimeType, quality);
  ImageUtil.trace.reportTimer('dataurl');

  var encodedImage = ImageEncoder.decodeDataURL(dataURL);

  var encodedMetadata = metadataEncoder.encode();

  var slices = [];

  // TODO(kaznacheev): refactor |stringToArrayBuffer| and |encode| to return
  // arrays instead of array buffers.
  function appendSlice(arrayBuffer) {
    slices.push(new DataView(arrayBuffer));
  }

  ImageUtil.trace.resetTimer('blob');
  if (encodedMetadata.byteLength != 0) {
    var metadataRange = metadataEncoder.findInsertionRange(encodedImage);
    appendSlice(ImageEncoder.stringToArrayBuffer(
        encodedImage, 0, metadataRange.from));

    appendSlice(metadataEncoder.encode());

    appendSlice(ImageEncoder.stringToArrayBuffer(
        encodedImage, metadataRange.to, encodedImage.length));
  } else {
    appendSlice(ImageEncoder.stringToArrayBuffer(
        encodedImage, 0, encodedImage.length));
  }
  var blob = new Blob(slices, {type: metadataEncoder.getMetadata().mimeType});
  ImageUtil.trace.reportTimer('blob');
  return blob;
};

/**
 * Decode a dataURL into a binary string containing the encoded image.
 *
 * Why return a string? Calling atob and having the rest of the code deal
 * with a string is several times faster than decoding base64 in Javascript.
 *
 * @param {string} dataURL Data URL to decode.
 * @return {string} A binary string (char codes are the actual byte values).
 */
ImageEncoder.decodeDataURL = function(dataURL) {
  // Skip the prefix ('data:image/<type>;base64,')
  var base64string = dataURL.substring(dataURL.indexOf(',') + 1);
  return atob(base64string);
};

/**
 * Return a thumbnail for an image.
 * @param {HTMLCanvasElement} canvas Original image.
 * @param {number=} opt_shrinkage Thumbnail should be at least this much smaller
 *     than the original image (in each dimension).
 * @return {HTMLCanvasElement} Thumbnail canvas.
 */
ImageEncoder.createThumbnail = function(canvas, opt_shrinkage) {
  var MAX_THUMBNAIL_DIMENSION = 320;

  opt_shrinkage = Math.max(opt_shrinkage || 4,
                       canvas.width / MAX_THUMBNAIL_DIMENSION,
                       canvas.height / MAX_THUMBNAIL_DIMENSION);

  var thumbnailCanvas = canvas.ownerDocument.createElement('canvas');
  thumbnailCanvas.width = Math.round(canvas.width / opt_shrinkage);
  thumbnailCanvas.height = Math.round(canvas.height / opt_shrinkage);

  var context = thumbnailCanvas.getContext('2d');
  context.drawImage(canvas,
      0, 0, canvas.width, canvas.height,
      0, 0, thumbnailCanvas.width, thumbnailCanvas.height);

  return thumbnailCanvas;
};

/**
 * TODO(JSDOC)
 * @param {string} string  // TODO(JSDOC).
 * @param {number} from  // TODO(JSDOC).
 * @param {number} to  // TODO(JSDOC).
 * @return {ArrayBuffer}  // TODO(JSDOC).
 */
ImageEncoder.stringToArrayBuffer = function(string, from, to) {
  var size = to - from;
  var array = new Uint8Array(size);
  for (var i = 0; i != size; i++) {
    array[i] = string.charCodeAt(from + i);
  }
  return array.buffer;
};

/**
 * A base class for a metadata encoder.
 *
 * Serves as a default metadata encoder for images that none of the metadata
 * parsers recognized.
 *
 * @param {Object} original_metadata Starting metadata.
 * @constructor
 */
ImageEncoder.MetadataEncoder = function(original_metadata) {
  this.metadata_ = MetadataCache.cloneMetadata(original_metadata) || {};
  if (this.metadata_.mimeType != 'image/jpeg') {
    // Chrome can only encode JPEG and PNG. Force PNG mime type so that we
    // can save to file and generate a thumbnail.
    this.metadata_.mimeType = 'image/png';
  }
};

/**
 * TODO(JSDOC)
 * @return {Object}   // TODO(JSDOC).
 */
ImageEncoder.MetadataEncoder.prototype.getMetadata = function() {
  return this.metadata_;
};

/**
 * @param {HTMLCanvasElement|Object} canvas Canvas or or anything with
 *                                          width and height properties.
 */
ImageEncoder.MetadataEncoder.prototype.setImageData = function(canvas) {
  this.metadata_.width = canvas.width;
  this.metadata_.height = canvas.height;
};

/**
 * @param {HTMLCanvasElement} canvas Canvas to use as thumbnail.
 * @param {number} quality Thumbnail quality.
 */
ImageEncoder.MetadataEncoder.prototype.setThumbnailData =
    function(canvas, quality) {
  this.metadata_.thumbnailURL =
      canvas.toDataURL(this.metadata_.mimeType, quality);
  delete this.metadata_.thumbnailTransform;
};

/**
 * Return a range where the metadata is (or should be) located.
 * @param {string} encodedImage // TODO(JSDOC).
 * @return {Object} An object with from and to properties.
 */
ImageEncoder.MetadataEncoder.prototype.
    findInsertionRange = function(encodedImage) { return {from: 0, to: 0}; };

/**
 * Return serialized metadata ready to write to an image file.
 * The return type is optimized for passing to Blob.append.
 * @return {ArrayBuffer} // TODO(JSDOC).
 */
ImageEncoder.MetadataEncoder.prototype.encode = function() {
  return new Uint8Array(0).buffer;
};
