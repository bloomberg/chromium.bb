// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TARGETER_H_
#define UI_AURA_WINDOW_TARGETER_H_

#include "ui/aura/aura_export.h"
#include "ui/events/event_targeter.h"

namespace aura {

class Window;

class AURA_EXPORT WindowTargeter : public ui::EventTargeter {
 public:
  WindowTargeter();
  virtual ~WindowTargeter();

 protected:
  bool WindowCanAcceptEvent(aura::Window* window,
                            const ui::LocatedEvent& event) const;

  // Returns whether the location of the event is in an actionable region of the
  // window. Note that the location etc. of |event| is in the |window|'s
  // parent's coordinate system.
  virtual bool EventLocationInsideBounds(aura::Window* window,
                                         const ui::LocatedEvent& event) const;

  // ui::EventTargeter:
  virtual ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                              ui::Event* event) OVERRIDE;
  virtual ui::EventTarget* FindTargetForLocatedEvent(
      ui::EventTarget* root,
      ui::LocatedEvent* event) OVERRIDE;
  virtual bool SubtreeShouldBeExploredForEvent(
      ui::EventTarget* target,
      const ui::LocatedEvent& event) OVERRIDE;

 private:
  Window* FindTargetInRootWindow(Window* root_window,
                                 const ui::LocatedEvent& event);

  DISALLOW_COPY_AND_ASSIGN(WindowTargeter);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TARGETER_H_
