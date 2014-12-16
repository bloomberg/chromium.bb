// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/timer/timer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_surface.h"
#include "ui/ozone/demo/gl_renderer.h"
#include "ui/ozone/demo/software_renderer.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ui_thread_gpu.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"

const int kTestWindowWidth = 800;
const int kTestWindowHeight = 600;

const int kFrameDelayMilliseconds = 16;

const int kAnimationSteps = 240;

const char kDisableGpu[] = "disable-gpu";

const char kWindowSize[] = "window-size";

class DemoWindow : public ui::PlatformWindowDelegate {
 public:
  DemoWindow() : widget_(gfx::kNullAcceleratedWidget) {
    int width = kTestWindowWidth;
    int height = kTestWindowHeight;
    sscanf(CommandLine::ForCurrentProcess()
               ->GetSwitchValueASCII(kWindowSize)
               .c_str(),
           "%dx%d", &width, &height);

    platform_window_ = ui::OzonePlatform::GetInstance()->CreatePlatformWindow(
        this, gfx::Rect(width, height));
  }
  ~DemoWindow() override {}

  gfx::AcceleratedWidget GetAcceleratedWidget() {
    // TODO(spang): We should start rendering asynchronously.
    DCHECK_NE(widget_, gfx::kNullAcceleratedWidget)
        << "Widget not available synchronously";
    return widget_;
  }

  gfx::Size GetSize() { return platform_window_->GetBounds().size(); }

  void Start() {
    if (!CommandLine::ForCurrentProcess()->HasSwitch(kDisableGpu) &&
        gfx::GLSurface::InitializeOneOff() && StartInProcessGpu()) {
      renderer_.reset(new ui::GlRenderer(GetAcceleratedWidget(), GetSize()));
    } else {
      renderer_.reset(
          new ui::SoftwareRenderer(GetAcceleratedWidget(), GetSize()));
    }

    if (renderer_->Initialize()) {
      timer_.Start(FROM_HERE,
                   base::TimeDelta::FromMicroseconds(kFrameDelayMilliseconds),
                   renderer_.get(), &ui::Renderer::RenderFrame);
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
   void OnBoundsChanged(const gfx::Rect& new_bounds) override {}
   void OnDamageRect(const gfx::Rect& damaged_region) override {}
   void DispatchEvent(ui::Event* event) override {}
   void OnCloseRequest() override { Quit(); }
   void OnClosed() override {}
   void OnWindowStateChanged(ui::PlatformWindowState new_state) override {}
   void OnLostCapture() override {}
   void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override {
    DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
    widget_ = widget;
  }
  void OnActivationChanged(bool active) override {}

 private:

  void StopAnimation() { timer_.Stop(); }

  bool StartInProcessGpu() { return ui_thread_gpu_.Initialize(); }

  scoped_ptr<ui::Renderer> renderer_;

  // Timer for animation.
  base::RepeatingTimer<ui::Renderer> timer_;

  // Window-related state.
  scoped_ptr<ui::PlatformWindow> platform_window_;
  gfx::AcceleratedWidget widget_;

  // Helper for applications that do GL on main thread.
  ui::UiThreadGpu ui_thread_gpu_;

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
