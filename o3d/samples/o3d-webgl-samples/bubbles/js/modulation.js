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
 * Creates a canvas element that contains the geometry modulations. Modulations
 * are represented as radial gradient "blips" drawn on the canvas. Once set,
 * the width and height of the texture cannot be changed.
 *
 * @param {number} width The width of the texture (should be power-of-two).
 * @param {number} height The height of the texture (should be power-of-two).
 * @param {!Array.<number>} bounds An array [min, max] for blip radius.
 * @param {number} blips The number of spots to modulate.
 * @constructor
 */
bsh.Modulation = function(width, height, bounds, blips) {
  this.canvas = document.createElement('canvas');
  this.canvas.width = width;
  this.canvas.height = height;
  this.init(bounds, blips);
};

/**
 * The canvas element that is written to.
 * @type {Canvas}
 */
bsh.Modulation.prototype.canvas = null;

/**
 * Creates a gradient with the provided position and radius.
 * @param {number} x x-coordinate of the blip center.
 * @param {number} y y-coordinate of the blip center.
 * @param {number} r Radius of the blip.
 * @param {CanvasRenderingContext2D} ctx The 2d drawing context.
 * @return {CanvasGradient}
 */
bsh.Modulation.makeGradient = function(x, y, r, ctx) {
  var radgrad = ctx.createRadialGradient(x, y, 0, x, y, r);
  radgrad.addColorStop(0, '#FFFFFF');
  radgrad.addColorStop(1, 'rgba(0, 0, 0, 0)');
  return radgrad;
}

/**
 * Regenerates the texture and writes to the canvas element.
 * @param {!Array.<number>} bounds An array containing [min, max] of the radius
 *   of the blips.
 * @param {number} blips Number of blips.
 */
bsh.Modulation.prototype.init = function(bounds, blips) {
  var ctx = this.canvas.getContext('2d');
  var width = this.canvas.width;
  var height = this.canvas.height;

  // Reset to opaque black first.
  ctx.fillStyle = "rgba(0, 0, 0, 1)";
  ctx.fillRect(0, 0, width, height);
  var r = bounds;
  var x = [r[1] / 1.5, width - r[1] / 1.5];
  var y = x;

  for (var i = 0; i < blips; ++i) {
    var posX = randf(x[0], x[1]);
    var posY = randf(y[0], y[1]);
    var radius = randf(r[0], r[1]);
    ctx.fillStyle = bsh.Modulation.makeGradient(posX, posY, radius, ctx);
    ctx.fillRect(0, 0, width, height);
  }
}
