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

#include "core/cross/render_surface.h"
#include "core/cross/command_buffer/renderer_cb.h"
#include "command_buffer/common/cross/resource.h"

namespace o3d {

class RenderSurfaceCB : public RenderSurface {
 public:
  typedef SmartPointer<RenderSurfaceCB> Ref;

  RenderSurfaceCB(ServiceLocator *service_locator,
                  int width,
                  int height,
                  int mip_level,
                  int side,
                  Texture *texture,
                  RendererCB *renderer);
  virtual ~RenderSurfaceCB();

  virtual Bitmap::Ref PlatformSpecificGetBitmap() const {
    // TODO(rlp): Add this functionality.
    DCHECK(false);
    return Bitmap::Ref();
  }

  virtual void Destroy();

  // Gets the render surface resource ID.
  command_buffer::ResourceID resource_id() const { return resource_id_; }
 private:
  command_buffer::ResourceID resource_id_;
  RendererCB* renderer_;
  DISALLOW_COPY_AND_ASSIGN(RenderSurfaceCB);
};

class RenderDepthStencilSurfaceCB : public RenderDepthStencilSurface {
 public:
  typedef SmartPointer<RenderDepthStencilSurfaceCB> Ref;

  RenderDepthStencilSurfaceCB(ServiceLocator *service_locator,
                              int width,
                              int height,
                              RendererCB *renderer);
  virtual ~RenderDepthStencilSurfaceCB() {}

  virtual void Destroy();

  // Gets the render depth stencil surface resource ID.
  command_buffer::ResourceID resource_id() const { return resource_id_; }
 private:
  command_buffer::ResourceID resource_id_;
  RendererCB* renderer_;
  DISALLOW_COPY_AND_ASSIGN(RenderDepthStencilSurfaceCB);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_COMMAND_BUFFER_RENDER_SURFACE_CB_H_

