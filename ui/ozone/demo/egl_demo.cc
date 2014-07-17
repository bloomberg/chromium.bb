// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"

const int kTestWindowWidth = 800;
const int kTestWindowHeight = 600;

class DemoWindow : public ui::PlatformWindowDelegate {
 public:
  DemoWindow() : widget_(gfx::kNullAcceleratedWidget) {
    platform_window_ = ui::OzonePlatform::GetInstance()->CreatePlatformWindow(
        this, gfx::Rect(kTestWindowWidth, kTestWindowHeight));
  }
  virtual ~DemoWindow() {}

  gfx::AcceleratedWidget GetAcceleratedWidget() {
    // TODO(spang): We should start rendering asynchronously.
    CHECK_NE(widget_, gfx::kNullAcceleratedWidget)
        << "widget not available synchronously";
    return widget_;
  }

  gfx::Size GetSize() { return platform_window_->GetBounds().size(); }

  // PlatformWindowDelegate:
  virtual void OnBoundsChanged(const gfx::Rect& new_bounds) OVERRIDE {}
  virtual void OnDamageRect(const gfx::Rect& damaged_region) OVERRIDE {}
  virtual void DispatchEvent(ui::Event* event) OVERRIDE {}
  virtual void OnCloseRequest() OVERRIDE {}
  virtual void OnClosed() OVERRIDE {}
  virtual void OnWindowStateChanged(
      ui::PlatformWindowState new_state) OVERRIDE {}
  virtual void OnLostCapture() OVERRIDE {}
  virtual void OnAcceleratedWidgetAvailable(
      gfx::AcceleratedWidget widget) OVERRIDE {
    CHECK_NE(widget, gfx::kNullAcceleratedWidget);
    widget_ = widget;
  }

 private:
  scoped_ptr<ui::PlatformWindow> platform_window_;
  gfx::AcceleratedWidget widget_;

  DISALLOW_COPY_AND_ASSIGN(DemoWindow);
};

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  base::AtExitManager exit_manager;

  base::MessageLoopForUI message_loop;

  ui::OzonePlatform::InitializeForUI();
  if (!gfx::GLSurface::InitializeOneOff())
    LOG(FATAL) << "Failed to initialize GL";

  DemoWindow window;

  scoped_refptr<gfx::GLSurface> surface =
      gfx::GLSurface::CreateViewGLSurface(window.GetAcceleratedWidget());
  if (!surface)
    LOG(FATAL) << "Failed to create GL surface";

  scoped_refptr<gfx::GLContext> context = gfx::GLContext::CreateGLContext(
      NULL, surface.get(), gfx::PreferIntegratedGpu);
  if (!context)
    LOG(FATAL) << "Failed to create GL context";

  gfx::Size window_size = window.GetSize();

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
