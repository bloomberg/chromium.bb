// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_factory.h"

#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace gl {
namespace init {

// TODO(fuchsia): Implement these functions.

std::vector<GLImplementation> GetAllowedGLImplementations() {
  NOTIMPLEMENTED();
  return std::vector<GLImplementation>();
}

bool GetGLWindowSystemBindingInfo(GLWindowSystemBindingInfo* info) {
  NOTIMPLEMENTED();
  return false;
}

scoped_refptr<GLContext> CreateGLContext(GLShareGroup* share_group,
                                         GLSurface* compatible_surface,
                                         const GLContextAttribs& attribs) {
  NOTIMPLEMENTED();
  return nullptr;
}

scoped_refptr<GLSurface> CreateViewGLSurface(gfx::AcceleratedWidget window) {
  NOTIMPLEMENTED();
  return nullptr;
}

scoped_refptr<GLSurface> CreateOffscreenGLSurfaceWithFormat(
    const gfx::Size& size,
    GLSurfaceFormat format) {
  NOTIMPLEMENTED();
  return nullptr;
}

void SetDisabledExtensionsPlatform(const std::string& disabled_extensions) {
  NOTIMPLEMENTED();
}

bool InitializeExtensionSettingsOneOffPlatform() {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace init
}  // namespace gl
