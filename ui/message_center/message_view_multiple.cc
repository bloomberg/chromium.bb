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

const int kNotificationItemInternalPaddingWidth = 12;
const SkColor kNotificationItemTextColor = SK_ColorBLACK;

// MessageViewMultiple's item view, which is responsible for drawing each item's
// title and message next to each other within a single column.
class ItemView : public views::View {
 public:
  ItemView(const message_center::NotificationList::NotificationItem& item)
    : item_(item), preferred_size_(0, 0) {
    gfx::Font font = GetDefaultFont();
    gfx::Font bold = font.DeriveFont(0, gfx::Font::BOLD);
    preferred_size_.Enlarge(bold.GetStringWidth(item.title), 0);
    preferred_size_.Enlarge(kNotificationItemInternalPaddingWidth, 0);
    preferred_size_.Enlarge(font.GetStringWidth(item.message), 0);
    preferred_size_.set_height(std::max(bold.GetHeight(), font.GetHeight()));
    PreferredSizeChanged();
    SchedulePaint();
  }

  // Overridden from View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual int GetBaseline() const OVERRIDE;
  virtual bool HitTestRect(const gfx::Rect& rect) const OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual bool GetTooltipText(const gfx::Point& point,
                              string16* tooltip) const OVERRIDE;

 protected:
  // Overridden from View:
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

  SkColor color = kNotificationItemTextColor;
  gfx::Font font = GetDefaultFont();
  gfx::Font bold = font.DeriveFont(0, gfx::Font::BOLD);

  int x = 0;
  int y = std::max(0, size().height() - preferred_size_.height()) / 2;
  int width = size().width();
  int height = preferred_size_.height();
  string16 text = ui::ElideText(item_.title, bold, width, ui::ELIDE_AT_END);
  canvas->DrawStringInt(text, bold, color, x, y, width, height);

  x = bold.GetStringWidth(text) + kNotificationItemInternalPaddingWidth;
  width = width - x;
  if (width > 0) {
    text = ui::ElideText(item_.message, font, width, ui::ELIDE_AT_END);
    canvas->DrawStringInt(text, font, color, x, y, width, height);
  }
}

// static
gfx::Font ItemView::GetDefaultFont() {
  return ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont);
}

}  // namespace

namespace message_center {

// Notification dimensions.
const int kNotificationPadding1Width = 12;
const int kNotificationColumn1Width = 64;
const int kNotificationPadding2Width = 12;
const int kNotificationPadding3Width = 12;
const int kNotificationColumn3Width = 47;
const int kNotificationPadding4Width = 10;
const int kNotificationColumn4Width = 10;
const int kNotificationPadding5Width = 8;
const int kNotificationPaddingTop = 12;
const int kNotificationPaddingBottom = 12;

const SkColor kNotificationColor = SkColorSetRGB(254, 254, 254);
const SkColor kNotificationReadColor = SkColorSetRGB(250, 250, 250);
const SkColor kSeparatorColor = SkColorSetRGB(219, 0, 0);

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
                       kNotificationReadColor :
                       kNotificationColor;
  set_background(views::Background::CreateSolidBackground(background));

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  // Four columns (icon, messages, time, close) surrounded by padding.
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddPaddingColumn(0, kNotificationPadding1Width);
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                     0, views::GridLayout::FIXED,
                     kNotificationColumn1Width, kNotificationColumn1Width);
  columns->AddPaddingColumn(0, kNotificationPadding2Width);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                     100, views::GridLayout::USE_PREF,
                     0, 0);
  columns->AddPaddingColumn(0, kNotificationPadding3Width);
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                     0, views::GridLayout::FIXED,
                     kNotificationColumn3Width, kNotificationColumn3Width);
  columns->AddPaddingColumn(0, kNotificationPadding4Width);
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                     0, views::GridLayout::FIXED,
                     kNotificationColumn4Width, kNotificationColumn4Width);
  columns->AddPaddingColumn(0, kNotificationPadding5Width);

  // First row has the icon, title, and close button after some top padding.
  layout->AddPaddingRow(0, kNotificationPaddingTop);
  layout->StartRow(0, 0);

  views::ImageView* icon = new views::ImageView;
  icon->SetImageSize(gfx::Size(kNotificationColumn1Width,
                               kNotificationColumn1Width));
  icon->SetImage(notification_.image);
  layout->AddView(icon, 1, 1 + notification_.items.size());

  views::Label* title = new views::Label(notification_.title);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetFont(title->font().DeriveFont(0, gfx::Font::BOLD));
  layout->AddView(title);

  // TODO(dharcourt): Timestamp as "1m/5m/1h/5h/1d/5d/..." (ago).
  views::Label* time = new views::Label();
  layout->AddView(time);

  DCHECK(close_button_);
  layout->AddView(close_button_);

  // One row for each notification item.
  std::vector<NotificationList::NotificationItem>::const_iterator i;
  for (i = notification_.items.begin(); i != notification_.items.end(); ++i) {
    layout->StartRow(0, 0);
    layout->SkipColumns(2);
    layout->AddView(new ItemView(*i));
  }

  // TODO(dharcourt): One row with a horizontal separator line.

  // One row for the summary message. TODO(dharcourt): Make it optional.
  views::Label* message = new views::Label(notification_.message);
  message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message->SetMultiLine(true);
  // TODO(dharcourt): Accomplish message->SizeToFit(message_width) without a
  // constant like "message_width".
  layout->StartRow(0, 0);
  layout->SkipColumns(2);
  layout->AddView(message);

  // TODO(dharcourt): Add a second icon to the right of the summary message.

  // Bottom padding.
  layout->AddPaddingRow(0, kNotificationPaddingBottom);
}

}  // namespace message_center
