// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_center_view.h"

#include <map>

#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_constants.h"
#include "ui/message_center/message_center_util.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/kennedy_scroll_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"


namespace message_center {

namespace {

const int kMinHeight = 80;
const int kFooterMargin = 16;
const int kFooterHeight = 24;
const SkColor kMessageCenterBackgroundColor = SkColorSetRGB(0xe5, 0xe5, 0xe5);
const SkColor kBorderDarkColor = SkColorSetRGB(0xaa, 0xaa, 0xaa);
const SkColor kTransparentColor = SkColorSetARGB(0, 0, 0, 0);
const SkColor kFooterDelimiterColor = SkColorSetRGB(0xcc, 0xcc, 0xcc);
const SkColor kFooterTextColor = SkColorSetRGB(0x80, 0x80, 0x80);
const SkColor kButtonTextHighlightColor = SkColorSetRGB(0x32, 0x32, 0x32);
const SkColor kButtonTextHoverColor = SkColorSetRGB(0x32, 0x32, 0x32);
// The focus color and focus-border logic is copied from ash tray.
// TODO(mukai): unite those implementations.
const SkColor kFocusBorderColor = SkColorSetRGB(0x40, 0x80, 0xfa);

// PoorMessageCenterButtonBar //////////////////////////////////////////////////

// The view for the buttons at the bottom of the message center window used
// when kEnableRichNotifications is false (hence the "poor" in the name :-).
class PoorMessageCenterButtonBar : public MessageCenterButtonBar,
                                   public views::ButtonListener {
 public:
  explicit PoorMessageCenterButtonBar(MessageCenter* message_center);

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PoorMessageCenterButtonBar);
};

PoorMessageCenterButtonBar::PoorMessageCenterButtonBar(
    MessageCenter* message_center)
    : MessageCenterButtonBar(message_center) {
  set_background(views::Background::CreateBackgroundPainter(
      true,
      views::Painter::CreateVerticalGradient(kBackgroundLightColor,
                                             kBackgroundDarkColor)));
  set_border(views::Border::CreateSolidSidedBorder(
      2, 0, 0, 0, kBorderDarkColor));

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddPaddingColumn(100, 0);
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                     0, /* resize percent */
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, 4);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  views::LabelButton* close_all_button = new views::LabelButton(
      this, rb.GetLocalizedString(IDS_MESSAGE_CENTER_CLEAR_ALL));
  close_all_button->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  close_all_button->set_request_focus_on_press(false);

  layout->AddPaddingRow(0, 4);
  layout->StartRow(0, 0);
  layout->AddView(close_all_button);
  set_close_all_button(close_all_button);
}

void PoorMessageCenterButtonBar::ButtonPressed(views::Button* sender,
                                               const ui::Event& event) {
  if (sender == close_all_button())
    message_center()->RemoveAllNotifications(true);  // Action by user.
}

// NotificationCenterButton ////////////////////////////////////////////////////

class NotificationCenterButton : public views::TextButton {
 public:
  NotificationCenterButton(views::ButtonListener* listener,
                           const string16& text);

 protected:
  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnPaintBorder(gfx::Canvas* canvas);
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas);

  DISALLOW_COPY_AND_ASSIGN(NotificationCenterButton);
};

NotificationCenterButton::NotificationCenterButton(
    views::ButtonListener* listener,
    const string16& text)
    : views::TextButton(listener, text) {
  set_border(views::Border::CreateEmptyBorder(0, 16, 0, 16));
  set_min_height(kFooterHeight);
  SetEnabledColor(kFooterTextColor);
  SetHighlightColor(kButtonTextHighlightColor);
  SetHoverColor(kButtonTextHoverColor);
}

gfx::Size NotificationCenterButton::GetPreferredSize() {
  // Returns an empty size when invisible, to trim its space in the parent's
  // GridLayout.
  if (!visible())
    return gfx::Size();
  return views::TextButton::GetPreferredSize();
}

void NotificationCenterButton::OnPaintBorder(gfx::Canvas* canvas) {
  // Just paint the left border.
  canvas->DrawLine(gfx::Point(0, 0), gfx::Point(0, height()),
                   kFooterDelimiterColor);
}

void NotificationCenterButton::OnPaintFocusBorder(gfx::Canvas* canvas) {
  if (HasFocus() && (focusable() || IsAccessibilityFocusable())) {
    canvas->DrawRect(gfx::Rect(2, 1, width() - 4, height() - 3),
                     kFocusBorderColor);
  }
}

// RichMessageCenterButtonBar //////////////////////////////////////////////////

// TODO(mukai): Merge this into MessageCenterButtonBar and get rid of
// PoorMessageCenterButtonBar when the kEnableRichNotifications flag disappears.
class RichMessageCenterButtonBar : public MessageCenterButtonBar,
                                   public views::ButtonListener {
 public:
  explicit RichMessageCenterButtonBar(MessageCenter* message_center);

 private:
  // Overridden from views::View:
  virtual void ChildVisibilityChanged(views::View* child) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  views::Label* notification_label_;
  views::Button* settings_button_;

  DISALLOW_COPY_AND_ASSIGN(RichMessageCenterButtonBar);
};

RichMessageCenterButtonBar::RichMessageCenterButtonBar(
    MessageCenter* message_center)
  : MessageCenterButtonBar(message_center) {
  set_background(views::Background::CreateSolidBackground(
      kMessageCenterBackgroundColor));
  set_border(views::Border::CreateSolidSidedBorder(
      1, 0, 0, 0, kFooterDelimiterColor));


  notification_label_ = new views::Label(l10n_util::GetStringUTF16(
      IDS_MESSAGE_CENTER_FOOTER_TITLE));
  notification_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  notification_label_->SetElideBehavior(views::Label::ELIDE_AT_END);
  notification_label_->SetEnabledColor(kFooterTextColor);
  AddChildView(notification_label_);
  settings_button_ = new NotificationCenterButton(
      this, l10n_util::GetStringUTF16(
          IDS_MESSAGE_CENTER_SETTINGS_BUTTON_LABEL));
  settings_button_->set_focusable(true);
  AddChildView(settings_button_);
  NotificationCenterButton* close_all_button = new NotificationCenterButton(
      this, l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_CLEAR_ALL));
  close_all_button->set_focusable(true);
  AddChildView(close_all_button);

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  layout->SetInsets(
      kMarginBetweenItems, kFooterMargin, kMarginBetweenItems, 0);
  views::ColumnSet* column = layout->AddColumnSet(0);
  column->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                    1.0f, views::GridLayout::USE_PREF, 0, 0);
  column->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                    0, views::GridLayout::FIXED,
                    settings_button_->GetPreferredSize().width(), 0);
  column->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                    0, views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  layout->AddView(notification_label_);
  layout->AddView(settings_button_);
  layout->AddView(close_all_button);
  set_close_all_button(close_all_button);
}

// Overridden from views::View:
void RichMessageCenterButtonBar::ChildVisibilityChanged(views::View* child) {
  InvalidateLayout();
}

// Overridden from views::ButtonListener:
void RichMessageCenterButtonBar::ButtonPressed(views::Button* sender,
                                               const ui::Event& event) {
  if (sender == close_all_button())
    message_center()->RemoveAllNotifications(true);  // Action by user.
  else if (sender == settings_button_)
    message_center()->ShowNotificationSettingsDialog(
        GetWidget()->GetNativeView());
  else
    NOTREACHED();
}

// BoundedScrollView ///////////////////////////////////////////////////////////

// A custom scroll view whose height has a minimum and maximum value and whose
// scroll bar disappears when not needed.
class BoundedScrollView : public views::ScrollView {
 public:
  BoundedScrollView(int min_height, int max_height);

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual int GetHeightForWidth(int width) OVERRIDE;
  virtual void Layout() OVERRIDE;

 private:
  int min_height_;
  int max_height_;

  DISALLOW_COPY_AND_ASSIGN(BoundedScrollView);
};

BoundedScrollView::BoundedScrollView(int min_height, int max_height)
    : min_height_(min_height),
      max_height_(max_height) {
  set_notify_enter_exit_on_child(true);
  // Cancels the default dashed focus border.
  set_focus_border(NULL);
  if (IsRichNotificationEnabled()) {
    set_background(views::Background::CreateSolidBackground(
        kMessageCenterBackgroundColor));
    SetVerticalScrollBar(new views::KennedyScrollBar(false));
  }
}

gfx::Size BoundedScrollView::GetPreferredSize() {
  gfx::Size size = contents()->GetPreferredSize();
  size.ClampToMin(gfx::Size(size.width(), min_height_));
  size.ClampToMax(gfx::Size(size.width(), max_height_));
  gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

int BoundedScrollView::GetHeightForWidth(int width) {
  gfx::Insets insets = GetInsets();
  width = std::max(0, width - insets.width());
  int height = contents()->GetHeightForWidth(width) + insets.height();
  return std::min(std::max(height, min_height_), max_height_);
}

void BoundedScrollView::Layout() {
  int content_width = width();
  int content_height = contents()->GetHeightForWidth(content_width);
  if (content_height > height()) {
    content_width = std::max(content_width - GetScrollBarWidth(), 0);
    content_height = contents()->GetHeightForWidth(content_width);
  }
  contents()->SetBounds(0, 0, content_width, content_height);

  views::ScrollView::Layout();
}

// MessageListView /////////////////////////////////////////////////////////////

// Displays a list of messages.
class MessageListView : public views::View {
 public:
  MessageListView();

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageListView);
};

MessageListView::MessageListView() {
  if (IsRichNotificationEnabled()) {
    // Set the margin to 0 for the layout. BoxLayout assumes the same margin
    // for top and bottom, but the bottom margin here should be smaller
    // because of the shadow of message view. Use an empty border instead
    // to provide this margin.
    gfx::Insets shadow_insets = MessageView::GetShadowInsets();
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical,
                             0,
                             0,
                             kMarginBetweenItems - shadow_insets.bottom()));
    set_background(views::Background::CreateSolidBackground(
        kMessageCenterBackgroundColor));
    set_border(views::Border::CreateEmptyBorder(
        kMarginBetweenItems - shadow_insets.top(), /* top */
        kMarginBetweenItems - shadow_insets.left(), /* left */
        kMarginBetweenItems - shadow_insets.bottom(),  /* bottom */
        kMarginBetweenItems - shadow_insets.right() /* right */ ));
  } else {
    views::BoxLayout* layout =
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1);
    layout->set_spread_blank_space(true);
    SetLayoutManager(layout);
  }
}

}  // namespace

// MessageCenterButtonBar //////////////////////////////////////////////////////

MessageCenterButtonBar::MessageCenterButtonBar(MessageCenter* message_center)
    : message_center_(message_center),
      close_all_button_(NULL) {
}

MessageCenterButtonBar::~MessageCenterButtonBar() {
}

void MessageCenterButtonBar::SetCloseAllVisible(bool visible) {
  if (close_all_button_)
    close_all_button_->SetVisible(visible);
}

// MessageCenterView ///////////////////////////////////////////////////////////

MessageCenterView::MessageCenterView(MessageCenter* message_center,
                                     int max_height)
    : message_center_(message_center) {
  int between_child = IsRichNotificationEnabled() ? 0 : 1;
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, between_child));


  if (IsRichNotificationEnabled())
    button_bar_ = new RichMessageCenterButtonBar(message_center);
  else
    button_bar_ = new PoorMessageCenterButtonBar(message_center);

  const int button_height = button_bar_->GetPreferredSize().height();
  scroller_ = new BoundedScrollView(kMinHeight - button_height,
                                    max_height - button_height);

  if (get_use_acceleration_when_possible()) {
    scroller_->SetPaintToLayer(true);
    scroller_->SetFillsBoundsOpaquely(false);
    scroller_->layer()->SetMasksToBounds(true);
  }

  message_list_view_ = new MessageListView();
  scroller_->SetContents(message_list_view_);

  AddChildView(scroller_);
  AddChildView(button_bar_);
}

MessageCenterView::~MessageCenterView() {
}

void MessageCenterView::SetNotifications(
    const NotificationList::Notifications& notifications)  {
  RemoveAllNotifications();
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    AddNotification(*(*iter));
    if (message_views_.size() >=
        NotificationList::kMaxVisibleMessageCenterNotifications) {
      break;
    }
  }
  if (message_views_.empty()) {
    views::Label* label = new views::Label(l10n_util::GetStringUTF16(
        IDS_MESSAGE_CENTER_NO_MESSAGES));
    label->SetFont(label->font().DeriveFont(1));
    label->SetEnabledColor(SK_ColorGRAY);
    // Set transparent background to ensure that subpixel rendering
    // is disabled. See crbug.com/169056
    label->SetBackgroundColor(kTransparentColor);
    message_list_view_->AddChildView(label);
    button_bar_->SetCloseAllVisible(false);
    scroller_->set_focusable(false);
  } else {
    button_bar_->SetCloseAllVisible(true);
    scroller_->set_focusable(true);
    scroller_->RequestFocus();
  }
  Layout();
}

size_t MessageCenterView::NumMessageViewsForTest() const {
  return message_list_view_->child_count();
}

void MessageCenterView::Layout() {
  scroller_->SetBounds(0, 0, width(), scroller_->GetHeightForWidth(width()));
  views::View::Layout();
  if (GetWidget())
    GetWidget()->GetRootView()->SchedulePaint();
}

bool MessageCenterView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  // Do not rely on the default scroll event handler of ScrollView because
  // the scroll happens only when the focus is on the ScrollView. The
  // notification center will allow the scrolling even when the focus is on
  // the buttons.
  if (scroller_->bounds().Contains(event.location()))
    return scroller_->OnMouseWheel(event);
  return views::View::OnMouseWheel(event);
}

void MessageCenterView::RemoveAllNotifications() {
  message_views_.clear();
  message_list_view_->RemoveAllChildViews(true);
  scroller_->InvalidateLayout();
}

void MessageCenterView::AddNotification(const Notification& notification) {
  // NotificationViews are expanded by default here until
  // http://crbug.com/217902 is fixed. TODO(dharcourt): Fix.
  MessageView* view = NotificationView::Create(
      notification, message_center_, true);
  view->set_scroller(scroller_);
  message_views_[notification.id()] = view;
  message_list_view_->AddChildView(view);
}

}  // namespace message_center
