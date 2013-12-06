// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_targeter.h"

namespace aura {

class WindowTargeter : public ui::EventTargeter {
 public:
  WindowTargeter();
  virtual ~WindowTargeter();

 protected:
  // ui::EventTargeter:
  virtual ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                              ui::Event* event) OVERRIDE;
  virtual bool SubtreeShouldBeExploredForEvent(
      ui::EventTarget* target,
      const ui::LocatedEvent& event) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(WindowTargeter);
};

}  // namespace aura
