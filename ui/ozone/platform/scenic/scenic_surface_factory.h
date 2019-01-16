// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SCENIC_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_SCENIC_SCENIC_SURFACE_FACTORY_H_

#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "gpu/vulkan/buildflags.h"
#include "ui/ozone/public/gl_ozone.h"
#include "ui/ozone/public/interfaces/scenic_gpu_host.mojom.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class ScenicSurface;

class ScenicSurfaceFactory : public SurfaceFactoryOzone {
 public:
  explicit ScenicSurfaceFactory(mojom::ScenicGpuHost* gpu_host);
  ~ScenicSurfaceFactory() override;

  // SurfaceFactoryOzone implementation.
  std::vector<gl::GLImplementation> GetAllowedGLImplementations() override;
  GLOzone* GetGLOzone(gl::GLImplementation implementation) override;
  std::unique_ptr<PlatformWindowSurface> CreatePlatformWindowSurface(
      gfx::AcceleratedWidget widget) override;
  std::unique_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget) override;
  scoped_refptr<gfx::NativePixmap> CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override;
#if BUILDFLAG(ENABLE_VULKAN)
  std::unique_ptr<gpu::VulkanImplementation> CreateVulkanImplementation()
      override;
#endif

  void AddSurface(gfx::AcceleratedWidget widget, ScenicSurface* surface);
  void RemoveSurface(gfx::AcceleratedWidget widget);
  ScenicSurface* GetSurface(gfx::AcceleratedWidget widget);

 private:
  fuchsia::ui::scenic::Scenic* GetScenic();

  base::flat_map<gfx::AcceleratedWidget, ScenicSurface*> surface_map_;

  mojom::ScenicGpuHost* const gpu_host_;
  std::unique_ptr<GLOzone> egl_implementation_;
  fuchsia::ui::scenic::ScenicPtr scenic_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(ScenicSurfaceFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SCENIC_SURFACE_FACTORY_H_
