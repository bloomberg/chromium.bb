// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_SURFACE_CONTEXT_FACTORY_H_
#define UI_VIEWS_MUS_SURFACE_CONTEXT_FACTORY_H_

#include <stdint.h>

#include "base/macros.h"
#include "components/mus/gles2/mojo_gpu_memory_buffer_manager.h"
#include "components/mus/gles2/raster_thread_helper.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "ui/compositor/compositor.h"
#include "ui/views/mus/mus_export.h"
#include "ui/views/mus/surface_binding.h"

namespace mojo {
namespace shell {
namespace mojom {
class Shell;
}
}
}

namespace mus {
class Window;
}

namespace views {

class VIEWS_MUS_EXPORT SurfaceContextFactory : public ui::ContextFactory {
 public:
  SurfaceContextFactory(mojo::shell::mojom::Shell* shell,
                        mus::Window* window,
                        mus::mojom::SurfaceType surface_type);
  ~SurfaceContextFactory() override;

 private:
  // ContextFactory:
  void CreateOutputSurface(base::WeakPtr<ui::Compositor> compositor) override;
  scoped_ptr<ui::Reflector> CreateReflector(
      ui::Compositor* mirrored_compositor,
      ui::Layer* mirroring_layer) override;
  void RemoveReflector(ui::Reflector* reflector) override;
  scoped_refptr<cc::ContextProvider> SharedMainThreadContextProvider() override;
  void RemoveCompositor(ui::Compositor* compositor) override;
  bool DoesCreateTestContexts() override;
  uint32_t GetImageTextureTarget(gfx::BufferFormat format,
                                 gfx::BufferUsage usage) override;
  cc::SharedBitmapManager* GetSharedBitmapManager() override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  scoped_ptr<cc::SurfaceIdAllocator> CreateSurfaceIdAllocator() override;
  void ResizeDisplay(ui::Compositor* compositor,
                     const gfx::Size& size) override;

  SurfaceBinding surface_binding_;
  uint32_t next_surface_id_namespace_;
  gles2::RasterThreadHelper raster_thread_helper_;
  mus::MojoGpuMemoryBufferManager gpu_memory_buffer_manager_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceContextFactory);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_SURFACE_CONTEXT_FACTORY_H_
