// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {

OverlaySurfaceCandidate::OverlaySurfaceCandidate() : is_clipped(false) {}

OverlaySurfaceCandidate::OverlaySurfaceCandidate(
    const OverlaySurfaceCandidate& other) = default;

OverlaySurfaceCandidate::~OverlaySurfaceCandidate() {}

}  // namespace ui
