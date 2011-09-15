// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_DESKTOP_BACKGROUND_VIEW_H_
#define UI_AURA_SHELL_DESKTOP_BACKGROUND_VIEW_H_
#pragma once

#include "views/view.h"
#include "views/widget/widget_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"

class DesktopBackgroundView : public views::WidgetDelegateView {
 public:
  DesktopBackgroundView();
  virtual ~DesktopBackgroundView();

 private:
  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;

  SkBitmap wallpaper_;

  DISALLOW_COPY_AND_ASSIGN(DesktopBackgroundView);
};

#endif  // UI_AURA_SHELL_DESKTOP_BACKGROUND_VIEW_H_
