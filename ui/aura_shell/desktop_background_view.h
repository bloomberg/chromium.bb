// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_DESKTOP_BACKGROUND_VIEW_H_
#define UI_AURA_SHELL_DESKTOP_BACKGROUND_VIEW_H_
#pragma once

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_delegate.h"

namespace aura_shell {
namespace internal {

class DesktopBackgroundView : public views::WidgetDelegateView {
 public:
  DesktopBackgroundView();
  virtual ~DesktopBackgroundView();

 private:
  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;

  SkBitmap wallpaper_;

  DISALLOW_COPY_AND_ASSIGN(DesktopBackgroundView);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_DESKTOP_BACKGROUND_VIEW_H_
