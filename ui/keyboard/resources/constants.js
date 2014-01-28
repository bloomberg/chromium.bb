// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Aspect ratio of keyboard.
 * @type {number}
 */
var ASPECT_RATIO = 4.5;

var RowAlignment = {
  STRETCH: "stretch",
  LEFT: "left",
  RIGHT: "right",
  CENTER: "center"
}

/**
 * Ratio of key height and font size.
 * @type {number}
 */
var FONT_SIZE_RATIO = 3;

/**
 * The number of rows in each keyset.
 * @type {number}
 */
// TODO(bshe): The number of rows should equal to the number of kb-row elements
// in kb-keyset. Remove this variable once figure out how to calculate the
// number from keysets.
var ROW_LENGTH = 4;

/**
 * The enumeration of swipe directions.
 * @const
 * @type {Enum}
 */
var SWIPE_DIRECTION = {
  RIGHT: 0x1,
  LEFT: 0x2,
  UP: 0x4,
  DOWN: 0x8
};

/**
 * The default weight of a key in the X direction.
 * @type {number}
 */
var DEFAULT_KEY_WEIGHT_X = 100;

/**
 * The default weight of a key in the Y direction.
 * @type {number}
 */
var DEFAULT_KEY_WEIGHT_Y = 60;

/**
 * The top padding on each key.
 * @type {number}
 */
// TODO(rsadam): Remove this variable once figure out how to calculate this
// number before the key is rendered.
var KEY_PADDING_TOP = 1;
var KEY_PADDING_BOTTOM = 1;
