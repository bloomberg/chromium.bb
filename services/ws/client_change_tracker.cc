// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/client_change_tracker.h"

#include "services/ws/client_change.h"

namespace ws {

ClientChangeTracker::ClientChangeTracker() = default;

ClientChangeTracker::~ClientChangeTracker() = default;

bool ClientChangeTracker::IsProcessingChangeForWindow(aura::Window* window,
                                                      ClientChangeType type) {
  return current_change_ && current_change_->window() == window &&
         current_change_->type() == type;
}

}  // namespace ws
