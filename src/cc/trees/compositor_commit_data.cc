// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/compositor_commit_data.h"

#include "cc/trees/swap_promise.h"

namespace cc {

CompositorCommitData::CompositorCommitData()
    : page_scale_delta(1.f),
      is_pinch_gesture_active(false),
      top_controls_delta(0.f),
      bottom_controls_delta(0.f),
      browser_controls_constraint(BrowserControlsState::kBoth),
      browser_controls_constraint_changed(false),
      scroll_gesture_did_end(false),
      manipulation_info(kManipulationInfoNone) {}

CompositorCommitData::~CompositorCommitData() = default;

CompositorCommitData::ScrollUpdateInfo::ScrollUpdateInfo() = default;

CompositorCommitData::ScrollUpdateInfo::ScrollUpdateInfo(
    ElementId id,
    gfx::ScrollOffset delta,
    base::Optional<TargetSnapAreaElementIds> snap_target_ids)
    : element_id(id),
      scroll_delta(delta),
      snap_target_element_ids(snap_target_ids) {}

CompositorCommitData::ScrollUpdateInfo::ScrollUpdateInfo(
    const ScrollUpdateInfo& other) = default;

CompositorCommitData::ScrollUpdateInfo&
CompositorCommitData::ScrollUpdateInfo::operator=(
    const ScrollUpdateInfo& other) = default;

}  // namespace cc
