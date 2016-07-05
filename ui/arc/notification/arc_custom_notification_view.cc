// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/arc_custom_notification_view.h"

#include "components/exo/notification_surface.h"
#include "components/exo/surface.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"

namespace arc {

ArcCustomNotificationView::ArcCustomNotificationView(
    ArcCustomNotificationItem* item,
    exo::NotificationSurface* surface)
    : item_(item), surface_(surface) {
  item_->AddObserver(this);
  OnItemPinnedChanged();
}

ArcCustomNotificationView::~ArcCustomNotificationView() {
  if (item_)
    item_->RemoveObserver(this);
}

void ArcCustomNotificationView::CreateFloatingCloseButton() {
  floating_close_button_ = new views::ImageButton(this);
  floating_close_button_->set_background(
      views::Background::CreateSolidBackground(SK_ColorTRANSPARENT));
  floating_close_button_->SetBorder(
      views::Border::CreateEmptyBorder(5, 5, 5, 5));

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  floating_close_button_->SetImage(
      views::CustomButton::STATE_NORMAL,
      rb.GetImageSkiaNamed(IDR_NOTIFICATION_CLOSE));
  floating_close_button_->SetImage(
      views::CustomButton::STATE_HOVERED,
      rb.GetImageSkiaNamed(IDR_NOTIFICATION_CLOSE_HOVER));
  floating_close_button_->SetImage(
      views::CustomButton::STATE_PRESSED,
      rb.GetImageSkiaNamed(IDR_NOTIFICATION_CLOSE_PRESSED));
  floating_close_button_->set_animate_on_state_change(false);
  floating_close_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_MESSAGE_CENTER_CLOSE_NOTIFICATION_BUTTON_ACCESSIBLE_NAME));

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = surface_->window();

  floating_close_button_widget_.reset(new views::Widget);
  floating_close_button_widget_->Init(params);
  floating_close_button_widget_->SetContentsView(floating_close_button_);
  floating_close_button_widget_->Show();

  Layout();
}

void ArcCustomNotificationView::ViewHierarchyChanged(
    const views::View::ViewHierarchyChangedDetails& details) {
  views::Widget* widget = GetWidget();

  // Bail if native_view() has attached to a different widget.
  if (widget && native_view() &&
      views::Widget::GetTopLevelWidgetForNativeView(native_view()) != widget) {
    return;
  }

  views::NativeViewHost::ViewHierarchyChanged(details);

  if (!widget || !surface_ || !details.is_add)
    return;

  SetPreferredSize(surface_->GetSize());
  Attach(surface_->window());
}

void ArcCustomNotificationView::Layout() {
  views::NativeViewHost::Layout();

  if (!floating_close_button_widget_ || !surface_ || !GetWidget())
    return;

  gfx::Rect surface_local_bounds(surface_->window()->bounds().size());
  gfx::Rect close_button_bounds(floating_close_button_->GetPreferredSize());
  close_button_bounds.set_x(surface_local_bounds.right() -
                            close_button_bounds.width());
  close_button_bounds.set_y(surface_local_bounds.y());
  floating_close_button_widget_->SetBounds(close_button_bounds);
}

void ArcCustomNotificationView::ButtonPressed(views::Button* sender,
                                              const ui::Event& event) {
  if (item_ && !item_->pinned() && sender == floating_close_button_) {
    item_->CloseFromCloseButton();
  }
}

void ArcCustomNotificationView::OnItemDestroying() {
  item_->RemoveObserver(this);
  item_ = nullptr;

  // Reset |surface_| with |item_| since no one is observing the |surface_|
  // after |item_| is gone and this view should be removed soon.
  surface_ = nullptr;
}

void ArcCustomNotificationView::OnItemPinnedChanged() {
  if (item_->pinned() && floating_close_button_widget_) {
    floating_close_button_widget_.reset();
  } else if (!item_->pinned() && !floating_close_button_widget_) {
    CreateFloatingCloseButton();
  }
}

void ArcCustomNotificationView::OnItemNotificationSurfaceRemoved() {
  surface_ = nullptr;
}

}  // namespace arc
