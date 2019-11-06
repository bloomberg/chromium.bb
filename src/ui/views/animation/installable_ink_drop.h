// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_H_
#define UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_H_

#include "base/feature_list.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/views_export.h"

namespace views {

extern const VIEWS_EXPORT base::Feature kInstallableInkDropFeature;

// Stub for future InkDrop implementation that will be installable on any View
// without needing InkDropHostView. This is currently non-functional and fails
// on some method calls. TODO(crbug.com/931964): implement the necessary parts
// of the API and remove the rest from the InkDrop interface.
class VIEWS_EXPORT InstallableInkDrop : public InkDrop {
 public:
  InstallableInkDrop();
  ~InstallableInkDrop() override;

  // InkDrop:
  void HostSizeChanged(const gfx::Size& new_size) override;
  InkDropState GetTargetInkDropState() const override;
  void AnimateToState(InkDropState ink_drop_state) override;
  void SetHoverHighlightFadeDurationMs(int duration_ms) override;
  void UseDefaultHoverHighlightFadeDuration() override;
  void SnapToActivated() override;
  void SnapToHidden() override;
  void SetHovered(bool is_hovered) override;
  void SetFocused(bool is_focused) override;
  bool IsHighlightFadingInOrVisible() const override;
  void SetShowHighlightOnHover(bool show_highlight_on_hover) override;
  void SetShowHighlightOnFocus(bool show_highlight_on_focus) override;

 private:
  InkDropState current_state_ = InkDropState::HIDDEN;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_H_
