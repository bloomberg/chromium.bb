// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/drag_image_view.h"

#include "ui/views/widget/widget.h"

namespace aura_shell {
namespace internal {

namespace {
using views::Widget;

Widget* CreateDragWidget() {
  Widget* drag_widget = new Widget;
  Widget::InitParams params;
  params.type = Widget::InitParams::TYPE_TOOLTIP;
  params.keep_on_top = true;
  params.accept_events = false;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.transparent = true;
  drag_widget->Init(params);
  drag_widget->SetOpacity(0xFF);
  return drag_widget;
}
}

DragImageView::DragImageView() : views::ImageView() {
  widget_.reset(CreateDragWidget());
  widget_->SetContentsView(this);
  widget_->SetAlwaysOnTop(true);

  // We are owned by the DragDropController.
  set_parent_owned(false);
}

DragImageView::~DragImageView() {
  widget_->Hide();
}

void DragImageView::SetScreenBounds(const gfx::Rect& bounds) {
  widget_->SetBounds(bounds);
}

void DragImageView::SetScreenPosition(const gfx::Point& position) {
  widget_->SetBounds(gfx::Rect(position, GetPreferredSize()));
}

void DragImageView::SetWidgetVisible(bool visible) {
  if (visible != widget_->IsVisible()) {
    if (visible)
      widget_->Show();
    else
      widget_->Hide();
  }
}

}  // namespace internal
}  // namespace aura_shell
