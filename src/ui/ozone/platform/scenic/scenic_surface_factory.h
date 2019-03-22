// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SCENIC_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_SCENIC_SCENIC_SURFACE_FACTORY_H_

#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "gpu/vulkan/buildflags.h"
#include "ui/ozone/public/gl_ozone.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class ScenicWindowManager;
class ScenicGpuService;

class ScenicSurfaceFactory : public SurfaceFactoryOzone {
 public:
  explicit ScenicSurfaceFactory(ScenicWindowManager* window_manager);
  explicit ScenicSurfaceFactory(ScenicGpuService* scenic_gpu_service);
  ~ScenicSurfaceFactory() override;

  // SurfaceFactoryOzone implementation.
  std::vector<gl::GLImplementation> GetAllowedGLImplementations() override;
  GLOzone* GetGLOzone(gl::GLImplementation implementation) override;
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

 private:
  fuchsia::ui::scenic::Scenic* GetScenic();

  ScenicWindowManager* const window_manager_ = nullptr;
  ScenicGpuService* scenic_gpu_service_ = nullptr;
  std::unique_ptr<GLOzone> egl_implementation_;
  fuchsia::ui::scenic::ScenicPtr scenic_;

  DISALLOW_COPY_AND_ASSIGN(ScenicSurfaceFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SCENIC_SURFACE_FACTORY_H_
