// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/demo/gl_renderer.h"

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace ui {

GlRenderer::GlRenderer(gfx::AcceleratedWidget widget, const gfx::Size& size)
    : RendererBase(widget, size) {
}

GlRenderer::~GlRenderer() {
}

bool GlRenderer::Initialize() {
  surface_ = CreateSurface();
  if (!surface_.get()) {
    LOG(ERROR) << "Failed to create GL surface";
    return false;
  }

  context_ = gfx::GLContext::CreateGLContext(NULL, surface_.get(),
                                             gfx::PreferIntegratedGpu);
  if (!context_.get()) {
    LOG(ERROR) << "Failed to create GL context";
    return false;
  }

  surface_->Resize(size_);

  if (!context_->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "Failed to make GL context current";
    return false;
  }

  return true;
}

void GlRenderer::RenderFrame() {
  float fraction = NextFraction();

  context_->MakeCurrent(surface_.get());

  glViewport(0, 0, size_.width(), size_.height());
  glClearColor(1 - fraction, fraction, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (!surface_->SwapBuffers())
    LOG(FATAL) << "Failed to swap buffers";
}

scoped_refptr<gfx::GLSurface> GlRenderer::CreateSurface() {
  return gfx::GLSurface::CreateViewGLSurface(widget_);
}

}  // namespace ui
