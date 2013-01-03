// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_view_multiple.h"

#include "base/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/size.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"

namespace {

// Notification dimensions.
const int kNotificationPrimaryIconSize = 64;
const int kNotificationSecondaryIconSize = 15;
const int kNotificationPadding1Width = 12;
const int kNotificationColumn1Width = kNotificationPrimaryIconSize;
const int kNotificationPadding2Width = 12;
const int kNotificationPadding3Width = 12;
const int kNotificationColumn3Width = 26;
const int kNotificationPadding4Width = 10;
const int kNotificationColumn4Width = 8;
const int kNotificationPadding5Width = 8;
const int kNotificationColumn1Top = 12;
const int kNotificationColumn2Top = 9;
const int kNotificationColumn3Top = 4;
const int kNotificationColumn4Top = 8;
const int kNotificationPaddingBottom = 19;
const int kNotificationItemInternalPadding = 12;

// The text background colors below are used only to prevent view::Label from
// modifying the text color and will never be used for drawing. See
// view::Label's SetEnabledColor() and SetBackgroundColor() for details.
const SkColor kNotificationBackgroundColor = SkColorSetRGB(254, 254, 254);
const SkColor kNotificationReadBackgroundColor = SkColorSetRGB(250, 250, 250);
const SkColor kNotificationTitleColor = SkColorSetRGB(68, 68, 68);
const SkColor kNotificationTitleBackgroundColor = SK_ColorWHITE;
const SkColor kNotificationMessageColor = SkColorSetRGB(136, 136, 136);
const SkColor kNotificationMessageBackgroundColor = SK_ColorBLACK;
const SkColor kNotificationTimeColor = SkColorSetRGB(176, 176, 176);
const SkColor kNotificationTimeBackgroundColor = SK_ColorBLACK;
const SkColor kNotificationItemTitleColor = SkColorSetRGB(68, 68, 68);
const SkColor kNotificationItemMessageColor = SkColorSetRGB(136, 136, 136);
const SkColor kNotificationSeparatorColor = SkColorSetRGB(226, 226, 226);

// ItemViews are responsible for drawing each MessageViewMultiple item's title
// and message next to each other within a single column.
class ItemView : public views::View {
 public:
  ItemView(const message_center::NotificationList::NotificationItem& item)
    : item_(item), preferred_size_(0, 0) {
    gfx::Font font = GetDefaultFont();
    preferred_size_.Enlarge(font.GetStringWidth(item.title), 0);
    preferred_size_.Enlarge(kNotificationItemInternalPadding, 0);
    preferred_size_.Enlarge(font.GetStringWidth(item.message), 0);
    preferred_size_.set_height(font.GetHeight());
    PreferredSizeChanged();
    SchedulePaint();
  }

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual int GetBaseline() const OVERRIDE;
  virtual bool HitTestRect(const gfx::Rect& rect) const OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual bool GetTooltipText(const gfx::Point& point,
                              string16* tooltip) const OVERRIDE;

 protected:
  // Overridden from views::View.
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  static gfx::Font GetDefaultFont();

  message_center::NotificationList::NotificationItem item_;
  gfx::Size preferred_size_;

  DISALLOW_COPY_AND_ASSIGN(ItemView);
};

gfx::Size ItemView::GetPreferredSize() {
  return preferred_size_;
}

int ItemView::GetBaseline() const {
  return GetDefaultFont().GetBaseline();
}

bool ItemView::HitTestRect(const gfx::Rect& rect) const {
  return false;
}

void ItemView::GetAccessibleState(
    ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_STATICTEXT;
  state->state = ui::AccessibilityTypes::STATE_READONLY;
  state->name = item_.message;  // TODO(dharcourt): Include title localizably.
}

bool ItemView::GetTooltipText(const gfx::Point& point,
                              string16* tooltip) const {
  if (preferred_size_.width() > width()) {
    *tooltip = item_.message;  // TODO(dharcourt): Include title localizably.
    return true;
  }
  return false;
}

void ItemView::OnPaint(gfx::Canvas* canvas) {
  OnPaintBackground(canvas);
  OnPaintBorder(canvas);
  OnPaintFocusBorder(canvas);

  gfx::Font font = GetDefaultFont();
  int y = std::max(0, height() - preferred_size_.height()) / 2;
  canvas->DrawStringInt(item_.title, font, kNotificationItemTitleColor,
                        0, y, width(), preferred_size_.height());

  int x = font.GetStringWidth(item_.title) + kNotificationItemInternalPadding;
  if (x < width()) {
    canvas->DrawStringInt(
        ui::ElideText(item_.message, font, width() - x, ui::ELIDE_AT_END),
        font, kNotificationItemMessageColor,
        x, y, width() - x, preferred_size_.height());
  }
}

// static
gfx::Font ItemView::GetDefaultFont() {
  return ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont);
}

// BoxView draws a color rectangle or just reserves some space.
class BoxView : public views::View {
 public:
  BoxView(int width, int height, SkColor color=SkColorSetARGB(0, 0, 0, 0))
      : size_(width, height) {
    if (SkColorGetA(color) > 0)
      set_background(views::Background::CreateSolidBackground(color));
    PreferredSizeChanged();
    SchedulePaint();
  }

  // Overridden from View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual bool HitTestRect(const gfx::Rect& rect) const OVERRIDE;

 private:
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(BoxView);
};

gfx::Size BoxView::GetPreferredSize() {
  return size_;
}

bool BoxView::HitTestRect(const gfx::Rect& rect) const {
  return false;
}

}  // namespace

namespace message_center {

MessageViewMultiple::MessageViewMultiple(
    NotificationList::Delegate* list_delegate,
    const NotificationList::Notification& notification)
    : MessageView(list_delegate, notification) {}

MessageViewMultiple::MessageViewMultiple() {}

MessageViewMultiple::~MessageViewMultiple() {}

// TODO(dharcourt): Make this a subclass of BaseFormatView or of a
// BaseFormatView superclass and leverage that class' SetUpView().
void MessageViewMultiple::SetUpView() {

  SkColor background = notification_.is_read ?
      kNotificationReadBackgroundColor : kNotificationBackgroundColor;
  set_background(views::Background::CreateSolidBackground(background));

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  // Four columns (icon, messages, time, close) surrounded by padding. The icon,
  // time, and close columns have fixed width and the message column fills up
  // the remaining space. Inter-column padding is included within columns
  // whenever horizontal allignment allows for it.
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddPaddingColumn(0, kNotificationPadding1Width);
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                     0, views::GridLayout::FIXED,
                     kNotificationColumn1Width, kNotificationColumn1Width);
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                     0, views::GridLayout::FIXED,
                     kNotificationPadding2Width, kNotificationPadding2Width);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                     100, views::GridLayout::USE_PREF,
                     0, 0);
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::LEADING,
                     0, views::GridLayout::FIXED,
                     kNotificationPadding3Width + kNotificationColumn3Width,
                     kNotificationPadding3Width + kNotificationColumn3Width);
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::LEADING,
                     0, views::GridLayout::FIXED,
                     kNotificationPadding4Width + kNotificationColumn4Width,
                     kNotificationPadding4Width + kNotificationColumn4Width);
  columns->AddPaddingColumn(0, kNotificationPadding5Width);

  // First row: Primary icon.
  layout->StartRow(0, 0);
  views::ImageView* primary_icon = new views::ImageView;
  primary_icon->SetImageSize(gfx::Size(kNotificationPrimaryIconSize,
                                       kNotificationPrimaryIconSize));
  primary_icon->SetImage(notification_.primary_icon);
  primary_icon->set_border(CreateTopBorder(kNotificationColumn1Top));
  primary_icon->SetVerticalAlignment(views::ImageView::LEADING);
  layout->AddView(primary_icon, 1, 3 + 2 * notification_.items.size());

  // First row: Title.
  // TODO(dharcourt): Skip the title Label when there's no title text.
  layout->SkipColumns(1);
  views::Label* title = new views::Label(notification_.title);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetFont(title->font().DeriveFont(4));
  title->SetEnabledColor(kNotificationTitleColor);
  title->SetBackgroundColor(kNotificationTitleBackgroundColor);
  title->set_border(CreateTopBorder(kNotificationColumn2Top));
  layout->AddView(title, 1, 2);

  // First row: Time.
  // TODO(dharcourt): Timestamp as "1m/5m/1h/5h/1d/5d/..." (ago).
  views::Label* time = new views::Label(UTF8ToUTF16(""));
  time->SetEnabledColor(kNotificationTimeColor);
  time->SetBackgroundColor(kNotificationTimeBackgroundColor);
  time->set_border(CreateTopBorder(kNotificationColumn3Top));
  layout->AddView(time, 1, 2);

  // First row: Close button padding.
  layout->AddView(new BoxView(1, kNotificationColumn4Top));

  // Second row: Close button, which has to be on a row of its own because it's
  // an ImageButton and so its padding can't be set using borders. This row is
  // set to resize vertically to ensure the first row stays at its minimum
  // height of kNotificationColumn4Top.
  layout->StartRow(100, 0);
  layout->SkipColumns(4);
  DCHECK(close_button_);
  layout->AddView(close_button_);

  // Rows for each notification item, including appropriate padding.
  layout->AddPaddingRow(0, 3);
  std::vector<NotificationList::NotificationItem>::const_iterator i;
  for (i = notification_.items.begin(); i != notification_.items.end(); ++i) {
    layout->StartRowWithPadding(0, 0, 0, 3);
    layout->SkipColumns(2);
    layout->AddView(new ItemView(*i));
    layout->SkipColumns(2);
  }

  // Rows for horizontal separator line with appropriate padding.
  layout->StartRowWithPadding(0, 0, 0, 6);
  layout->SkipColumns(1);
  layout->AddView(new BoxView(1000000, 1, kNotificationSeparatorColor), 4, 1);

  // Last row: Summary message with padding.
  // TODO(dharcourt): Skip the message Label when there's no message text.
  layout->StartRowWithPadding(0, 0, 0, 5);
  layout->SkipColumns(2);
  views::Label* message = new views::Label(notification_.message);
  message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message->SetMultiLine(true);
  message->SetEnabledColor(kNotificationMessageColor);
  message->SetBackgroundColor(kNotificationMessageBackgroundColor);
  message->set_border(CreateTopBorder(1));
  layout->AddView(message);

  // Last row: Secondary icon.
  layout->SkipColumns(1);
  views::ImageView* secondary_icon = new views::ImageView;
  secondary_icon->SetImageSize(gfx::Size(kNotificationSecondaryIconSize,
                                         kNotificationSecondaryIconSize));
  secondary_icon->SetImage(notification_.secondary_icon);
  secondary_icon->SetVerticalAlignment(views::ImageView::LEADING);
  layout->AddView(secondary_icon);

  // Final row with the bottom padding.
  layout->AddPaddingRow(0, kNotificationPaddingBottom);
}

views::Border* MessageViewMultiple::CreateTopBorder(int height) {
  return views::Border::CreateEmptyBorder(height, 0, 0, 0);
}

}  // namespace message_center
