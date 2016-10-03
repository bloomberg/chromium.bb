// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/surfaces/display_compositor.h"

namespace ui {

DisplayCompositor::DisplayCompositor() : next_client_id_(1u) {}

DisplayCompositor::~DisplayCompositor() {}

uint32_t DisplayCompositor::GenerateNextClientId() {
  return next_client_id_++;
}

}  // namespace ui
