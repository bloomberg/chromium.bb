// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_SURFACE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ui/gl/gl_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class WaylandConnection;

class WaylandSurfaceFactory : public SurfaceFactoryOzone {
 public:
  explicit WaylandSurfaceFactory(WaylandConnection* connection);
  ~WaylandSurfaceFactory() override;

  // SurfaceFactoryOzone:
  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gl::GLImplementation implementation,
      gfx::AcceleratedWidget widget) override;
  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      gl::GLImplementation implementation,
      const gfx::Size& size) override;
  intptr_t GetNativeDisplay() override;
  bool LoadEGLGLES2Bindings() override;
  std::unique_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget) override;
  scoped_refptr<NativePixmap> CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override;
  scoped_refptr<NativePixmap> CreateNativePixmapFromHandle(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      gfx::BufferFormat format,
      const gfx::NativePixmapHandle& handle) override;

 private:
  WaylandConnection* connection_;

  DISALLOW_COPY_AND_ASSIGN(WaylandSurfaceFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_SURFACE_FACTORY_H_
