// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_surface_factory.h"

#include <memory>

#include "gpu/vulkan/buildflags.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/linux/gpu_memory_buffer_support_x11.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/x/connection.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_egl_x11_gles2.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/common/gl_ozone_egl.h"
#include "ui/ozone/platform/x11/gl_ozone_glx.h"
#include "ui/ozone/platform/x11/gl_surface_egl_readback_x11.h"
#include "ui/ozone/platform/x11/x11_canvas_surface.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/x/vulkan_implementation_x11.h"
#endif

namespace ui {
namespace {

class GLOzoneEGLX11 : public GLOzoneEGL {
 public:
  GLOzoneEGLX11() = default;

  GLOzoneEGLX11(const GLOzoneEGLX11&) = delete;
  GLOzoneEGLX11& operator=(const GLOzoneEGLX11&) = delete;

  ~GLOzoneEGLX11() override = default;

  // GLOzone:
  bool InitializeStaticGLBindings(
      const gl::GLImplementationParts& implementation) override {
    is_swiftshader_ = gl::IsSoftwareGLImplementation(implementation);
    return GLOzoneEGL::InitializeStaticGLBindings(implementation);
  }

  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gfx::AcceleratedWidget window) override {
    if (is_swiftshader_) {
      return gl::InitializeGLSurface(
          base::MakeRefCounted<GLSurfaceEglReadbackX11>(window));
    } else {
      switch (gl::GetGLImplementation()) {
        case gl::kGLImplementationEGLGLES2:
          DCHECK(window != gfx::kNullAcceleratedWidget);
          return gl::InitializeGLSurface(new gl::NativeViewGLSurfaceEGLX11GLES2(
              static_cast<x11::Window>(window)));
        case gl::kGLImplementationEGLANGLE:
          DCHECK(window != gfx::kNullAcceleratedWidget);
          return gl::InitializeGLSurface(new gl::NativeViewGLSurfaceEGLX11(
              static_cast<x11::Window>(window)));
        default:
          NOTREACHED();
          return nullptr;
      }
    }
  }

  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      const gfx::Size& size) override {
    if (gl::GLSurfaceEGL::IsEGLSurfacelessContextSupported() &&
        size.width() == 0 && size.height() == 0) {
      return InitializeGLSurface(new gl::SurfacelessEGL(size));
    } else {
      return InitializeGLSurface(new gl::PbufferGLSurfaceEGL(size));
    }
  }

 protected:
  // GLOzoneEGL:
  gl::EGLDisplayPlatform GetNativeDisplay() override {
    return gl::EGLDisplayPlatform(reinterpret_cast<EGLNativeDisplayType>(
        x11::Connection::Get()->GetXlibDisplay().display()));
  }

  bool LoadGLES2Bindings(
      const gl::GLImplementationParts& implementation) override {
    return LoadDefaultEGLGLES2Bindings(implementation);
  }

 private:
  bool is_swiftshader_ = false;
};

}  // namespace

X11SurfaceFactory::X11SurfaceFactory(
    std::unique_ptr<x11::Connection> connection)
    : glx_implementation_(std::make_unique<GLOzoneGLX>()),
      egl_implementation_(std::make_unique<GLOzoneEGLX11>()),
      connection_(std::move(connection)) {}

X11SurfaceFactory::~X11SurfaceFactory() = default;

std::vector<gl::GLImplementationParts>
X11SurfaceFactory::GetAllowedGLImplementations() {
  return std::vector<gl::GLImplementationParts>{
      gl::GLImplementationParts(gl::kGLImplementationDesktopGL),
      gl::GLImplementationParts(gl::kGLImplementationEGLGLES2),
      gl::GLImplementationParts(gl::kGLImplementationEGLANGLE),
      gl::GLImplementationParts(gl::kGLImplementationSwiftShaderGL)};
}

GLOzone* X11SurfaceFactory::GetGLOzone(
    const gl::GLImplementationParts& implementation) {
  switch (implementation.gl) {
    case gl::kGLImplementationDesktopGL:
      return glx_implementation_.get();
    case gl::kGLImplementationEGLGLES2:
    case gl::kGLImplementationEGLANGLE:
    case gl::kGLImplementationSwiftShaderGL:
      return egl_implementation_.get();
    default:
      return nullptr;
  }
}

#if BUILDFLAG(ENABLE_VULKAN)
std::unique_ptr<gpu::VulkanImplementation>
X11SurfaceFactory::CreateVulkanImplementation(bool use_swiftshader,
                                              bool allow_protected_memory) {
  return std::make_unique<gpu::VulkanImplementationX11>(use_swiftshader);
}
#endif

std::unique_ptr<SurfaceOzoneCanvas> X11SurfaceFactory::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  // X11SoftwareBitmapPresenter (created via X11CanvasSurface) requres a
  // Connection TLS instance and a PlatformEventSource.
  if (connection_) {
    auto* connection = connection_.get();
    x11::Connection::Set(std::move(connection_));
    connection->platform_event_source =
        std::make_unique<X11EventSource>(connection);
  }
  return std::make_unique<X11CanvasSurface>(widget);
}

scoped_refptr<gfx::NativePixmap> X11SurfaceFactory::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    VkDevice vk_device,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    absl::optional<gfx::Size> framebuffer_size) {
  scoped_refptr<gfx::NativePixmapDmaBuf> pixmap;
  auto buffer = ui::GpuMemoryBufferSupportX11::GetInstance()->CreateBuffer(
      format, size, usage);
  if (buffer) {
    gfx::NativePixmapHandle handle = buffer->ExportHandle();
    pixmap = base::MakeRefCounted<gfx::NativePixmapDmaBuf>(size, format,
                                                           std::move(handle));
  }

  // CreateNativePixmap is non-blocking operation. Thus, it is safe to call it
  // and return the result with the provided callback.
  return pixmap;
}

void X11SurfaceFactory::CreateNativePixmapAsync(gfx::AcceleratedWidget widget,
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

}  // namespace ui
