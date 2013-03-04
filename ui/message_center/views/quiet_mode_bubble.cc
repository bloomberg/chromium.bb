// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/quiet_mode_bubble.h"

#include "base/time.h"
#include "grit/ui_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/insets.h"
#include "ui/message_center/notification_list.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

const int kButtonVerticalMargin = 10;
const int kButtonHorizontalMargin = 20;
const SkColor kButtonNormalBackgroundColor = SK_ColorWHITE;
const SkColor kButtonHoveredBackgroundColor = SkColorSetRGB(0xf5, 0xf5, 0xf5);

class QuietModeButton : public views::TextButton {
 public:
  QuietModeButton(views::ButtonListener* listener, int message_id)
      : views::TextButton(listener, l10n_util::GetStringUTF16(message_id)) {
    set_border(views::Border::CreateEmptyBorder(
        kButtonVerticalMargin, kButtonHorizontalMargin,
        kButtonVerticalMargin, kButtonHorizontalMargin));
    set_alignment(views::TextButtonBase::ALIGN_LEFT);
    set_background(views::Background::CreateSolidBackground(
        kButtonNormalBackgroundColor));
  }

 protected:
  virtual void StateChanged() OVERRIDE {
    set_background(views::Background::CreateSolidBackground(
        (state() == views::CustomButton::STATE_HOVERED) ?
        kButtonHoveredBackgroundColor : kButtonNormalBackgroundColor));
  }
};

}  // namespace

namespace message_center {

QuietModeBubble::QuietModeBubble(views::View* anchor_view,
                                 gfx::NativeView parent_window,
                                 NotificationList* notification_list)
    : notification_list_(notification_list) {
  DCHECK(notification_list_);
  bubble_ = new views::BubbleDelegateView(
      anchor_view, views::BubbleBorder::BOTTOM_RIGHT);
  bubble_->set_notify_enter_exit_on_child(true);

  if (views::View::get_use_acceleration_when_possible()) {
    bubble_->SetPaintToLayer(true);
    bubble_->SetFillsBoundsOpaquely(true);
  }

  bubble_->set_parent_window(parent_window);
  bubble_->set_margins(gfx::Insets());
  InitializeBubbleContents();
  views::BubbleDelegateView::CreateBubble(bubble_);
  bubble_->Show();
}

QuietModeBubble::~QuietModeBubble() {
  Close();
}

void QuietModeBubble::Close() {
  if (bubble_) {
    bubble_->GetWidget()->Close();
    bubble_ = NULL;
    quiet_mode_ = NULL;
    quiet_mode_1hour_ = NULL;
    quiet_mode_1day_ = NULL;
  }
}

views::Widget* QuietModeBubble::GetBubbleWidget() {
  return bubble_ ? bubble_->GetWidget() : NULL;
}

void QuietModeBubble::InitializeBubbleContents() {
  views::View* contents_view = bubble_->GetContentsView();
  contents_view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
  // TODO(mukai): Determine the actual UI to denote "enter/exit" quiet mode.
  quiet_mode_ = new QuietModeButton(
      this, (notification_list_->quiet_mode()) ?
      IDS_MESSAGE_CENTER_QUIET_MODE_EXIT : IDS_MESSAGE_CENTER_QUIET_MODE);
  contents_view->AddChildView(quiet_mode_);
  quiet_mode_1hour_ = new QuietModeButton(
      this, IDS_MESSAGE_CENTER_QUIET_MODE_1HOUR);
  contents_view->AddChildView(quiet_mode_1hour_);
  quiet_mode_1day_ = new QuietModeButton(
      this, IDS_MESSAGE_CENTER_QUIET_MODE_1DAY);
  contents_view->AddChildView(quiet_mode_1day_);
}

void QuietModeBubble::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  DCHECK(sender == quiet_mode_ ||
         sender == quiet_mode_1hour_ || sender == quiet_mode_1day_);
  if (sender == quiet_mode_) {
    notification_list_->SetQuietMode(!notification_list_->quiet_mode());
    LOG(INFO) << notification_list_->quiet_mode();
  } else {
    base::TimeDelta expires_in = (sender == quiet_mode_1day_) ?
        base::TimeDelta::FromDays(1) : base::TimeDelta::FromHours(1);
    notification_list_->EnterQuietModeWithExpire(expires_in);
  }
  Close();
}

}  // namespace message_center
