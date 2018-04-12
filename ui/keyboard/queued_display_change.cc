// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/queued_display_change.h"
#include "base/bind.h"
#include "ui/display/display.h"
#include "ui/keyboard/keyboard_controller.h"

namespace keyboard {

QueuedDisplayChange::QueuedDisplayChange(const display::Display& display)
    : new_display_(display){};

QueuedDisplayChange::~QueuedDisplayChange(){};

}  // namespace keyboard
