// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/timer/timer.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"

const int kTestWindowWidth = 800;
const int kTestWindowHeight = 600;

const int kFrameDelayMilliseconds = 16;

const int kAnimationSteps = 240;

const char kDisableGpu[] = "disable-gpu";

class DemoWindow : public ui::PlatformWindowDelegate {
 public:
  DemoWindow() : widget_(gfx::kNullAcceleratedWidget), iteration_(0) {
    platform_window_ = ui::OzonePlatform::GetInstance()->CreatePlatformWindow(
        this, gfx::Rect(kTestWindowWidth, kTestWindowHeight));
  }
  virtual ~DemoWindow() {}

  gfx::AcceleratedWidget GetAcceleratedWidget() {
    // TODO(spang): We should start rendering asynchronously.
    DCHECK_NE(widget_, gfx::kNullAcceleratedWidget)
        << "Widget not available synchronously";
    return widget_;
  }

  gfx::Size GetSize() { return platform_window_->GetBounds().size(); }

  void Start() {
    if (!CommandLine::ForCurrentProcess()->HasSwitch(kDisableGpu) &&
        gfx::GLSurface::InitializeOneOff() && InitializeGLSurface()) {
      StartAnimationGL();
    } else if (InitializeSoftwareSurface()) {
      StartAnimationSoftware();
    } else {
      LOG(ERROR) << "Failed to create drawing surface";
      Quit();
    }
  }

  void Quit() {
    StopAnimation();
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&base::DeletePointer<DemoWindow>, this));
   }

  // PlatformWindowDelegate:
  virtual void OnBoundsChanged(const gfx::Rect& new_bounds) OVERRIDE {}
  virtual void OnDamageRect(const gfx::Rect& damaged_region) OVERRIDE {}
  virtual void DispatchEvent(ui::Event* event) OVERRIDE {}
  virtual void OnCloseRequest() OVERRIDE {
    Quit();
  }
  virtual void OnClosed() OVERRIDE {}
  virtual void OnWindowStateChanged(
      ui::PlatformWindowState new_state) OVERRIDE {}
  virtual void OnLostCapture() OVERRIDE {}
  virtual void OnAcceleratedWidgetAvailable(
      gfx::AcceleratedWidget widget) OVERRIDE {
    DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
    widget_ = widget;
  }
  virtual void OnActivationChanged(bool active) OVERRIDE {}

 private:
  bool InitializeGLSurface() {
    surface_ = gfx::GLSurface::CreateViewGLSurface(GetAcceleratedWidget());
    if (!surface_) {
      LOG(ERROR) << "Failed to create GL surface";
      return false;
    }

    context_ = gfx::GLContext::CreateGLContext(
        NULL, surface_.get(), gfx::PreferIntegratedGpu);
    if (!context_) {
      LOG(ERROR) << "Failed to create GL context";
      surface_ = NULL;
      return false;
    }

    surface_->Resize(GetSize());

    if (!context_->MakeCurrent(surface_.get())) {
      LOG(ERROR) << "Failed to make GL context current";
      surface_ = NULL;
      context_ = NULL;
      return false;
    }

    return true;
  }

  bool InitializeSoftwareSurface() {
    software_surface_ =
        ui::SurfaceFactoryOzone::GetInstance()->CreateCanvasForWidget(
            GetAcceleratedWidget());
    if (!software_surface_) {
      LOG(ERROR) << "Failed to create software surface";
      return false;
    }

    software_surface_->ResizeCanvas(GetSize());
    return true;
  }

  void StartAnimationGL() {
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromMicroseconds(kFrameDelayMilliseconds),
                 this,
                 &DemoWindow::RenderFrameGL);
  }

  void StartAnimationSoftware() {
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromMicroseconds(kFrameDelayMilliseconds),
                 this,
                 &DemoWindow::RenderFrameSoftware);
  }

  void StopAnimation() { timer_.Stop(); }

  float NextFraction() {
    float fraction = (sinf(iteration_ * 2 * M_PI / kAnimationSteps) + 1) / 2;

    iteration_++;
    iteration_ %= kAnimationSteps;

    return fraction;
  }

  void RenderFrameGL() {
    float fraction = NextFraction();
    gfx::Size window_size = GetSize();

    glViewport(0, 0, window_size.width(), window_size.height());
    glClearColor(1 - fraction, fraction, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!surface_->SwapBuffers())
      LOG(FATAL) << "Failed to swap buffers";
  }

  void RenderFrameSoftware() {
    float fraction = NextFraction();
    gfx::Size window_size = GetSize();

    skia::RefPtr<SkCanvas> canvas = software_surface_->GetCanvas();

    SkColor color =
        SkColorSetARGB(0xff, 0, 0xff * fraction, 0xff * (1 - fraction));

    canvas->clear(color);

    software_surface_->PresentCanvas(gfx::Rect(window_size));
  }

  // Timer for animation.
  base::RepeatingTimer<DemoWindow> timer_;

  // Bits for GL rendering.
  scoped_refptr<gfx::GLSurface> surface_;
  scoped_refptr<gfx::GLContext> context_;

  // Bits for software rendeirng.
  scoped_ptr<ui::SurfaceOzoneCanvas> software_surface_;

  // Window-related state.
  scoped_ptr<ui::PlatformWindow> platform_window_;
  gfx::AcceleratedWidget widget_;

  // Animation state.
  int iteration_;

  DISALLOW_COPY_AND_ASSIGN(DemoWindow);
};

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  base::AtExitManager exit_manager;

  // Build UI thread message loop. This is used by platform
  // implementations for event polling & running background tasks.
  base::MessageLoopForUI message_loop;

  ui::OzonePlatform::InitializeForUI();

  DemoWindow* window = new DemoWindow;
  window->Start();

  // Run the message loop until there's nothing left to do.
  // TODO(spang): Should we use QuitClosure instead?
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  return 0;
}
