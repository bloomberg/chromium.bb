// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/multi_window_resize_controller.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/image/image.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_animations.h"

namespace ash {
namespace {

// Delay before showing.
const int kShowDelayMS = 400;

// Delay before hiding.
const int kHideDelayMS = 500;

// Padding from the bottom/right edge the resize widget is shown at.
const int kResizeWidgetPadding = 15;

gfx::PointF ConvertPointFromScreen(aura::Window* window,
                                   const gfx::PointF& point) {
  gfx::PointF result(point);
  ::wm::ConvertPointFromScreen(window, &result);
  return result;
}

gfx::Point ConvertPointToTarget(aura::Window* source,
                                aura::Window* target,
                                const gfx::Point& point) {
  gfx::Point result(point);
  aura::Window::ConvertPointToTarget(source, target, &result);
  return result;
}

gfx::Rect ConvertRectToScreen(aura::Window* source, const gfx::Rect& rect) {
  gfx::Rect result(rect);
  ::wm::ConvertRectToScreen(source, &result);
  return result;
}

bool ContainsX(aura::Window* window, int x) {
  return x >= 0 && x <= window->bounds().width();
}

bool ContainsScreenX(aura::Window* window, int x_in_screen) {
  gfx::PointF window_loc =
      ConvertPointFromScreen(window, gfx::PointF(x_in_screen, 0));
  return ContainsX(window, window_loc.x());
}

bool ContainsY(aura::Window* window, int y) {
  return y >= 0 && y <= window->bounds().height();
}

bool ContainsScreenY(aura::Window* window, int y_in_screen) {
  gfx::PointF window_loc =
      ConvertPointFromScreen(window, gfx::PointF(0, y_in_screen));
  return ContainsY(window, window_loc.y());
}

// Returns true if |p| is on the edge |edge_want| of |window|.
bool PointOnWindowEdge(aura::Window* window,
                       int edge_want,
                       const gfx::Point& p) {
  switch (edge_want) {
    case HTLEFT:
      return ContainsY(window, p.y()) && p.x() == 0;
    case HTRIGHT:
      return ContainsY(window, p.y()) && p.x() == window->bounds().width();
    case HTTOP:
      return ContainsX(window, p.x()) && p.y() == 0;
    case HTBOTTOM:
      return ContainsX(window, p.x()) && p.y() == window->bounds().height();
    default:
      NOTREACHED();
      return false;
  }
}

bool Intersects(int x1, int max_1, int x2, int max_2) {
  return x2 <= max_1 && max_2 > x1;
}

}  // namespace

// View contained in the widget. Passes along mouse events to the
// MultiWindowResizeController so that it can start/stop the resize loop.
class MultiWindowResizeController::ResizeView : public views::View {
 public:
  explicit ResizeView(MultiWindowResizeController* controller,
                      Direction direction)
      : controller_(controller), direction_(direction) {}

  // views::View overrides:
  gfx::Size CalculatePreferredSize() const override {
    const bool vert = direction_ == LEFT_RIGHT;
    return gfx::Size(vert ? kShortSide : kLongSide,
                     vert ? kLongSide : kShortSide);
  }
  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setColor(SkColorSetA(SK_ColorBLACK, 0x7F));
    flags.setAntiAlias(true);
    canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()), 2, flags);

    // Craft the left arrow.
    const SkRect kArrowBounds = SkRect::MakeXYWH(4, 28, 4, 8);
    SkPath path;
    path.moveTo(kArrowBounds.right(), kArrowBounds.y());
    path.lineTo(kArrowBounds.x(), kArrowBounds.centerY());
    path.lineTo(kArrowBounds.right(), kArrowBounds.bottom());
    path.close();

    // Do the same for the right arrow.
    SkMatrix flip;
    flip.setScale(-1, 1, kShortSide / 2, kLongSide / 2);
    path.addPath(path, flip);

    // The arrows are drawn for the vertical orientation; rotate if need be.
    if (direction_ == TOP_BOTTOM) {
      SkMatrix transform;
      constexpr int kHalfShort = kShortSide / 2;
      constexpr int kHalfLong = kLongSide / 2;
      transform.setRotate(90, kHalfShort, kHalfLong);
      transform.postTranslate(kHalfLong - kHalfShort, kHalfShort - kHalfLong);
      path.transform(transform);
    }

    flags.setColor(SK_ColorWHITE);
    canvas->DrawPath(path, flags);
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    gfx::Point location(event.location());
    views::View::ConvertPointToScreen(this, &location);
    controller_->StartResize(gfx::PointF(location));
    return true;
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    gfx::Point location(event.location());
    views::View::ConvertPointToScreen(this, &location);
    controller_->Resize(gfx::PointF(location), event.flags());
    return true;
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    controller_->CompleteResize();
  }

  void OnMouseCaptureLost() override { controller_->CancelResize(); }

  gfx::NativeCursor GetCursor(const ui::MouseEvent& event) override {
    int component = (direction_ == LEFT_RIGHT) ? HTRIGHT : HTBOTTOM;
    return ::wm::CompoundEventFilter::CursorForWindowComponent(component);
  }

 private:
  static constexpr int kLongSide = 64;
  static constexpr int kShortSide = 28;

  MultiWindowResizeController* controller_;
  const Direction direction_;

  DISALLOW_COPY_AND_ASSIGN(ResizeView);
};

// MouseWatcherHost implementation for MultiWindowResizeController. Forwards
// Contains() to MultiWindowResizeController.
class MultiWindowResizeController::ResizeMouseWatcherHost
    : public views::MouseWatcherHost {
 public:
  ResizeMouseWatcherHost(MultiWindowResizeController* host) : host_(host) {}

  // MouseWatcherHost overrides:
  bool Contains(const gfx::Point& point_in_screen, EventType type) override {
    return (type == EventType::kPress)
               ? host_->IsOverResizeWidget(point_in_screen)
               : host_->IsOverWindows(point_in_screen);
  }

 private:
  MultiWindowResizeController* host_;

  DISALLOW_COPY_AND_ASSIGN(ResizeMouseWatcherHost);
};

MultiWindowResizeController::ResizeWindows::ResizeWindows()
    : window1(nullptr), window2(nullptr), direction(TOP_BOTTOM) {}

MultiWindowResizeController::ResizeWindows::ResizeWindows(
    const ResizeWindows& other) = default;

MultiWindowResizeController::ResizeWindows::~ResizeWindows() = default;

bool MultiWindowResizeController::ResizeWindows::Equals(
    const ResizeWindows& other) const {
  return window1 == other.window1 && window2 == other.window2 &&
         direction == other.direction;
}

MultiWindowResizeController::MultiWindowResizeController() = default;

MultiWindowResizeController::~MultiWindowResizeController() {
  ResetResizer();
}

void MultiWindowResizeController::Show(aura::Window* window,
                                       int component,
                                       const gfx::Point& point_in_window) {
  // When the resize widget is showing we ignore Show() requests. Instead we
  // only care about mouse movements from MouseWatcher. This is necessary as
  // WorkspaceEventHandler only sees mouse movements over the windows, not all
  // windows or over the desktop.
  if (resize_widget_)
    return;

  ResizeWindows windows(DetermineWindows(window, component, point_in_window));
  if (IsShowing() && windows_.Equals(windows))
    return;

  Hide();
  if (!windows.is_valid()) {
    windows_ = ResizeWindows();
    return;
  }

  windows_ = windows;
  StartObserving(windows_.window1);
  StartObserving(windows_.window2);
  show_location_in_parent_ =
      ConvertPointToTarget(window, window->parent(), point_in_window);
  show_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kShowDelayMS),
                    this,
                    &MultiWindowResizeController::ShowIfValidMouseLocation);
}

void MultiWindowResizeController::MouseMovedOutOfHost() {
  Hide();
}

void MultiWindowResizeController::OnWindowPropertyChanged(aura::Window* window,
                                                          const void* key,
                                                          intptr_t old) {
  // If the window is now non-resizeable, make sure the resizer is not showing.
  if ((window->GetProperty(aura::client::kResizeBehaviorKey) &
       aura::client::kResizeBehaviorCanResize) == 0)
    ResetResizer();
}

void MultiWindowResizeController::OnWindowVisibilityChanged(
    aura::Window* window,
    bool visible) {
  if (!visible)
    ResetResizer();
}

void MultiWindowResizeController::OnWindowDestroying(aura::Window* window) {
  ResetResizer();
}

void MultiWindowResizeController::OnPostWindowStateTypeChange(
    WindowState* window_state,
    WindowStateType old_type) {
  if (window_state->IsMaximized() || window_state->IsFullscreen() ||
      window_state->IsMinimized()) {
    ResetResizer();
  }
}

MultiWindowResizeController::ResizeWindows
MultiWindowResizeController::DetermineWindowsFromScreenPoint(
    aura::Window* window) const {
  gfx::Point mouse_location(
      display::Screen::GetScreen()->GetCursorScreenPoint());
  wm::ConvertPointFromScreen(window, &mouse_location);
  const int component =
      window_util::GetNonClientComponent(window, mouse_location);
  return DetermineWindows(window, component, mouse_location);
}

void MultiWindowResizeController::CreateMouseWatcher() {
  mouse_watcher_ = std::make_unique<views::MouseWatcher>(
      std::make_unique<ResizeMouseWatcherHost>(this), this);
  mouse_watcher_->set_notify_on_exit_time(
      base::TimeDelta::FromMilliseconds(kHideDelayMS));
  DCHECK(resize_widget_);
  mouse_watcher_->Start(resize_widget_->GetNativeWindow());
}

MultiWindowResizeController::ResizeWindows
MultiWindowResizeController::DetermineWindows(aura::Window* window,
                                              int window_component,
                                              const gfx::Point& point) const {
  ResizeWindows result;

  // Check if the window is non-resizeable.
  if ((window->GetProperty(aura::client::kResizeBehaviorKey) &
       aura::client::kResizeBehaviorCanResize) == 0)
    return result;

  gfx::Point point_in_parent =
      ConvertPointToTarget(window, window->parent(), point);
  switch (window_component) {
    case HTRIGHT:
      result.direction = LEFT_RIGHT;
      result.window1 = window;
      result.window2 = FindWindowByEdge(
          window, HTLEFT, window->bounds().right(), point_in_parent.y());
      break;
    case HTLEFT:
      result.direction = LEFT_RIGHT;
      result.window1 = FindWindowByEdge(window, HTRIGHT, window->bounds().x(),
                                        point_in_parent.y());
      result.window2 = window;
      break;
    case HTTOP:
      result.direction = TOP_BOTTOM;
      result.window1 = FindWindowByEdge(window, HTBOTTOM, point_in_parent.x(),
                                        window->bounds().y());
      result.window2 = window;
      break;
    case HTBOTTOM:
      result.direction = TOP_BOTTOM;
      result.window1 = window;
      result.window2 = FindWindowByEdge(window, HTTOP, point_in_parent.x(),
                                        window->bounds().bottom());
      break;
    default:
      break;
  }
  return result;
}

aura::Window* MultiWindowResizeController::FindWindowByEdge(
    aura::Window* window_to_ignore,
    int edge_want,
    int x_in_parent,
    int y_in_parent) const {
  aura::Window* parent = window_to_ignore->parent();
  const aura::Window::Windows& windows = parent->children();
  for (auto i = windows.rbegin(); i != windows.rend(); ++i) {
    aura::Window* window = *i;
    if (window == window_to_ignore || !window->IsVisible())
      continue;

    // Ignore windows without a non-client area.
    if (!window->delegate())
      continue;

    // Return the window if it is resizeable and the wanted edge has the point.
    if ((window->GetProperty(aura::client::kResizeBehaviorKey) &
         aura::client::kResizeBehaviorCanResize) != 0 &&
        PointOnWindowEdge(
            window, edge_want,
            ConvertPointToTarget(parent, window,
                                 gfx::Point(x_in_parent, y_in_parent)))) {
      return window;
    }

    // Having determined that the window is not a suitable return value, if it
    // contains the point, then it is obscuring that point on any remaining
    // window that also contains the point.
    if (window->bounds().Contains(x_in_parent, y_in_parent))
      return NULL;
  }
  return NULL;
}

aura::Window* MultiWindowResizeController::FindWindowTouching(
    aura::Window* window,
    Direction direction) const {
  int right = window->bounds().right();
  int bottom = window->bounds().bottom();
  aura::Window* parent = window->parent();
  const aura::Window::Windows& windows = parent->children();
  for (auto i = windows.rbegin(); i != windows.rend(); ++i) {
    aura::Window* other = *i;
    if (other == window || !other->IsVisible())
      continue;
    switch (direction) {
      case TOP_BOTTOM:
        if (other->bounds().y() == bottom &&
            Intersects(other->bounds().x(), other->bounds().right(),
                       window->bounds().x(), window->bounds().right())) {
          return other;
        }
        break;
      case LEFT_RIGHT:
        if (other->bounds().x() == right &&
            Intersects(other->bounds().y(), other->bounds().bottom(),
                       window->bounds().y(), window->bounds().bottom())) {
          return other;
        }
        break;
      default:
        NOTREACHED();
    }
  }
  return NULL;
}

void MultiWindowResizeController::FindWindowsTouching(
    aura::Window* start,
    Direction direction,
    aura::Window::Windows* others) const {
  while (start) {
    start = FindWindowTouching(start, direction);
    if (start)
      others->push_back(start);
  }
}

void MultiWindowResizeController::StartObserving(aura::Window* window) {
  window->AddObserver(this);
  WindowState::Get(window)->AddObserver(this);
}

void MultiWindowResizeController::StopObserving(aura::Window* window) {
  window->RemoveObserver(this);
  WindowState::Get(window)->RemoveObserver(this);
}

void MultiWindowResizeController::ShowIfValidMouseLocation() {
  if (DetermineWindowsFromScreenPoint(windows_.window1).Equals(windows_) ||
      DetermineWindowsFromScreenPoint(windows_.window2).Equals(windows_)) {
    ShowNow();
  } else {
    Hide();
  }
}

void MultiWindowResizeController::ShowNow() {
  DCHECK(!resize_widget_.get());
  DCHECK(windows_.is_valid());
  show_timer_.Stop();
  resize_widget_.reset(new views::Widget);
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.name = "MultiWindowResizeController";
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = windows_.window1->GetRootWindow()->GetChildById(
      kShellWindowId_AlwaysOnTopContainer);
  ResizeView* view = new ResizeView(this, windows_.direction);
  resize_widget_->set_focus_on_creation(false);
  resize_widget_->Init(std::move(params));
  ::wm::SetWindowVisibilityAnimationType(
      resize_widget_->GetNativeWindow(),
      ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  resize_widget_->SetContentsView(view);
  show_bounds_in_screen_ = ConvertRectToScreen(
      windows_.window1->parent(),
      CalculateResizeWidgetBounds(gfx::PointF(show_location_in_parent_)));
  resize_widget_->SetBounds(show_bounds_in_screen_);
  resize_widget_->Show();
  CreateMouseWatcher();
}

bool MultiWindowResizeController::IsShowing() const {
  return resize_widget_.get() || show_timer_.IsRunning();
}

void MultiWindowResizeController::Hide() {
  if (window_resizer_)
    return;  // Ignore hides while actively resizing.

  if (windows_.window1) {
    StopObserving(windows_.window1);
    windows_.window1 = nullptr;
  }
  if (windows_.window2) {
    StopObserving(windows_.window2);
    windows_.window2 = nullptr;
  }

  show_timer_.Stop();

  if (!resize_widget_)
    return;

  for (auto* window : windows_.other_windows)
    StopObserving(window);
  mouse_watcher_.reset();
  resize_widget_.reset();
  windows_ = ResizeWindows();
}

void MultiWindowResizeController::ResetResizer() {
  // Have to explicitly reset the WindowResizer, otherwise Hide() does nothing.
  window_resizer_.reset();
  Hide();
}

void MultiWindowResizeController::StartResize(
    const gfx::PointF& location_in_screen) {
  DCHECK(!window_resizer_.get());
  DCHECK(windows_.is_valid());
  gfx::PointF location_in_parent =
      ConvertPointFromScreen(windows_.window2->parent(), location_in_screen);
  aura::Window::Windows windows;
  windows.push_back(windows_.window2);
  DCHECK(windows_.other_windows.empty());
  FindWindowsTouching(windows_.window2, windows_.direction,
                      &windows_.other_windows);
  for (size_t i = 0; i < windows_.other_windows.size(); ++i) {
    StartObserving(windows_.other_windows[i]);
    windows.push_back(windows_.other_windows[i]);
  }
  int component = windows_.direction == LEFT_RIGHT ? HTRIGHT : HTBOTTOM;
  WindowState* window_state = WindowState::Get(windows_.window1);
  window_state->CreateDragDetails(location_in_parent, component,
                                  ::wm::WINDOW_MOVE_SOURCE_MOUSE);
  window_resizer_.reset(WorkspaceWindowResizer::Create(window_state, windows));

  // Do not hide the resize widget while a drag is active.
  mouse_watcher_.reset();
}

void MultiWindowResizeController::Resize(const gfx::PointF& location_in_screen,
                                         int event_flags) {
  gfx::PointF location_in_parent =
      ConvertPointFromScreen(windows_.window1->parent(), location_in_screen);
  window_resizer_->Drag(location_in_parent, event_flags);
  gfx::Rect bounds =
      ConvertRectToScreen(windows_.window1->parent(),
                          CalculateResizeWidgetBounds(location_in_parent));

  if (windows_.direction == LEFT_RIGHT)
    bounds.set_y(show_bounds_in_screen_.y());
  else
    bounds.set_x(show_bounds_in_screen_.x());
  resize_widget_->SetBounds(bounds);
}

void MultiWindowResizeController::CompleteResize() {
  window_resizer_->CompleteDrag();
  WindowState::Get(window_resizer_->GetTarget())->DeleteDragDetails();
  window_resizer_.reset();

  // Mouse may still be over resizer, if not hide.
  gfx::Point screen_loc = display::Screen::GetScreen()->GetCursorScreenPoint();
  if (!resize_widget_->GetWindowBoundsInScreen().Contains(screen_loc)) {
    Hide();
  } else {
    // If the mouse is over the resizer we need to remove observers on any of
    // the |other_windows|. If we start another resize we'll recalculate the
    // |other_windows| and invoke AddObserver() as necessary.
    for (size_t i = 0; i < windows_.other_windows.size(); ++i)
      StopObserving(windows_.other_windows[i]);
    windows_.other_windows.clear();

    CreateMouseWatcher();
  }
}

void MultiWindowResizeController::CancelResize() {
  if (!window_resizer_)
    return;  // Happens if window was destroyed and we nuked the WindowResizer.
  window_resizer_->RevertDrag();
  WindowState::Get(window_resizer_->GetTarget())->DeleteDragDetails();
  ResetResizer();
}

gfx::Rect MultiWindowResizeController::CalculateResizeWidgetBounds(
    const gfx::PointF& location_in_parent) const {
  gfx::Size pref = resize_widget_->GetContentsView()->GetPreferredSize();
  int x = 0, y = 0;
  if (windows_.direction == LEFT_RIGHT) {
    x = windows_.window1->bounds().right() - pref.width() / 2;
    y = location_in_parent.y() + kResizeWidgetPadding;
    if (y + pref.height() / 2 > windows_.window1->bounds().bottom() &&
        y + pref.height() / 2 > windows_.window2->bounds().bottom()) {
      y = location_in_parent.y() - kResizeWidgetPadding - pref.height();
    }
  } else {
    x = location_in_parent.x() + kResizeWidgetPadding;
    if (x + pref.height() / 2 > windows_.window1->bounds().right() &&
        x + pref.height() / 2 > windows_.window2->bounds().right()) {
      x = location_in_parent.x() - kResizeWidgetPadding - pref.width();
    }
    y = windows_.window1->bounds().bottom() - pref.height() / 2;
  }
  return gfx::Rect(x, y, pref.width(), pref.height());
}

bool MultiWindowResizeController::IsOverResizeWidget(
    const gfx::Point& location_in_screen) const {
  return resize_widget_->GetWindowBoundsInScreen().Contains(location_in_screen);
}

bool MultiWindowResizeController::IsOverWindows(
    const gfx::Point& location_in_screen) const {
  if (IsOverResizeWidget(location_in_screen))
    return true;

  if (windows_.direction == TOP_BOTTOM) {
    if (!ContainsScreenX(windows_.window1, location_in_screen.x()) ||
        !ContainsScreenX(windows_.window2, location_in_screen.x())) {
      return false;
    }
  } else {
    if (!ContainsScreenY(windows_.window1, location_in_screen.y()) ||
        !ContainsScreenY(windows_.window2, location_in_screen.y())) {
      return false;
    }
  }

  // Check whether |location_in_screen| is in the event target's resize region.
  // This is tricky because a window's resize region can extend outside a
  // window's bounds.
  aura::Window* target = RootWindowController::ForWindow(windows_.window1)
                             ->FindEventTarget(location_in_screen);
  if (target == windows_.window1) {
    return IsOverComponent(
        windows_.window1, location_in_screen,
        windows_.direction == TOP_BOTTOM ? HTBOTTOM : HTRIGHT);
  }
  if (target == windows_.window2) {
    return IsOverComponent(windows_.window2, location_in_screen,
                           windows_.direction == TOP_BOTTOM ? HTTOP : HTLEFT);
  }
  return false;
}

bool MultiWindowResizeController::IsOverComponent(
    aura::Window* window,
    const gfx::Point& location_in_screen,
    int component) const {
  gfx::Point window_loc(location_in_screen);
  ::wm::ConvertPointFromScreen(window, &window_loc);
  return window_util::GetNonClientComponent(window, window_loc) == component;
}

}  // namespace ash
