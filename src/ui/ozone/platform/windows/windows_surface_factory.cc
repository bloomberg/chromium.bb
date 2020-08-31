// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/windows/windows_surface_factory.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/vsync_provider_win.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/common/gl_ozone_egl.h"
#include "ui/ozone/platform/windows/windows_window.h"
#include "ui/ozone/platform/windows/windows_window_manager.h"

namespace ui {

namespace {

class GLOzoneEGLWindows : public GLOzoneEGL {
 public:
  GLOzoneEGLWindows() = default;
  ~GLOzoneEGLWindows() override = default;

  // GLOzone:
  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gfx::AcceleratedWidget window) override {
    return InitializeGLSurface(base::MakeRefCounted<gl::NativeViewGLSurfaceEGL>(
        window, std::make_unique<gl::VSyncProviderWin>(window)));
  }

  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      const gfx::Size& size) override {
    return gl::InitializeGLSurface(
        base::MakeRefCounted<gl::PbufferGLSurfaceEGL>(size));
  }

 protected:
  // GLOzoneEGL:
  gl::EGLDisplayPlatform GetNativeDisplay() override {
    return gl::EGLDisplayPlatform(GetWindowDC(nullptr));
  }

  bool LoadGLES2Bindings(gl::GLImplementation implementation) override {
    return LoadDefaultEGLGLES2Bindings(implementation);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GLOzoneEGLWindows);
};

}  // namespace

WindowsSurfaceFactory::WindowsSurfaceFactory()
    : egl_implementation_(std::make_unique<GLOzoneEGLWindows>()) {}

WindowsSurfaceFactory::~WindowsSurfaceFactory() = default;

std::vector<gl::GLImplementation>
WindowsSurfaceFactory::GetAllowedGLImplementations() {
  return std::vector<gl::GLImplementation>{gl::kGLImplementationEGLGLES2,
                                           gl::kGLImplementationSwiftShaderGL};
}

GLOzone* WindowsSurfaceFactory::GetGLOzone(
    gl::GLImplementation implementation) {
  switch (implementation) {
    case gl::kGLImplementationSwiftShaderGL:
    case gl::kGLImplementationEGLGLES2:
      return egl_implementation_.get();
    default:
      return nullptr;
  }
}

}  // namespace ui
