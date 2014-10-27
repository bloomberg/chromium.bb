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
  ~WindowTargeter() override;

 protected:
  // ui::EventTargeter:
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override;
  ui::EventTarget* FindTargetForLocatedEvent(ui::EventTarget* root,
                                             ui::LocatedEvent* event) override;
  bool SubtreeCanAcceptEvent(ui::EventTarget* target,
                             const ui::LocatedEvent& event) const override;
  bool EventLocationInsideBounds(ui::EventTarget* target,
                                 const ui::LocatedEvent& event) const override;

 private:
  Window* FindTargetForKeyEvent(Window* root_window,
                                const ui::KeyEvent& event);
  Window* FindTargetInRootWindow(Window* root_window,
                                 const ui::LocatedEvent& event);

  DISALLOW_COPY_AND_ASSIGN(WindowTargeter);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TARGETER_H_
