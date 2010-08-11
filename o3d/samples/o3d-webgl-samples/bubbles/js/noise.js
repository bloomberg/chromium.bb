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
var bsh = bsh || {};


/**
 * Generates a perlin noise texture. The width and height cannot be changed
 * after construction.
 *
 * @param {number} frequency Frequency of the texture.
 * @param {number} seed A seed for random values.
 * @param {number} width The width of the texture (should be power-of-two).
 * @param {number} height The height of the texture (should be power-of-two).
 * @constructor
 */
bsh.Perlin = function(frequency, seed, width, height) {
  this.canvas = document.createElement('canvas');
  this.canvas.width = width;
  this.canvas.height = height;
  this.init(frequency, seed);
}


/**
 * The canvas that stores the texture.
 * @type {Canvas}
 */
bsh.Perlin.prototype.canvas = null;


/**
 * The frequency
 * @type {number}
 */
bsh.Perlin.prototype.frequency = null;


/**
 * An array that contains the perlin noise values before adding attenuation at
 * the poles.
 * @type {!Array.<number>}
 * @private
 */
bsh.Perlin.prototype.values_ = [];


/**
 * Permutation array.
 * @type {!Array.<number>}
 * @private
 */
bsh.Perlin.prototype.permutation_ = [];


/**
 * Gradient array.
 * @type {!Array.<number>}
 * @private
 */
bsh.Perlin.prototype.gradients_ = [];


/**
 * Generates and writes the perlin noise into the canvas element.
 * @param {number} frequency Frequency of the texture.
 * @param {number} seed A seed for random values.
 */
bsh.Perlin.prototype.init = function(frequency, seed) {
  this.frequency = frequency;
  this.values_ = [];
  this.permutation_ = [];
  this.gradients_ = [];

  this.prep_(seed);
  this.generate_();

  var ctx = this.canvas.getContext('2d');
  var width = this.canvas.width;
  var height = this.canvas.height;
  var imageData = ctx.getImageData(0, 0, width, height);
  var counter = 0;
  for (var y = 0; y < height; ++y) {
    // Attenuate the noise value with a function that goes to 0 on the poles to
    // avoid discontinuities.
    // Note: it'd be better to have a 3D noise texture, but they are way
    // too expensive.
    var attenuation = Math.sin(y * kPi / height);
    for (var x = 0; x < width; ++x) {
      var attenuated_value = this.values_[y * width + x] * attenuation;
      // remap [-1..1] to [0..1] and convert to color values.
      var value = bsh.clamp(attenuated_value * 0.5 + 0.5);
      imageData.data[counter++] = value;
      imageData.data[counter++] = value;
      imageData.data[counter++] = value;
      imageData.data[counter++] = value;
    }
  }
  ctx.putImageData(imageData, 0, 0);
}


/**
 * Initializes the values, permutation and gradients arrays that are used by the
 * generate method.
 * @param {number} seed A seed for random values.
 * @private
 */
bsh.Perlin.prototype.prep_ = function(seed) {
  //Initialize the gradients table with a random unit direction.
  // Initialize the permutation table to be the identity.
  for (var i = 0; i < this.frequency; ++i) {
    var theta = Math.random() * (2.0 * kPi);
    this.gradients_[i] = [Math.cos(theta), Math.sin(theta)];
    this.permutation_[i] = i;
  }
  // Generate the permutation table by switching each element with a further
  // element. Also duplicate the permutation table so that constructs like
  // permutation[x + permutation[y]] work without additional modulo.
  for (var i = 0; i < this.frequency; ++i) {
    var j = i + (Math.floor(Math.random() * seed) % (this.frequency - i));
    var tmp = this.permutation_[j];
    this.permutation_[j] = this.permutation_[i];
    this.permutation_[i] = tmp;
    this.permutation_[i + this.frequency] = tmp;
  }
}

/**
 * Generates the perlin noise texture into the values_ array.
 * @private
 */
bsh.Perlin.prototype.generate_ = function() {
  var width = this.canvas.width;
  var height = this.canvas.height;
  var counter = 0;
  for (var y = 0; y < height; ++y) {
    for (var x = 0; x < width; ++x) {
      // The texture is decomposed into a lattice of frequency_ points in each
      // direction. A (x, y) point falls between 4 lattice vertices.
      // (xx, yy) are the coordinate of the bottom left lattice vertex
      // corresponding to an (x, y) image point.
      var xx = Math.floor((x * this.frequency) / width);
      var yy = Math.floor((y * this.frequency) / height);
      // xt and yt are the barycentric coordinates of the (x, y) point relative
      // to the vertices (between 0 and 1).
      var xt = 1.0/width * ((x * this.frequency) % width);
      var yt = 1.0/height * ((y * this.frequency) % height);
      // contribution of each lattice vertex to the point value.
      var contrib = [];
      for (var y_offset = 0; y_offset < 2; ++y_offset) {
        for (var x_offset = 0; x_offset < 2; ++x_offset) {
          var index = this.permutation_[xx + x_offset] + y_offset + yy;
          var gradient = this.gradients_[this.permutation_[index]];
          contrib[y_offset*2+x_offset] = gradient[0] * (xt - x_offset) +
                                         gradient[1] * (yt - y_offset);
        }
      }
      // We interpolate between the vertex contributions using a smooth step
      // function of the barycentric coordinates.
      var xt_smooth = bsh.smoothStep(xt);
      var yt_smooth = bsh.smoothStep(yt);
      var contrib_bottom = bsh.lerp(xt_smooth, contrib[0], contrib[1]);
      var contrib_top = bsh.lerp(xt_smooth, contrib[2], contrib[3]);
      var sq = bsh.lerp(yt_smooth, contrib_bottom, contrib_top);
      this.values_[counter++] = sq;
    }
  }
}

/**
 * Copies the data from the canvas into the texture.
 * @param {o3d.Texture} texture The texture to. Should be same size.
 */
bsh.Perlin.prototype.refresh = function(texture) {
  var data = this.canvas.getContext('2d').getImageData(0, 0, this.canvas.width,
      this.canvas.height).data;
  var pixels = [];
  for (var i = 0; i < data.length; ++i) {
    pixels[i] = data[i] / 255.0;
  }
  texture.set(0, pixels);
}
