// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/fake_context_factory.h"

#include "base/command_line.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/base/switches.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/layer_tree_frame_sink_client.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/switches.h"

namespace ui {

FakeContextFactory::FakeContextFactory() {
#if defined(OS_WIN)
  renderer_settings_.finish_rendering_on_resize = true;
#elif defined(OS_MACOSX)
  renderer_settings_.release_overlay_resources_after_gpu_query = true;
#endif
  // Populate buffer_to_texture_target_map for all buffer usage/formats.
  for (int usage_idx = 0; usage_idx <= static_cast<int>(gfx::BufferUsage::LAST);
       ++usage_idx) {
    gfx::BufferUsage usage = static_cast<gfx::BufferUsage>(usage_idx);
    for (int format_idx = 0;
         format_idx <= static_cast<int>(gfx::BufferFormat::LAST);
         ++format_idx) {
      gfx::BufferFormat format = static_cast<gfx::BufferFormat>(format_idx);
      renderer_settings_.resource_settings
          .buffer_to_texture_target_map[std::make_pair(usage, format)] =
          GL_TEXTURE_2D;
    }
  }
}

FakeContextFactory::~FakeContextFactory() = default;

const cc::CompositorFrame& FakeContextFactory::GetLastCompositorFrame() const {
  return *frame_sink_->last_sent_frame();
}

void FakeContextFactory::CreateLayerTreeFrameSink(
    base::WeakPtr<ui::Compositor> compositor) {
  auto frame_sink = cc::FakeLayerTreeFrameSink::Create3d();
  frame_sink_ = frame_sink.get();
  compositor->SetLayerTreeFrameSink(std::move(frame_sink));
}

scoped_refptr<cc::ContextProvider>
FakeContextFactory::SharedMainThreadContextProvider() {
  return nullptr;
}

void FakeContextFactory::RemoveCompositor(ui::Compositor* compositor) {
  frame_sink_ = nullptr;
}

double FakeContextFactory::GetRefreshRate() const {
  return 200.0;
}

gpu::GpuMemoryBufferManager* FakeContextFactory::GetGpuMemoryBufferManager() {
  return &gpu_memory_buffer_manager_;
}

cc::TaskGraphRunner* FakeContextFactory::GetTaskGraphRunner() {
  return &task_graph_runner_;
}

const cc::ResourceSettings& FakeContextFactory::GetResourceSettings() const {
  return renderer_settings_.resource_settings;
}

}  // namespace ui
