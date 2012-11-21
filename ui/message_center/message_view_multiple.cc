// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_view_multiple.h"

#include "base/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"

namespace message_center {

typedef std::vector<NotificationList::NotificationItem> NotificationItems;

const SkColor kNotificationColor = SkColorSetRGB(0xfe, 0xfe, 0xfe);
const SkColor kNotificationReadColor = SkColorSetRGB(0xfa, 0xfa, 0xfa);

MessageViewMultiple::MessageViewMultiple(
    NotificationList::Delegate* list_delegate,
    const NotificationList::Notification& notification)
    : MessageView(list_delegate, notification) {}

MessageViewMultiple::MessageViewMultiple() {}

MessageViewMultiple::~MessageViewMultiple() {}

// TODO(dharcourt): Make this a subclass of BaseFormatView or of a
// BaseFormatView superclass and leverage that class' SetUpView().
void MessageViewMultiple::SetUpView() {
  DCHECK(close_button_);

  SkColor bg_color = notification_.is_read ?
      kNotificationReadColor : kNotificationColor;
  set_background(views::Background::CreateSolidBackground(bg_color));

  // TODO(dharcourt): If possible, use GridLayout fill functionality to make the
  // message as wide as possible without reference to the kWebNotificationWidth.
  // TODO(dharcourt): Verify intended meaning of MessageView size constants.
  const int padding_width = kPaddingHorizontal / 2;
  const int message_width = kWebNotificationWidth - kWebNotificationIconSize -
      kWebNotificationButtonWidth - (padding_width * 3);

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  // Three columns (icon, messages, close button) surrounded by padding.
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddPaddingColumn(0, padding_width);
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                     0, /* resize weight */
                     views::GridLayout::FIXED,
                     kWebNotificationIconSize, kWebNotificationIconSize);
  columns->AddPaddingColumn(0, padding_width);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                     100, /* resize weight */
                     views::GridLayout::FIXED,
                     message_width, message_width);
  columns->AddPaddingColumn(0, padding_width);
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                     0, /* resize weight */
                     views::GridLayout::FIXED,
                     kWebNotificationButtonWidth,
                     kWebNotificationButtonWidth);

  // First row has the icon, title, and close button after some top padding.
  layout->AddPaddingRow(0, kPaddingBetweenItems);
  layout->StartRow(0, 0);

  views::ImageView* icon = new views::ImageView;
  icon->SetImageSize(
      gfx::Size(kWebNotificationIconSize, kWebNotificationIconSize));
  icon->SetImage(notification_.image);
  layout->AddView(icon, 1, 1 + notification_.items.size());

  views::Label* title = new views::Label(notification_.title);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetFont(title->font().DeriveFont(0, gfx::Font::BOLD));
  layout->AddView(title);

  close_button_ = new views::ImageButton(this);
  close_button_->SetImage(
      views::CustomButton::STATE_NORMAL,
      ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_MESSAGE_CLOSE));
  close_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                   views::ImageButton::ALIGN_MIDDLE);
  layout->AddView(close_button_);

  // One row for each notification item.
  NotificationItems::const_iterator i;
  for (i = notification_.items.begin(); i != notification_.items.end(); ++i) {
    views::Label* item_title = new views::Label(i->title);
    item_title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    layout->StartRow(0,0);
    layout->SkipColumns(2);
    layout->AddView(item_title);
    // TODO(dharcourt): Item details.
  }

  // TODO(dharcourt): Add a horizontal line.

  // One row for the summary message. TODO(dharcourt): Make it optional.
  views::Label* message = new views::Label(notification_.message);
  message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message->SetMultiLine(true);
  layout->StartRow(0, 0);
  layout->SkipColumns(2);
  layout->AddView(message);

  // TODO(dharcourt): Add a second icon to the right of the summary message.

  // Bottom padding.
  layout->AddPaddingRow(0, kPaddingBetweenItems);
}

}  // namespace message_center
