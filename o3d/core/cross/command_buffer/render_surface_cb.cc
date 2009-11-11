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


#include "core/cross/command_buffer/render_surface_cb.h"
#include "command_buffer/client/o3d_cmd_helper.h"

namespace o3d {

using command_buffer::ResourceId;
using command_buffer::CommandBufferEntry;
using command_buffer::O3DCmdHelper;

RenderSurfaceCB::RenderSurfaceCB(ServiceLocator *service_locator,
                                 int width,
                                 int height,
                                 int mip_level,
                                 int side,
                                 Texture *texture,
                                 RendererCB *renderer)
    : RenderSurface(service_locator, width, height, texture),
      resource_id_(command_buffer::kInvalidResource),
      renderer_(renderer) {
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);
  DCHECK_GT(mip_level, -1);
  DCHECK(texture);
  DCHECK(renderer);

  ResourceId id = renderer_->render_surface_ids().AllocateID();
  resource_id_ = id;
  O3DCmdHelper* helper = renderer_->helper();
  helper->CreateRenderSurface(
      id,
      reinterpret_cast<uint32>(texture->GetTextureHandle()),
      width, height, mip_level, side);
}

RenderSurfaceCB::~RenderSurfaceCB() {
  Destroy();
}

void RenderSurfaceCB::Destroy() {
  // This should never be called during rendering.
  if (resource_id_ != command_buffer::kInvalidResource) {
    O3DCmdHelper* helper = renderer_->helper();
    helper->DestroyRenderSurface(resource_id_);
    renderer_->render_surface_ids().FreeID(resource_id_);
    resource_id_ = command_buffer::kInvalidResource;
  }
}

RenderDepthStencilSurfaceCB::RenderDepthStencilSurfaceCB(
    ServiceLocator *service_locator,
    int width,
    int height,
    RendererCB *renderer)
    : RenderDepthStencilSurface(service_locator, width, height),
      resource_id_(command_buffer::kInvalidResource),
      renderer_(renderer) {
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);
  DCHECK(renderer);
  ResourceId id = renderer_->depth_surface_ids().AllocateID();
  resource_id_ = id;
  O3DCmdHelper* helper = renderer_->helper();
  helper->CreateDepthSurface(id, width, height);
}

void RenderDepthStencilSurfaceCB::Destroy() {
  if (resource_id_ != command_buffer::kInvalidResource) {
    O3DCmdHelper* helper = renderer_->helper();
    helper->DestroyDepthSurface(resource_id_);
    renderer_->depth_surface_ids().FreeID(resource_id_);
    resource_id_ = command_buffer::kInvalidResource;
  }
}

}  // namespace o3d

