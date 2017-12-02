// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/VideoFrameResourceProvider.h"

#include <memory>
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "cc/resources/layer_tree_resource_provider.h"
#include "cc/resources/video_resource_updater.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "media/base/video_frame.h"

namespace cc {
class VideoFrameExternalResources;
}  // namespace cc

namespace blink {

VideoFrameResourceProvider::VideoFrameResourceProvider(
    WebContextProviderCallback context_provider_callback,
    viz::SharedBitmapManager* shared_bitmap_manager,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const cc::LayerTreeSettings& settings)
    : context_provider_callback_(std::move(context_provider_callback)),
      shared_bitmap_manager_(shared_bitmap_manager),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      settings_(settings),
      weak_ptr_factory_(this) {}

VideoFrameResourceProvider::~VideoFrameResourceProvider() {
  viz::ContextProvider::ScopedContextLock lock(context_provider_);
  resource_provider_ = nullptr;
}

void VideoFrameResourceProvider::ObtainContextProvider() {
  context_provider_callback_.Run(base::BindOnce(
      &VideoFrameResourceProvider::Initialize, weak_ptr_factory_.GetWeakPtr()));
}

void VideoFrameResourceProvider::Initialize(
    viz::ContextProvider* media_context_provider) {
  // TODO(lethalantidote): Need to handle null contexts.
  // https://crbug/768565
  CHECK(media_context_provider);
  context_provider_ = media_context_provider;

  viz::ContextProvider::ScopedContextLock lock(context_provider_);
  resource_provider_ = std::make_unique<cc::LayerTreeResourceProvider>(
      media_context_provider, shared_bitmap_manager_,
      gpu_memory_buffer_manager_, true, settings_.resource_settings);

  resource_updater_ = std::make_unique<cc::VideoResourceUpdater>(
      media_context_provider, resource_provider_.get(),
      settings_.use_stream_video_draw_quad);
}

void VideoFrameResourceProvider::AppendQuads(
    viz::RenderPass* render_pass,
    scoped_refptr<media::VideoFrame> frame) {
  viz::ContextProvider::ScopedContextLock lock(context_provider_);
  resource_updater_->ObtainFrameResources(frame);
  // TODO(lethalantidote) : update with true value;
  bool contents_opaque = true;
  gfx::Rect visible_layer_rect = gfx::Rect(frame->coded_size());
  gfx::Rect clip_rect = gfx::Rect(frame->coded_size());
  bool is_clipped = false;
  float draw_opacity = 1.0f;
  int sorting_context_id = 0;

  // Internal to this compositor frame, this video quad is never occluded,
  // thus the full quad is visible.
  gfx::Rect visible_quad_rect = gfx::Rect(frame->coded_size());

  resource_updater_->AppendQuads(
      render_pass, frame, gfx::Transform(), frame->coded_size(),
      visible_layer_rect, clip_rect, is_clipped, contents_opaque, draw_opacity,
      sorting_context_id, visible_quad_rect);
}

void VideoFrameResourceProvider::ReleaseFrameResources() {
  viz::ContextProvider::ScopedContextLock lock(context_provider_);
  resource_updater_->ReleaseFrameResources();
}

void VideoFrameResourceProvider::PrepareSendToParent(
    const cc::LayerTreeResourceProvider::ResourceIdArray& resource_ids,
    std::vector<viz::TransferableResource>* transferable_resources) {
  viz::ContextProvider::ScopedContextLock lock(context_provider_);
  resource_provider_->PrepareSendToParent(resource_ids, transferable_resources);
}

void VideoFrameResourceProvider::ReceiveReturnsFromParent(
    const std::vector<viz::ReturnedResource>& transferable_resources) {
  viz::ContextProvider::ScopedContextLock lock(context_provider_);
  resource_provider_->ReceiveReturnsFromParent(transferable_resources);
}

}  // namespace blink
