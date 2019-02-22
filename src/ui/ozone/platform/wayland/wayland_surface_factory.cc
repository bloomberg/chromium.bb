// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_surface_factory.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <wayland-client.h>

#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/common/gl_ozone_egl.h"
#include "ui/ozone/platform/wayland/gl_surface_wayland.h"
#include "ui/ozone/platform/wayland/gpu/wayland_connection_proxy.h"
#include "ui/ozone/platform/wayland/wayland_object.h"
#include "ui/ozone/platform/wayland/wayland_window.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

#if defined(WAYLAND_GBM)
#include "ui/ozone/platform/wayland/gpu/gbm_pixmap_wayland.h"
#include "ui/ozone/platform/wayland/gpu/gbm_surfaceless_wayland.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace ui {

static void DeleteSharedMemory(void* pixels, void* context) {
  delete static_cast<base::SharedMemory*>(context);
}

class WaylandCanvasSurface : public SurfaceOzoneCanvas {
 public:
  WaylandCanvasSurface(WaylandConnectionProxy* connection,
                       WaylandWindow* window_);
  ~WaylandCanvasSurface() override;

  // SurfaceOzoneCanvas
  sk_sp<SkSurface> GetSurface() override;
  void ResizeCanvas(const gfx::Size& viewport_size) override;
  void PresentCanvas(const gfx::Rect& damage) override;
  std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() override;

 private:
  WaylandConnectionProxy* connection_;
  WaylandWindow* window_;

  gfx::Size size_;
  sk_sp<SkSurface> sk_surface_;
  wl::Object<wl_shm_pool> pool_;
  wl::Object<wl_buffer> buffer_;

  DISALLOW_COPY_AND_ASSIGN(WaylandCanvasSurface);
};

WaylandCanvasSurface::WaylandCanvasSurface(WaylandConnectionProxy* connection,
                                           WaylandWindow* window)
    : connection_(connection),
      window_(window),
      size_(window->GetBounds().size()) {}

WaylandCanvasSurface::~WaylandCanvasSurface() {}

sk_sp<SkSurface> WaylandCanvasSurface::GetSurface() {
  if (sk_surface_)
    return sk_surface_;

  size_t length = size_.width() * size_.height() * 4;
  auto shared_memory = base::WrapUnique(new base::SharedMemory);
  if (!shared_memory->CreateAndMapAnonymous(length))
    return nullptr;

  wl::Object<wl_shm_pool> pool(wl_shm_create_pool(
      connection_->shm(), shared_memory->handle().GetHandle(), length));
  if (!pool)
    return nullptr;
  wl::Object<wl_buffer> buffer(
      wl_shm_pool_create_buffer(pool.get(), 0, size_.width(), size_.height(),
                                size_.width() * 4, WL_SHM_FORMAT_ARGB8888));
  if (!buffer)
    return nullptr;

  sk_surface_ = SkSurface::MakeRasterDirectReleaseProc(
      SkImageInfo::MakeN32Premul(size_.width(), size_.height()),
      shared_memory->memory(), size_.width() * 4, &DeleteSharedMemory,
      shared_memory.get(), nullptr);
  if (!sk_surface_)
    return nullptr;
  pool_ = std::move(pool);
  buffer_ = std::move(buffer);
  (void)shared_memory.release();
  return sk_surface_;
}

void WaylandCanvasSurface::ResizeCanvas(const gfx::Size& viewport_size) {
  if (size_ == viewport_size)
    return;
  // TODO(forney): We could implement more efficient resizes by allocating
  // buffers rounded up to larger sizes, and then reusing them if the new size
  // still fits (but still reallocate if the new size is much smaller than the
  // old size).
  if (sk_surface_) {
    sk_surface_.reset();
    buffer_.reset();
    pool_.reset();
  }
  size_ = viewport_size;
}

void WaylandCanvasSurface::PresentCanvas(const gfx::Rect& damage) {
  // TODO(forney): This is just a naive implementation that allows chromium to
  // draw to the buffer at any time, even if it is being used by the Wayland
  // compositor. Instead, we should track buffer releases and frame callbacks
  // from Wayland to ensure perfect frames (while minimizing copies).
  wl_surface* surface = window_->surface();
  wl_surface_damage(surface, damage.x(), damage.y(), damage.width(),
                    damage.height());
  wl_surface_attach(surface, buffer_.get(), 0, 0);
  wl_surface_commit(surface);
  connection_->ScheduleFlush();
}

std::unique_ptr<gfx::VSyncProvider>
WaylandCanvasSurface::CreateVSyncProvider() {
  // TODO(forney): This can be implemented with information from frame
  // callbacks, and possibly output refresh rate.
  NOTIMPLEMENTED();
  return nullptr;
}

namespace {

class GLOzoneEGLWayland : public GLOzoneEGL {
 public:
  explicit GLOzoneEGLWayland(WaylandConnectionProxy* connection)
      : connection_(connection) {}
  ~GLOzoneEGLWayland() override {}

  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gfx::AcceleratedWidget widget) override;

  scoped_refptr<gl::GLSurface> CreateSurfacelessViewGLSurface(
      gfx::AcceleratedWidget window) override {
#if defined(WAYLAND_GBM)
    // If there is a gbm device available, use surfaceless gl surface.
    if (!connection_->gbm_device())
      return nullptr;
    return gl::InitializeGLSurface(new GbmSurfacelessWayland(
        static_cast<WaylandSurfaceFactory*>(
            OzonePlatform::GetInstance()->GetSurfaceFactoryOzone()),
        window));
#else
    return nullptr;
#endif
  }

  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      const gfx::Size& size) override;

 protected:
  intptr_t GetNativeDisplay() override;
  bool LoadGLES2Bindings(gl::GLImplementation impl) override;

 private:
  WaylandConnectionProxy* connection_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(GLOzoneEGLWayland);
};

scoped_refptr<gl::GLSurface> GLOzoneEGLWayland::CreateViewGLSurface(
    gfx::AcceleratedWidget widget) {
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
    egl_implementation_ = std::make_unique<GLOzoneEGLWayland>(connection_);
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
  if (!connection_)
    return nullptr;
  WaylandWindow* window = connection_->GetWindow(widget);
  DCHECK(window);
  return std::make_unique<WaylandCanvasSurface>(connection_, window);
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
