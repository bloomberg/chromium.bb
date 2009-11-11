/*
 * Copyright 2009, Google Inc.
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


#ifndef GPU_COMMAND_BUFFER_SERVICE_WIN_D3D9_RENDER_SURFACE_D3D9_H_
#define GPU_COMMAND_BUFFER_SERVICE_WIN_D3D9_RENDER_SURFACE_D3D9_H_

// This file contains the definition of the D3D9 versions of
// render surface-related resource classes.

#include <d3d9.h>
#include "base/scoped_ptr.h"
#include "command_buffer/common/gapi_interface.h"
#include "command_buffer/service/resource.h"
#include "command_buffer/service/d3d9_utils.h"
#include "command_buffer/service/texture_d3d9.h"

namespace command_buffer {
namespace o3d {

class GAPID3D9;

// The RenderSurfaceD3D class represents a render surface resource in the d3d
// backend of the command buffer server.
class RenderSurfaceD3D9 : public RenderSurface {
 public:

  // Creates a render surface resource based on D3D.
  // Parameters:
  //   width - width of the surface to be created. Must match width of texture
  //     at mip_level.
  //   height - height of the surface to be created. Must match width of
  //     texture at mip_level.
  //   mip_level - mip level of the texture to which the render surface maps.
  //   side - side of a cube if texture is a cube texture. Does not apply to
  //     texture 2d's.
  //   texture - the texture to which this render surface maps. Not owned by
  //     the RenderSurfaceD3D9.
  //   direct3d_surface - a surface to be used as this render surface's
  //     rendering surface. The new RenderSurfaceD3D9 will own the
  //     direct3d_surface.
  RenderSurfaceD3D9(int width,
                    int height,
                    int mip_level,
                    int side,
                    TextureD3D9 *texture,
                    IDirect3DSurface9 *direct3d_surface);

  // Destructor for the render surface.
  virtual ~RenderSurfaceD3D9() {}

  // Performs the setup necessary to create a render surface resource based on
  // D3D and returns a new one if possibe.
  // Parameters:
  //   gapi - the gapi interface to D3D.
  //   width - width of the surface to be created. Must match width of texture
  //     at mip_level.
  //   height - height of the surface to be created. Must match width of
  //     texture at mip_level.
  //   mip_level - mip level of the texture to which the render surface maps.
  //   side - side of a cube if texture is a cube texture. Does not apply to
  //     texture 2d's.
  //   texture - the texture to which this render surface maps.
  // Returns:
  //   a new RenderSurfaceD3D9 or NULL on failure
  static RenderSurfaceD3D9* Create(GAPID3D9 *gapi,
                                   int width,
                                   int height,
                                   int mip_level,
                                   int side,
                                   TextureD3D9 *texture);

  // Returns a reference to the actual direct3d surface that is rendered to.
  IDirect3DSurface9* direct3d_surface() const {
    return direct3d_surface_;
  }
 private:
  CComPtr<IDirect3DSurface9> direct3d_surface_;
  int width_;
  int height_;
  int mip_level_;
  TextureD3D9 *texture_;
  DISALLOW_COPY_AND_ASSIGN(RenderSurfaceD3D9);
};

// The RenderDepthStencilSurfaceD3D class represents a depth stencil surface
// resource in the d3d backend of the command buffer server.
class RenderDepthStencilSurfaceD3D9 : public RenderDepthStencilSurface {
 public:

  // Creates a depth stencil surface resource based on D3D.
  // Parameters:
  //   width - width of the surface to be created.
  //   height - height of the surface to be created.
  //   direct3d_surface - a surface to be used as this depth stencil surface's
  //     rendering rendering surface. The new RenderDepthStencilSurfaceD3D9
  //     will own the direct3d_surface.
  RenderDepthStencilSurfaceD3D9(int width,
                                int height,
                                IDirect3DSurface9 *direct3d_surface);

  // Destructor for the depth stencil surface.
  virtual ~RenderDepthStencilSurfaceD3D9() {}


  // Performs the setup necessary to create a depth stencil surface resource
  // based on D3D and returns a new one if possibe.
  // Parameters:
  //   gapi - the gapi interface to D3D.
  //   width - width of the surface to be created.
  //   height - height of the surface to be created.
  // Returns:
  //   a new RenderDepthStencilSurfaceD3D9 or NULL on failure.
  static RenderDepthStencilSurfaceD3D9* Create(GAPID3D9 *gapi,
                                               int width,
                                               int height);

  // Returns a reference to the actual direct3d surface that is rendered to.
  IDirect3DSurface9* direct3d_surface() const {
    return direct3d_surface_;
  }
 private:
  CComPtr<IDirect3DSurface9> direct3d_surface_;
  int width_;
  int height_;
  DISALLOW_COPY_AND_ASSIGN(RenderDepthStencilSurfaceD3D9);
};

}  // namespace o3d
}  // namespace command_buffer

#endif  // GPU_COMMAND_BUFFER_SERVICE_WIN_D3D9_RENDER_SURFACE_D3D9_H_

