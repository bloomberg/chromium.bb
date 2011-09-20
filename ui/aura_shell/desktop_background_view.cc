// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/desktop_background_view.h"

#include "grit/ui_resources.h"
#include "ui/aura/desktop.h"
#include "ui/aura_shell/aura_shell_export.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "views/widget/widget.h"

namespace aura_shell {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// DesktopBackgroundView, public:

DesktopBackgroundView::DesktopBackgroundView() {
  wallpaper_ = *ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_AURA_WALLPAPER);
}

DesktopBackgroundView::~DesktopBackgroundView() {
}

////////////////////////////////////////////////////////////////////////////////
// DesktopBackgroundView, views::View overrides:

void DesktopBackgroundView::OnPaint(gfx::Canvas* canvas) {
  canvas->TileImageInt(wallpaper_, 0, 0, width(), height());
}

AURA_SHELL_EXPORT views::Widget* CreateDesktopBackground() {
  views::Widget* desktop_widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.bounds = gfx::Rect(0, 0, 1024, 768);
  params.parent = aura::Desktop::GetInstance()->window();
  DesktopBackgroundView* view = new DesktopBackgroundView;
  params.delegate = view;
  desktop_widget->Init(params);
  desktop_widget->SetContentsView(view);
  desktop_widget->Show();
  desktop_widget->GetNativeView()->set_name(L"DesktopBackgroundView");
  return desktop_widget;
}

}  // namespace internal
}  // namespace aura_shell
