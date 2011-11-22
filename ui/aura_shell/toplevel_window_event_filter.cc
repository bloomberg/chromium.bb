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

int GetXMultiplierForWindowComponent(int window_component) {
  return window_component == HTTOPRIGHT ? -1 : 1;
}

int GetYMultiplierForWindowComponent(int window_component) {
  return window_component == HTBOTTOMLEFT ? -1 : 1;
}

}

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
      mouse_down_bounds_ = target->bounds();
      mouse_down_offset_in_target_ = event->location();
      mouse_down_offset_in_parent_ = mouse_down_offset_in_target_;
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

  target->SetBounds(gfx::Rect(GetOriginForDrag(bounds_change, target, event),
                              GetSizeForDrag(bounds_change, target, event)));
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
    aura::Window* target,
    aura::MouseEvent* event) const {
  gfx::Point origin = mouse_down_bounds_.origin();
  if (bounds_change & kBoundsChange_Repositions) {
    int pos_change_direction =
        GetPositionChangeDirectionForWindowComponent(window_component_);

    if (pos_change_direction & kBoundsChangeDirection_Horizontal) {
      origin.set_x(event->location().x());
      origin.Offset(-mouse_down_offset_in_target_.x(), 0);
      origin.Offset(target->bounds().x(), 0);
    }
    if (pos_change_direction & kBoundsChangeDirection_Vertical) {
      origin.set_y(event->location().y());
      origin.Offset(0, -mouse_down_offset_in_target_.y());
      origin.Offset(0, target->bounds().y());
    }
  }
  return origin;
}

gfx::Size ToplevelWindowEventFilter::GetSizeForDrag(
    int bounds_change,
    aura::Window* target,
    aura::MouseEvent* event) const {
  gfx::Size size = mouse_down_bounds_.size();
  if (bounds_change & kBoundsChange_Resizes) {
    int size_change_direction =
        GetSizeChangeDirectionForWindowComponent(window_component_);

    gfx::Point event_location_in_parent(event->location());
    aura::Window::ConvertPointToWindow(target, target->parent(),
                                       &event_location_in_parent);

    // The math changes depending on whether the window is being resized, or
    // repositioned in addition to being resized.
    int first_x = bounds_change & kBoundsChange_Repositions ?
        mouse_down_offset_in_parent_.x() : event_location_in_parent.x();
    int first_y = bounds_change & kBoundsChange_Repositions ?
        mouse_down_offset_in_parent_.y() : event_location_in_parent.y();
    int second_x = bounds_change & kBoundsChange_Repositions ?
        event_location_in_parent.x() : mouse_down_offset_in_parent_.x();
    int second_y = bounds_change & kBoundsChange_Repositions ?
        event_location_in_parent.y() : mouse_down_offset_in_parent_.y();

    int x_multiplier = GetXMultiplierForWindowComponent(window_component_);
    int y_multiplier = GetYMultiplierForWindowComponent(window_component_);

    int width = size.width() +
        (size_change_direction & kBoundsChangeDirection_Horizontal ?
         x_multiplier * (first_x - second_x) : 0);
    int height = size.height() +
        (size_change_direction & kBoundsChangeDirection_Vertical ?
         y_multiplier * (first_y - second_y) : 0);

    size.SetSize(width, height);
  }
  return size;
}

}  // namespace aura
