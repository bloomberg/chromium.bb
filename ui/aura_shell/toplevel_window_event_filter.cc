// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/toplevel_window_event_filter.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/cursor.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura_shell/window_util.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"

namespace aura_shell {

namespace {

// Identifies the types of bounds change operations performed by a drag to a
// particular window component.
const int kBoundsChange_None = 0;
const int kBoundsChange_Repositions = 1;
const int kBoundsChange_Resizes = 2;

int GetBoundsChangeForWindowComponent(int window_component) {
  int bounds_change = kBoundsChange_None;
  switch (window_component) {
    case HTTOPLEFT:
    case HTTOP:
    case HTTOPRIGHT:
    case HTLEFT:
    case HTBOTTOMLEFT:
      bounds_change |= kBoundsChange_Repositions | kBoundsChange_Resizes;
      break;
    case HTCAPTION:
      bounds_change |= kBoundsChange_Repositions;
      break;
    case HTRIGHT:
    case HTBOTTOMRIGHT:
    case HTBOTTOM:
    case HTGROWBOX:
      bounds_change |= kBoundsChange_Resizes;
      break;
    default:
      break;
  }
  return bounds_change;
}

// Possible directions for changing bounds.

const int kBoundsChangeDirection_None = 0;
const int kBoundsChangeDirection_Horizontal = 1;
const int kBoundsChangeDirection_Vertical = 2;

int GetPositionChangeDirectionForWindowComponent(int window_component) {
  int pos_change_direction = kBoundsChangeDirection_None;
  switch (window_component) {
    case HTTOPLEFT:
    case HTBOTTOMRIGHT:
    case HTGROWBOX:
    case HTCAPTION:
      pos_change_direction |=
          kBoundsChangeDirection_Horizontal | kBoundsChangeDirection_Vertical;
      break;
    case HTTOP:
    case HTTOPRIGHT:
    case HTBOTTOM:
      pos_change_direction |= kBoundsChangeDirection_Vertical;
      break;
    case HTBOTTOMLEFT:
    case HTRIGHT:
    case HTLEFT:
      pos_change_direction |= kBoundsChangeDirection_Horizontal;
      break;
    default:
      break;
  }
  return pos_change_direction;
}

int GetSizeChangeDirectionForWindowComponent(int window_component) {
  int size_change_direction = kBoundsChangeDirection_None;
  switch (window_component) {
    case HTTOPLEFT:
    case HTTOPRIGHT:
    case HTBOTTOMLEFT:
    case HTBOTTOMRIGHT:
    case HTGROWBOX:
    case HTCAPTION:
      size_change_direction |=
          kBoundsChangeDirection_Horizontal | kBoundsChangeDirection_Vertical;
      break;
    case HTTOP:
    case HTBOTTOM:
      size_change_direction |= kBoundsChangeDirection_Vertical;
      break;
    case HTRIGHT:
    case HTLEFT:
      size_change_direction |= kBoundsChangeDirection_Horizontal;
      break;
    default:
      break;
  }
  return size_change_direction;
}

// Returns true for resize components along the right edge, where a drag in
// positive x will make the window larger.
bool IsRightEdge(int window_component) {
  return window_component == HTTOPRIGHT ||
      window_component == HTRIGHT ||
      window_component == HTBOTTOMRIGHT ||
      window_component == HTGROWBOX;
}

// Returns true for resize components in along the bottom edge, where a drag
// in positive y will make the window larger.
bool IsBottomEdge(int window_component) {
  return window_component == HTBOTTOMLEFT ||
      window_component == HTBOTTOM ||
      window_component == HTBOTTOMRIGHT ||
      window_component == HTGROWBOX;
}

void ToggleMaximizedState(aura::Window* window) {
  window->SetIntProperty(aura::kShowStateKey,
                         IsWindowMaximized(window) ? ui::SHOW_STATE_NORMAL
                                                   : ui::SHOW_STATE_MAXIMIZED);
}

}  // namespace

ToplevelWindowEventFilter::ToplevelWindowEventFilter(aura::Window* owner)
    : EventFilter(owner),
      window_component_(HTNOWHERE) {
}

ToplevelWindowEventFilter::~ToplevelWindowEventFilter() {
}

bool ToplevelWindowEventFilter::PreHandleKeyEvent(aura::Window* target,
                                                  aura::KeyEvent* event) {
  return false;
}

bool ToplevelWindowEventFilter::PreHandleMouseEvent(aura::Window* target,
                                                    aura::MouseEvent* event) {
  // Process EventFilters implementation first so that it processes
  // activation/focus first.
  switch (event->type()) {
    case ui::ET_MOUSE_MOVED:
      UpdateWindowComponentForEvent(target, event);
      break;
    case ui::ET_MOUSE_PRESSED:
      // We also update the current window component here because for the
      // mouse-drag-release-press case, where the mouse is released and
      // pressed without mouse move event.
      UpdateWindowComponentForEvent(target, event);
      if (window_component_ == HTCAPTION &&
          event->flags() & ui::EF_IS_DOUBLE_CLICK) {
        ToggleMaximizedState(target);
      }
      mouse_down_bounds_ = target->bounds();
      mouse_down_offset_in_parent_ = event->location();
      aura::Window::ConvertPointToWindow(target, target->parent(),
                                         &mouse_down_offset_in_parent_);
      return GetBoundsChangeForWindowComponent(window_component_) !=
          kBoundsChange_None;
    case ui::ET_MOUSE_DRAGGED:
      return HandleDrag(target, event);
    case ui::ET_MOUSE_RELEASED:
      window_component_ = HTNOWHERE;
      break;
    default:
      break;
  }
  return false;
}

ui::TouchStatus ToplevelWindowEventFilter::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  // Process EventFilters implementation first so that it processes
  // activation/focus first.
  // TODO(sad): Allow moving/resizing/maximizing etc. from touch?
  return ui::TOUCH_STATUS_UNKNOWN;
}

void ToplevelWindowEventFilter::MoveWindowToFront(aura::Window* target) {
  aura::Window* parent = target->parent();
  aura::Window* child = target;
  while (parent) {
    parent->StackChildAtTop(child);
    if (parent == owner())
      break;
    parent = parent->parent();
    child = child->parent();
  }
}

bool ToplevelWindowEventFilter::HandleDrag(aura::Window* target,
                                           aura::MouseEvent* event) {
  int bounds_change = GetBoundsChangeForWindowComponent(window_component_);
  if (bounds_change == kBoundsChange_None)
    return false;

  // Only a normal/default window can be moved/resized.
  if (target->GetIntProperty(aura::kShowStateKey) != ui::SHOW_STATE_NORMAL &&
      target->GetIntProperty(aura::kShowStateKey) != ui::SHOW_STATE_DEFAULT)
    return false;

  // Dragging a window moves the local coordinate frame, so do arithmetic
  // in the parent coordinate frame.
  gfx::Point event_location_in_parent(event->location());
  aura::Window::ConvertPointToWindow(target, target->parent(),
                                     &event_location_in_parent);
  int delta_x = event_location_in_parent.x() - mouse_down_offset_in_parent_.x();
  int delta_y = event_location_in_parent.y() - mouse_down_offset_in_parent_.y();

  // The minimize size constraint may limit how much we change the window
  // position.  For example, dragging the left edge to the right should stop
  // repositioning the window when the minimize size is reached.
  gfx::Size size = GetSizeForDrag(bounds_change, target, &delta_x, &delta_y);
  gfx::Point origin = GetOriginForDrag(bounds_change, delta_x, delta_y);

  target->SetBounds(gfx::Rect(origin, size));
  return true;
}

void ToplevelWindowEventFilter::UpdateWindowComponentForEvent(
    aura::Window* target,
    aura::MouseEvent* event) {
  window_component_ =
      target->delegate()->GetNonClientComponent(event->location());
}

gfx::Point ToplevelWindowEventFilter::GetOriginForDrag(
    int bounds_change,
    int delta_x,
    int delta_y) const {
  gfx::Point origin = mouse_down_bounds_.origin();
  if (bounds_change & kBoundsChange_Repositions) {
    int pos_change_direction =
        GetPositionChangeDirectionForWindowComponent(window_component_);
    if (pos_change_direction & kBoundsChangeDirection_Horizontal)
      origin.Offset(delta_x, 0);
    if (pos_change_direction & kBoundsChangeDirection_Vertical)
      origin.Offset(0, delta_y);
  }
  return origin;
}

gfx::Size ToplevelWindowEventFilter::GetSizeForDrag(
    int bounds_change,
    aura::Window* target,
    int* delta_x,
    int* delta_y) const {
  gfx::Size size = mouse_down_bounds_.size();
  if (bounds_change & kBoundsChange_Resizes) {
    gfx::Size min_size = target->delegate()->GetMinimumSize();
    int size_change_direction =
        GetSizeChangeDirectionForWindowComponent(window_component_);
    size.SetSize(
      GetWidthForDrag(size_change_direction, min_size.width(), delta_x),
      GetHeightForDrag(size_change_direction, min_size.height(), delta_y));
  }
  return size;
}

int ToplevelWindowEventFilter::GetWidthForDrag(int size_change_direction,
                                               int min_width,
                                               int* delta_x) const {
  int width = mouse_down_bounds_.width();
  if (size_change_direction & kBoundsChangeDirection_Horizontal) {
    // Along the right edge, positive delta_x increases the window size.
    int x_multiplier = IsRightEdge(window_component_) ? 1 : -1;
    width += x_multiplier * (*delta_x);

    // Ensure we don't shrink past the minimum width and clamp delta_x
    // for the window origin computation.
    if (width < min_width) {
      width = min_width;
      *delta_x = -x_multiplier * (mouse_down_bounds_.width() - min_width);
    }
  }
  return width;
}

int ToplevelWindowEventFilter::GetHeightForDrag(int size_change_direction,
                                                int min_height,
                                                int* delta_y) const {
  int height = mouse_down_bounds_.height();
  if (size_change_direction & kBoundsChangeDirection_Vertical) {
    // Along the bottom edge, positive delta_y increases the window size.
    int y_multiplier = IsBottomEdge(window_component_) ? 1 : -1;
    height += y_multiplier * (*delta_y);

    // Ensure we don't shrink past the minimum height and clamp delta_y
    // for the window origin computation.
    if (height < min_height) {
      height = min_height;
      *delta_y = -y_multiplier * (mouse_down_bounds_.height() - min_height);
    }
  }
  return height;
}

}  // namespace aura
