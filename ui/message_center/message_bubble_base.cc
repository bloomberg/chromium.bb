// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_bubble_base.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "ui/message_center/message_center_switches.h"
#include "ui/message_center/message_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace {
// Delay laying out the MessageBubbleBase until all notifications have been
// added and icons have had a chance to load.
const int kUpdateDelayMs = 50;
const int kMessageBubbleBaseDefaultMaxHeight = 400;
}

namespace message_center {

const SkColor MessageBubbleBase::kBackgroundColor =
    SkColorSetRGB(0xfe, 0xfe, 0xfe);
const SkColor MessageBubbleBase::kHeaderBackgroundColorLight =
    SkColorSetRGB(0xf1, 0xf1, 0xf1);
const SkColor MessageBubbleBase::kHeaderBackgroundColorDark =
    SkColorSetRGB(0xe7, 0xe7, 0xe7);

MessageBubbleBase::MessageBubbleBase(NotificationList::Delegate* list_delegate)
    : list_delegate_(list_delegate),
      bubble_view_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      max_height_(kMessageBubbleBaseDefaultMaxHeight) {
}

MessageBubbleBase::~MessageBubbleBase() {
  if (bubble_view_)
    bubble_view_->reset_delegate();
}

void MessageBubbleBase::BubbleViewDestroyed() {
  bubble_view_ = NULL;
  OnBubbleViewDestroyed();
}

void MessageBubbleBase::ScheduleUpdate() {
  weak_ptr_factory_.InvalidateWeakPtrs();  // Cancel any pending update.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&MessageBubbleBase::UpdateBubbleView,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kUpdateDelayMs));
}

bool MessageBubbleBase::IsVisible() const {
  return bubble_view() && bubble_view()->GetWidget()->IsVisible();
}

void MessageBubbleBase::SetMaxHeight(int height) {
  // Maximum height makes sense only for the new design.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableNewMessageCenterBubble)) {
    return;
  }

  if (height == 0)
    height = kMessageBubbleBaseDefaultMaxHeight;
  if (height == max_height_)
    return;

  max_height_ = height;
  if (bubble_view_)
    bubble_view_->SetMaxHeight(max_height_);
}

views::TrayBubbleView::InitParams MessageBubbleBase::GetDefaultInitParams(
    views::TrayBubbleView::AnchorAlignment anchor_alignment) {
  views::TrayBubbleView::InitParams init_params(
      views::TrayBubbleView::ANCHOR_TYPE_TRAY,
      anchor_alignment,
      kWebNotificationWidth,
      kWebNotificationWidth);
  init_params.arrow_color = kHeaderBackgroundColorDark;
  return init_params;
}

}  // namespace message_center
