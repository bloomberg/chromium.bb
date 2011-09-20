// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/launcher/launcher_view.h"

#include "base/utf_string_conversions.h"
#include "ui/aura/desktop.h"
#include "ui/aura_shell/aura_shell_export.h"
#include "ui/aura_shell/launcher/launcher_button.h"
#include "ui/gfx/canvas.h"
#include "views/widget/widget.h"

namespace aura_shell {
namespace internal {

LauncherView::LauncherView()
    : ALLOW_THIS_IN_INITIALIZER_LIST(chrome_button_(new LauncherButton(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          applist_button_(new LauncherButton(this))) {
  AddChildView(chrome_button_);
  AddChildView(applist_button_);
}
LauncherView::~LauncherView() {
}

void LauncherView::Layout() {
}

void LauncherView::OnPaint(gfx::Canvas* canvas) {
  canvas->FillRectInt(SK_ColorRED, 0, 0, width(), height());
}

void LauncherView::ButtonPressed(views::Button* sender,
                                 const views::Event& event) {
}

AURA_SHELL_EXPORT views::Widget* CreateLauncher() {
  views::Widget* launcher_widget = new views::Widget;
  views::Widget::InitParams params2(views::Widget::InitParams::TYPE_CONTROL);
  params2.bounds = gfx::Rect(0, 0, 300, 64);
  params2.parent = aura::Desktop::GetInstance()->window();
  LauncherView* launcher_view = new LauncherView;
  params2.delegate = launcher_view;
  launcher_widget->Init(params2);
  launcher_widget->SetContentsView(launcher_view);
  launcher_widget->Show();
  launcher_widget->GetNativeView()->set_name(ASCIIToUTF16("LauncherView"));
  return launcher_widget;
}

}  // namespace internal
}  // namespace aura_shell
