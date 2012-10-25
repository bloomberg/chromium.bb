// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_gl_api_implementation.h"

namespace gfx {

RealGLApi* g_real_gl;

void InitializeGLBindingsGL() {
  g_driver_gl.InitializeBindings();
  if (!g_real_gl) {
    g_real_gl = new RealGLApi();
  }
  g_real_gl->Initialize(&g_driver_gl);
  g_current_gl_context = g_real_gl;
}

void InitializeGLExtensionBindingsGL(GLContext* context) {
  g_driver_gl.InitializeExtensionBindings(context);
}

void InitializeDebugGLBindingsGL() {
  g_driver_gl.InitializeDebugBindings();
}

void ClearGLBindingsGL() {
  if (g_real_gl) {
    delete g_real_gl;
    g_real_gl = NULL;
  }
  g_current_gl_context = NULL;
  g_driver_gl.ClearBindings();
}

GLApi::GLApi() {
}

GLApi::~GLApi() {
}

RealGLApi::RealGLApi() {
}

void RealGLApi::Initialize(DriverGL* driver) {
  driver_ = driver;
}

}  // namespace gfx


