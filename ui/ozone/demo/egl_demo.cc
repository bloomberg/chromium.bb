// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  base::AtExitManager exit_manager;

  ui::OzonePlatform::InitializeForUI();
  if (!gfx::GLSurface::InitializeOneOff())
    LOG(FATAL) << "Failed to initialize GL";

  gfx::AcceleratedWidget widget =
      ui::SurfaceFactoryOzone::GetInstance()->GetAcceleratedWidget();
  scoped_refptr<gfx::GLSurface> surface =
      gfx::GLSurface::CreateViewGLSurface(widget);
  if (!surface)
    LOG(FATAL) << "Failed to create GL surface";

  scoped_refptr<gfx::GLContext> context = gfx::GLContext::CreateGLContext(
      NULL, surface.get(), gfx::PreferIntegratedGpu);
  if (!context)
    LOG(FATAL) << "Failed to create GL context";

  const gfx::Size window_size(800, 600);
  int iterations = 120;

  surface->Resize(window_size);
  if (!context->MakeCurrent(surface.get()))
    LOG(FATAL) << "Failed to make current on GL context";

  for (int i = 0; i < iterations; ++i) {
    glViewport(0, 0, window_size.width(), window_size.height());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    float fraction = static_cast<float>(i) / iterations;
    glClearColor(1 - fraction, fraction, 0.0, 1.0);

    if (!surface->SwapBuffers())
      LOG(FATAL) << "Failed to swap buffers";
  }

  return 0;
}
