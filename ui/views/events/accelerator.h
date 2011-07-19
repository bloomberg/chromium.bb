// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EVENTS_ACCELERATOR_H_
#define UI_VIEWS_EVENTS_ACCELERATOR_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "ui/base/models/accelerator.h"
#include "ui/views/events/event.h"

namespace ui {

string16 GetShortcutTextForAccelerator(const Accelerator& accelerator);

// An interface that classes that want to register for keyboard accelerators
// should implement.
class AcceleratorTarget {
 public:
  // This method should return true if the accelerator was processed.
  virtual bool AcceleratorPressed(const Accelerator& accelerator) = 0;

 protected:
  virtual ~AcceleratorTarget() {}
};

}  // namespace ui

#endif  // UI_VIEWS_EVENTS_ACCELERATOR_H_
