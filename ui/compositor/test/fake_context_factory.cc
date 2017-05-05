// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/fake_context_factory.h"

#include "base/threading/thread_task_runner_handle.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_sink_client.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/test/fake_compositor_frame_sink.h"

namespace ui {

const cc::CompositorFrame& FakeContextFactory::GetLastCompositorFrame() const {
  return *frame_sink_->last_sent_frame();
}

void FakeContextFactory::CreateCompositorFrameSink(
    base::WeakPtr<ui::Compositor> compositor) {
  auto frame_sink = cc::FakeCompositorFrameSink::Create3d();
  frame_sink_ = frame_sink.get();
  compositor->SetCompositorFrameSink(std::move(frame_sink));
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

uint32_t FakeContextFactory::GetImageTextureTarget(gfx::BufferFormat format,
                                                   gfx::BufferUsage usage) {
  return GL_TEXTURE_2D;
}

gpu::GpuMemoryBufferManager* FakeContextFactory::GetGpuMemoryBufferManager() {
  return &gpu_memory_buffer_manager_;
}

cc::TaskGraphRunner* FakeContextFactory::GetTaskGraphRunner() {
  return &task_graph_runner_;
}

}  // namespace ui
