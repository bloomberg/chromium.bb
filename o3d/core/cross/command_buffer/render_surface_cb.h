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


#ifndef O3D_CORE_CROSS_COMMAND_BUFFER_RENDER_SURFACE_CB_H_
#define O3D_CORE_CROSS_COMMAND_BUFFER_RENDER_SURFACE_CB_H_

// This file contains the definition of the CB versions of
// render surface sub-classes.

#include "core/cross/render_surface.h"
#include "core/cross/command_buffer/renderer_cb.h"
#include "gpu/command_buffer/common/resource.h"

namespace o3d {

// The RenderSurfaceCB class represents a render surface in the core library
// of the client for command buffers. This class is responsible for sending
// calls across the command buffer to create an actual render surface resource
// on the server.
class RenderSurfaceCB : public RenderSurface {
 public:
  typedef SmartPointer<RenderSurfaceCB> Ref;

  // The render surface maintains a reference to its texture and renderer but
  // does not own them. The owner of the render surface is responsible for
  // properly deleting any textures.
  // Parameters:
  //   service_locator - for central lookup. Not owned by RenderSurfaceCB.
  //   width - width of the bitmap for this render surface. It must match the
  //     the width of the texture at 'mip_level'
  //   height - height of the bitmap for this render surface. It must match the
  //     the height of the texture at 'mip_level'
  //   mip_level - mip level of 'texture' for this render surface.
  //   side - which side of a cube texture the render surface represents. The
  //     'side' parameter will not be used for a texture2d render surface.
  //   texture - the texture this render surface maps to. Not owned by
  //     RenderSurfaceCB.
  //   renderer - the renderer to render to render surface. Not owned by
  //     RenderSurfaceCB.
  RenderSurfaceCB(ServiceLocator *service_locator,
                  int width,
                  int height,
                  int mip_level,
                  int side,
                  Texture *texture,
                  RendererCB *renderer);
  virtual ~RenderSurfaceCB();

  // The CB specific implementation of GetBitmap.
  // Returns:
  //   a reference to the underlying bitmap of the render surface.
  virtual Bitmap::Ref PlatformSpecificGetBitmap() const {
    // TODO(rlp): Add this functionality.
    DCHECK(false);
    return Bitmap::Ref();
  }

  // Destroys any data structures associated with the render surface and
  // resets any allocated IDs. This function should never be called during
  // rendering.
  virtual void Destroy();

  // Returns the render surface resource ID.
  command_buffer::ResourceId resource_id() const { return resource_id_; }

 private:
  command_buffer::ResourceId resource_id_;
  RendererCB *renderer_;
  DISALLOW_COPY_AND_ASSIGN(RenderSurfaceCB);
};

// The RenderDepthStencilSurfaceCB class represents a depth stencil surface in
// the core library of the client for command buffers. This class is
// responsible for sending calls across the command buffer to create an actual
// depth stencil surface resource on the server.
class RenderDepthStencilSurfaceCB : public RenderDepthStencilSurface {
 public:
  typedef SmartPointer<RenderDepthStencilSurfaceCB> Ref;

  // The depth stencil surface maintains a reference to its renderer which is
  // also what typically creates it (though not always). The depth stencil
  // does not maintain ownership of the renderer.
  // Parameters:
  //   service_locator - for central lookup.
  //   width - width of the bitmap for this render surface.
  //   height - height of the bitmap for this render surface.
  //   renderer - the renderer to render to render surface. Not owned by
  //     RenderDepthStencilSurfaceCB.
  RenderDepthStencilSurfaceCB(ServiceLocator *service_locator,
                              int width,
                              int height,
                              RendererCB *renderer);
  virtual ~RenderDepthStencilSurfaceCB() {}

  // Destroys any data structures associated with the render surface and
  // resets any allocated IDs. This function should never be called during
  // rendering.
  virtual void Destroy();

  // Returns the render depth stencil surface resource ID.
  command_buffer::ResourceId resource_id() const { return resource_id_; }

 private:
  command_buffer::ResourceId resource_id_;
  RendererCB *renderer_;
  DISALLOW_COPY_AND_ASSIGN(RenderDepthStencilSurfaceCB);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_COMMAND_BUFFER_RENDER_SURFACE_CB_H_

