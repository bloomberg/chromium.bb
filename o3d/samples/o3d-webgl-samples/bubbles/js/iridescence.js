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
 * Creates a canvas element that contains the interference pattern colors, to
 * be used as a texture. Once set, the width and height cannot be changed.
 *
 * @param {number} width The width of the texture (should be power-of-two).
 * @param {number} height The height of the texture (should be power-of-two).
 * @param {number} refraction_index The refraction index of the medium.
 * @param {number} thickness The thickness of the medium between the interfaces.
 * @constructor
 */
bsh.Iridescence = function(width, height, refraction_index, thickness) {
  this.canvas = document.createElement('canvas');
  this.canvas.width = width;
  this.canvas.height = height;
  this.init(refraction_index, thickness);
};


/**
 * The canvas element that is written to.
 * @type {Canvas}
 */
bsh.Iridescence.prototype.canvas = null;


/**
 * Computes the fresnel coefficients for amplitude.
 * http://physics.tamuk.edu/~suson/html/4323/prop-em.html has them.
 * @param {number} n
 * @param {number} cos_i
 * @param {number} cos_t
 * @return {Object} object containing reflection and transmitted values.
 */
bsh.Iridescence.ComputeFresnel = function(n, cos_i, cos_t) {
  var coeff = {};
  coeff.reflected_perp = (cos_i - n*cos_t)/(cos_i + n*cos_t);
  coeff.transmitted_perp = 2.0*cos_i/(cos_i + n*cos_t);
  coeff.reflected_para = (n*cos_i - cos_t)/(n*cos_i + cos_t);
  coeff.transmitted_para = 2.0*cos_i/(n*cos_i + cos_t);
  return coeff;
}


/**
 * Uses Snell-Descartes' Law: sin_i = n * sin_t
 * @param {number} n
 * @param {number} cos_i
 * @return {number}
 */
bsh.Iridescence.RefractedRay = function(n, cos_i) {
  var sin2_i = 1 - cos_i * cos_i;
  var sin2_t = sin2_i / (n*n);
  var cos2_t = 1 - sin2_t;
  var cos_t = Math.sqrt(Math.max(0, cos2_t));
  return cos_t;
}


/**
 * Understanding the notations in the following two functions.
 *
 *               \ A       \ A'          / B         i = incident angle.
 *    incident ray \         \         /             t = transmitted angle.
 *                   \ i|      \ i|i /
 *                     \|        \|/      air (n = 1) outside the bubble
 *  -outer-interface----P---------R---------------------------------------
 *                      |\       /|      ^
 *                      |t\     /t|      |thin layer (e.g. water, n > 1)
 *         transmitted ray \t|t/         |thickness
 *                          \|/          v
 *  -inner-interface---------Q--------------------------------------------
 *                           |\           air (n = 1) inside the bubble
 *                           |i \
 *                                \ C
 *
 *  Incident ray A gets refracted by the outer interface at point P, then
 *  reflected by the inner interface at point Q, then refracted into B at point
 *  R ("trt" ray). At the same time, incident ray A', coherent with A, gets
 *  directly reflected into B ("r" ray) at point R, so it interferes with the
 *  "trt" ray.
 *  At point Q the ray also gets refracted inside the bubble, leading to the
 *  "tt" ray C.
 */

/**
 * Computes the interference between the reflected ray ('r' - one reflection
 * one the outer interface) and the reflection of the transmitted ray ('trt' -
 * transmitted through the outer interface, reflected on the inner interface,
 * then transmitted again through the outer interface).
 * @param {number} thickness The thickness of the medium between the interfaces.
 * @param {number} wavelength The wavelength of the incident light.
 * @param {number} n The refraction index of the medium (relative to the outer
 *  medium).
 * @param {number} r_perp The amplitude coefficient for the r ray for
 *  perpendicular polarization.
 * @param {number} r_para The amplitude coefficient for the r ray for parallel
 *  polarization.
 * @param {number} trt_perp The amplitude coefficient for the trt ray for
 *  perpendicular polarization.
 * @param {number} trt_para The amplitude coefficient for the trt ray for
 *  parallel polarization.
 * @param {number} cos_t The cosine of the refracted angle.
 * @return {number} The reflected power after interference.
 */
bsh.Iridescence.Interference = function(thickness,
    wavelength,
    n,
    r_perp,
    r_para,
    trt_perp,
    trt_para,
    cos_t) {
  // Difference in wave propagation between the trt ray and the r ray.
  var delta_phase = 2.0 * thickness/wavelength * n * cos_t;
  // For a given polarization, power = ||r + trt * e^(i*2*pi*delta_phase)||^2
  var cos_delta = Math.cos(2.0 * kPi * delta_phase);
  var power_perp =
  r_perp*r_perp + trt_perp*trt_perp + 2.0 * r_perp*trt_perp*cos_delta;
  var power_para =
  r_para*r_para + trt_para*trt_para + 2.0 * r_para*trt_para*cos_delta;
  // Total power is the average between 2 polarization modes (for non-polarized
  // light).
  return (power_perp+power_para)/2.0;
}


/**
 * Regenerates and writes iridescence map to the canvas element.
 * @param {number} refraction_index The refraction index of the medium.
 * @param {number} thickness The thickness of the medium between the interfaces.
 */
bsh.Iridescence.prototype.init = function(refraction_index, thickness) {
  var kRedWavelength = bsh.Globals.kRedWavelength;
  var kGreenWavelength = bsh.Globals.kGreenWavelength;
  var kBlueWavelength = bsh.Globals.kBlueWavelength;
  var width = this.canvas.width;
  var height = this.canvas.height;
  var n = refraction_index;
  var max_thickness = thickness * 1.5;
  var ctx = this.canvas.getContext('2d');

  var counter = 0;
  var imageData = ctx.getImageData(0, 0, width, height);
  for (var y = 0; y < height; ++y) {
    var thickness = (y + .5) * max_thickness / height;
    for (var x = 0; x < width; ++x) {
      var cos_i = (x + .5) * 1.0 / width;
      var cos_t = bsh.Iridescence.RefractedRay(n, cos_i);
      // Fresnel coefficient for the "outer" interface (outside->inside).
      var outer = bsh.Iridescence.ComputeFresnel(n, cos_i, cos_t);
      // Fresnel coefficient for the "inner" interface (inside->outside).
      var inner = bsh.Iridescence.ComputeFresnel(1.0/n, cos_t, cos_i);
      var r_perp = outer.reflected_perp;
      var r_para = outer.reflected_para;
      var trt_perp = outer.transmitted_perp * inner.reflected_perp *
          inner.transmitted_perp;
      var trt_para = outer.transmitted_para * inner.reflected_para *
          inner.transmitted_para;
      var red = bsh.Iridescence.Interference(thickness, kRedWavelength, n,
          r_perp, r_para, trt_perp, trt_para, cos_t);
      var green = bsh.Iridescence.Interference(thickness, kGreenWavelength, n,
          r_perp, r_para, trt_perp, trt_para, cos_t);
      var blue = bsh.Iridescence.Interference(thickness, kBlueWavelength, n,
          r_perp, r_para, trt_perp, trt_para, cos_t);
      var tt_perp = outer.transmitted_perp * inner.transmitted_perp;
      var tt_para = outer.transmitted_para * inner.transmitted_para;
      var alpha = (tt_perp*tt_perp + tt_para*tt_para)/2.0;
      imageData.data[counter++] = bsh.clamp(red);
      imageData.data[counter++] = bsh.clamp(green);
      imageData.data[counter++] = bsh.clamp(blue);
      imageData.data[counter++] = bsh.clamp(alpha);
    }
  }
  ctx.putImageData(imageData, 0, 0);
}


/**
 * Copies the data from the canvas into the texture.
 * @param {o3d.Texture} texture The texture to write to. Should be same size.
 */
bsh.Iridescence.prototype.refresh = function(texture) {
  var data = this.canvas.getContext('2d').getImageData(0, 0, this.canvas.width,
      this.canvas.height).data;
  var pixels = [];
  for (var i = 0; i < data.length; ++i) {
    pixels[i] = data[i] / 255.0;
  }
  texture.set(0, pixels);
}
