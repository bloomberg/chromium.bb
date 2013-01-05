// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification_view.h"

#include "base/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/size.h"
#include "ui/message_center/message_center_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace {

// Notification dimensions.
const int kIconTopPadding = 0;
const int kIconLeftPadding = 0;
const int kIconBottomPadding = 0;
const int kIconColumnWidth = message_center::kNotificationIconWidth;
const int kIconToTextPadding = 15;
const int kTextTopPadding = 9;
const int kTextBottomPadding = 12;
const int kTextToClosePadding = 10;
const int kCloseTopPadding = 6;
const int kCloseRightPadding = 6;
const int kCloseColumnWidth = 8;
const int kItemTitleToDetailsPadding = 3;
const int kImageTopPadding = 0;
const int kImageLeftPadding = 0;
const int kImageBottomPadding = 0;
const int kImageRightPadding = 0;

// Notification colors. The text background colors below are used only to keep
// view::Label from modifying the text color and will not actually be drawn.
// See view::Label's SetEnabledColor() and SetBackgroundColor() for details.
const SkColor kBackgroundColor = SkColorSetRGB(255, 255, 255);
const SkColor kTitleColor = SkColorSetRGB(68, 68, 68);
const SkColor kTitleBackgroundColor = SK_ColorWHITE;
const SkColor kMessageColor = SkColorSetRGB(136, 136, 136);
const SkColor kMessageBackgroundColor = SK_ColorBLACK;

// Static function to create an empty border to be used as padding.
views::Border* MakePadding(int top, int left, int bottom, int right) {
  return views::Border::CreateEmptyBorder(top, left, bottom, right);
}

// ItemViews are responsible for drawing each NotificationView item's title and
// message next to each other within a single column.
class ItemView : public views::View {
 public:
  ItemView(const message_center::NotificationList::NotificationItem& item);
  virtual ~ItemView();

 private:
  DISALLOW_COPY_AND_ASSIGN(ItemView);
};

ItemView::ItemView(
    const message_center::NotificationList::NotificationItem& item) {
  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal,
                           0, 0, kItemTitleToDetailsPadding);
  SetLayoutManager(layout);

  views::Label* title = new views::Label(item.title);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetElideBehavior(views::Label::ELIDE_AT_END);
  title->SetEnabledColor(kTitleColor);
  title->SetBackgroundColor(kTitleBackgroundColor);
  AddChildViewAt(title, 0);

  views::Label* details = new views::Label(item.message);
  details->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  details->SetElideBehavior(views::Label::ELIDE_AT_END);
  details->SetEnabledColor(kMessageColor);
  details->SetBackgroundColor(kMessageBackgroundColor);
  AddChildViewAt(details, 1);

  PreferredSizeChanged();
  SchedulePaint();
}

ItemView::~ItemView() {
}

// ProportionalImageViews match their heights to their widths to preserve the
// proportions of their images.
class ProportionalImageView : public views::ImageView {
 public:
  ProportionalImageView();
  virtual ~ProportionalImageView();

  // Overridden from views::View.
  virtual int GetHeightForWidth(int width) OVERRIDE;
};

ProportionalImageView::ProportionalImageView() {
}

ProportionalImageView::~ProportionalImageView() {
}

int ProportionalImageView::GetHeightForWidth(int width) {
  int height = 0;
  gfx::ImageSkia image = GetImage();
  if (image.width() > 0 && image.height() > 0) {
    double proportion = image.height() / (double) image.width();
    height = 0.5 + width * proportion;
    if (height > message_center::kNotificationMaximumImageHeight) {
      height = message_center::kNotificationMaximumImageHeight;
      width = 0.5 + height / proportion;
    }
    SetImageSize(gfx::Size(width, height));
  }
  return height;
}

}  // namespace

namespace message_center {

NotificationView::NotificationView(
    NotificationList::Delegate* list_delegate,
    const NotificationList::Notification& notification)
    : MessageView(list_delegate, notification) {
}

NotificationView::~NotificationView() {
}

void NotificationView::SetUpView() {
  set_background(views::Background::CreateSolidBackground(kBackgroundColor));

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  // Three columns (icon, text, close button) surrounded by padding. The icon
  // and close button columns and the padding have fixed widths and the text
  // column fills up the remaining space. To minimize the number of columns and
  // simplify column spanning padding is applied to each view within columns
  // instead of through padding columns.
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                     0, views::GridLayout::FIXED,
                     kIconLeftPadding + kIconColumnWidth + kIconToTextPadding,
                     kIconLeftPadding + kIconColumnWidth + kIconToTextPadding);
                     // Padding + icon + padding.
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                     100, views::GridLayout::USE_PREF,
                     0, 0);
                     // Text + padding (kTextToClosePadding).
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                     0, views::GridLayout::FIXED,
                     kCloseColumnWidth + kCloseRightPadding,
                     kCloseColumnWidth + kCloseRightPadding);
                     // Close button + padding.

  // Figure out how many rows the icon should span.
  int span = 2;  // Two rows for the close button padding and close button.
  int displayed_item_count =
      std::min(notification_.items.size(), kNotificationMaximumItems);
  if (displayed_item_count > 0)
    span += displayed_item_count;  // + one row per item.
  else
    span += 1;  // + one row for the message.

  // First row: Icon.
  layout->StartRow(0, 0);
  views::ImageView* icon = new views::ImageView();
  icon->SetImageSize(gfx::Size(message_center::kNotificationIconWidth,
                               message_center::kNotificationIconWidth));
  icon->SetImage(notification_.primary_icon);
  icon->SetHorizontalAlignment(views::ImageView::LEADING);
  icon->SetVerticalAlignment(views::ImageView::LEADING);
  icon->set_border(MakePadding(kIconTopPadding, kIconLeftPadding,
                               kIconBottomPadding, kIconToTextPadding));
  layout->AddView(icon, 1, span);

  // First row: Title. This vertically spans the close button padding row and
  // the close button row.
  // TODO(dharcourt): Skip the title Label when there's no title text.
  views::Label* title = new views::Label(notification_.title);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetElideBehavior(views::Label::ELIDE_AT_END);
  title->SetFont(title->font().DeriveFont(4));
  title->SetEnabledColor(kTitleColor);
  title->SetBackgroundColor(kTitleBackgroundColor);
  title->set_border(MakePadding(kTextTopPadding, 0, 3, kTextToClosePadding));
  layout->AddView(title, 1, 2,
                  views::GridLayout::LEADING, views::GridLayout::LEADING);

  // First row: Close button padding.
  views::View* padding = new views::ImageView();
  padding->set_border(MakePadding(kCloseTopPadding, 1, 0, 0));
  layout->AddView(padding);

  // Second row: Close button, which has to be on a row of its own because its
  // top padding can't be set using empty borders (ImageButtons don't support
  // borders). The resize factor of this row (1) is higher than that of the
  // first rows (0) to ensure the first row's height stays at kCloseTopPadding.
  layout->StartRow(1, 0);
  layout->SkipColumns(2);
  DCHECK(close_button_);
  layout->AddView(close_button_);

  // One row for the message if appropriate. The resize factor of this row (2)
  // is higher than that of preceding rows (0 and 1) to ensure the content of
  // the notification is top-aligned.
  if (notification_.items.size() == 0) {
    layout->StartRow(2, 0);
    layout->SkipColumns(1);
    views::Label* message = new views::Label(notification_.message);
    message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    message->SetElideBehavior(views::Label::ELIDE_AT_END);
    message->SetEnabledColor(kMessageColor);
    message->SetBackgroundColor(kMessageBackgroundColor);
    message->set_border(MakePadding(0, 0, 3, kTextToClosePadding));
    layout->AddView(message, 1, 1,
                    views::GridLayout::LEADING, views::GridLayout::LEADING);
    layout->SkipColumns(1);
  }

  // One row for each notification item, including appropriate padding. The
  // resize factor of the last row of items (3) is higher than that of all
  // preceding rows (0, 1, and 2) to ensure the content of the notification is
  // top-aligned.
  for (int i = 0, n = displayed_item_count; i < n; ++i) {
    int bottom_padding = (i < n - 1) ? 4 : (kTextBottomPadding - 2);
    int resize_factor =  (i < n - 1) ? 2 : 3;
    layout->StartRow(resize_factor, 0);
    layout->SkipColumns(1);
    ItemView* item = new ItemView(notification_.items[i]);
    item->set_border(MakePadding(0, 0, bottom_padding, kTextToClosePadding));
    layout->AddView(item);
    layout->SkipColumns(1);
  }

  // One row for the image.
  layout->StartRow(0, 0);
  views::ImageView* image = new ProportionalImageView();
  image->SetImageSize(notification_.image.size());
  image->SetImage(notification_.image);
  image->SetHorizontalAlignment(views::ImageView::CENTER);
  image->SetVerticalAlignment(views::ImageView::LEADING);
  image->set_border(MakePadding(kImageTopPadding, kImageLeftPadding,
                                kImageBottomPadding, kImageRightPadding));
  layout->AddView(image, 3, 1,
                  views::GridLayout::FILL, views::GridLayout::LEADING);
}

}  // namespace message_center
