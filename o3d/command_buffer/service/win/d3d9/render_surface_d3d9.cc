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


// This file implements the D3D9 versions of the render surface resources,
// as well as the related GAPID3D9 function implementations.

#include "command_buffer/service/win/d3d9/render_surface_d3d9.h"
#include "command_buffer/service/win/d3d9/gapi_d3d9.h"
#include "command_buffer/service/win/d3d9/texture_d3d9.h"


namespace o3d {
namespace command_buffer {
namespace o3d {

RenderSurfaceD3D9::RenderSurfaceD3D9(int width,
                                     int height,
                                     int mip_level,
                                     int side,
                                     TextureD3D9 *texture,
                                     IDirect3DSurface9 *direct3d_surface)
    : width_(width),
      height_(height),
      mip_level_(mip_level),
      texture_(texture),
      direct3d_surface_(direct3d_surface) {
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);
  DCHECK_GT(mip_level, -1);
  DCHECK(texture);
}

RenderSurfaceD3D9* RenderSurfaceD3D9::Create(GAPID3D9 *gapi,
                                             int width,
                                             int height,
                                             int mip_level,
                                             int side,
                                             TextureD3D9 *texture) {
  DCHECK(gapi);
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);
  DCHECK_GT(mip_level, -1);
  DCHECK(texture);
  CComPtr<IDirect3DSurface9> direct3d_surface_handle;
  bool success =
    texture->CreateRenderSurface(width, height, mip_level, side,
                                 &direct3d_surface_handle);
  if (!success || direct3d_surface_handle == NULL) {
    // If the surface was not created properly, delete and return nothing.
    return NULL;
  }
  RenderSurfaceD3D9 *render_surface =
    new RenderSurfaceD3D9(width,
                          height,
                          mip_level,
                          side,
                          texture,
                          direct3d_surface_handle);
  return render_surface;
}

RenderDepthStencilSurfaceD3D9::RenderDepthStencilSurfaceD3D9(
    int width,
    int height,
    IDirect3DSurface9 *direct3d_surface)
    : width_(width),
      height_(height),
      direct3d_surface_(direct3d_surface) {
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);
}

RenderDepthStencilSurfaceD3D9* RenderDepthStencilSurfaceD3D9::Create(
    GAPID3D9 *gapi,
    int width,
    int height) {
  DCHECK(gapi);
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);

  CComPtr<IDirect3DSurface9> direct3d_surface;
  gapi->d3d_device()->CreateDepthStencilSurface(
      width,
      height,
      D3DFMT_D24S8,  // d3d format
      D3DMULTISAMPLE_NONE,  // multisampling type
      0,  // multisample quality level
      FALSE,  // z-buffer discarding disabled
      &direct3d_surface,
      NULL);  // This parameter is required to be NULL.
  if (direct3d_surface == NULL) {
    return NULL;
  }
  RenderDepthStencilSurfaceD3D9 *depth_stencil =
    new RenderDepthStencilSurfaceD3D9(width, height, direct3d_surface);
  return depth_stencil;
}

// GAPI Interface functions ---------------------------------------------------

// Copies the data from a texture resource.
parse_error::ParseError GAPID3D9::CreateRenderSurface(
    ResourceId id,
    unsigned int width,
    unsigned int height,
    unsigned int mip_level,
    unsigned int side,
    ResourceId texture_id) {
  if (id == current_surface_id_) {
    // This will delete the current surface which would be bad.
    return parse_error::kParseInvalidArguments;
  }
  TextureD3D9 *texture = textures_.Get(texture_id);
  if (!texture->render_surfaces_enabled()) {
    return parse_error::kParseInvalidArguments;
  } else {
    RenderSurfaceD3D9 *render_surface = RenderSurfaceD3D9::Create(this,
                                                                  width,
                                                                  height,
                                                                  mip_level,
                                                                  side,
                                                                  texture);
    if (render_surface == NULL) {
      return parse_error::kParseInvalidArguments;
    }
    render_surfaces_.Assign(id, render_surface);
  }
  return parse_error::kParseNoError;
}

parse_error::ParseError GAPID3D9::DestroyRenderSurface(ResourceId id) {
  if (id == current_surface_id_) {
    return parse_error::kParseInvalidArguments;
  }
  return render_surfaces_.Destroy(id) ?
    parse_error::kParseNoError :
    parse_error::kParseInvalidArguments;
}

parse_error::ParseError GAPID3D9::CreateDepthSurface(
    ResourceId id,
    unsigned int width,
    unsigned int height) {
  if (id == current_depth_surface_id_) {
    // This will delete the current surface which would be bad.
    return parse_error::kParseInvalidArguments;
  }
  RenderDepthStencilSurfaceD3D9 *depth_surface =
    RenderDepthStencilSurfaceD3D9::Create(this, width, height);
  if (depth_surface == NULL) {
    return parse_error::kParseInvalidArguments;
  }
  depth_surfaces_.Assign(id, depth_surface);
  return parse_error::kParseNoError;
}

parse_error::ParseError GAPID3D9::DestroyDepthSurface(ResourceId id) {
  if (id == current_depth_surface_id_) {
    return parse_error::kParseInvalidArguments;
  }
  return depth_surfaces_.Destroy(id) ?
    parse_error::kParseNoError :
    parse_error::kParseInvalidArguments;
}

parse_error::ParseError GAPID3D9::SetRenderSurface(
    ResourceId render_surface_id,
    ResourceId depth_stencil_id) {
  RenderSurfaceD3D9 *d3d_render_surface =
    render_surfaces_.Get(render_surface_id);
  RenderDepthStencilSurfaceD3D9 *d3d_render_depth_surface =
    depth_surfaces_.Get(depth_stencil_id);

  if (d3d_render_surface == NULL && d3d_render_depth_surface == NULL) {
    return parse_error::kParseInvalidArguments;
  }

  IDirect3DSurface9 *d3d_surface =
    d3d_render_surface ? d3d_render_surface->direct3d_surface() : NULL;
  IDirect3DSurface9 *d3d_depth_surface = d3d_render_depth_surface ?
    d3d_render_depth_surface->direct3d_surface() : NULL;

  // Get the device and set the render target and the depth stencil surface.
  IDirect3DDevice9 *device = this->d3d_device();

  HR(device->SetRenderTarget(0, d3d_surface));
  HR(device->SetDepthStencilSurface(d3d_depth_surface));
  current_surface_id_ = render_surface_id;
  current_depth_surface_id_ = depth_stencil_id;
  return parse_error::kParseNoError;
}

void GAPID3D9::SetBackSurfaces() {
  // Get the device and set the render target and the depth stencil surface.
  IDirect3DDevice9 *device = this->d3d_device();
  HR(d3d_device()->SetRenderTarget(0, back_buffer_surface_));
  HR(d3d_device()->SetDepthStencilSurface(back_buffer_depth_surface_));
}

}  // namespace o3d
}  // namespace command_buffer
}  // namespace o3d

