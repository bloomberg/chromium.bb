// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/desktop_background_view.h"

#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"

////////////////////////////////////////////////////////////////////////////////
// DesktopBackgroundView, public:

DesktopBackgroundView::DesktopBackgroundView() {
  /*
  wallpaper_ = *ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_AURA_WALLPAPER);
      */
}

DesktopBackgroundView::~DesktopBackgroundView() {
}

////////////////////////////////////////////////////////////////////////////////
// DesktopBackgroundView, views::View overrides:

void DesktopBackgroundView::OnPaint(gfx::Canvas* canvas) {
  //canvas->TileImageInt(wallpaper_, 0, 0, width(), height());
}

void DesktopBackgroundView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  LOG(WARNING) << "Here";
}
