// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/surfaces/display_compositor.h"

namespace ui {

DisplayCompositor::DisplayCompositor(
    cc::mojom::DisplayCompositorClientPtr client)
    : client_(std::move(client)), next_client_id_(1u) {
  manager_.AddObserver(this);
}

uint32_t DisplayCompositor::GenerateNextClientId() {
  return next_client_id_++;
}

void DisplayCompositor::ReturnSurfaceReference(
    const cc::SurfaceSequence& sequence) {
  std::vector<uint32_t> sequences;
  sequences.push_back(sequence.sequence);
  manager_.DidSatisfySequences(sequence.frame_sink_id, &sequences);
}

DisplayCompositor::~DisplayCompositor() {
  manager_.RemoveObserver(this);
}

void DisplayCompositor::OnSurfaceCreated(const cc::SurfaceId& surface_id,
                                         const gfx::Size& frame_size,
                                         float device_scale_factor) {
  if (client_)
    client_->OnSurfaceCreated(surface_id, frame_size, device_scale_factor);
}

void DisplayCompositor::OnSurfaceDamaged(const cc::SurfaceId& surface_id,
                                         bool* changed) {}

}  // namespace ui
