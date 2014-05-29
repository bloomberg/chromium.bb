// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/touch_exploration_controller.h"

#include "base/logging.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"

namespace ui {

TouchExplorationController::TouchExplorationController(
    aura::Window* root_window)
        : root_window_(root_window) {
  CHECK(root_window);
  root_window->GetHost()->GetEventSource()->AddEventRewriter(this);
}

TouchExplorationController::~TouchExplorationController() {
  root_window_->GetHost()->GetEventSource()->RemoveEventRewriter(this);
}

ui::EventRewriteStatus TouchExplorationController::RewriteEvent(
    const ui::Event& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  if (!event.IsTouchEvent())
    return ui::EVENT_REWRITE_CONTINUE;

  const ui::TouchEvent& touch_event =
      static_cast<const ui::TouchEvent&>(event);
  const ui::EventType type = touch_event.type();
  const gfx::PointF& location = touch_event.location_f();
  const int touch_id = touch_event.touch_id();
  const int flags = touch_event.flags();

  if (type == ui::ET_TOUCH_PRESSED) {
    touch_ids_.push_back(touch_id);
    touch_locations_.insert(std::pair<int, gfx::PointF>(touch_id, location));
    // If this is the first and only finger touching - rewrite the touch as a
    // mouse move. Otherwise let the it go through as is.
    if (touch_ids_.size() == 1) {
      *rewritten_event = CreateMouseMoveEvent(location, flags);
      EnterTouchToMouseMode();
      return ui::EVENT_REWRITE_REWRITTEN;
    }
    return ui::EVENT_REWRITE_CONTINUE;
  } else if (type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) {
    std::vector<int>::iterator it =
        std::find(touch_ids_.begin(), touch_ids_.end(), touch_id);
    // We may fail to find the finger if the exploration mode was turned on
    // while the user had some fingers touching the screen. We simply ignore
    // those fingers for the purposes of event transformation.
    if (it == touch_ids_.end())
      return ui::EVENT_REWRITE_CONTINUE;
    const bool first_finger_released = it == touch_ids_.begin();
    touch_ids_.erase(it);
    int num_erased = touch_locations_.erase(touch_id);
    DCHECK_EQ(num_erased, 1);
    const int num_fingers_remaining = touch_ids_.size();

    if (num_fingers_remaining == 0) {
      *rewritten_event = CreateMouseMoveEvent(location, flags);
      return ui::EVENT_REWRITE_REWRITTEN;
    }

    // If we are left with one finger - enter the mouse move mode.
    const bool enter_mouse_move_mode = num_fingers_remaining == 1;

    if (!enter_mouse_move_mode && !first_finger_released) {
      // No special handling needed.
      return ui::EVENT_REWRITE_CONTINUE;
    }

    // If the finger which was released was the first one, - we need to rewrite
    // the release event as a release of the was second / now first finger.
    // This is the finger which will now be getting "substracted".
    if (first_finger_released) {
      int rewritten_release_id = touch_ids_[0];
      const gfx::PointF& rewritten_release_location =
          touch_locations_[rewritten_release_id];
      ui::TouchEvent* rewritten_release_event = new ui::TouchEvent(
          ui::ET_TOUCH_RELEASED,
          rewritten_release_location,
          rewritten_release_id,
          event.time_stamp());
      rewritten_release_event->set_flags(touch_event.flags());
      rewritten_event->reset(rewritten_release_event);
    } else if (enter_mouse_move_mode) {
      // Dispatch the release event as is.
      // TODO(mfomitchev): We can get rid of this clause once we have
      // EVENT_REWRITE_DISPATCH_ANOTHER working without having to set
      // rewritten_event.
      rewritten_event->reset(new ui::TouchEvent(touch_event));
    }

    if (enter_mouse_move_mode) {
      // Since we are entering the mouse move mode - also dispatch a mouse move
      // event at the location of the one remaining finger. (num_fingers == 1)
      const gfx::PointF& mouse_move_location = touch_locations_[touch_ids_[0]];
      next_dispatch_event_ =
          CreateMouseMoveEvent(mouse_move_location, flags).Pass();
      return ui::EVENT_REWRITE_DISPATCH_ANOTHER;
    }
    return ui::EVENT_REWRITE_REWRITTEN;
  } else if (type == ui::ET_TOUCH_MOVED) {
    std::vector<int>::iterator it =
        std::find(touch_ids_.begin(), touch_ids_.end(), touch_id);
    // We may fail to find the finger if the exploration mode was turned on
    // while the user had some fingers touching the screen. We simply ignore
    // those fingers for the purposes of event transformation.
    if (it == touch_ids_.end())
      return ui::EVENT_REWRITE_CONTINUE;
    touch_locations_[*it] = location;
    if (touch_ids_.size() == 1) {
      // Touch moves are rewritten as mouse moves when there's only one finger
      // touching the screen.
      *rewritten_event = CreateMouseMoveEvent(location, flags).Pass();
      return ui::EVENT_REWRITE_REWRITTEN;
    }
    if (touch_id == touch_ids_.front()) {
      // Touch moves of the first finger are discarded when there's more than
      // one finger touching.
      return ui::EVENT_REWRITE_DISCARD;
    }
    return ui::EVENT_REWRITE_CONTINUE;
  }
  NOTREACHED() << "Unexpected event type received.";
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus TouchExplorationController::NextDispatchEvent(
    const ui::Event& last_event,
    scoped_ptr<ui::Event>* new_event) {
  CHECK(next_dispatch_event_);
  DCHECK(last_event.IsTouchEvent());
  *new_event = next_dispatch_event_.Pass();
  // Enter the mouse move mode if needed
  if ((*new_event)->IsMouseEvent())
    EnterTouchToMouseMode();
  return ui::EVENT_REWRITE_REWRITTEN;
}

scoped_ptr<ui::Event> TouchExplorationController::CreateMouseMoveEvent(
    const gfx::PointF& location,
    int flags) {
  return scoped_ptr<ui::Event>(
      new ui::MouseEvent(
          ui::ET_MOUSE_MOVED,
          location,
          location,
          flags | ui::EF_IS_SYNTHESIZED | ui::EF_TOUCH_ACCESSIBILITY,
          0));
}

void TouchExplorationController::EnterTouchToMouseMode() {
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window_);
  if (cursor_client && !cursor_client->IsMouseEventsEnabled())
    cursor_client->EnableMouseEvents();
  if (cursor_client && cursor_client->IsCursorVisible())
    cursor_client->HideCursor();
}

}  // namespace ui
