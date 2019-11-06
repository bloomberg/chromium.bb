// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_action_handler.h"

#include "ui/accessibility/ax_tree_id_registry.h"

namespace ui {

bool AXActionHandler::RequiresPerformActionPointInPixels() const {
  return false;
}

AXActionHandler::AXActionHandler()
    : tree_id_(AXTreeIDRegistry::GetInstance()->GetOrCreateAXTreeID(this)) {}

AXActionHandler::~AXActionHandler() {
  AXTreeIDRegistry::GetInstance()->RemoveAXTreeID(tree_id_);
}

}  // namespace ui
