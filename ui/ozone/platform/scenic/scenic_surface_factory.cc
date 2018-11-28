// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_surface_factory.h"

#include <lib/zx/event.h>

#include "base/fuchsia/component_context.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/common/gl_ozone_egl.h"
#include "ui/ozone/platform/scenic/scenic_window.h"
#include "ui/ozone/platform/scenic/scenic_window_canvas.h"
#include "ui/ozone/platform/scenic/scenic_window_manager.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "ui/ozone/platform/scenic/vulkan_implementation_scenic.h"
#endif

namespace ui {

namespace {

class GLOzoneEGLScenic : public GLOzoneEGL {
 public:
  GLOzoneEGLScenic() = default;
  ~GLOzoneEGLScenic() override = default;

  // GLOzone:
  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gfx::AcceleratedWidget window) override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      const gfx::Size& size) override {
    return gl::InitializeGLSurface(
        base::MakeRefCounted<gl::PbufferGLSurfaceEGL>(size));
  }

  EGLNativeDisplayType GetNativeDisplay() override {
    return EGL_DEFAULT_DISPLAY;
  }

 protected:
  bool LoadGLES2Bindings(gl::GLImplementation implementation) override {
    return LoadDefaultEGLGLES2Bindings(implementation);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GLOzoneEGLScenic);
};

// TODO(crbug.com/852011): Implement this class - currently it's just a stub.
class ScenicPixmap : public gfx::NativePixmap {
 public:
  explicit ScenicPixmap(gfx::AcceleratedWidget widget,
                        gfx::Size size,
                        gfx::BufferFormat format)
      : size_(size), format_(format) {
    NOTIMPLEMENTED();
  }

  bool AreDmaBufFdsValid() const override { return false; }
  size_t GetDmaBufFdCount() const override { return 0; }
  int GetDmaBufFd(size_t plane) const override { return -1; }
  int GetDmaBufPitch(size_t plane) const override { return 0; }
  int GetDmaBufOffset(size_t plane) const override { return 0; }
  uint64_t GetDmaBufModifier(size_t plane) const override { return 0; }
  gfx::BufferFormat GetBufferFormat() const override { return format_; }
  gfx::Size GetBufferSize() const override { return size_; }
  uint32_t GetUniqueId() const override { return 0; }
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int plane_z_order,
                            gfx::OverlayTransform plane_transform,
                            const gfx::Rect& display_bounds,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override {
    NOTIMPLEMENTED();
    return false;
  }
  gfx::NativePixmapHandle ExportHandle() override {
    NOTIMPLEMENTED();
    return gfx::NativePixmapHandle();
  }

 private:
  ~ScenicPixmap() override {}

  gfx::Size size_;
  gfx::BufferFormat format_;

  DISALLOW_COPY_AND_ASSIGN(ScenicPixmap);
};

}  // namespace

ScenicSurfaceFactory::ScenicSurfaceFactory(ScenicWindowManager* window_manager)
    : window_manager_(window_manager),
      egl_implementation_(std::make_unique<GLOzoneEGLScenic>()) {}

ScenicSurfaceFactory::~ScenicSurfaceFactory() = default;

fuchsia::ui::scenic::Scenic* ScenicSurfaceFactory::GetScenic() {
  if (!scenic_) {
    scenic_ = base::fuchsia::ComponentContext::GetDefault()
                  ->ConnectToService<fuchsia::ui::scenic::Scenic>();
    scenic_.set_error_handler([](zx_status_t status) {
      ZX_LOG(FATAL, status) << "Scenic connection failed";
    });
  }
  return scenic_.get();
}

std::vector<gl::GLImplementation>
ScenicSurfaceFactory::GetAllowedGLImplementations() {
  // TODO(spang): Remove this after crbug.com/897208 is fixed.
  return std::vector<gl::GLImplementation>{gl::kGLImplementationSwiftShaderGL};
}

GLOzone* ScenicSurfaceFactory::GetGLOzone(gl::GLImplementation implementation) {
  switch (implementation) {
    case gl::kGLImplementationSwiftShaderGL:
      return egl_implementation_.get();
    default:
      return nullptr;
  }
}

std::unique_ptr<SurfaceOzoneCanvas> ScenicSurfaceFactory::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  ScenicWindow* window = window_manager_->GetWindow(widget);
  if (!window)
    return nullptr;
  return std::make_unique<ScenicWindowCanvas>(GetScenic(), window);
}

scoped_refptr<gfx::NativePixmap> ScenicSurfaceFactory::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  return new ScenicPixmap(widget, size, format);
}

#if BUILDFLAG(ENABLE_VULKAN)
std::unique_ptr<gpu::VulkanImplementation>
ScenicSurfaceFactory::CreateVulkanImplementation() {
  return std::make_unique<ui::VulkanImplementationScenic>(window_manager_,
                                                          GetScenic());
}
#endif

}  // namespace ui
