// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/app_list/app_list_item_view.h"

#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura_shell/app_list/app_list_item_group_view.h"
#include "ui/aura_shell/app_list/app_list_item_model.h"
#include "ui/aura_shell/app_list/app_list_item_view_listener.h"
#include "ui/aura_shell/app_list/drop_shadow_label.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace aura_shell {

namespace {

const double kFocusedScale = 1.1;

const SkColor kTitleColor = SK_ColorWHITE;

gfx::Font GetTitleFont() {
  static gfx::Font* font = NULL;
  if (!font) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    font = new gfx::Font(rb.GetFont(ResourceBundle::BaseFont).DeriveFont(
        2, gfx::Font::BOLD));
  }
  return *font;
}

}  // namespace

AppListItemView::AppListItemView(AppListItemModel* model,
                                 AppListItemViewListener* listener)
    : model_(model),
      listener_(listener),
      icon_(new views::ImageView),
      title_(new DropShadowLabel) {
  set_focusable(true);

  title_->SetFont(GetTitleFont());
  title_->SetBackgroundColor(0);
  title_->SetEnabledColor(kTitleColor);

  AddChildView(icon_);
  AddChildView(title_);

  ItemIconChanged();
  ItemTitleChanged();
  model_->AddObserver(this);
}

AppListItemView::~AppListItemView() {
  model_->RemoveObserver(this);
}

void AppListItemView::NotifyActivated(int event_flags) {
  if (listener_)
    listener_->AppListItemActivated(this, event_flags);
}

void AppListItemView::ItemIconChanged() {
  icon_->SetImage(model_->icon());
}

void AppListItemView::ItemTitleChanged() {
  title_->SetText(UTF8ToUTF16(model_->title()));
}

gfx::Size AppListItemView::GetPreferredSize() {
  return gfx::Size(kTileSize, kTileSize);
}

void AppListItemView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  gfx::Size title_size = title_->GetPreferredSize();

  if (!HasFocus()) {
    const int horiz_padding = (kTileSize - kIconSize) / 2;
    const int vert_padding = (kTileSize - title_size.height() - kIconSize) / 2;
    rect.Inset(horiz_padding, vert_padding);
  }

  icon_->SetBounds(rect.x(), rect.y(),
      rect.width(), rect.height() - title_size.height());

  title_->SetBounds(rect.x(), rect.bottom() - title_size.height(),
      rect.width(), title_size.height());
}

void AppListItemView::OnFocus() {
  View::OnFocus();

  gfx::Size icon_size = icon_->GetPreferredSize();
  icon_size.set_width(icon_size.width() * kFocusedScale);
  icon_size.set_height(icon_size.height() * kFocusedScale);

  gfx::Size max_size = GetPreferredSize();
  if (icon_size.width() > max_size.width() ||
      icon_size.height() > max_size.height()) {
    double aspect =
        static_cast<double>(icon_size.width()) / icon_size.height();

    if (aspect > 1) {
      icon_size.set_width(max_size.width());
      icon_size.set_height(icon_size.width() / aspect);
    } else {
      icon_size.set_height(max_size.height());
      icon_size.set_width(icon_size.height() * aspect);
    }
  }

  icon_->SetImageSize(icon_size);
  Layout();

  AppListItemGroupView* group_view =
      static_cast<AppListItemGroupView*>(parent());
  group_view->UpdateFocusedTile(this);
}

void AppListItemView::OnBlur() {
  icon_->ResetImageSize();
  Layout();
  SchedulePaint();
}

bool AppListItemView::OnKeyPressed(const views::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RETURN) {
    NotifyActivated(event.flags());
    return true;
  }

  return false;
}

bool AppListItemView::OnMousePressed(const views::MouseEvent& event) {
  views::View* hit_view = GetEventHandlerForPoint(event.location());
  bool hit = hit_view != this;
  if (hit)
    RequestFocus();

  return hit;
}

void AppListItemView::OnMouseReleased(const views::MouseEvent& event) {
  views::View* hit_view = GetEventHandlerForPoint(event.location());
  if (hit_view != this)
    NotifyActivated(event.flags());
}

void AppListItemView::OnPaintFocusBorder(gfx::Canvas* canvas) {
  // No focus border for AppListItemView.
}

}  // namespace aura_shell
