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
#include "command_buffer/client/cross/cmd_buffer_helper.h"

namespace o3d {

using command_buffer::ResourceID;
using command_buffer::CommandBufferEntry;
using command_buffer::CommandBufferHelper;
namespace create_render_surface_cmd = command_buffer::create_render_surface_cmd;

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
  DCHECK(texture);

  ResourceID id = renderer_->render_surface_ids().AllocateID();
  resource_id_ = id;
  CommandBufferHelper *helper = renderer_->helper();
  CommandBufferEntry args[4];
  args[0].value_uint32 = id;
  args[1].value_uint32 =
      create_render_surface_cmd::Width::MakeValue(width) |
      create_render_surface_cmd::Height::MakeValue(height);
  args[2].value_uint32 =
    create_render_surface_cmd::Levels::MakeValue(mip_level) |
    create_render_surface_cmd::Side::MakeValue(side);
  args[3].value_uint32 =
      reinterpret_cast<ResourceID>(texture->GetTextureHandle());
  helper->AddCommand(command_buffer::CREATE_RENDER_SURFACE, 4, args);
}

RenderSurfaceCB::~RenderSurfaceCB() {
  Destroy();
}

void RenderSurfaceCB::Destroy() {
  // This should never get called during rendering.
  if (resource_id_ != command_buffer::kInvalidResource) {
    CommandBufferHelper *helper = renderer_->helper();
    CommandBufferEntry args[1];
    args[0].value_uint32 = resource_id_;
    helper->AddCommand(command_buffer::DESTROY_RENDER_SURFACE, 1, args);
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
  ResourceID id = renderer_->depth_surface_ids().AllocateID();
  resource_id_ = id;
  CommandBufferHelper *helper = renderer_->helper();
  CommandBufferEntry args[2];
  args[0].value_uint32 = id;
  args[1].value_uint32 =
      create_render_surface_cmd::Width::MakeValue(width) |
      create_render_surface_cmd::Height::MakeValue(height);
  helper->AddCommand(command_buffer::CREATE_DEPTH_SURFACE, 2, args);
}

void RenderDepthStencilSurfaceCB::Destroy() {
  if (resource_id_ != command_buffer::kInvalidResource) {
    CommandBufferHelper *helper = renderer_->helper();
    CommandBufferEntry args[1];
    args[0].value_uint32 = resource_id_;
    helper->AddCommand(command_buffer::DESTROY_DEPTH_SURFACE, 1, args);
    renderer_->depth_surface_ids().FreeID(resource_id_);
    resource_id_ = command_buffer::kInvalidResource;
  }
}

}  // namespace o3d

