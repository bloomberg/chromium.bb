// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_wgl_api_implementation.h"

namespace gfx {

RealWGLApi g_real_wgl;

void InitializeGLBindingsWGL() {
  g_driver_wgl.InitializeBindings();
  g_real_wgl.Initialize(&g_driver_wgl);
  g_current_wgl_context = &g_real_wgl;
}

void InitializeGLExtensionBindingsWGL(GLContext* context) {
  g_driver_wgl.InitializeExtensionBindings(context);
}

void InitializeDebugGLBindingsWGL() {
  g_driver_wgl.InitializeDebugBindings();
}

void ClearGLBindingsWGL() {
  g_driver_wgl.ClearBindings();
}

WGLApi::WGLApi() {
}

WGLApi::~WGLApi() {
}

RealWGLApi::RealWGLApi() {
}

void RealWGLApi::Initialize(DriverWGL* driver) {
  driver_ = driver;
}

}  // namespace gfx


