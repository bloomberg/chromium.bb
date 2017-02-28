// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/folder_background_view.h"

#include <algorithm>

#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/views/app_list_folder_view.h"
#include "ui/app_list/views/apps_container_view.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/transform_util.h"

namespace app_list {

namespace {

const float kFolderInkBubbleScale = 1.2f;
const int kBubbleTransitionDurationMs = 200;

}  // namespace

FolderBackgroundView::FolderBackgroundView()
    : folder_view_(NULL),
      show_state_(NO_BUBBLE) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

FolderBackgroundView::~FolderBackgroundView() {
}

void FolderBackgroundView::UpdateFolderContainerBubble(ShowState state) {
  if (show_state_ == state ||
      (state == HIDE_BUBBLE && show_state_ == NO_BUBBLE)) {
    return;
  }

  show_state_ = state;

  // Set the initial state before the animation starts.
  const gfx::Rect bounds(layer()->bounds().size());
  gfx::Transform transform =
      gfx::GetScaleTransform(bounds.CenterPoint(), kFolderInkBubbleScale);
  if (show_state_ == SHOW_BUBBLE) {
    layer()->SetOpacity(0.0f);
    layer()->SetTransform(transform);
  } else {
    layer()->SetOpacity(1.0f);
    layer()->SetTransform(gfx::Transform());
  }

  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  settings.AddObserver(this);
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds((kBubbleTransitionDurationMs)));
  if (show_state_ == SHOW_BUBBLE) {
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
    layer()->SetOpacity(1.0f);
    layer()->SetTransform(gfx::Transform());
  } else {
    settings.SetTweenType(gfx::Tween::FAST_OUT_LINEAR_IN);
    layer()->SetOpacity(0.0f);
    layer()->SetTransform(transform);
  }

  SchedulePaint();
}

int FolderBackgroundView::GetFolderContainerBubbleRadius() const {
  return std::max(GetContentsBounds().width(), GetContentsBounds().height()) /
         2;
}

void FolderBackgroundView::OnPaint(gfx::Canvas* canvas) {
  if (show_state_ == NO_BUBBLE)
    return;

  // Draw ink bubble that shows the folder boundary.
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(kFolderBubbleColor);
  canvas->DrawCircle(GetContentsBounds().CenterPoint(),
                     GetFolderContainerBubbleRadius(), flags);
}

void FolderBackgroundView::OnImplicitAnimationsCompleted() {
  // Show folder name after the ink bubble disappears.
  if (show_state_ == HIDE_BUBBLE) {
    static_cast<AppsContainerView*>(parent())->app_list_folder_view()->
        UpdateFolderNameVisibility(true);
  }
}

}  // namespace app_list
