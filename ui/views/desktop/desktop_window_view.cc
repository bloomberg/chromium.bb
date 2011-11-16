// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/desktop/desktop_window_view.h"

#include "base/utf_string_conversions.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/gfx/transform.h"
#include "ui/views/desktop/desktop_background.h"
#include "ui/views/desktop/desktop_window_manager.h"
#include "ui/views/window/native_frame_view.h"
#include "views/widget/native_widget_view.h"
#include "views/widget/native_widget_views.h"
#include "views/widget/widget.h"

#if defined(USE_AURA)
#include "views/widget/native_widget_aura.h"
#elif defined(OS_WIN)
#include "views/widget/native_widget_win.h"
#elif defined(USE_WAYLAND)
#include "views/widget/native_widget_wayland.h"
#elif defined(TOOLKIT_USES_GTK)
#include "views/widget/native_widget_gtk.h"
#endif

namespace views {
namespace desktop {

// The Widget that hosts the DesktopWindowView. Subclasses Widget to override
// CreateRootView() so that the DesktopWindowRootView can be supplied instead
// for custom event filtering.
class DesktopWindow : public Widget {
 public:
  explicit DesktopWindow(DesktopWindowView* desktop_window_view)
      : desktop_window_view_(desktop_window_view) {}
  virtual ~DesktopWindow() {}

 private:
  // Overridden from Widget:
  virtual bool OnKeyEvent(const KeyEvent& event) OVERRIDE {
    return WindowManager::Get()->HandleKeyEvent(this, event);
  }

  virtual bool OnMouseEvent(const MouseEvent& event) OVERRIDE {
    return WindowManager::Get()->HandleMouseEvent(this, event) ||
        Widget::OnMouseEvent(event);
  }

  virtual ui::TouchStatus OnTouchEvent(const TouchEvent& event) OVERRIDE {
    ui::TouchStatus status = WindowManager::Get()->
        HandleTouchEvent(this, event);
    if (status == ui::TOUCH_STATUS_UNKNOWN)
      status = Widget::OnTouchEvent(event);
    return status;
  }

  DesktopWindowView* desktop_window_view_;

  DISALLOW_COPY_AND_ASSIGN(DesktopWindow);
};

class TestWindowContentView : public WidgetDelegateView {
 public:
  TestWindowContentView(const string16& title, SkColor color)
      : title_(title),
        color_(color) {
  }
  virtual ~TestWindowContentView() {}

 private:
  // Overridden from View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRect(color_, GetLocalBounds());
  }

  // Overridden from WindowDelegate:
  virtual string16 GetWindowTitle() const OVERRIDE {
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

  string16 title_;
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowContentView);
};

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowView, public:

// static
DesktopWindowView* DesktopWindowView::desktop_window_view = NULL;

DesktopWindowView::DesktopWindowView(DesktopType type)
    : type_(type) {
  switch (type_) {
    case DESKTOP_DEFAULT:
    case DESKTOP_NETBOOK:
      set_background(new DesktopBackground);
      break;
    case DESKTOP_OTHER:
      set_background(Background::CreateStandardPanelBackground());
      break;
  }
}

DesktopWindowView::~DesktopWindowView() {
}

// static
void DesktopWindowView::CreateDesktopWindow(DesktopType type) {
  DCHECK(!desktop_window_view);
  desktop_window_view = new DesktopWindowView(type);
  views::Widget* window = new DesktopWindow(desktop_window_view);
  desktop_window_view->widget_ = window;

  WindowManager::Install(new DesktopWindowManager(window));

  views::Widget::InitParams params;
  params.delegate = desktop_window_view;
  // In this environment, CreateChromeWindow will default to creating a views-
  // window, so we need to construct a NativeWidgetWin by hand.
  // TODO(beng): Replace this with NativeWindow::CreateNativeRootWindow().
#if defined(USE_AURA)
  params.native_widget = new views::NativeWidgetAura(window);
#elif defined(OS_WIN)
  params.native_widget = new views::NativeWidgetWin(window);
#elif defined(USE_WAYLAND)
  params.native_widget = new views::NativeWidgetWayland(window);
#elif defined(TOOLKIT_USES_GTK)
  params.native_widget = new views::NativeWidgetGtk(window);
  params.show_state = ui::SHOW_STATE_MAXIMIZED;
#endif
  params.bounds = gfx::Rect(20, 20, 1920, 1200);
  window->Init(params);
  window->Show();
}

void DesktopWindowView::CreateTestWindow(const string16& title,
                                         SkColor color,
                                         gfx::Rect initial_bounds,
                                         bool rotate) {
  views::Widget* window = views::Widget::CreateWindowWithBounds(
      new TestWindowContentView(title, color),
      initial_bounds);
  window->Show();

  NativeWidgetViews* native_widget_views =
      static_cast<NativeWidgetViews*>(window->native_widget());

  if (rotate) {
    ui::Transform transform;
    transform.SetRotate(90.0f);
    transform.SetTranslateX(window->GetWindowScreenBounds().width());
    native_widget_views->GetView()->SetTransform(transform);
  }

  native_widget_views->GetView()->SetPaintToLayer(true);
  if (native_widget_views->GetView()->layer()) {
    native_widget_views->GetView()->layer()->SetAnimator(
        ui::LayerAnimator::CreateImplicitAnimator());
  }
}

void DesktopWindowView::AddObserver(DesktopWindowView::Observer* observer) {
  observers_.AddObserver(observer);
}

void DesktopWindowView::RemoveObserver(DesktopWindowView::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool DesktopWindowView::HasObserver(DesktopWindowView::Observer* observer) {
  return observers_.HasObserver(observer);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowView, View overrides:

void DesktopWindowView::Layout() {
}

void DesktopWindowView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  static_cast<DesktopWindowManager*>(WindowManager::Get())->
      UpdateWindowsAfterScreenSizeChanged(bounds());

  FOR_EACH_OBSERVER(Observer, observers_,
                    OnDesktopBoundsChanged(previous_bounds));
}

void DesktopWindowView::ViewHierarchyChanged(
    bool is_add, View* parent, View* child) {
  if (child->GetClassName() == internal::NativeWidgetView::kViewClassName) {
    Widget* widget =
        static_cast<internal::NativeWidgetView*>(child)->GetAssociatedWidget();
    if (is_add)
      WindowManager::Get()->Register(widget);
  }
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowView, WidgetDelegate implementation:

Widget* DesktopWindowView::GetWidget() {
  return widget_;
}

const Widget* DesktopWindowView::GetWidget() const {
  return widget_;
}

bool DesktopWindowView::CanResize() const {
  return true;
}

bool DesktopWindowView::CanMaximize() const {
  return CanResize();
}

string16 DesktopWindowView::GetWindowTitle() const {
  return ASCIIToUTF16("Aura Desktop");
}

SkBitmap DesktopWindowView::GetWindowAppIcon() {
  return SkBitmap();
}

SkBitmap DesktopWindowView::GetWindowIcon() {
  return SkBitmap();
}

bool DesktopWindowView::ShouldShowWindowIcon() const {
  return false;
}

void DesktopWindowView::WindowClosing() {
  MessageLoopForUI::current()->Quit();
}

View* DesktopWindowView::GetContentsView() {
  return this;
}

NonClientFrameView* DesktopWindowView::CreateNonClientFrameView() {
  switch (type_) {
    case DESKTOP_DEFAULT:
    case DESKTOP_NETBOOK:
      return NULL;

    case DESKTOP_OTHER:
      return new NativeFrameView(widget_);
  }
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// ui::LayerAnimationObserver Implementation:

void DesktopWindowView::OnLayerAnimationEnded(
    const ui::LayerAnimationSequence* animation) {
  // The layer, and all the observers should be notified of the
  // transformed size of the desktop.
  if (widget_) {
    gfx::Rect current_bounds(widget_->GetClientAreaScreenBounds().size());
    layer()->transform().TransformRect(&current_bounds);
    SetBoundsRect(gfx::Rect(current_bounds.size()));
  }
}

void DesktopWindowView::OnLayerAnimationScheduled(
    const ui::LayerAnimationSequence* animation) {
}

void DesktopWindowView::OnLayerAnimationAborted(
    const ui::LayerAnimationSequence* animation) {
}

}  // namespace desktop
}  // namespace views
