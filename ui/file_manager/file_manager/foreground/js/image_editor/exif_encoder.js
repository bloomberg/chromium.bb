// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// TODO:(kaznacheev) Share the EXIF constants with exif_parser.js
var EXIF_MARK_SOS = 0xffda;  // Start of "stream" (the actual image data).
var EXIF_MARK_SOI = 0xffd8;  // Start of image data.
var EXIF_MARK_EOI = 0xffd9;  // End of image data.

var EXIF_MARK_APP0 = 0xffe0;  // APP0 block, most commonly JFIF data.
var EXIF_MARK_EXIF = 0xffe1;  // Start of exif block.

var EXIF_ALIGN_LITTLE = 0x4949;  // Indicates little endian exif data.
var EXIF_ALIGN_BIG = 0x4d4d;  // Indicates big endian exif data.

var EXIF_TAG_TIFF = 0x002a;  // First directory containing TIFF data.
var EXIF_TAG_GPSDATA = 0x8825;  // Pointer from TIFF to the GPS directory.
var EXIF_TAG_EXIFDATA = 0x8769;  // Pointer from TIFF to the EXIF IFD.

var EXIF_TAG_JPG_THUMB_OFFSET = 0x0201;  // Pointer from TIFF to thumbnail.
var EXIF_TAG_JPG_THUMB_LENGTH = 0x0202;  // Length of thumbnail data.

var EXIF_TAG_IMAGE_WIDTH = 0x0100;
var EXIF_TAG_IMAGE_HEIGHT = 0x0101;

var EXIF_TAG_ORIENTATION = 0x0112;
var EXIF_TAG_X_DIMENSION = 0xA002;
var EXIF_TAG_Y_DIMENSION = 0xA003;

/**
 * The Exif metadata encoder.
 * Uses the metadata format as defined by ExifParser.
 * @param {Object} original_metadata Metadata to encode.
 * @constructor
 * @extends {ImageEncoder.MetadataEncoder}
 */
function ExifEncoder(original_metadata) {
  ImageEncoder.MetadataEncoder.apply(this, arguments);

  this.ifd_ = this.metadata_.ifd;
  if (!this.ifd_)
    this.ifd_ = this.metadata_.ifd = {};
}

ExifEncoder.prototype = {__proto__: ImageEncoder.MetadataEncoder.prototype};

ImageEncoder.registerMetadataEncoder(ExifEncoder, 'image/jpeg');

/**
 * @param {HTMLCanvasElement|Object} canvas Canvas or anything with
 *                                          width and height properties.
 */
ExifEncoder.prototype.setImageData = function(canvas) {
  var image = this.ifd_.image;
  if (!image)
    image = this.ifd_.image = {};

  // Only update width/height in this directory if they are present.
  if (image[EXIF_TAG_IMAGE_WIDTH] && image[EXIF_TAG_IMAGE_HEIGHT]) {
    image[EXIF_TAG_IMAGE_WIDTH].value = canvas.width;
    image[EXIF_TAG_IMAGE_HEIGHT].value = canvas.height;
  }

  var exif = this.ifd_.exif;
  if (!exif)
    exif = this.ifd_.exif = {};
  ExifEncoder.findOrCreateTag(image, EXIF_TAG_EXIFDATA);
  ExifEncoder.findOrCreateTag(exif, EXIF_TAG_X_DIMENSION).value = canvas.width;
  ExifEncoder.findOrCreateTag(exif, EXIF_TAG_Y_DIMENSION).value = canvas.height;

  this.metadata_.width = canvas.width;
  this.metadata_.height = canvas.height;

  // Always save in default orientation.
  delete this.metadata_.imageTransform;
  ExifEncoder.findOrCreateTag(image, EXIF_TAG_ORIENTATION).value = 1;
};


/**
 * @param {HTMLCanvasElement} canvas Thumbnail canvas.
 * @param {number} quality (0..1] Thumbnail encoding quality.
 */
ExifEncoder.prototype.setThumbnailData = function(canvas, quality) {
  // Empirical formula with reasonable behavior:
  // 10K for 1Mpix, 30K for 5Mpix, 50K for 9Mpix and up.
  var pixelCount = this.metadata_.width * this.metadata_.height;
  var maxEncodedSize = 5000 * Math.min(10, 1 + pixelCount / 1000000);

  var DATA_URL_PREFIX = 'data:' + this.mimeType + ';base64,';
  var BASE64_BLOAT = 4 / 3;
  var maxDataURLLength =
      DATA_URL_PREFIX.length + Math.ceil(maxEncodedSize * BASE64_BLOAT);

  for (;; quality *= 0.8) {
    ImageEncoder.MetadataEncoder.prototype.setThumbnailData.call(
        this, canvas, quality);
    if (this.metadata_.thumbnailURL.length <= maxDataURLLength || quality < 0.2)
      break;
  }

  if (this.metadata_.thumbnailURL.length <= maxDataURLLength) {
    var thumbnail = this.ifd_.thumbnail;
    if (!thumbnail)
      thumbnail = this.ifd_.thumbnail = {};

    ExifEncoder.findOrCreateTag(thumbnail, EXIF_TAG_IMAGE_WIDTH).value =
        canvas.width;

    ExifEncoder.findOrCreateTag(thumbnail, EXIF_TAG_IMAGE_HEIGHT).value =
        canvas.height;

    // The values for these tags will be set in ExifWriter.encode.
    ExifEncoder.findOrCreateTag(thumbnail, EXIF_TAG_JPG_THUMB_OFFSET);
    ExifEncoder.findOrCreateTag(thumbnail, EXIF_TAG_JPG_THUMB_LENGTH);

    // Always save in default orientation.
    ExifEncoder.findOrCreateTag(thumbnail, EXIF_TAG_ORIENTATION).value = 1;
  } else {
    console.warn(
       'Thumbnail URL too long: ' + this.metadata_.thumbnailURL.length);
    // Delete thumbnail ifd so that it is not written out to a file, but
    // keep thumbnailURL for display purposes.
    if (this.ifd_.thumbnail) {
      delete this.ifd_.thumbnail;
    }
  }
  delete this.metadata_.thumbnailTransform;
};

/**
 * Return a range where the metadata is (or should be) located.
 * @param {string} encodedImage Raw image data to look for metadata.
 * @return {Object} An object with from and to properties.
 */
ExifEncoder.prototype.findInsertionRange = function(encodedImage) {
  function getWord(pos) {
    if (pos + 2 > encodedImage.length)
      throw 'Reading past the buffer end @' + pos;
    return encodedImage.charCodeAt(pos) << 8 | encodedImage.charCodeAt(pos + 1);
  }

  if (getWord(0) != EXIF_MARK_SOI)
    throw new Error('Jpeg data starts from 0x' + getWord(0).toString(16));

  var sectionStart = 2;

  // Default: an empty range right after SOI.
  // Will be returned in absence of APP0 or Exif sections.
  var range = {from: sectionStart, to: sectionStart};

  for (;;) {
    var tag = getWord(sectionStart);

    if (tag == EXIF_MARK_SOS)
      break;

    var nextSectionStart = sectionStart + 2 + getWord(sectionStart + 2);
    if (nextSectionStart <= sectionStart ||
        nextSectionStart > encodedImage.length)
      throw new Error('Invalid section size in jpeg data');

    if (tag == EXIF_MARK_APP0) {
      // Assert that we have not seen the Exif section yet.
      if (range.from != range.to)
        throw new Error('APP0 section found after EXIF section');
      // An empty range right after the APP0 segment.
      range.from = range.to = nextSectionStart;
    } else if (tag == EXIF_MARK_EXIF) {
      // A range containing the existing EXIF section.
      range.from = sectionStart;
      range.to = nextSectionStart;
    }
    sectionStart = nextSectionStart;
  }

  return range;
};

/**
 * @return {ArrayBuffer} serialized metadata ready to write to an image file.
 */
ExifEncoder.prototype.encode = function() {
  var HEADER_SIZE = 10;

  // Allocate the largest theoretically possible size.
  var bytes = new Uint8Array(0x10000);

  // Serialize header
  var hw = new ByteWriter(bytes.buffer, 0, HEADER_SIZE);
  hw.writeScalar(EXIF_MARK_EXIF, 2);
  hw.forward('size', 2);
  hw.writeString('Exif\0\0');  // Magic string.

  // First serialize the content of the exif section.
  // Use a ByteWriter starting at HEADER_SIZE offset so that tell() positions
  // can be directly mapped to offsets as encoded in the dictionaries.
  var bw = new ByteWriter(bytes.buffer, HEADER_SIZE);

  if (this.metadata_.littleEndian) {
    bw.setByteOrder(ByteWriter.LITTLE_ENDIAN);
    bw.writeScalar(EXIF_ALIGN_LITTLE, 2);
  } else {
    bw.setByteOrder(ByteWriter.BIG_ENDIAN);
    bw.writeScalar(EXIF_ALIGN_BIG, 2);
  }

  bw.writeScalar(EXIF_TAG_TIFF, 2);

  bw.forward('image-dir', 4);  // The pointer should point right after itself.
  bw.resolveOffset('image-dir');

  ExifEncoder.encodeDirectory(bw, this.ifd_.image,
      [EXIF_TAG_EXIFDATA, EXIF_TAG_GPSDATA], 'thumb-dir');

  if (this.ifd_.exif) {
    bw.resolveOffset(EXIF_TAG_EXIFDATA);
    ExifEncoder.encodeDirectory(bw, this.ifd_.exif);
  } else {
    if (EXIF_TAG_EXIFDATA in this.ifd_.image)
      throw new Error('Corrupt exif dictionary reference');
  }

  if (this.ifd_.gps) {
    bw.resolveOffset(EXIF_TAG_GPSDATA);
    ExifEncoder.encodeDirectory(bw, this.ifd_.gps);
  } else {
    if (EXIF_TAG_GPSDATA in this.ifd_.image)
      throw new Error('Missing gps dictionary reference');
  }

  if (this.ifd_.thumbnail) {
    bw.resolveOffset('thumb-dir');
    ExifEncoder.encodeDirectory(
        bw,
        this.ifd_.thumbnail,
        [EXIF_TAG_JPG_THUMB_OFFSET, EXIF_TAG_JPG_THUMB_LENGTH]);

    var thumbnailDecoded =
        ImageEncoder.decodeDataURL(this.metadata_.thumbnailURL);
    bw.resolveOffset(EXIF_TAG_JPG_THUMB_OFFSET);
    bw.resolve(EXIF_TAG_JPG_THUMB_LENGTH, thumbnailDecoded.length);
    bw.writeString(thumbnailDecoded);
  } else {
    bw.resolve('thumb-dir', 0);
  }

  bw.checkResolved();

  var totalSize = HEADER_SIZE + bw.tell();
  hw.resolve('size', totalSize - 2);  // The marker is excluded.
  hw.checkResolved();

  var subarray = new Uint8Array(totalSize);
  for (var i = 0; i != totalSize; i++) {
    subarray[i] = bytes[i];
  }
  return subarray.buffer;
};

/*
 * Static methods.
 */

/**
 * Write the contents of an IFD directory.
 * @param {ByteWriter} bw ByteWriter to use.
 * @param {Object} directory A directory map as created by ExifParser.
 * @param {Array} resolveLater An array of tag ids for which the values will be
 *                resolved later.
 * @param {string} nextDirPointer A forward key for the pointer to the next
 *                 directory. If omitted the pointer is set to 0.
 */
ExifEncoder.encodeDirectory = function(
    bw, directory, resolveLater, nextDirPointer) {

  var longValues = [];

  bw.forward('dir-count', 2);
  var count = 0;

  for (var key in directory) {
    var tag = directory[key];
    bw.writeScalar(tag.id, 2);
    bw.writeScalar(tag.format, 2);
    bw.writeScalar(tag.componentCount, 4);

    var width = ExifEncoder.getComponentWidth(tag) * tag.componentCount;

    if (resolveLater && (resolveLater.indexOf(tag.id) >= 0)) {
      // The actual value depends on further computations.
      if (tag.componentCount != 1 || width > 4)
        throw new Error('Cannot forward the pointer for ' + tag.id);
      bw.forward(tag.id, width);
    } else if (width <= 4) {
      // The value fits into 4 bytes, write it immediately.
      ExifEncoder.writeValue(bw, tag);
    } else {
      // The value does not fit, forward the 4 byte offset to the actual value.
      width = 4;
      bw.forward(tag.id, width);
      longValues.push(tag);
    }
    bw.skip(4 - width);  // Align so that the value take up exactly 4 bytes.
    count++;
  }

  bw.resolve('dir-count', count);

  if (nextDirPointer) {
    bw.forward(nextDirPointer, 4);
  } else {
    bw.writeScalar(0, 4);
  }

  // Write out the long values and resolve pointers.
  for (var i = 0; i != longValues.length; i++) {
    var longValue = longValues[i];
    bw.resolveOffset(longValue.id);
    ExifEncoder.writeValue(bw, longValue);
  }
};

/**
 * @param {{format:number, id:number}} tag EXIF tag object.
 * @return {number} Width in bytes of the data unit associated with this tag.
 * TODO(kaznacheev): Share with ExifParser?
 */
ExifEncoder.getComponentWidth = function(tag) {
  switch (tag.format) {
    case 1:  // Byte
    case 2:  // String
    case 7:  // Undefined
      return 1;

    case 3:  // Short
      return 2;

    case 4:  // Long
    case 9:  // Signed Long
      return 4;

    case 5:  // Rational
    case 10:  // Signed Rational
      return 8;

    default:  // ???
      console.warn('Unknown tag format 0x' +
          Number(tag.id).toString(16) + ': ' + tag.format);
      return 4;
  }
};

/**
 * Writes out the tag value.
 * @param {ByteWriter} bw Writer to use.
 * @param {Object} tag Tag, which value to write.
 */
ExifEncoder.writeValue = function(bw, tag) {
  if (tag.format == 2) {  // String
    if (tag.componentCount != tag.value.length) {
      throw new Error(
          'String size mismatch for 0x' + Number(tag.id).toString(16));
    }
    bw.writeString(tag.value);
  } else {  // Scalar or rational
    var width = ExifEncoder.getComponentWidth(tag);

    var writeComponent = function(value, signed) {
      if (width == 8) {
        bw.writeScalar(value[0], 4, signed);
        bw.writeScalar(value[1], 4, signed);
      } else {
        bw.writeScalar(value, width, signed);
      }
    };

    var signed = (tag.format == 9 || tag.format == 10);
    if (tag.componentCount == 1) {
       writeComponent(tag.value, signed);
    } else {
      for (var i = 0; i != tag.componentCount; i++) {
        writeComponent(tag.value[i], signed);
      }
    }
  }
};

/**
 * @param {{Object.<number,Object>}} directory EXIF directory.
 * @param {number} id Tag id.
 * @param {number} format Tag format
 *                        (used in {@link ExifEncoder#getComponentWidth}).
 * @param {number} componentCount Number of components in this tag.
 * @return {{id:number, format:number, componentCount:number}}
 *     Tag found or created.
 */
ExifEncoder.findOrCreateTag = function(directory, id, format, componentCount) {
  if (!(id in directory)) {
    directory[id] = {
      id: id,
      format: format || 3,  // Short
      componentCount: componentCount || 1
    };
  }
  return directory[id];
};

/**
 * ByteWriter class.
 * @param {ArrayBuffer} arrayBuffer Underlying buffer to use.
 * @param {number} offset Offset at which to start writing.
 * @param {number} length Maximum length to use.
 * @class
 * @constructor
 */
function ByteWriter(arrayBuffer, offset, length) {
  length = length || (arrayBuffer.byteLength - offset);
  this.view_ = new DataView(arrayBuffer, offset, length);
  this.littleEndian_ = false;
  this.pos_ = 0;
  this.forwards_ = {};
}

/**
 * Little endian byte order.
 * @type {number}
 */
ByteWriter.LITTLE_ENDIAN = 0;

/**
 * Bug endian byte order.
 * @type {number}
 */
ByteWriter.BIG_ENDIAN = 1;

/**
 * Set the byte ordering for future writes.
 * @param {number} order ByteOrder to use {ByteWriter.LITTLE_ENDIAN}
 *   or {ByteWriter.BIG_ENDIAN}.
 */
ByteWriter.prototype.setByteOrder = function(order) {
  this.littleEndian_ = (order == ByteWriter.LITTLE_ENDIAN);
};

/**
 * @return {number} the current write position.
 */
ByteWriter.prototype.tell = function() { return this.pos_ };

/**
 * Skips desired amount of bytes in output stream.
 * @param {number} count Byte count to skip.
 */
ByteWriter.prototype.skip = function(count) {
  this.validateWrite(count);
  this.pos_ += count;
};

/**
 * Check if the buffer has enough room to read 'width' bytes. Throws an error
 * if it has not.
 * @param {number} width Amount of bytes to check.
 */
ByteWriter.prototype.validateWrite = function(width) {
  if (this.pos_ + width > this.view_.byteLength)
    throw new Error('Writing past the end of the buffer');
};

/**
 * Writes scalar value to output stream.
 * @param {number} value Value to write.
 * @param {number} width Desired width of written value.
 * @param {boolean=} opt_signed True if value represents signed number.
 */
ByteWriter.prototype.writeScalar = function(value, width, opt_signed) {
  var method;
// The below switch is so verbose for two reasons:
// 1. V8 is faster on method names which are 'symbols'.
// 2. Method names are discoverable by full text search.
  switch (width) {
    case 1:
      method = opt_signed ? 'setInt8' : 'setUint8';
      break;

    case 2:
      method = opt_signed ? 'setInt16' : 'setUint16';
      break;

    case 4:
      method = opt_signed ? 'setInt32' : 'setUint32';
      break;

    case 8:
      method = opt_signed ? 'setInt64' : 'setUint64';
      break;

    default:
      throw new Error('Invalid width: ' + width);
      break;
  }

  this.validateWrite(width);
  this.view_[method](this.pos_, value, this.littleEndian_);
  this.pos_ += width;
};

/**
 * Writes string.
 * @param {string} str String to write.
 */
ByteWriter.prototype.writeString = function(str) {
  this.validateWrite(str.length);
  for (var i = 0; i != str.length; i++) {
    this.view_.setUint8(this.pos_++, str.charCodeAt(i));
  }
};

/**
 * Allocate the space for 'width' bytes for the value that will be set later.
 * To be followed by a 'resolve' call with the same key.
 * @param {string} key A key to identify the value.
 * @param {number} width Width of the value in bytes.
 */
ByteWriter.prototype.forward = function(key, width) {
  if (key in this.forwards_)
    throw new Error('Duplicate forward key ' + key);
  this.validateWrite(width);
  this.forwards_[key] = {
    pos: this.pos_,
    width: width
  };
  this.pos_ += width;
};

/**
 * Set the value previously allocated with a 'forward' call.
 * @param {string} key A key to identify the value.
 * @param {number} value value to write in pre-allocated space.
 */
ByteWriter.prototype.resolve = function(key, value) {
  if (!(key in this.forwards_))
    throw new Error('Undeclared forward key ' + key.toString(16));
  var forward = this.forwards_[key];
  var curPos = this.pos_;
  this.pos_ = forward.pos;
  this.writeScalar(value, forward.width);
  this.pos_ = curPos;
  delete this.forwards_[key];
};

/**
 * A shortcut to resolve the value to the current write position.
 * @param {string} key A key to identify pre-allocated position.
 */
ByteWriter.prototype.resolveOffset = function(key) {
  this.resolve(key, this.tell());
};

/**
 * Check if every forward has been resolved, throw and error if not.
 */
ByteWriter.prototype.checkResolved = function() {
  for (var key in this.forwards_) {
    throw new Error('Unresolved forward pointer ' + key.toString(16));
  }
};
