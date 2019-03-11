// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_surface_factory.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "ui/gfx/linux/client_native_pixmap_dmabuf.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/common/gl_ozone_egl.h"
#include "ui/ozone/platform/wayland/gl_surface_wayland.h"
#include "ui/ozone/platform/wayland/gpu/wayland_canvas_surface.h"
#include "ui/ozone/platform/wayland/gpu/wayland_connection_proxy.h"
#include "ui/ozone/platform/wayland/wayland_object.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

#if defined(WAYLAND_GBM)
#include "ui/ozone/platform/wayland/gpu/gbm_pixmap_wayland.h"
#include "ui/ozone/platform/wayland/gpu/gbm_surfaceless_wayland.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace ui {

namespace {

class GLOzoneEGLWayland : public GLOzoneEGL {
 public:
  GLOzoneEGLWayland(WaylandConnectionProxy* connection,
                    WaylandSurfaceFactory* factory)
      : connection_(connection), factory_(factory) {}
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
  WaylandConnectionProxy* const connection_;
  WaylandSurfaceFactory* const factory_;

  DISALLOW_COPY_AND_ASSIGN(GLOzoneEGLWayland);
};

scoped_refptr<gl::GLSurface> GLOzoneEGLWayland::CreateViewGLSurface(
    gfx::AcceleratedWidget widget) {
  // Only EGLGLES2 is supported with surfaceless view gl.
  if (gl::GetGLImplementation() != gl::kGLImplementationEGLGLES2)
    return nullptr;

  DCHECK(connection_);
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
  if (!connection_->gbm_device())
    return nullptr;
  return gl::InitializeGLSurface(new GbmSurfacelessWayland(factory_, window));
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
  return connection_->Display();
}

bool GLOzoneEGLWayland::LoadGLES2Bindings(gl::GLImplementation impl) {
  // TODO: It may not be necessary to set this environment variable when using
  // swiftshader.
  setenv("EGL_PLATFORM", "wayland", 0);
  return LoadDefaultEGLGLES2Bindings(impl);
}

}  // namespace

WaylandSurfaceFactory::WaylandSurfaceFactory(WaylandConnectionProxy* connection)
    : connection_(connection) {
  if (connection_)
    egl_implementation_ =
        std::make_unique<GLOzoneEGLWayland>(connection_, this);
}

WaylandSurfaceFactory::~WaylandSurfaceFactory() {}

void WaylandSurfaceFactory::RegisterSurface(gfx::AcceleratedWidget widget,
                                            GbmSurfacelessWayland* surface) {
  widget_to_surface_map_.insert(std::make_pair(widget, surface));
}

void WaylandSurfaceFactory::UnregisterSurface(gfx::AcceleratedWidget widget) {
  widget_to_surface_map_.erase(widget);
}

GbmSurfacelessWayland* WaylandSurfaceFactory::GetSurface(
    gfx::AcceleratedWidget widget) const {
  auto it = widget_to_surface_map_.find(widget);
  DCHECK(it != widget_to_surface_map_.end());
  return it->second;
}

void WaylandSurfaceFactory::ScheduleBufferSwap(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::Rect& damage_region,
    wl::BufferSwapCallback callback) {
  connection_->ScheduleBufferSwap(widget, buffer_id, damage_region,
                                  std::move(callback));
}

std::unique_ptr<SurfaceOzoneCanvas>
WaylandSurfaceFactory::CreateCanvasForWidget(gfx::AcceleratedWidget widget) {
  return std::make_unique<WaylandCanvasSurface>(connection_, widget);
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
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
#if defined(WAYLAND_GBM)
  scoped_refptr<GbmPixmapWayland> pixmap =
      base::MakeRefCounted<GbmPixmapWayland>(this, connection_);
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
    const gfx::NativePixmapHandle& handle) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace ui
