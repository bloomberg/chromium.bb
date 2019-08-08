// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/installable_ink_drop.h"

#include "base/logging.h"

namespace views {

const base::Feature kInstallableInkDropFeature{
    "InstallableInkDrop", base::FEATURE_DISABLED_BY_DEFAULT};

InstallableInkDrop::InstallableInkDrop() = default;

InstallableInkDrop::~InstallableInkDrop() = default;

void InstallableInkDrop::HostSizeChanged(const gfx::Size& new_size) {}

InkDropState InstallableInkDrop::GetTargetInkDropState() const {
  return current_state_;
}

void InstallableInkDrop::AnimateToState(InkDropState ink_drop_state) {
  current_state_ = ink_drop_state;
}

void InstallableInkDrop::SetHoverHighlightFadeDurationMs(int duration_ms) {
  NOTREACHED();
}

void InstallableInkDrop::UseDefaultHoverHighlightFadeDuration() {
  NOTREACHED();
}

void InstallableInkDrop::SnapToActivated() {
  NOTREACHED();
}

void InstallableInkDrop::SnapToHidden() {
  NOTREACHED();
}

void InstallableInkDrop::SetHovered(bool is_hovered) {}

void InstallableInkDrop::SetFocused(bool is_focused) {}

bool InstallableInkDrop::IsHighlightFadingInOrVisible() const {
  return false;
}

void InstallableInkDrop::SetShowHighlightOnHover(bool show_highlight_on_hover) {
  NOTREACHED();
}

void InstallableInkDrop::SetShowHighlightOnFocus(bool show_highlight_on_focus) {
  NOTREACHED();
}

}  // namespace views
