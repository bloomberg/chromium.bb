// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_ACCELERATOR_MANAGER_CONTEXT_H_
#define UI_BASE_ACCELERATORS_ACCELERATOR_MANAGER_CONTEXT_H_

#include "ui/base/events/event_constants.h"
#include "ui/base/ui_export.h"

namespace ui {

class UI_EXPORT AcceleratorManagerContext {
 public:
  AcceleratorManagerContext() : last_event_type_(ET_KEY_PRESSED) {}
  ~AcceleratorManagerContext() {}

  EventType GetLastEventType() const { return last_event_type_; }

 private:
  friend class AcceleratorManager;

  // Type of the last (previous) event.
  EventType last_event_type_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorManagerContext);
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_ACCELERATOR_MANAGER_CONTEXT_H_
