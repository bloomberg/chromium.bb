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
 * A RenderSurfaceSet node will bind depth and color RenderSurface nodes
 * to the current rendering context.  All RenderNodes descending
 * from the given RenderSurfaceSet node will operate on the contents of
 * the bound depth and color buffers.
 * Note the following usage constraints of this node:
 * 1)  If both a color and depth surface is bound, then they must be of matching
 *     dimensions.
 * 2)  At least one of render_surface and render_depth_surface must non-null.
 * 
 * @param {o3d.RenderSurface} opt_renderSurface The render surface to set.
 * @param {o3d.RenderDepthStencilSurface} opt_renderDepthStencilSurface The
 *     depth stencil render surface to set.
 * @constructor
 */
o3d.RenderSurfaceSet = function() {
  o3d.RenderSurfaceSet.prototype.renderSurface =
      opt_renderSurface;
  o3d.RenderSurfaceSet.prototype.renderDepthStencilSurface =
      opt_renderDepthStencilSurface;
};
o3d.inherit('RenderSurfaceSet', 'RenderNode');


/**
 * The render surface to which the color contents of all RenderNode children
 * should be drawn.
 * @type {o3d.RenderSurface}
 */
o3d.RenderSurfaceSet.prototype.renderSurface = null;



/**
 * The render depth stencil surface to which the depth contents of all
 * RenderNode children should be drawn.
 * @type {o3d.RenderDepthStencilSurface}
 */
o3d.RenderSurfaceSet.prototype.renderDepthStencilSurface = null;



