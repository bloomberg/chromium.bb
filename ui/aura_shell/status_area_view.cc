// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/status_area_view.h"

#include "base/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "ui/aura/desktop.h"
#include "ui/aura_shell/aura_shell_export.h"
#include "ui/aura_shell/shell.h"
#include "ui/aura_shell/shell_window_ids.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/widget/widget.h"

namespace aura_shell {
namespace internal {

StatusAreaView::StatusAreaView()
    : status_mock_(*ResourceBundle::GetSharedInstance().GetBitmapNamed(
          IDR_AURA_STATUS_MOCK)) {
}
StatusAreaView::~StatusAreaView() {
}

gfx::Size StatusAreaView::GetPreferredSize() {
  return gfx::Size(status_mock_.width(), status_mock_.height());
}

void StatusAreaView::OnPaint(gfx::Canvas* canvas) {
  canvas->DrawBitmapInt(status_mock_, 0, 0);
}

AURA_SHELL_EXPORT views::Widget* CreateStatusArea() {
  StatusAreaView* status_area_view = new StatusAreaView;
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  gfx::Size ps = status_area_view->GetPreferredSize();
  params.bounds = gfx::Rect(0, 0, ps.width(), ps.height());
  params.parent = Shell::GetInstance()->GetContainer(
      aura_shell::internal::kShellWindowId_StatusContainer);
  params.delegate = status_area_view;
  params.transparent = true;
  widget->Init(params);
  widget->SetContentsView(status_area_view);
  widget->Show();
  widget->GetNativeView()->SetName("StatusAreaView");
  return widget;
}

}  // namespace internal
}  // namespace aura_shell
