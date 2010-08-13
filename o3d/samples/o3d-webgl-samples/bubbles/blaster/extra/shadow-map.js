/*
 * Copyright 2010, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Represents the surface area that has been hit on a texture map by creating
 * a same-sized canvas composed of uniquely colored pixels.
 *
 * The original intention was to render the target from a single bubble's point
 * of view using this as the texture map in place of the original texture. With
 * stenciling, you render only the pixels that the bubble occludes and thus
 * intersects with when it hits the block. Since each pixel in the rendered
 * buffer has a unique color, it can be reverse mapped to the corresponding
 * texel position and marked off as "hit".
 *
 * @param {number} width The width of the texture to track hits for.
 * @param {number} height The height of the texture to track hits for.
 * @param {o3d.Pack} pack A pack to create objects with.
 * @constructor
 */
var ShadowMap = function(width, height, pack) {
  this.width = width;
  this.height = height;
  this.canvas = document.createElement("canvas");
  this.canvas.width = width;
  this.canvas.height = height;

  this.cached_percent_ = 0;
  this.dirty_cache_ = true;

  this.texture = pack.createTexture2D(width, height,
      g.o3d.Texture.XRGB8,
      1,
      false);

  ShadowMap.init(this.canvas);
  this.redrawTexture();
};

/**
 * Updates the texture object to reflect the most recent values in the canvas.
 */
ShadowMap.prototype.redrawTexture = function() {
  this.texture.setFromCanvas_(this.canvas, true, false, false);
};

/**
 * Calculates the percent of the shadow map covered of the entire map.
 * @private
 */
ShadowMap.prototype.calculatePercent_ = function() {
  var context = this.canvas.getContext("2d");
  var imageData = context.getImageData(0, 0, this.width, this.height);
  var count = 0;
  for (var i = 0; i < this.width * this.height; i++) {
    if (imageData.data[i * 4 + 3] != ShadowMap.ALPHA_START) {
      count++;
    }
  }
  this.cached_percent_ =
      Math.round(count * 100.0 / (this.width * this.height));
};


/**
 * Returns the percentage of the texture that is currently covered as a value
 * from 0 to 100. Caching is used so this value is not recomputed if no changes
 * have been made to the texture (via the apply method).
 *
 * @return {number} The percent, or -1 if the block has not been initialized.
 */
ShadowMap.prototype.percentCovered = function() {
  if (this.dirty_cache_) {
    this.dirty_cache_ = false;
    this.calculatePercent_();
  }
  return this.cached_percent_;
};

/**
 * Given a canvas whose pixels contain the "hit" pixels, marks off the
 * corresponding pixels in the original texture. Any pixels in the canvas with
 * an alpha of 0 are ignored.
 *
 * @param {Canvas} renderedCanvas
 */
ShadowMap.prototype.apply = function(renderedCanvas) {
  var data = this.canvas.getContext("2d").getImageData(0, 0,
      this.width, this.height);
  var rWidth = renderedCanvas.width;
  var rHeight = renderedCanvas.height;
  var rContext = renderedCanvas.getContext("2d");
  var rData = rContext.getImageData(0, 0, rWidth, rHeight);

  for (var i = 0; i < rWidth * rHeight; i++) {
    if (rData.data[i * 4 + 3] > 0) { // Alpha of the rendered canvas > 0.
      ShadowMap.updatePixel_(data, rData.data[i * 4],
          rData.data[i * 4 + 1], rData.data[i * 4 + 2]);
    }
  }
  this.canvas.getContext("2d").putImageData(data, 0, 0);
};

/**
 * Number of values per channel.
 * @type {number}
 */
ShadowMap.base = 256;

/**
 * The alpha value of unhit pixels in this map. In [0, 255] range.
 * @type {number}
 */
ShadowMap.ALPHA_START = 127;

/**
 * The alpha value of hit pixels in this map. In [0, 255] range.
 * @type {number}
 */
ShadowMap.ALPHA_PUT = 255;

/**
 * Given the index of a pixel in an image, computes the RGB color of that pixel
 * based on the pattern we used to initialize the map.
 * @param {number} index
 * @return {!Array.<number>}
 */
ShadowMap.indexToRGB_ = function(index) {
  var base = ShadowMap.base;
  var r = Math.floor(index / (base * base));
  var g = Math.floor((index % (base * base)) / base);
  var b = index % base;
  return [r, g, b];
}

/**
 * Given an array [r, g, b] of colors, computes the index of that pixel based on
 * the pattern we used to initialize the map.
 * @param {!Array.<number>} colors
 * @return {number}
 */
ShadowMap.rgbToIndex_ = function(colors) {
  var r = colors[0];
  var g = colors[1];
  var b = colors[2];
  var base = ShadowMap.base;
  return b + (g * base) + (r * base * base);
};

/**
 * Given a color, marks off the corresponding location in this map.
 * @param {ImageData} the canvas' ImageData object
 * @param {number} r
 * @param {number} g
 * @param {number} b
 * @return {ImageData} a modified ImageData object
 */
ShadowMap.updatePixel_ = function(imageData, r, g, b) {
  var index = ShadowMap.rgbToIndex_([r, g, b]);
  imageData.data[index * 4 + 3] = ShadowMap.ALPHA_PUT;
  return imageData;
};

/**
 * Initializes this map so each pixel is a unique color, where the channels are
 * incremented iteratively in B-G-R order.
 * @param {Canvas} canvas The canvas object to initialize to unique colors.
 */
ShadowMap.init = function(canvas) {
  var width = canvas.width;
  var height = canvas.height;
  var context = canvas.getContext("2d");
  var imageData = context.getImageData(0, 0, width, height);
  for (var h = 0; h < height; h++) {
    for (var w = 0; w < width; w++) {
      var pixelNum = w + h * width;
      var index = pixelNum * 4;
      var color = ShadowMap.indexToRGB_(pixelNum);
      imageData.data[index] = color[0];
      imageData.data[index + 1] = color[1];
      imageData.data[index + 2] = color[2];
      imageData.data[index + 3] = ShadowMap.ALPHA_START;
    }
  }
  context.putImageData(imageData, 0, 0);
};
