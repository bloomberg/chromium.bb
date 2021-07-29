// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/compositor_frame_helpers.h"

#include <memory>
#include <set>
#include <utility>

namespace viz {
namespace {

constexpr gfx::Rect kDefaultOutputRect(20, 20);
constexpr gfx::Rect kDefaultDamageRect(0, 0);

}  // namespace

CompositorFrameBuilder::CompositorFrameBuilder() {
  frame_ = MakeInitCompositorFrame();
}

CompositorFrameBuilder::~CompositorFrameBuilder() = default;

CompositorFrame CompositorFrameBuilder::Build() {
  CompositorFrame temp_frame(std::move(frame_.value()));
  frame_ = MakeInitCompositorFrame();
  return temp_frame;
}

CompositorFrameBuilder& CompositorFrameBuilder::AddDefaultRenderPass() {
  return AddRenderPass(kDefaultOutputRect, kDefaultDamageRect);
}

CompositorFrameBuilder& CompositorFrameBuilder::AddRenderPass(
    const gfx::Rect& output_rect,
    const gfx::Rect& damage_rect) {
  auto pass = CompositorRenderPass::Create();
  pass->SetNew(render_pass_id_generator_.GenerateNextId(), output_rect,
               damage_rect, gfx::Transform());
  frame_->render_pass_list.push_back(std::move(pass));
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::AddRenderPass(
    std::unique_ptr<CompositorRenderPass> render_pass) {
  // Give the render pass a unique id if one hasn't been assigned.
  if (render_pass->id.is_null())
    render_pass->id = render_pass_id_generator_.GenerateNextId();
  frame_->render_pass_list.push_back(std::move(render_pass));
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::SetRenderPassList(
    CompositorRenderPassList render_pass_list) {
  DCHECK(frame_->render_pass_list.empty());
  frame_->render_pass_list = std::move(render_pass_list);
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::AddTransferableResource(
    TransferableResource resource) {
  frame_->resource_list.push_back(std::move(resource));
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::SetTransferableResources(
    std::vector<TransferableResource> resource_list) {
  DCHECK(frame_->resource_list.empty());
  frame_->resource_list = std::move(resource_list);
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::PopulateResources() {
  PopulateTransferableResources(frame_.value());
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::SetBeginFrameAck(
    const BeginFrameAck& ack) {
  frame_->metadata.begin_frame_ack = ack;
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::SetDeviceScaleFactor(
    float device_scale_factor) {
  frame_->metadata.device_scale_factor = device_scale_factor;
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::AddLatencyInfo(
    ui::LatencyInfo latency_info) {
  frame_->metadata.latency_info.push_back(std::move(latency_info));
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::AddLatencyInfos(
    std::vector<ui::LatencyInfo> latency_info) {
  if (frame_->metadata.latency_info.empty()) {
    frame_->metadata.latency_info.swap(latency_info);
  } else {
    for (auto& latency : latency_info) {
      frame_->metadata.latency_info.push_back(std::move(latency));
    }
  }
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::SetActivationDependencies(
    std::vector<SurfaceId> activation_dependencies) {
  frame_->metadata.activation_dependencies = std::move(activation_dependencies);
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::SetDeadline(
    const FrameDeadline& deadline) {
  frame_->metadata.deadline = deadline;
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::SetReferencedSurfaces(
    std::vector<SurfaceRange> referenced_surfaces) {
  frame_->metadata.referenced_surfaces = std::move(referenced_surfaces);
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::SetSendFrameTokenToEmbedder(
    bool send) {
  DCHECK(frame_->metadata.frame_token);
  frame_->metadata.send_frame_token_to_embedder = send;
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::AddDelegatedInkMetadata(
    const gfx::DelegatedInkMetadata& metadata) {
  frame_->metadata.delegated_ink_metadata =
      std::make_unique<gfx::DelegatedInkMetadata>(metadata);
  return *this;
}

CompositorFrame CompositorFrameBuilder::MakeInitCompositorFrame() const {
  static FrameTokenGenerator next_token;
  CompositorFrame frame;
  frame.metadata.begin_frame_ack = BeginFrameAck::CreateManualAckWithDamage();
  frame.metadata.device_scale_factor = 1.f;
  frame.metadata.frame_token = ++next_token;
  return frame;
}

CompositorFrame MakeDefaultCompositorFrame() {
  return CompositorFrameBuilder().AddDefaultRenderPass().Build();
}

AggregatedFrame MakeDefaultAggregatedFrame(size_t num_render_passes) {
  static AggregatedRenderPassId::Generator id_generator;
  AggregatedFrame frame;
  for (size_t i = 0; i < num_render_passes; ++i) {
    frame.render_pass_list.push_back(std::make_unique<AggregatedRenderPass>());
    frame.render_pass_list.back()->SetNew(id_generator.GenerateNextId(),
                                          kDefaultOutputRect,
                                          kDefaultDamageRect, gfx::Transform());
  }
  return frame;
}

CompositorFrame MakeEmptyCompositorFrame() {
  return CompositorFrameBuilder().Build();
}

void PopulateTransferableResources(CompositorFrame& frame) {
  DCHECK(frame.resource_list.empty());

  std::set<ResourceId> resources_added;
  for (auto& render_pass : frame.render_pass_list) {
    for (auto* quad : render_pass->quad_list) {
      for (ResourceId resource_id : quad->resources) {
        if (resource_id == kInvalidResourceId)
          continue;

        // Adds a TransferableResource the first time seeing a ResourceId.
        if (resources_added.insert(resource_id).second) {
          frame.resource_list.push_back(TransferableResource::MakeSoftware(
              SharedBitmap::GenerateId(), quad->rect.size(), RGBA_8888));
          frame.resource_list.back().id = resource_id;
        }
      }
    }
  }
}

}  // namespace viz
