// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_TOUCH_EXPLORATION_CONTROLLER_H_
#define UI_CHROMEOS_TOUCH_EXPLORATION_CONTROLLER_H_

#include "base/values.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/events/event_rewriter.h"
#include "ui/gfx/geometry/point.h"

namespace aura {
class Window;
}

namespace ui {

class Event;

// TouchExplorationController is used in tandem with "Spoken Feedback" to
// make the touch UI accessible. TouchExplorationController rewrites the
// incoming touch events as follows:
// - When one finger is touching the screen, touch events are converted to mouse
//   moves. This is the "Touch Exploration Mode". (The idea is that mouse moves
//   will be subsequently used by another component to move focus between UI
//   elements, and the elements will be read out to the user.)
// - When more than one finger is touching the screen, touches from the
//   first (i.e. "oldest") finger are ignored, and the other touches go through
//   as is.
// The caller is expected to retain ownership of instances of this class and
// destroy them before |root_window| is destroyed.
class UI_CHROMEOS_EXPORT TouchExplorationController :
    public ui::EventRewriter {
 public:
  explicit TouchExplorationController(aura::Window* root_window);
  virtual ~TouchExplorationController();

 private:
  scoped_ptr<ui::Event> CreateMouseMoveEvent(const gfx::PointF& location,
                                             int flags);

  void EnterTouchToMouseMode();

  // Overridden from ui::EventRewriter
  virtual ui::EventRewriteStatus RewriteEvent(
      const ui::Event& event, scoped_ptr<ui::Event>* rewritten_event) OVERRIDE;
  virtual ui::EventRewriteStatus NextDispatchEvent(
      const ui::Event& last_event, scoped_ptr<ui::Event>* new_event) OVERRIDE;

  // A set of touch ids for fingers currently touching the screen.
  std::vector<int> touch_ids_;

  // Map of touch ids to their last known location.
  std::map<int, gfx::PointF> touch_locations_;

  // Initialized from RewriteEvent() and dispatched in NextDispatchEvent().
  scoped_ptr<ui::Event> next_dispatch_event_;

  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(TouchExplorationController);
};

}  // namespace ui

#endif  // UI_CHROMEOS_TOUCH_EXPLORATION_CONTROLLER_H_
