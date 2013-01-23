// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_simple_view.h"

#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/grid_layout.h"

namespace message_center {

const SkColor kNotificationColor = SkColorSetRGB(0xfe, 0xfe, 0xfe);
const SkColor kNotificationReadColor = SkColorSetRGB(0xfa, 0xfa, 0xfa);

MessageSimpleView::MessageSimpleView(
    NotificationList::Delegate* list_delegate,
    const NotificationList::Notification& notification)
    : MessageView(list_delegate, notification) {
}

MessageSimpleView::~MessageSimpleView() {
}

void MessageSimpleView::SetUpView() {
  SkColor bg_color = notification().is_read ?
      kNotificationReadColor : kNotificationColor;
  set_background(views::Background::CreateSolidBackground(bg_color));

  views::ImageView* icon = new views::ImageView;
  icon->SetImageSize(
      gfx::Size(kWebNotificationIconSize, kWebNotificationIconSize));
  icon->SetImage(notification().primary_icon);

  views::Label* title = new views::Label(notification().title);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetFont(title->font().DeriveFont(0, gfx::Font::BOLD));
  views::Label* message = new views::Label(notification().message);
  message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message->SetMultiLine(true);

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  views::ColumnSet* columns = layout->AddColumnSet(0);

  const int padding_width = kPaddingHorizontal / 2;
  columns->AddPaddingColumn(0, padding_width);

  // Notification Icon.
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                     0, /* resize percent */
                     views::GridLayout::FIXED,
                     kWebNotificationIconSize, kWebNotificationIconSize);

  columns->AddPaddingColumn(0, padding_width);

  // Notification message text.
  const int message_width = kWebNotificationWidth - kWebNotificationIconSize -
      kWebNotificationButtonWidth - (padding_width * 3) -
      (scroller() ? scroller()->GetScrollBarWidth() : 0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                     100, /* resize percent */
                     views::GridLayout::FIXED,
                     message_width, message_width);

  columns->AddPaddingColumn(0, padding_width);
  message->SizeToFit(message_width);

  // Close button.
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                     0, /* resize percent */
                     views::GridLayout::FIXED,
                     kWebNotificationButtonWidth,
                     kWebNotificationButtonWidth);

  // Layout rows
  layout->AddPaddingRow(0, kPaddingBetweenItems);

  layout->StartRow(0, 0);
  layout->AddView(icon, 1, 2);
  layout->AddView(title, 1, 1);
  layout->AddView(close_button(), 1, 1);

  layout->StartRow(0, 0);
  layout->SkipColumns(2);
  layout->AddView(message, 1, 1);
  layout->AddPaddingRow(0, kPaddingBetweenItems);
}

}  // namespace message_center
