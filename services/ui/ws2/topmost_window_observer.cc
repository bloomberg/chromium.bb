// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/topmost_window_observer.h"

#include "services/ui/ws2/window_service.h"
#include "services/ui/ws2/window_service_delegate.h"
#include "services/ui/ws2/window_tree.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace {

gfx::Point GetLocationInScreen(const ui::LocatedEvent& event) {
  gfx::Point location_in_screen = event.location();
  ::wm::ConvertPointToScreen(static_cast<aura::Window*>(event.target()),
                             &location_in_screen);
  return location_in_screen;
}

}  // namespace

namespace ui {
namespace ws2 {

TopmostWindowObserver::TopmostWindowObserver(WindowTree* window_tree,
                                             ui::mojom::MoveLoopSource source,
                                             aura::Window* initial_target)
    : window_tree_(window_tree),
      source_(source),
      last_target_(initial_target),
      root_(initial_target->GetRootWindow()) {
  root_->AddPreTargetHandler(this, ui::EventTarget::Priority::kSystem);
  if (source == ui::mojom::MoveLoopSource::MOUSE) {
    last_location_ =
        root_->GetHost()->dispatcher()->GetLastMouseLocationInRoot();
    ::wm::ConvertPointToScreen(root_, &last_location_);
  } else {
    gfx::PointF point;
    ui::GestureRecognizer* gesture_recognizer =
        initial_target->env()->gesture_recognizer();
    if (gesture_recognizer->GetLastTouchPointForTarget(last_target_, &point))
      last_location_ = gfx::Point(point.x(), point.y());
    ::wm::ConvertPointToScreen(last_target_, &last_location_);
  }
  UpdateTopmostWindows();
}

TopmostWindowObserver::~TopmostWindowObserver() {
  root_->RemovePreTargetHandler(this);
  if (topmost_)
    topmost_->RemoveObserver(this);
  if (real_topmost_)
    real_topmost_->RemoveObserver(this);
}

void TopmostWindowObserver::OnMouseEvent(ui::MouseEvent* event) {
  CHECK_EQ(ui::EP_PRETARGET, event->phase());
  if (source_ != ui::mojom::MoveLoopSource::MOUSE)
    return;
  // The event target can change when the dragged browser tab is detached into a
  // new window.
  last_target_ = static_cast<aura::Window*>(event->target());
  last_location_ = GetLocationInScreen(*event);
  UpdateTopmostWindows();
}

void TopmostWindowObserver::OnTouchEvent(ui::TouchEvent* event) {
  CHECK_EQ(ui::EP_PRETARGET, event->phase());
  if (source_ != ui::mojom::MoveLoopSource::TOUCH)
    return;
  // The event target can change when the dragged browser tab is detached into a
  // new window.
  last_target_ = static_cast<aura::Window*>(event->target());
  last_location_ = GetLocationInScreen(*event);
  UpdateTopmostWindows();
}

void TopmostWindowObserver::OnWindowVisibilityChanged(aura::Window* window,
                                                      bool visible) {
  if (visible)
    return;
  if (!window->Contains(topmost_) && !window->Contains(real_topmost_))
    return;
  UpdateTopmostWindows();
}

void TopmostWindowObserver::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  UpdateTopmostWindows();
}

void TopmostWindowObserver::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  gfx::Rect screen_bounds = new_bounds;
  ::wm::ConvertRectToScreen(window->parent(), &screen_bounds);
  if (!screen_bounds.Contains(last_location_))
    UpdateTopmostWindows();
}

void TopmostWindowObserver::UpdateTopmostWindows() {
  std::set<aura::Window*> ignore;
  ignore.insert(last_target_);
  aura::Window* real_topmost = nullptr;
  aura::Window* topmost =
      window_tree_->window_service()->delegate()->GetTopmostWindowAtPoint(
          last_location_, ignore, &real_topmost);

  if (topmost == topmost_ && real_topmost == real_topmost_)
    return;

  if (topmost_ != topmost) {
    if (topmost_)
      topmost_->RemoveObserver(this);
    topmost_ = topmost;
    if (topmost_)
      topmost_->AddObserver(this);
  }
  if (real_topmost_ != real_topmost) {
    if (real_topmost_)
      real_topmost_->RemoveObserver(this);
    real_topmost_ = real_topmost;
    if (real_topmost_)
      real_topmost_->RemoveObserver(this);
  }

  std::vector<aura::Window*> windows;
  if (real_topmost_)
    windows.push_back(real_topmost_);
  if (topmost_ && topmost_ != real_topmost_)
    windows.push_back(topmost_);
  window_tree_->SendTopmostWindows(windows);
}

}  // namespace ws2
}  // namespace ui
