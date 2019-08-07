// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_surface_factory.h"

#include <lib/zx/event.h>
#include <memory>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/service_directory_client.h"
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
#include "ui/ozone/platform/scenic/sysmem_buffer_collection.h"

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

}  // namespace

ScenicSurfaceFactory::ScenicSurfaceFactory(mojom::ScenicGpuHost* gpu_host)
    : gpu_host_(gpu_host),
      egl_implementation_(std::make_unique<GLOzoneEGLScenic>()),
      sysmem_buffer_manager_(
          base::fuchsia::ServiceDirectoryClient::ForCurrentProcess()
              ->ConnectToServiceSync<fuchsia::sysmem::Allocator>()),
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
  return std::vector<gl::GLImplementation>{gl::kGLImplementationSwiftShaderGL,
                                           gl::kGLImplementationStubGL};
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
  DCHECK(gpu_host_);
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
    VkDevice vk_device,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  auto collection = sysmem_buffer_manager_.CreateCollection(vk_device, size,
                                                            format, usage, 1);
  if (!collection)
    return nullptr;

  return collection->CreateNativePixmap(0);
}

#if BUILDFLAG(ENABLE_VULKAN)
std::unique_ptr<gpu::VulkanImplementation>
ScenicSurfaceFactory::CreateVulkanImplementation() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!gpu_host_)
    LOG(FATAL) << "Vulkan implementation requires InitializeForGPU";

  return std::make_unique<ui::VulkanImplementationScenic>(
      this, &sysmem_buffer_manager_);
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
  auto create_session_task =
      base::BindOnce(&ScenicSurfaceFactory::CreateScenicSessionOnMainThread,
                     weak_ptr_factory_.GetWeakPtr(), session.NewRequest(),
                     listener_handle.Bind());
  if (main_thread_task_runner_->BelongsToCurrentThread()) {
    // In a single threaded environment, we need to connect the session
    // before returning so that synchronous calls do not deadlock the
    // current thread.
    std::move(create_session_task).Run();
  } else {
    main_thread_task_runner_->PostTask(FROM_HERE,
                                       std::move(create_session_task));
  }

  return {std::move(session), std::move(listener_request)};
}

void ScenicSurfaceFactory::CreateScenicSessionOnMainThread(
    fidl::InterfaceRequest<fuchsia::ui::scenic::Session> session_request,
    fidl::InterfaceHandle<fuchsia::ui::scenic::SessionListener> listener) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!scenic_) {
    scenic_ = base::fuchsia::ServiceDirectoryClient::ForCurrentProcess()
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
