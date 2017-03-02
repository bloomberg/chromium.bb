// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/surface_factory_ozone.h"

#include <stdlib.h>

#include "base/command_line.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace ui {

SurfaceFactoryOzone::SurfaceFactoryOzone() {}

SurfaceFactoryOzone::~SurfaceFactoryOzone() {}

std::vector<gl::GLImplementation>
SurfaceFactoryOzone::GetAllowedGLImplementations() {
  return std::vector<gl::GLImplementation>{gl::kGLImplementationOSMesaGL};
}

GLOzone* SurfaceFactoryOzone::GetGLOzone(gl::GLImplementation implementation) {
  return nullptr;
}

std::unique_ptr<SurfaceOzoneCanvas> SurfaceFactoryOzone::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  return nullptr;
}

std::vector<gfx::BufferFormat> SurfaceFactoryOzone::GetScanoutFormats(
    gfx::AcceleratedWidget widget) {
  return std::vector<gfx::BufferFormat>();
}

scoped_refptr<ui::NativePixmap> SurfaceFactoryOzone::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  return nullptr;
}

scoped_refptr<ui::NativePixmap>
SurfaceFactoryOzone::CreateNativePixmapFromHandle(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    const gfx::NativePixmapHandle& handle) {
  return nullptr;
}

}  // namespace ui
