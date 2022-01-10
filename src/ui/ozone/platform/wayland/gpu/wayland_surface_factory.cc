// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_surface_factory.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "ui/gfx/linux/client_native_pixmap_dmabuf.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/common/gl_ozone_egl.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/gpu/gl_surface_egl_readback_wayland.h"
#include "ui/ozone/platform/wayland/gpu/gl_surface_wayland.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/platform/wayland/gpu/wayland_canvas_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"

#if defined(WAYLAND_GBM)
#include "ui/ozone/platform/wayland/gpu/gbm_pixmap_wayland.h"
#include "ui/ozone/platform/wayland/gpu/gbm_surfaceless_wayland.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(ENABLE_VULKAN)
#include "ui/ozone/platform/wayland/gpu/vulkan_implementation_wayland.h"
#endif

namespace ui {

namespace {

class GLOzoneEGLWayland : public GLOzoneEGL {
 public:
  GLOzoneEGLWayland(WaylandConnection* connection,
                    WaylandBufferManagerGpu* buffer_manager)
      : connection_(connection), buffer_manager_(buffer_manager) {}

  GLOzoneEGLWayland(const GLOzoneEGLWayland&) = delete;
  GLOzoneEGLWayland& operator=(const GLOzoneEGLWayland&) = delete;

  ~GLOzoneEGLWayland() override {}

  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gfx::AcceleratedWidget widget) override;

  scoped_refptr<gl::GLSurface> CreateSurfacelessViewGLSurface(
      gfx::AcceleratedWidget window) override;

  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      const gfx::Size& size) override;

 protected:
  gl::EGLDisplayPlatform GetNativeDisplay() override;
  bool LoadGLES2Bindings(const gl::GLImplementationParts& impl) override;

 private:
  WaylandConnection* const connection_;
  WaylandBufferManagerGpu* const buffer_manager_;
};

scoped_refptr<gl::GLSurface> GLOzoneEGLWayland::CreateViewGLSurface(
    gfx::AcceleratedWidget widget) {
  // Only EGLGLES2 is supported with surfaceless view gl.
  if ((gl::GetGLImplementation() != gl::kGLImplementationEGLGLES2) ||
      !connection_)
    return nullptr;

  WaylandWindow* window =
      connection_->wayland_window_manager()->GetWindow(widget);
  if (!window)
    return nullptr;

  // The wl_egl_window needs to be created before the GLSurface so it can be
  // used in the GLSurface constructor.
  auto egl_window = CreateWaylandEglWindow(window);
  if (!egl_window)
    return nullptr;
  return gl::InitializeGLSurface(
      new GLSurfaceWayland(std::move(egl_window), window));
}

scoped_refptr<gl::GLSurface> GLOzoneEGLWayland::CreateSurfacelessViewGLSurface(
    gfx::AcceleratedWidget window) {
  if (gl::IsSoftwareGLImplementation(gl::GetGLImplementationParts())) {
    return gl::InitializeGLSurface(
        base::MakeRefCounted<GLSurfaceEglReadbackWayland>(window,
                                                          buffer_manager_));
  } else {
#if defined(WAYLAND_GBM)
  // If there is a gbm device available, use surfaceless gl surface.
  if (!buffer_manager_->GetGbmDevice())
    return nullptr;
  return gl::InitializeGLSurface(
      new GbmSurfacelessWayland(buffer_manager_, window));
#else
  return nullptr;
#endif
  }
}

scoped_refptr<gl::GLSurface> GLOzoneEGLWayland::CreateOffscreenGLSurface(
    const gfx::Size& size) {
  if (gl::GLSurfaceEGL::IsEGLSurfacelessContextSupported() &&
      size.width() == 0 && size.height() == 0) {
    return gl::InitializeGLSurface(new gl::SurfacelessEGL(size));
  } else {
    return gl::InitializeGLSurface(new gl::PbufferGLSurfaceEGL(size));
  }
}

gl::EGLDisplayPlatform GLOzoneEGLWayland::GetNativeDisplay() {
  if (connection_)
    return gl::EGLDisplayPlatform(
        reinterpret_cast<EGLNativeDisplayType>(connection_->display()));
  return gl::EGLDisplayPlatform(EGL_DEFAULT_DISPLAY);
}

bool GLOzoneEGLWayland::LoadGLES2Bindings(
    const gl::GLImplementationParts& impl) {
  // TODO: It may not be necessary to set this environment variable when using
  // swiftshader.
  setenv("EGL_PLATFORM", "wayland", 0);
  return LoadDefaultEGLGLES2Bindings(impl);
}

}  // namespace

WaylandSurfaceFactory::WaylandSurfaceFactory(
    WaylandConnection* connection,
    WaylandBufferManagerGpu* buffer_manager)
    : connection_(connection), buffer_manager_(buffer_manager) {
  egl_implementation_ =
      std::make_unique<GLOzoneEGLWayland>(connection_, buffer_manager_);
}

WaylandSurfaceFactory::~WaylandSurfaceFactory() = default;

std::unique_ptr<SurfaceOzoneCanvas>
WaylandSurfaceFactory::CreateCanvasForWidget(gfx::AcceleratedWidget widget) {
  return std::make_unique<WaylandCanvasSurface>(buffer_manager_, widget);
}

std::vector<gl::GLImplementationParts>
WaylandSurfaceFactory::GetAllowedGLImplementations() {
  std::vector<gl::GLImplementationParts> impls;
  if (egl_implementation_) {
    impls.emplace_back(
        gl::GLImplementationParts(gl::kGLImplementationEGLGLES2));
    impls.emplace_back(
        gl::GLImplementationParts(gl::kGLImplementationSwiftShaderGL));
    impls.emplace_back(
        gl::GLImplementationParts(gl::ANGLEImplementation::kSwiftShader));
  }
  return impls;
}

GLOzone* WaylandSurfaceFactory::GetGLOzone(
    const gl::GLImplementationParts& implementation) {
  switch (implementation.gl) {
    case gl::kGLImplementationEGLGLES2:
    case gl::kGLImplementationSwiftShaderGL:
    case gl::kGLImplementationEGLANGLE:
      return egl_implementation_.get();
    default:
      return nullptr;
  }
}

#if BUILDFLAG(ENABLE_VULKAN)
std::unique_ptr<gpu::VulkanImplementation>
WaylandSurfaceFactory::CreateVulkanImplementation(bool use_swiftshader,
                                                  bool allow_protected_memory) {
  return std::make_unique<VulkanImplementationWayland>(use_swiftshader);
}
#endif

scoped_refptr<gfx::NativePixmap> WaylandSurfaceFactory::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    VkDevice vk_device,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    absl::optional<gfx::Size> framebuffer_size) {
  if (framebuffer_size &&
      !gfx::Rect(size).Contains(gfx::Rect(*framebuffer_size))) {
    return nullptr;
  }
#if defined(WAYLAND_GBM)
  scoped_refptr<GbmPixmapWayland> pixmap =
      base::MakeRefCounted<GbmPixmapWayland>(buffer_manager_);

  if (!pixmap->InitializeBuffer(widget, size, format, usage, framebuffer_size))
    return nullptr;
  return pixmap;
#else
  return nullptr;
#endif
}

void WaylandSurfaceFactory::CreateNativePixmapAsync(
    gfx::AcceleratedWidget widget,
    VkDevice vk_device,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    NativePixmapCallback callback) {
  // CreateNativePixmap is non-blocking operation. Thus, it is safe to call it
  // and return the result with the provided callback.
  std::move(callback).Run(
      CreateNativePixmap(widget, vk_device, size, format, usage));
}

scoped_refptr<gfx::NativePixmap>
WaylandSurfaceFactory::CreateNativePixmapFromHandle(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::NativePixmapHandle handle) {
#if defined(WAYLAND_GBM)
  scoped_refptr<GbmPixmapWayland> pixmap =
      base::MakeRefCounted<GbmPixmapWayland>(buffer_manager_);

  if (!pixmap->InitializeBufferFromHandle(widget, size, format,
                                          std::move(handle)))
    return nullptr;
  return pixmap;
#else
  return nullptr;
#endif
}

}  // namespace ui
