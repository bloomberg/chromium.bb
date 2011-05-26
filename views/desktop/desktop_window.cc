// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/desktop/desktop_window.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/transform.h"
#include "views/desktop/desktop_background.h"
#include "views/desktop/desktop_window_root_view.h"
#include "views/widget/native_widget_view.h"
#include "views/window/native_window_views.h"
#include "views/window/window.h"

#if defined(OS_WIN)
#include "views/window/native_window_win.h"
#elif defined(TOOLKIT_USES_GTK)
#include "views/window/native_window_gtk.h"
#endif

namespace views {
namespace desktop {

// TODO(beng): resolve naming!
class DesktopWindowWindow : public Window {
 public:
  explicit DesktopWindowWindow(DesktopWindow* desktop_window)
      : desktop_window_(desktop_window) {}
  virtual ~DesktopWindowWindow() {}

 private:
  // Overridden from Widget:
  virtual internal::RootView* CreateRootView() OVERRIDE {
    return new DesktopWindowRootView(desktop_window_, this);
  }

  DesktopWindow* desktop_window_;

  DISALLOW_COPY_AND_ASSIGN(DesktopWindowWindow);
};

class TestWindowContentView : public View,
                              public WindowDelegate {
 public:
  TestWindowContentView(const std::wstring& title, SkColor color)
      : title_(title),
        color_(color) {
  }
  virtual ~TestWindowContentView() {}

 private:
  // Overridden from View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRectInt(color_, 0, 0, width(), height());
  }

  // Overridden from WindowDelegate:
  virtual std::wstring GetWindowTitle() const OVERRIDE {
    return title_;
  }
  virtual View* GetContentsView() {
    return this;
  }

  std::wstring title_;
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowContentView);
};

////////////////////////////////////////////////////////////////////////////////
// DesktopWindow, public:

DesktopWindow::DesktopWindow() : active_widget_(NULL) {
  set_background(new DesktopBackground);
}

DesktopWindow::~DesktopWindow() {
}

// static
void DesktopWindow::CreateDesktopWindow() {
  DesktopWindow* desktop = new DesktopWindow;
  views::Window* window = new DesktopWindowWindow(desktop);
  views::Window::InitParams params(desktop);
  // In this environment, CreateChromeWindow will default to creating a views-
  // window, so we need to construct a NativeWindowWin by hand.
  // TODO(beng): Replace this with NativeWindow::CreateNativeRootWindow().
#if defined(OS_WIN)
  params.native_window = new views::NativeWindowWin(window);
#elif defined(TOOLKIT_USES_GTK)
  params.native_window = new views::NativeWindowGtk(window);
#endif
  params.widget_init_params.bounds = gfx::Rect(20, 20, 1920, 1200);
  window->InitWindow(params);
  window->Show();

  desktop->CreateTestWindow(L"Sample Window 1", SK_ColorWHITE,
                            gfx::Rect(500, 200, 400, 400), true);
  desktop->CreateTestWindow(L"Sample Window 2", SK_ColorRED,
                            gfx::Rect(600, 650, 450, 300), false);
}

void DesktopWindow::ActivateWidget(Widget* widget) {
  if (widget && widget->IsActive())
    return;

  if (active_widget_)
    active_widget_->OnActivate(false);
  if (widget) {
    active_widget_ = static_cast<NativeWidgetViews*>(widget->native_widget());
    active_widget_->OnActivate(true);
  }
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindow, View overrides:

void DesktopWindow::Layout() {
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindow, WindowDelegate implementation:

bool DesktopWindow::CanResize() const {
  return true;
}

bool DesktopWindow::CanMaximize() const {
  return CanResize();
}

std::wstring DesktopWindow::GetWindowTitle() const {
  return L"Aura Desktop";
}

SkBitmap DesktopWindow::GetWindowAppIcon() {
  return SkBitmap();
}

SkBitmap DesktopWindow::GetWindowIcon() {
  return SkBitmap();
}

bool DesktopWindow::ShouldShowWindowIcon() const {
  return false;
}

void DesktopWindow::WindowClosing() {
  MessageLoopForUI::current()->Quit();
}

View* DesktopWindow::GetContentsView() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindow, private:

void DesktopWindow::CreateTestWindow(const std::wstring& title,
                                     SkColor color,
                                     gfx::Rect initial_bounds,
                                     bool rotate) {
  views::Window* window = new views::Window;
  views::NativeWindowViews* nwv = new views::NativeWindowViews(this, window);
  views::Window::InitParams params(new TestWindowContentView(title, color));
  params.native_window = nwv;
  params.widget_init_params.bounds = initial_bounds;
  window->InitWindow(params);
  window->Show();

  if (rotate) {
    ui::Transform transform;
    transform.SetRotate(90.0f);
    transform.SetTranslateX(window->GetBounds().width());
    nwv->GetView()->SetTransform(transform);
  }
}

}  // namespace desktop
}  // namespace views
