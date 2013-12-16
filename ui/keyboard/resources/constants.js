// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Aspect ratio of keyboard.
 * @type {number}
 */
var ASPECT_RATIO = 4.5;

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

