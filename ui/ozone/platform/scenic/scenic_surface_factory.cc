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
#include "ui/ozone/platform/scenic/scenic_gpu_service.h"
#include "ui/ozone/platform/scenic/scenic_surface.h"
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

ScenicSurfaceFactory::ScenicSurfaceFactory(mojom::ScenicGpuHost* gpu_host)
    : gpu_host_(gpu_host),
      egl_implementation_(std::make_unique<GLOzoneEGLScenic>()),
      weak_ptr_factory_(this) {
  // TODO(spang, crbug.com/923445): Add message loop to GPU tests.
  if (base::ThreadTaskRunnerHandle::IsSet())
    main_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

ScenicSurfaceFactory::~ScenicSurfaceFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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

std::unique_ptr<PlatformWindowSurface>
ScenicSurfaceFactory::CreatePlatformWindowSurface(
    gfx::AcceleratedWidget widget) {
  auto surface =
      std::make_unique<ScenicSurface>(this, widget, CreateScenicSession());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ScenicSurfaceFactory::LinkSurfaceToParent,
                                weak_ptr_factory_.GetWeakPtr(), widget,
                                surface->CreateParentExportToken()));
  return surface;
}

std::unique_ptr<SurfaceOzoneCanvas> ScenicSurfaceFactory::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  ScenicSurface* surface = GetSurface(widget);
  return std::make_unique<ScenicWindowCanvas>(surface);
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!gpu_host_)
    LOG(FATAL) << "Vulkan implementation requires InitializeForGPU";

  return std::make_unique<ui::VulkanImplementationScenic>(this);
}
#endif

void ScenicSurfaceFactory::AddSurface(gfx::AcceleratedWidget widget,
                                      ScenicSurface* surface) {
  base::AutoLock lock(surface_lock_);
  DCHECK(!base::ContainsKey(surface_map_, widget));
  surface->AssertBelongsToCurrentThread();
  surface_map_.insert(std::make_pair(widget, surface));
}

void ScenicSurfaceFactory::RemoveSurface(gfx::AcceleratedWidget widget) {
  base::AutoLock lock(surface_lock_);
  auto it = surface_map_.find(widget);
  DCHECK(it != surface_map_.end());
  ScenicSurface* surface = it->second;
  surface->AssertBelongsToCurrentThread();
  surface_map_.erase(it);
}

ScenicSurface* ScenicSurfaceFactory::GetSurface(gfx::AcceleratedWidget widget) {
  base::AutoLock lock(surface_lock_);
  auto it = surface_map_.find(widget);
  DCHECK(it != surface_map_.end());
  ScenicSurface* surface = it->second;
  surface->AssertBelongsToCurrentThread();
  return surface;
}

scenic::SessionPtrAndListenerRequest
ScenicSurfaceFactory::CreateScenicSession() {
  fuchsia::ui::scenic::SessionPtr session;
  fidl::InterfaceHandle<fuchsia::ui::scenic::SessionListener> listener_handle;
  auto listener_request = listener_handle.NewRequest();
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ScenicSurfaceFactory::CreateScenicSessionOnMainThread,
                     weak_ptr_factory_.GetWeakPtr(), session.NewRequest(),
                     listener_handle.Bind()));
  return {std::move(session), std::move(listener_request)};
}

void ScenicSurfaceFactory::CreateScenicSessionOnMainThread(
    fidl::InterfaceRequest<fuchsia::ui::scenic::Session> session_request,
    fidl::InterfaceHandle<fuchsia::ui::scenic::SessionListener> listener) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!scenic_) {
    scenic_ = base::fuchsia::ComponentContext::GetDefault()
                  ->ConnectToService<fuchsia::ui::scenic::Scenic>();
    scenic_.set_error_handler([](zx_status_t status) {
      ZX_LOG(FATAL, status) << "Scenic connection failed";
    });
  }
  scenic_->CreateSession(std::move(session_request), std::move(listener));
}

void ScenicSurfaceFactory::LinkSurfaceToParent(
    gfx::AcceleratedWidget widget,
    mojo::ScopedHandle export_token_mojo) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  gpu_host_->ExportParent(widget, std::move(export_token_mojo));
}

}  // namespace ui
