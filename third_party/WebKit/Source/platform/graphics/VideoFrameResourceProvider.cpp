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
#include "platform/wtf/WeakPtr.h"

namespace blink {

VideoFrameResourceProvider::VideoFrameResourceProvider(
    WebContextProviderCallback context_provider_callback,
    viz::SharedBitmapManager* shared_bitmap_manager,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager)
    : context_provider_callback_(std::move(context_provider_callback)),
      shared_bitmap_manager_(shared_bitmap_manager),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
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

  // TODO(lethalantidote): Get real value for delegated_sync_points_required.
  // TODO(lethalantidote): Get real resource_settings.
  resource_provider_ = std::make_unique<cc::LayerTreeResourceProvider>(
      media_context_provider, shared_bitmap_manager_,
      gpu_memory_buffer_manager_, false, resource_settings_);

  // TODO(lethalantidote): Get real value for use_stream_video_draw_quad.
  // use_stream_video_draw_quad only seems relevant to android, where it is
  // true.
  resource_updater_ = std::make_unique<cc::VideoResourceUpdater>(
      media_context_provider, resource_provider_.get(), false);
}

void VideoFrameResourceProvider::AppendQuads(viz::RenderPass* render_pass) {
  gfx::Rect rect(0, 0, 10000, 10000);
  gfx::Rect visible_rect(0, 0, 10000, 10000);
  bool is_clipped = false;
  bool are_contents_opaque = true;
  viz::SharedQuadState* shared_state =
      render_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), rect, rect, rect, is_clipped,
                       are_contents_opaque, 1, SkBlendMode::kSrcOver, 0);
  viz::SolidColorDrawQuad* solid_color_quad =
      render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  // Fluxuate colors for placeholder testing.
  static int r = 0;
  static int g = 0;
  static int b = 0;
  r++;
  g += 2;
  b += 3;
  solid_color_quad->SetNew(shared_state, rect, visible_rect,
                           SkColorSetRGB(r % 255, g % 255, b % 255), false);
}

}  // namespace blink
