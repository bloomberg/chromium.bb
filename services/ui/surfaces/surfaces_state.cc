// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/surfaces/surfaces_state.h"

namespace ui {

SurfacesState::SurfacesState() : next_client_id_(1u) {}

SurfacesState::~SurfacesState() {}

}  // namespace ui
