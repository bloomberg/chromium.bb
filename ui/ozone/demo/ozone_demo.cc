// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/timer/timer.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom3/dom_code.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_surface.h"
#include "ui/ozone/demo/gl_renderer.h"
#include "ui/ozone/demo/software_renderer.h"
#include "ui/ozone/demo/surfaceless_gl_renderer.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/ozone/public/ui_thread_gpu.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"

const int kTestWindowWidth = 800;
const int kTestWindowHeight = 600;

const int kFrameDelayMilliseconds = 16;

const char kDisableGpu[] = "disable-gpu";

const char kWindowSize[] = "window-size";

class DemoWindow : public ui::PlatformWindowDelegate {
 public:
  DemoWindow() : widget_(gfx::kNullAcceleratedWidget) {
    int width = kTestWindowWidth;
    int height = kTestWindowHeight;
    sscanf(base::CommandLine::ForCurrentProcess()
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

  void Start(const base::Closure& quit_closure) {
    quit_closure_ = quit_closure;

    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(kDisableGpu) &&
        gfx::GLSurface::InitializeOneOff() && StartInProcessGpu()) {
      if (base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kOzoneUseSurfaceless)) {
        renderer_.reset(
            new ui::SurfacelessGlRenderer(GetAcceleratedWidget(), GetSize()));
      } else {
        renderer_.reset(new ui::GlRenderer(GetAcceleratedWidget(), GetSize()));
      }
    } else {
      renderer_.reset(
          new ui::SoftwareRenderer(GetAcceleratedWidget(), GetSize()));
    }

    if (renderer_->Initialize()) {
      timer_.Start(FROM_HERE,
                   base::TimeDelta::FromMilliseconds(kFrameDelayMilliseconds),
                   renderer_.get(), &ui::Renderer::RenderFrame);
    } else {
      LOG(ERROR) << "Failed to create drawing surface";
      Quit();
    }
  }

  void Quit() {
    StopAnimation();
    quit_closure_.Run();
   }

  // PlatformWindowDelegate:
   void OnBoundsChanged(const gfx::Rect& new_bounds) override {}
   void OnDamageRect(const gfx::Rect& damaged_region) override {}
   void DispatchEvent(ui::Event* event) override {
     if (event->IsKeyEvent() &&
         static_cast<ui::KeyEvent*>(event)->code() == ui::DomCode::KEY_Q)
       Quit();
   }
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

  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(DemoWindow);
};

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::AtExitManager exit_manager;

  // Build UI thread message loop. This is used by platform
  // implementations for event polling & running background tasks.
  base::MessageLoopForUI message_loop;

  ui::OzonePlatform::InitializeForUI();

  base::RunLoop run_loop;

  scoped_ptr<DemoWindow> window(new DemoWindow);
  window->Start(run_loop.QuitClosure());

  run_loop.Run();

  return 0;
}
