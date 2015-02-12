// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "ui/events/event_targeter.h"
#include "ui/events/events_export.h"

namespace ui {

class EVENTS_EXPORT NullEventTargeter : public EventTargeter {
 public:
  NullEventTargeter();
  ~NullEventTargeter() override;

  EventTarget* FindTargetForEvent(EventTarget* root, Event* event) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NullEventTargeter);
};

}  // namespace ui
