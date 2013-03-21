// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_center_bubble.h"

#include <map>

#include "grit/ui_strings.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/message_center/message_center_constants.h"
#include "ui/message_center/message_center_util.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/kennedy_scroll_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace message_center {

namespace {

const int kMessageBubbleBaseMinHeight = 80;
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

// WebNotificationButtonViewBase ///////////////////////////////////////////////

class WebNotificationButtonViewBase : public views::View {
 public:
  explicit WebNotificationButtonViewBase(NotificationChangeObserver* observer);

  void SetCloseAllVisible(bool visible);
  void set_close_all_button(views::Button* button) {
    close_all_button_ = button;
  }

 protected:
  NotificationChangeObserver* observer() { return observer_; }
  views::Button* close_all_button() { return close_all_button_; }

 private:
  NotificationChangeObserver* observer_;  // Weak reference.
  views::Button* close_all_button_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationButtonViewBase);
};

WebNotificationButtonViewBase::WebNotificationButtonViewBase(
    NotificationChangeObserver* observer)
    : observer_(observer),
      close_all_button_(NULL) {
}

void WebNotificationButtonViewBase::SetCloseAllVisible(bool visible) {
  if (close_all_button_)
    close_all_button_->SetVisible(visible);
}

// WebNotificationButtonView ///////////////////////////////////////////////////

// The view for the buttons at the bottom of the web notification tray.
class WebNotificationButtonView : public WebNotificationButtonViewBase,
                                  public views::ButtonListener {
 public:
  explicit WebNotificationButtonView(NotificationChangeObserver* observer);

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebNotificationButtonView);
};

WebNotificationButtonView::WebNotificationButtonView(
    NotificationChangeObserver* observer)
    : WebNotificationButtonViewBase(observer) {
  set_background(views::Background::CreateBackgroundPainter(
      true,
      views::Painter::CreateVerticalGradient(
          MessageBubbleBase::kHeaderBackgroundColorLight,
          MessageBubbleBase::kHeaderBackgroundColorDark)));
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
  views::TextButton* close_all_button = new views::TextButton(
      this, rb.GetLocalizedString(IDS_MESSAGE_CENTER_CLEAR_ALL));
  close_all_button->set_alignment(views::TextButton::ALIGN_CENTER);
  close_all_button->set_request_focus_on_press(false);

  layout->AddPaddingRow(0, 4);
  layout->StartRow(0, 0);
  layout->AddView(close_all_button);
  set_close_all_button(close_all_button);
}

void WebNotificationButtonView::ButtonPressed(views::Button* sender,
                                              const ui::Event& event) {
  if (sender == close_all_button())
    observer()->OnRemoveAllNotifications(true);  // Action by user.
}

// WebNotificationButton ///////////////////////////////////////////////////////

class WebNotificationButton : public views::TextButton {
 public:
  WebNotificationButton(views::ButtonListener* listener, const string16& text);

 protected:
  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnPaintBorder(gfx::Canvas* canvas);
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas);

  DISALLOW_COPY_AND_ASSIGN(WebNotificationButton);
};

WebNotificationButton::WebNotificationButton(views::ButtonListener* listener,
                                             const string16& text)
    : views::TextButton(listener, text) {
  set_border(views::Border::CreateEmptyBorder(0, 16, 0, 16));
  set_min_height(kFooterHeight);
  SetEnabledColor(kFooterTextColor);
  SetHighlightColor(kButtonTextHighlightColor);
  SetHoverColor(kButtonTextHoverColor);
}

gfx::Size WebNotificationButton::GetPreferredSize() {
  // Returns an empty size when invisible, to trim its space in the parent's
  // GridLayout.
  if (!visible())
    return gfx::Size();
  return views::TextButton::GetPreferredSize();
}

void WebNotificationButton::OnPaintBorder(gfx::Canvas* canvas) {
  // Just paint the left border.
  canvas->DrawLine(gfx::Point(0, 0), gfx::Point(0, height()),
                   kFooterDelimiterColor);
}

void WebNotificationButton::OnPaintFocusBorder(gfx::Canvas* canvas) {
  if (HasFocus() && (focusable() || IsAccessibilityFocusable())) {
    canvas->DrawRect(gfx::Rect(2, 1, width() - 4, height() - 3),
                     kFocusBorderColor);
  }
}

// WebNotificationButtonView2 //////////////////////////////////////////////////

// TODO(mukai): remove the trailing '2' when the kEnableNewMessageCenterBubble
// flag disappears.
class WebNotificationButtonView2 : public WebNotificationButtonViewBase,
                                   public views::ButtonListener {
 public:
  explicit WebNotificationButtonView2(NotificationChangeObserver* observer);

 private:
  // Overridden from views::View:
  virtual void ChildVisibilityChanged(views::View* child) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  views::Label* notification_label_;
  views::Button* settings_button_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationButtonView2);
};

WebNotificationButtonView2::WebNotificationButtonView2(
    NotificationChangeObserver* observer)
  : WebNotificationButtonViewBase(observer) {
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
  settings_button_ = new WebNotificationButton(
      this, l10n_util::GetStringUTF16(
          IDS_MESSAGE_CENTER_SETTINGS_BUTTON_LABEL));
  settings_button_->set_focusable(true);
  AddChildView(settings_button_);
  WebNotificationButton* close_all_button = new WebNotificationButton(
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
void WebNotificationButtonView2::ChildVisibilityChanged(views::View* child) {
  InvalidateLayout();
}

// Overridden from views::ButtonListener:
void WebNotificationButtonView2::ButtonPressed(views::Button* sender,
                                               const ui::Event& event) {
  if (sender == close_all_button())
    observer()->OnRemoveAllNotifications(true);  // Action by user.
  else if (sender == settings_button_)
    observer()->OnShowNotificationSettingsDialog(
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
  virtual void Layout() OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds);

 private:
  int min_height_;
  int max_height_;

  DISALLOW_COPY_AND_ASSIGN(BoundedScrollView);
};

BoundedScrollView::BoundedScrollView(int min_height, int max_height)
    : min_height_(min_height),
      max_height_(max_height) {
  set_notify_enter_exit_on_child(true);
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

void BoundedScrollView::Layout() {
  // Lay out the view as if it will have a scroll bar.
  gfx::Rect content_bounds = gfx::Rect(contents()->GetPreferredSize());
  content_bounds.set_width(std::max(0, width() - GetScrollBarWidth()));
  contents()->SetBoundsRect(content_bounds);
  views::ScrollView::Layout();

  // But use the scroll bar space if no scroll bar is needed.
  if (!vertical_scroll_bar()->visible()) {
    content_bounds = contents()->bounds();
    content_bounds.set_width(content_bounds.width() + GetScrollBarWidth());
    contents()->SetBoundsRect(content_bounds);
  }
}

void BoundedScrollView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // Make sure any content resizing takes into account the scroll bar.
  gfx::Rect content_bounds = gfx::Rect(contents()->GetPreferredSize());
  content_bounds.set_width(std::max(0, width() - GetScrollBarWidth()));
  contents()->SetBoundsRect(content_bounds);
}

// MessageListView /////////////////////////////////////////////////////////////

// Displays a list of messages.
class MessageListView : public views::View {
 public:
  MessageListView(views::View* container);

 protected:
  // Overridden from views::View:
  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE;

 private:
  views::View* container_;  // Weak reference.

  DISALLOW_COPY_AND_ASSIGN(MessageListView);
};

MessageListView::MessageListView(views::View* container)
    : container_(container) {
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

void MessageListView::ChildPreferredSizeChanged(View* child) {
  container_->Layout();
}

}  // namespace

// MessageCenterView ///////////////////////////////////////////////////////////

// View that displays the whole message center.
class MessageCenterView : public views::View {
 public:
  MessageCenterView(MessageCenterBubble* bubble,
                    NotificationChangeObserver* observer);

  void FocusContents();
  void UpdateAllNotifications(
      const NotificationList::Notifications& notifications);

  size_t NumMessageViews() const;

 protected:
  // Overridden from views::View:
  virtual void Layout() OVERRIDE;

 private:
  void RemoveAllNotifications();
  void AddNotification(const Notification& notification);

  MessageCenterBubble* bubble_;  // Weak reference.
  NotificationChangeObserver* observer_;  // Weak reference.
  std::map<std::string,MessageView*> message_views_;
  BoundedScrollView* scroller_;
  MessageListView* message_list_view_;
  WebNotificationButtonViewBase* button_view_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterView);
};

MessageCenterView::MessageCenterView(MessageCenterBubble* bubble,
                                     NotificationChangeObserver* observer)
    : bubble_(bubble),
      observer_(observer) {
  int between_child = IsRichNotificationEnabled() ? 0 : 1;
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, between_child));


  if (IsRichNotificationEnabled())
    button_view_ = new WebNotificationButtonView2(observer);
  else
    button_view_ = new WebNotificationButtonView(observer);

  const int button_height = button_view_->GetPreferredSize().height();
  const int min_height = kMessageBubbleBaseMinHeight - button_height;
  const int max_height = bubble_->max_height() - button_height;
  scroller_ = new BoundedScrollView(min_height, max_height);

  if (get_use_acceleration_when_possible()) {
    scroller_->SetPaintToLayer(true);
    scroller_->SetFillsBoundsOpaquely(false);
    scroller_->layer()->SetMasksToBounds(true);
  }

  message_list_view_ = new MessageListView(this);
  scroller_->SetContents(message_list_view_);

  AddChildView(scroller_);
  AddChildView(button_view_);
}

void MessageCenterView::FocusContents() {
}

void MessageCenterView::UpdateAllNotifications(
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
    button_view_->SetCloseAllVisible(false);
    scroller_->set_focusable(false);
  } else {
    button_view_->SetCloseAllVisible(true);
    scroller_->set_focusable(true);
  }
  Layout();
}

size_t MessageCenterView::NumMessageViews() const {
  return message_list_view_->child_count();
}

void MessageCenterView::Layout() {
  // Start with a layout so scroller_ has the right width to calculate its
  // PreferredSize (fixes http://crrev.com/222221), then do another layout after
  // scroller_ is resized to adjust the button_view_ location.
  // TODO(dharcourt) Change how things are done so only one layout is required.
  views::View::Layout();
  scroller_->SizeToPreferredSize();
  views::View::Layout();
  if (GetWidget())
    GetWidget()->GetRootView()->SchedulePaint();
  bubble_->bubble_view()->UpdateBubble();
}

void MessageCenterView::RemoveAllNotifications() {
  message_views_.clear();
  message_list_view_->RemoveAllChildViews(true);
}

void MessageCenterView::AddNotification(const Notification& notification) {
  // NotificationViews are expanded by default here until
  // http://crbug.com/217902 is fixed. TODO(dharcourt): Fix.
  MessageView* view = NotificationView::Create(notification, observer_, true);
  view->set_scroller(scroller_);
  message_views_[notification.id()] = view;
  message_list_view_->AddChildView(view);
}

// MessageCenterBubble /////////////////////////////////////////////////////////

// Message Center Bubble.
MessageCenterBubble::MessageCenterBubble(MessageCenter* message_center)
    : MessageBubbleBase(message_center),
      contents_view_(NULL) {
}

MessageCenterBubble::~MessageCenterBubble() {
}

views::TrayBubbleView::InitParams MessageCenterBubble::GetInitParams(
    views::TrayBubbleView::AnchorAlignment anchor_alignment) {
  views::TrayBubbleView::InitParams init_params =
      GetDefaultInitParams(anchor_alignment);
  if (IsRichNotificationEnabled()) {
    init_params.min_width += kMarginBetweenItems * 2;
    init_params.max_width += kMarginBetweenItems * 2;
  }
  init_params.max_height = max_height();
  init_params.can_activate = true;
  return init_params;
}

void MessageCenterBubble::InitializeContents(
    views::TrayBubbleView* new_bubble_view) {
  set_bubble_view(new_bubble_view);
  contents_view_ = new MessageCenterView(this, message_center());
  bubble_view()->AddChildView(contents_view_);
  // Resize the content of the bubble view to the given bubble size. This is
  // necessary in case of the bubble border forcing a bigger size then the
  // |new_bubble_view| actually wants. See crbug.com/169390.
  bubble_view()->Layout();
  UpdateBubbleView();
  contents_view_->FocusContents();
}

void MessageCenterBubble::OnBubbleViewDestroyed() {
  contents_view_ = NULL;
}

void MessageCenterBubble::UpdateBubbleView() {
  if (!bubble_view())
    return;  // Could get called after view is closed
  const NotificationList::Notifications& notifications =
      message_center()->notification_list()->GetNotifications();
  contents_view_->UpdateAllNotifications(notifications);
  bubble_view()->GetWidget()->Show();
  bubble_view()->UpdateBubble();
}

void MessageCenterBubble::OnMouseEnteredView() {
}

void MessageCenterBubble::OnMouseExitedView() {
}

size_t MessageCenterBubble::NumMessageViewsForTest() const {
  return contents_view_->NumMessageViews();
}

}  // namespace message_center
