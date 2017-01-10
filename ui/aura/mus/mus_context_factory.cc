// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/mus_context_factory.h"

#include "base/memory/ptr_util.h"
#include "services/ui/public/cpp/gpu/gpu.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gl/gl_bindings.h"

namespace aura {

MusContextFactory::MusContextFactory(ui::Gpu* gpu)
    : gpu_(gpu), weak_ptr_factory_(this) {}

MusContextFactory::~MusContextFactory() {}

void MusContextFactory::OnEstablishedGpuChannel(
    base::WeakPtr<ui::Compositor> compositor,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel) {
  if (!compositor)
    return;
  WindowTreeHost* host =
      WindowTreeHost::GetForAcceleratedWidget(compositor->widget());
  WindowPortMus* window_port = WindowPortMus::Get(host->window());
  DCHECK(window_port);
  auto compositor_frame_sink = window_port->RequestCompositorFrameSink(
      gpu_->CreateContextProvider(std::move(gpu_channel)),
      gpu_->gpu_memory_buffer_manager());
  compositor->SetCompositorFrameSink(std::move(compositor_frame_sink));
}

void MusContextFactory::CreateCompositorFrameSink(
    base::WeakPtr<ui::Compositor> compositor) {
  gpu_->EstablishGpuChannel(
      base::Bind(&MusContextFactory::OnEstablishedGpuChannel,
                 weak_ptr_factory_.GetWeakPtr(), compositor));
}

scoped_refptr<cc::ContextProvider>
MusContextFactory::SharedMainThreadContextProvider() {
  // NOTIMPLEMENTED();
  return nullptr;
}

void MusContextFactory::RemoveCompositor(ui::Compositor* compositor) {
  // NOTIMPLEMENTED();
}

bool MusContextFactory::DoesCreateTestContexts() {
  return false;
}

uint32_t MusContextFactory::GetImageTextureTarget(gfx::BufferFormat format,
                                                  gfx::BufferUsage usage) {
  // TODO(sad): http://crbug.com/675431
  return GL_TEXTURE_2D;
}

gpu::GpuMemoryBufferManager* MusContextFactory::GetGpuMemoryBufferManager() {
  return gpu_->gpu_memory_buffer_manager();
}

cc::TaskGraphRunner* MusContextFactory::GetTaskGraphRunner() {
  return raster_thread_helper_.task_graph_runner();
}

}  // namespace aura
