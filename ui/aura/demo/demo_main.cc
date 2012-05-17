// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/aura/client/stacking_client.h"
#include "ui/aura/env.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/single_monitor_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/compositor/test/compositor_test_support.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"

#if defined(USE_X11)
#include "base/message_pump_x.h"
#endif

namespace {

// Trivial WindowDelegate implementation that draws a colored background.
class DemoWindowDelegate : public aura::WindowDelegate {
 public:
  explicit DemoWindowDelegate(SkColor color) : color_(color) {}

  // Overridden from WindowDelegate:
  virtual gfx::Size GetMinimumSize() const OVERRIDE {
    return gfx::Size();
  }
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) OVERRIDE {}
  virtual void OnFocus() OVERRIDE {}
  virtual void OnBlur() OVERRIDE {}
  virtual bool OnKeyEvent(aura::KeyEvent* event) OVERRIDE {
    return false;
  }
  virtual gfx::NativeCursor GetCursor(const gfx::Point& point) OVERRIDE {
    return gfx::kNullCursor;
  }
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE {
    return HTCAPTION;
  }
  virtual bool OnMouseEvent(aura::MouseEvent* event) OVERRIDE {
    return true;
  }
  virtual ui::TouchStatus OnTouchEvent(aura::TouchEvent* event) OVERRIDE {
    return ui::TOUCH_STATUS_END;
  }
  virtual ui::GestureStatus OnGestureEvent(aura::GestureEvent* event) OVERRIDE {
    return ui::GESTURE_STATUS_UNKNOWN;
  }
  virtual bool CanFocus() OVERRIDE { return true; }
  virtual void OnCaptureLost() OVERRIDE {}
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->DrawColor(color_, SkXfermode::kSrc_Mode);
  }
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {}
  virtual void OnWindowDestroying() OVERRIDE {}
  virtual void OnWindowDestroyed() OVERRIDE {}
  virtual void OnWindowVisibilityChanged(bool visible) OVERRIDE {}

 private:
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(DemoWindowDelegate);
};

class DemoStackingClient : public aura::client::StackingClient {
 public:
  explicit DemoStackingClient(aura::RootWindow* root_window)
      : root_window_(root_window) {
    aura::client::SetStackingClient(this);
  }

  virtual ~DemoStackingClient() {
    aura::client::SetStackingClient(NULL);
  }

  // Overridden from aura::client::StackingClient:
  virtual aura::Window* GetDefaultParent(aura::Window* window) OVERRIDE {
    return root_window_;
  }

 private:
  aura::RootWindow* root_window_;

  DISALLOW_COPY_AND_ASSIGN(DemoStackingClient);
};

}  // namespace

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  ui::RegisterPathProvider();
  icu_util::Initialize();
  ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);

  // Create the message-loop here before creating the root window.
  MessageLoop message_loop(MessageLoop::TYPE_UI);
  ui::CompositorTestSupport::Initialize();
  aura::Env::GetInstance()->SetMonitorManager(new aura::SingleMonitorManager);
  scoped_ptr<aura::RootWindow> root_window(
      aura::MonitorManager::CreateRootWindowForPrimaryMonitor());
  scoped_ptr<DemoStackingClient> stacking_client(new DemoStackingClient(
      root_window.get()));

  // Create a hierarchy of test windows.
  DemoWindowDelegate window_delegate1(SK_ColorBLUE);
  aura::Window window1(&window_delegate1);
  window1.set_id(1);
  window1.Init(ui::LAYER_TEXTURED);
  window1.SetBounds(gfx::Rect(100, 100, 400, 400));
  window1.Show();
  window1.SetParent(NULL);

  DemoWindowDelegate window_delegate2(SK_ColorRED);
  aura::Window window2(&window_delegate2);
  window2.set_id(2);
  window2.Init(ui::LAYER_TEXTURED);
  window2.SetBounds(gfx::Rect(200, 200, 350, 350));
  window2.Show();
  window2.SetParent(NULL);

  DemoWindowDelegate window_delegate3(SK_ColorGREEN);
  aura::Window window3(&window_delegate3);
  window3.set_id(3);
  window3.Init(ui::LAYER_TEXTURED);
  window3.SetBounds(gfx::Rect(10, 10, 50, 50));
  window3.Show();
  window3.SetParent(&window2);

  root_window->ShowRootWindow();
  MessageLoopForUI::current()->Run();

  ui::CompositorTestSupport::Terminate();

  return 0;
}
