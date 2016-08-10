// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/surface_context_factory.h"

#include "base/memory/ptr_util.h"
#include "cc/output/output_surface.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "services/ui/public/cpp/output_surface.h"
#include "services/ui/public/cpp/window.h"
#include "ui/compositor/reflector.h"
#include "ui/gl/gl_bindings.h"
#include "ui/views/mus/native_widget_mus.h"

namespace views {
namespace {

class FakeReflector : public ui::Reflector {
 public:
  FakeReflector() {}
  ~FakeReflector() override {}
  void OnMirroringCompositorResized() override {}
  void AddMirroringLayer(ui::Layer* layer) override {}
  void RemoveMirroringLayer(ui::Layer* layer) override {}
};

}  // namespace

SurfaceContextFactory::SurfaceContextFactory()
    : next_surface_id_namespace_(1u) {}

SurfaceContextFactory::~SurfaceContextFactory() {}

void SurfaceContextFactory::CreateOutputSurface(
    base::WeakPtr<ui::Compositor> compositor) {
  ui::Window* window = compositor->window();
  NativeWidgetMus* native_widget = NativeWidgetMus::GetForWindow(window);
  ui::mojom::SurfaceType surface_type = native_widget->surface_type();
  std::unique_ptr<cc::OutputSurface> surface(
      new ui::OutputSurface(window->RequestSurface(surface_type)));
  compositor->SetOutputSurface(std::move(surface));
}

std::unique_ptr<ui::Reflector> SurfaceContextFactory::CreateReflector(
    ui::Compositor* mirroed_compositor,
    ui::Layer* mirroring_layer) {
  // NOTIMPLEMENTED();
  return base::WrapUnique(new FakeReflector);
}

void SurfaceContextFactory::RemoveReflector(ui::Reflector* reflector) {
  // NOTIMPLEMENTED();
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

cc::SharedBitmapManager* SurfaceContextFactory::GetSharedBitmapManager() {
  // NOTIMPLEMENTED();
  return nullptr;
}

gpu::GpuMemoryBufferManager*
SurfaceContextFactory::GetGpuMemoryBufferManager() {
  return &gpu_memory_buffer_manager_;
}

cc::TaskGraphRunner* SurfaceContextFactory::GetTaskGraphRunner() {
  return raster_thread_helper_.task_graph_runner();
}

uint32_t SurfaceContextFactory::AllocateSurfaceClientId() {
  return next_surface_id_namespace_++;
}

cc::SurfaceManager* SurfaceContextFactory::GetSurfaceManager() {
  // NOTIMPLEMENTED();
  return nullptr;
}

void SurfaceContextFactory::ResizeDisplay(ui::Compositor* compositor,
                                          const gfx::Size& size) {
  // NOTIMPLEMENTED();
}

}  // namespace views
