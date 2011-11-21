// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/shadow.h"

#include "grit/ui_resources.h"
#include "ui/aura_shell/image_grid.h"
#include "ui/base/resource/resource_bundle.h"

namespace aura_shell {
namespace internal {

Shadow::Shadow() {
}

Shadow::~Shadow() {
}

ui::Layer* Shadow::layer() const { return image_grid_->layer(); }

void Shadow::Init() {
  image_grid_.reset(new ImageGrid);

  ResourceBundle& res = ResourceBundle::GetSharedInstance();
  image_grid_->Init(&res.GetImageNamed(IDR_AURA_SHADOW_RECT_TOP_LEFT),
                    &res.GetImageNamed(IDR_AURA_SHADOW_RECT_TOP),
                    &res.GetImageNamed(IDR_AURA_SHADOW_RECT_TOP_RIGHT),
                    &res.GetImageNamed(IDR_AURA_SHADOW_RECT_LEFT),
                    NULL,
                    &res.GetImageNamed(IDR_AURA_SHADOW_RECT_RIGHT),
                    &res.GetImageNamed(IDR_AURA_SHADOW_RECT_BOTTOM_LEFT),
                    &res.GetImageNamed(IDR_AURA_SHADOW_RECT_BOTTOM),
                    &res.GetImageNamed(IDR_AURA_SHADOW_RECT_BOTTOM_RIGHT));
}

void Shadow::SetContentBounds(const gfx::Rect& content_bounds) {
  content_bounds_ = content_bounds;
  image_grid_->SetSize(
      gfx::Size(content_bounds.width() +
                    image_grid_->left_image_width() +
                    image_grid_->right_image_width(),
                content_bounds.height() +
                    image_grid_->top_image_height() +
                    image_grid_->bottom_image_height()));
  image_grid_->layer()->SetBounds(
      gfx::Rect(content_bounds.x() - image_grid_->left_image_width(),
                content_bounds.y() - image_grid_->top_image_height(),
                image_grid_->layer()->bounds().width(),
                image_grid_->layer()->bounds().height()));
}

}  // namespace internal
}  // namespace aura_shell
