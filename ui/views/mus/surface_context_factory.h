// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_SURFACE_CONTEXT_FACTORY_H_
#define UI_VIEWS_MUS_SURFACE_CONTEXT_FACTORY_H_

#include <stdint.h>

#include "base/macros.h"
#include "cc/surfaces/surface_manager.h"
#include "services/ui/public/cpp/raster_thread_helper.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "ui/compositor/compositor.h"
#include "ui/views/mus/mus_export.h"

namespace ui {
class Gpu;
}

namespace views {

class VIEWS_MUS_EXPORT SurfaceContextFactory : public ui::ContextFactory {
 public:
  explicit SurfaceContextFactory(ui::Gpu* gpu);
  ~SurfaceContextFactory() override;

 private:
  // ContextFactory:
  void CreateCompositorFrameSink(
      base::WeakPtr<ui::Compositor> compositor) override;
  scoped_refptr<cc::ContextProvider> SharedMainThreadContextProvider() override;
  void RemoveCompositor(ui::Compositor* compositor) override;
  bool DoesCreateTestContexts() override;
  uint32_t GetImageTextureTarget(gfx::BufferFormat format,
                                 gfx::BufferUsage usage) override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  void AddObserver(ui::ContextFactoryObserver* observer) override {}
  void RemoveObserver(ui::ContextFactoryObserver* observer) override {}

  ui::RasterThreadHelper raster_thread_helper_;
  ui::Gpu* gpu_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceContextFactory);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_SURFACE_CONTEXT_FACTORY_H_
