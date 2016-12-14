// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/surface_context_factory.h"

#include "base/memory/ptr_util.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "services/ui/public/cpp/context_provider.h"
#include "services/ui/public/cpp/gpu/gpu.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_compositor_frame_sink.h"
#include "ui/compositor/reflector.h"
#include "ui/gl/gl_bindings.h"
#include "ui/views/mus/native_widget_mus.h"

namespace views {

SurfaceContextFactory::SurfaceContextFactory(ui::Gpu* gpu) : gpu_(gpu) {}

SurfaceContextFactory::~SurfaceContextFactory() {}

void SurfaceContextFactory::CreateCompositorFrameSink(
    base::WeakPtr<ui::Compositor> compositor) {
  ui::Window* window = compositor->window();
  NativeWidgetMus* native_widget = NativeWidgetMus::GetForWindow(window);
  ui::mojom::CompositorFrameSinkType compositor_frame_sink_type =
      native_widget->compositor_frame_sink_type();
  auto compositor_frame_sink = window->RequestCompositorFrameSink(
      compositor_frame_sink_type, make_scoped_refptr(new ui::ContextProvider(
                                      gpu_->EstablishGpuChannelSync())),
      gpu_->gpu_memory_buffer_manager());
  compositor->SetCompositorFrameSink(std::move(compositor_frame_sink));
}

scoped_refptr<cc::ContextProvider>
SurfaceContextFactory::SharedMainThreadContextProvider() {
  // NOTIMPLEMENTED();
  return nullptr;
}

void SurfaceContextFactory::RemoveCompositor(ui::Compositor* compositor) {
  // NOTIMPLEMENTED();
}

bool SurfaceContextFactory::DoesCreateTestContexts() {
  return false;
}

uint32_t SurfaceContextFactory::GetImageTextureTarget(gfx::BufferFormat format,
                                                      gfx::BufferUsage usage) {
  // No GpuMemoryBuffer support, so just return GL_TEXTURE_2D.
  return GL_TEXTURE_2D;
}

gpu::GpuMemoryBufferManager*
SurfaceContextFactory::GetGpuMemoryBufferManager() {
  return gpu_->gpu_memory_buffer_manager();
}

cc::TaskGraphRunner* SurfaceContextFactory::GetTaskGraphRunner() {
  return raster_thread_helper_.task_graph_runner();
}

}  // namespace views
