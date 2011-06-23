// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/desktop/desktop_window.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/transform.h"
#include "views/desktop/desktop_background.h"
#include "views/desktop/desktop_window_root_view.h"
#include "views/widget/native_widget_view.h"
#include "views/widget/native_widget_views.h"
#include "views/widget/widget.h"

#if defined(OS_WIN)
#include "views/widget/native_widget_win.h"
#elif defined(TOOLKIT_USES_GTK)
#include "views/widget/native_widget_gtk.h"
#endif

namespace views {
namespace desktop {

// TODO(beng): resolve naming!
class DesktopWindowWindow : public Widget {
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

class TestWindowContentView : public WidgetDelegateView {
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
  virtual bool CanMaximize() const OVERRIDE {
    return true;
  }
  virtual bool OnMousePressed(const MouseEvent& event) OVERRIDE {
    Widget* widget = View::GetWidget();
    if (widget->IsMinimized())
      widget->Restore();
    else
      widget->Minimize();
    return true;
  }

  std::wstring title_;
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowContentView);
};

////////////////////////////////////////////////////////////////////////////////
// DesktopWindow, public:

// static
DesktopWindow* DesktopWindow::desktop_window = NULL;

DesktopWindow::DesktopWindow() : active_widget_(NULL) {
  set_background(new DesktopBackground);
}

DesktopWindow::~DesktopWindow() {
}

// static
void DesktopWindow::CreateDesktopWindow() {
  DCHECK(!desktop_window);
  desktop_window = new DesktopWindow;
  views::Widget* window = new DesktopWindowWindow(desktop_window);
  views::Widget::InitParams params;
  params.delegate = desktop_window;
  // In this environment, CreateChromeWindow will default to creating a views-
  // window, so we need to construct a NativeWidgetWin by hand.
  // TODO(beng): Replace this with NativeWindow::CreateNativeRootWindow().
#if defined(OS_WIN)
  params.native_widget = new views::NativeWidgetWin(window);
#elif defined(TOOLKIT_USES_GTK)
  params.native_widget = new views::NativeWidgetGtk(window);
#endif
  params.bounds = gfx::Rect(20, 20, 1920, 1200);
  window->Init(params);
  window->Show();
}

void DesktopWindow::ActivateWidget(Widget* widget) {
  if (widget && widget->IsActive())
    return;

  if (active_widget_)
    active_widget_->OnActivate(false);
  if (widget) {
    widget->MoveToTop();
    active_widget_ = static_cast<NativeWidgetViews*>(widget->native_widget());
    active_widget_->OnActivate(true);
  }
}


void DesktopWindow::CreateTestWindow(const std::wstring& title,
                                     SkColor color,
                                     gfx::Rect initial_bounds,
                                     bool rotate) {
  views::Widget* window = views::Widget::CreateWindowWithBounds(
      new TestWindowContentView(title, color),
      initial_bounds);
  window->Show();

  if (rotate) {
    ui::Transform transform;
    transform.SetRotate(90.0f);
    transform.SetTranslateX(window->GetWindowScreenBounds().width());
    static_cast<NativeWidgetViews*>(window->native_widget())->GetView()->
        SetTransform(transform);
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

}  // namespace desktop
}  // namespace views
