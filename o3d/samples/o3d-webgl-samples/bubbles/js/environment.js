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
 * Creates a cubemap texture object given six faces of the cube.
 * @param {!Array.<HTMLElementImage>} images An array of 6 HTML image elements,
 *   representing the six faces of the cubemap. Should be in X+, X-, Y+, Y-, Z+,
 *   Z- order.
 * @constructor
 */
bsh.Environment = function(images) {
  if (images.length != 6) {
    throw 'Expecting six cube map faces.';
  }
  var rawDataArray = [];
  for (var i = 0; i < 6; ++i) {
    var rawData = g_pack.createObject('RawData');
    // TODO(luchen): Change this once textureCUBE set() is implemented
    rawData.image_ = images[i];
    rawData.width = 64;
    rawData.height = 64;
    rawDataArray.push(rawData);
  }
  this.texture =
      o3djs.texture.createTextureFromRawDataArray(g_pack, rawDataArray, true);
}

/**
 * The created texture.
 * @type {o3d.TextureCUBE}
 */
bsh.Environment.prototype.texture = null;
