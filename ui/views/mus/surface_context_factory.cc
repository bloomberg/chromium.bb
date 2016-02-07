// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/surface_context_factory.h"

#include "cc/output/output_surface.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "components/mus/public/cpp/window.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"
#include "ui/compositor/reflector.h"
#include "ui/gl/gl_bindings.h"

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

SurfaceContextFactory::SurfaceContextFactory(
    mojo::Shell* shell,
    mus::Window* window,
    mus::mojom::SurfaceType surface_type)
    : surface_binding_(shell, window, surface_type),
      next_surface_id_namespace_(1u) {}

SurfaceContextFactory::~SurfaceContextFactory() {}

void SurfaceContextFactory::CreateOutputSurface(
    base::WeakPtr<ui::Compositor> compositor) {
  // NOTIMPLEMENTED();
  compositor->SetOutputSurface(surface_binding_.CreateOutputSurface());
}

scoped_ptr<ui::Reflector> SurfaceContextFactory::CreateReflector(
    ui::Compositor* mirroed_compositor,
    ui::Layer* mirroring_layer) {
  // NOTIMPLEMENTED();
  return make_scoped_ptr(new FakeReflector);
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

scoped_ptr<cc::SurfaceIdAllocator>
SurfaceContextFactory::CreateSurfaceIdAllocator() {
  return make_scoped_ptr(
      new cc::SurfaceIdAllocator(next_surface_id_namespace_++));
}

void SurfaceContextFactory::ResizeDisplay(ui::Compositor* compositor,
                                          const gfx::Size& size) {
  // NOTIMPLEMENTED();
}

}  // namespace views
