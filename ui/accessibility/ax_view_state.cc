// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_view_state.h"

namespace ui {

AXViewState::AXViewState()
    : role(AX_ROLE_CLIENT),
      state(0),
      selection_start(-1),
      selection_end(-1),
      index(-1),
      count(-1) { }

AXViewState::~AXViewState() { }

}  // namespace ui
