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
#include "ui/ozone/platform/wayland/gpu/gl_surface_wayland.h"
#include "ui/ozone/platform/wayland/gpu/wayland_canvas_surface.h"
#include "ui/ozone/platform/wayland/gpu/wayland_connection_proxy.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

#if defined(WAYLAND_GBM)
#include "ui/ozone/platform/wayland/gpu/gbm_pixmap_wayland.h"
#include "ui/ozone/platform/wayland/gpu/gbm_surfaceless_wayland.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace ui {

namespace {

class GLOzoneEGLWayland : public GLOzoneEGL {
 public:
  GLOzoneEGLWayland(WaylandConnection* connection,
                    WaylandConnectionProxy* connection_proxy,
                    WaylandSurfaceFactory* factory)
      : connection_(connection),
        connection_proxy_(connection_proxy),
        factory_(factory) {}
  ~GLOzoneEGLWayland() override {}

  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gfx::AcceleratedWidget widget) override;

  scoped_refptr<gl::GLSurface> CreateSurfacelessViewGLSurface(
      gfx::AcceleratedWidget window) override;

  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      const gfx::Size& size) override;

 protected:
  intptr_t GetNativeDisplay() override;
  bool LoadGLES2Bindings(gl::GLImplementation impl) override;

 private:
  WaylandConnection* const connection_;
  WaylandConnectionProxy* const connection_proxy_;
  WaylandSurfaceFactory* const factory_;

  DISALLOW_COPY_AND_ASSIGN(GLOzoneEGLWayland);
};

scoped_refptr<gl::GLSurface> GLOzoneEGLWayland::CreateViewGLSurface(
    gfx::AcceleratedWidget widget) {
  // Only EGLGLES2 is supported with surfaceless view gl.
  if ((gl::GetGLImplementation() != gl::kGLImplementationEGLGLES2) ||
      !connection_)
    return nullptr;

  WaylandWindow* window = connection_->GetWindow(widget);
  if (!window)
    return nullptr;

  // The wl_egl_window needs to be created before the GLSurface so it can be
  // used in the GLSurface constructor.
  auto egl_window = CreateWaylandEglWindow(window);
  if (!egl_window)
    return nullptr;
  return gl::InitializeGLSurface(new GLSurfaceWayland(std::move(egl_window)));
}

scoped_refptr<gl::GLSurface> GLOzoneEGLWayland::CreateSurfacelessViewGLSurface(
    gfx::AcceleratedWidget window) {
  DCHECK(factory_);

  // Only EGLGLES2 is supported with surfaceless view gl.
  if (gl::GetGLImplementation() != gl::kGLImplementationEGLGLES2)
    return nullptr;

#if defined(WAYLAND_GBM)
  // If there is a gbm device available, use surfaceless gl surface.
  if (!connection_proxy_->gbm_device())
    return nullptr;
  return gl::InitializeGLSurface(
      new GbmSurfacelessWayland(factory_, connection_proxy_, window));
#else
  return nullptr;
#endif
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

intptr_t GLOzoneEGLWayland::GetNativeDisplay() {
  if (connection_)
    return reinterpret_cast<intptr_t>(connection_->display());
  return EGL_DEFAULT_DISPLAY;
}

bool GLOzoneEGLWayland::LoadGLES2Bindings(gl::GLImplementation impl) {
  // TODO: It may not be necessary to set this environment variable when using
  // swiftshader.
  setenv("EGL_PLATFORM", "wayland", 0);
  return LoadDefaultEGLGLES2Bindings(impl);
}

}  // namespace

WaylandSurfaceFactory::WaylandSurfaceFactory(WaylandConnection* connection)
    : connection_(connection) {}

WaylandSurfaceFactory::~WaylandSurfaceFactory() = default;

void WaylandSurfaceFactory::SetProxy(WaylandConnectionProxy* proxy) {
  DCHECK(!connection_proxy_ && proxy);
  connection_proxy_ = proxy;

  egl_implementation_ =
      std::make_unique<GLOzoneEGLWayland>(connection_, connection_proxy_, this);
}

void WaylandSurfaceFactory::RegisterSurface(gfx::AcceleratedWidget widget,
                                            GbmSurfacelessWayland* surface) {
  widget_to_surface_map_.insert(std::make_pair(widget, surface));
}

void WaylandSurfaceFactory::UnregisterSurface(gfx::AcceleratedWidget widget) {
  widget_to_surface_map_.erase(widget);
}

GbmSurfacelessWayland* WaylandSurfaceFactory::GetSurface(
    gfx::AcceleratedWidget widget) const {
  GbmSurfacelessWayland* surface = nullptr;
  auto it = widget_to_surface_map_.find(widget);
  if (it != widget_to_surface_map_.end())
    surface = it->second;
  return surface;
}


std::unique_ptr<SurfaceOzoneCanvas>
WaylandSurfaceFactory::CreateCanvasForWidget(gfx::AcceleratedWidget widget) {
  return std::make_unique<WaylandCanvasSurface>(connection_proxy_, widget);
}

std::vector<gl::GLImplementation>
WaylandSurfaceFactory::GetAllowedGLImplementations() {
  std::vector<gl::GLImplementation> impls;
  if (egl_implementation_) {
    impls.push_back(gl::kGLImplementationEGLGLES2);
    impls.push_back(gl::kGLImplementationSwiftShaderGL);
  }
  return impls;
}

GLOzone* WaylandSurfaceFactory::GetGLOzone(
    gl::GLImplementation implementation) {
  switch (implementation) {
    case gl::kGLImplementationEGLGLES2:
    case gl::kGLImplementationSwiftShaderGL:
      return egl_implementation_.get();
    default:
      return nullptr;
  }
}

scoped_refptr<gfx::NativePixmap> WaylandSurfaceFactory::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    VkDevice vk_device,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
#if defined(WAYLAND_GBM)
  scoped_refptr<GbmPixmapWayland> pixmap =
      base::MakeRefCounted<GbmPixmapWayland>(this, connection_proxy_, widget);
  if (!pixmap->InitializeBuffer(size, format, usage))
    return nullptr;
  return pixmap;
#else
  return nullptr;
#endif
}

scoped_refptr<gfx::NativePixmap>
WaylandSurfaceFactory::CreateNativePixmapFromHandle(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::NativePixmapHandle handle) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace ui
