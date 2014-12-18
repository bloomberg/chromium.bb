// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/**
 * Test case for ordinal exif encoding and decoding.
 */
function testExifEncodeAndDecode() {
  var canvas = getSampleCanvas();
  var data = canvas.toDataURL('image/jpeg');

  var metadata = {
    media: {
      mimeType: 'image/jpeg',
      ifd: {
        image: {
          // Manufacture
          271: {
            id: 0x10f,
            format: 2,
            componentCount: 12,
            value: 'Manufacture'
          },
          // Device model
          272: {
            id: 0x110,
            format: 2,
            componentCount: 12,
            value: 'DeviceModel'
          }
        }
      }
    }
  };

  var encoder = ImageEncoder.encodeMetadata(metadata, canvas);

  // Assert that ExifEncoder is returned.
  assertTrue(encoder instanceof ExifEncoder);

  var encodedResult = encoder.encode();

  // Decode encoded exif data.
  var exifParser = new ExifParser(this);

  // Redirect .log and .vlog to console.log for debugging.
  exifParser.log = function(arg) { console.log(arg); };
  exifParser.vlog = function(arg) { console.log(arg); };

  var parsedMetadata = {};
  var byteReader = new ByteReader(encodedResult);
  byteReader.readString(2 + 2); // Skip marker and size.
  exifParser.parseExifSection(parsedMetadata, encodedResult, byteReader);

  // Check ifd.image.
  assertEquals(1, parsedMetadata.ifd.image[0x112].value); // Orientation

  // Check ifd.exif.
  assertEquals(1920, parsedMetadata.ifd.exif[0xA002].value); // PixelXDimension
  assertEquals(1080, parsedMetadata.ifd.exif[0xA003].value); // PixelYDimension

  // TODO(yawano) Change exif_encoder to preserve these fields.
  // These fileds are not encoded.
  assertEquals(undefined, parsedMetadata.ifd.image[0x10F]); // Manufacture
  assertEquals(undefined, parsedMetadata.ifd.image[0x110]); // Device model

  // Thumbnail image
  assert(parsedMetadata.thumbnailTransform);
  assert(parsedMetadata.thumbnailURL);
}
