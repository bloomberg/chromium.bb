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


#ifndef O3D_COMMAND_BUFFER_SERVICE_WIN_D3D9_RENDER_SURFACE_D3D9_H__
#define O3D_COMMAND_BUFFER_SERVICE_WIN_D3D9_RENDER_SURFACE_D3D9_H__

// This file contains the definition of the D3D9 versions of
// render surface-related resource classes.

#include <d3d9.h>
#include "command_buffer/common/cross/gapi_interface.h"
#include "command_buffer/service/win/d3d9/d3d9_utils.h"
#include "command_buffer/service/cross/resource.h"

namespace o3d {
namespace command_buffer {

class GAPID3D9;

class RenderSurfaceD3D9 : public RenderSurface {
 public:
  RenderSurfaceD3D9(int width,
                    int height,
                    int mip_level,
                    int side,
                    TextureD3D9 *texture,
                    IDirect3DSurface9* direct3d_surface);
  virtual ~RenderSurfaceD3D9() {}

  static RenderSurfaceD3D9* Create(GAPID3D9 *gapi,
                                   int width,
                                   int height,
                                   int mip_level,
                                   int side,
                                   TextureD3D9 *texture);
  IDirect3DSurface9* GetSurfaceHandle() const {
    return direct3d_surface_;
  }
 private:
  CComPtr<IDirect3DSurface9> direct3d_surface_;
  unsigned int width_;
  unsigned int height_;
  unsigned int mip_level_;
  TextureD3D9* texture_;
  DISALLOW_COPY_AND_ASSIGN(RenderSurfaceD3D9);
};

class RenderDepthStencilSurfaceD3D9 : public RenderDepthStencilSurface {
 public:
  RenderDepthStencilSurfaceD3D9(int width,
                                int height,
                                IDirect3DSurface9* direct3d_surface);
  virtual ~RenderDepthStencilSurfaceD3D9() {}

  static RenderDepthStencilSurfaceD3D9* Create(
    GAPID3D9 *gapi,
    int width,
    int height);
  IDirect3DSurface9* GetSurfaceHandle() const {
    return direct3d_surface_;
  }
 private:
  CComPtr<IDirect3DSurface9> direct3d_surface_;
  unsigned int width_;
  unsigned int height_;
  DISALLOW_COPY_AND_ASSIGN(RenderDepthStencilSurfaceD3D9);
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SERVICE_WIN_D3D9_RENDER_SURFACE_D3D9_H__

