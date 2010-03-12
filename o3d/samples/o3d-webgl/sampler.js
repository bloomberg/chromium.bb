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
 * Sampler is the base of all texture samplers.  Texture samplers encapsulate
 * a texture reference with a set of states that define how the texture
 * gets applied to a surface.  Sampler states are set either via Params defined
 * on the Sampler object or directly via one the convenience methods defined
 * on the Sampler.  The following states are supported (default values are in
 * parenthesis):
 *  \li 'addressModeU' (WRAP)
 *  \li 'addressModeV' (WRAP)
 *  \li 'addressModeW' (WRAP)
 *  \li 'magFilter' (LINEAR)
 *  \li 'minFilter' (LINEAR)
 *  \li 'mipFilter' (POINT)
 *  \li 'borderColor' ([0,0,0,0])
 *  \li 'maxAnisotropy' (1)
 */
o3d.Sampler = function() {
  o3d.ParamObject.call(this);
  this.borderColor = [0, 0, 0, 0];
};
o3d.inherit('Sampler', 'ParamObject');



/**
 * @type {number}
 */
o3d.Sampler.AddressMode = goog.typedef;


/**
 *  AddressMode,
 *   Controls what happens with texture coordinates outside the [0..1] range.
 *  WRAP
 *  MIRROR
 *  CLAMP
 *  BORDER
 */
o3d.Sampler.WRAP = 0;
o3d.Sampler.MIRROR = 1;
o3d.Sampler.CLAMP = 2;
o3d.Sampler.BORDER = 3;


/**
 * @type {number}
 */
o3d.Sampler.FilterType = goog.typedef;

/**
 *  FilterType,
 *   Texture filtering types.
 *  NONE
 *  POINT
 *  LINEAR
 *  ANISOTROPIC
 */
o3d.Sampler.NONE = 0;
o3d.Sampler.POINT = 1;
o3d.Sampler.LINEAR = 2;
o3d.Sampler.ANISOTROPIC = 3;


/**
 * The texture address mode for the u coordinate.
 * @type {!o3d.Sampler.AddressMode}
 */
o3d.Sampler.prototype.addressModeU = o3d.Sampler.WRAP;



/**
 * The texture address mode for the v coordinate.
 * @type {!o3d.Sampler.AddressMode}
 */
o3d.Sampler.prototype.addressModeV = o3d.Sampler.WRAP;



/**
 * The texture address mode for the w coordinate.
 * @type {!o3d.Sampler.AddressMode}
 */
o3d.Sampler.prototype.addressModeW = o3d.Sampler.WRAP;



/**
 * The magnification filter.  Valid values for the mag filter are: POINT and
 * @type {!o3d.Sampler.FilterType}
 */
o3d.Sampler.prototype.magFilter = o3d.Sampler.LINEAR;



/**
 * The minification filter. Valid values for the min filter are: POINT, LINEAR
 * and ANISOTROPIC.
 * @type {!o3d.Sampler.FilterType}
 */
o3d.Sampler.prototype.minFilter = o3d.Sampler.LINEAR;



/**
 * The mipmap filter used during minification.  Valid values for the mip filter
 * are: NONE, POINT and LINEAR.
 * @type {!o3d.Sampler.FilterType}
 */
o3d.Sampler.prototype.mipFilter = o3d.Sampler.LINEAR;



/**
 * Color returned for texture coordinates outside the [0,1] range when the
 * address mode is set to BORDER.
 * @type {!Array.<number>}
 */
o3d.Sampler.prototype.borderColor = [0, 0, 0, 0];



/**
 * Degree of anisotropy used when the ANISOTROPIC filter type is used.
 * @type {number}
 */
o3d.Sampler.prototype.maxAnisotropy = 1;



/**
 * The Texture object used by this Sampler.
 * @type {o3d.Texture}
 */
o3d.Sampler.prototype.texture = null;



