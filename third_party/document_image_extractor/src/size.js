// Copyright 2015  The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('image.collections.extension.domextractor.Size');


goog.scope(function() {



/**
 * @param {number} width
 * @param {number} height
 * @constructor
 */
image.collections.extension.domextractor.Size = function(width, height) {
  /** @type {number} */
  this.width = width;

  /** @type {number} */
  this.height = height;
};


/**
 * @return {number} The area of the size (width * height).
 */
image.collections.extension.domextractor.Size.prototype.area = function() {
  return this.width * this.height;
};
});
