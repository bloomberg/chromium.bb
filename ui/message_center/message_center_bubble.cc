// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_bubble.h"

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
#include "ui/message_center/message_view.h"
#include "ui/message_center/notification_view.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
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

class WebNotificationButtonViewBase : public views::View {
 public:
  WebNotificationButtonViewBase(NotificationList::Delegate* list_delegate)
      : list_delegate_(list_delegate),
        close_all_button_(NULL) {}

  void SetCloseAllVisible(bool visible) {
    if (close_all_button_)
      close_all_button_->SetVisible(visible);
  }

  void set_close_all_button(views::Button* button) {
    close_all_button_ = button;
  }

 protected:
  NotificationList::Delegate* list_delegate() { return list_delegate_; }
  views::Button* close_all_button() { return close_all_button_; }

 private:
  NotificationList::Delegate* list_delegate_;
  views::Button* close_all_button_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationButtonViewBase);
};

// The view for the buttons at the bottom of the web notification tray.
class WebNotificationButtonView : public WebNotificationButtonViewBase,
                                  public views::ButtonListener {
 public:
  explicit WebNotificationButtonView(NotificationList::Delegate* list_delegate)
      : WebNotificationButtonViewBase(list_delegate) {
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
    close_all_button->set_focusable(true);
    close_all_button->set_request_focus_on_press(false);

    layout->AddPaddingRow(0, 4);
    layout->StartRow(0, 0);
    layout->AddView(close_all_button);
    set_close_all_button(close_all_button);
  }

  virtual ~WebNotificationButtonView() {}

  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    if (sender == close_all_button())
      list_delegate()->SendRemoveAllNotifications(true);  // Action by user.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebNotificationButtonView);
};

class WebNotificationButton : public views::TextButton {
 public:
  WebNotificationButton(views::ButtonListener* listener, const string16& text)
      : views::TextButton(listener, text) {
    set_border(views::Border::CreateEmptyBorder(0, 16, 0, 16));
    set_min_height(kFooterHeight);
    SetEnabledColor(kFooterTextColor);
    SetHighlightColor(kButtonTextHighlightColor);
    SetHoverColor(kButtonTextHoverColor);
  }

 protected:
  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    // Returns an empty size when invisible, to trim its space in the parent's
    // GridLayout.
    if (!visible())
      return gfx::Size();
    return views::TextButton::GetPreferredSize();
  }

  virtual void OnPaintBorder(gfx::Canvas* canvas) OVERRIDE {
    // Just paint the left border.
    canvas->DrawLine(gfx::Point(0, 0), gfx::Point(0, height()),
                     kFooterDelimiterColor);
  }

  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE {
    if (HasFocus() && (focusable() || IsAccessibilityFocusable())) {
      canvas->DrawRect(gfx::Rect(2, 1, width() - 4, height() - 3),
                       kFocusBorderColor);
    }
  }

  DISALLOW_COPY_AND_ASSIGN(WebNotificationButton);
};

// TODO(mukai): remove the trailing '2' when the kEnableNewMessageCenterBubble
// flag disappears.
class WebNotificationButtonView2 : public WebNotificationButtonViewBase,
                                   public views::ButtonListener {
 public:
  explicit WebNotificationButtonView2(NotificationList::Delegate* list_delegate)
      : WebNotificationButtonViewBase(list_delegate) {
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
    AddChildView(settings_button_);
    WebNotificationButton* close_all_button = new WebNotificationButton(
        this, l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_CLEAR_ALL));
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

 private:
  // views::ButtonListener overrides:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    if (sender == close_all_button())
      list_delegate()->SendRemoveAllNotifications(true);  // Action by user.
    else if (sender == settings_button_)
      list_delegate()->ShowNotificationSettingsDialog(
          GetWidget()->GetNativeView());
    else
      NOTREACHED();
  }

  views::Label* notification_label_;
  views::Button* settings_button_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationButtonView2);
};

// A custom scroll-view that has a specified size.
class FixedSizedScrollView : public views::ScrollView {
 public:
  FixedSizedScrollView() {
    set_focusable(true);
    set_notify_enter_exit_on_child(true);
    if (IsRichNotificationEnabled()) {
      set_background(views::Background::CreateSolidBackground(
          kMessageCenterBackgroundColor));
    }
  }

  virtual ~FixedSizedScrollView() {}

  void SetFixedSize(const gfx::Size& size) {
    if (fixed_size_ == size)
      return;
    fixed_size_ = size;
    PreferredSizeChanged();
  }

  // views::View overrides.
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    gfx::Size size = fixed_size_.IsEmpty() ?
        contents()->GetPreferredSize() : fixed_size_;
    gfx::Insets insets = GetInsets();
    size.Enlarge(insets.width(), insets.height());
    return size;
  }

  virtual void Layout() OVERRIDE {
    gfx::Rect bounds = gfx::Rect(contents()->GetPreferredSize());
    bounds.set_width(std::max(0, width() - GetScrollBarWidth()));
    contents()->SetBoundsRect(bounds);

    views::ScrollView::Layout();
    if (!vertical_scroll_bar()->visible()) {
      gfx::Rect bounds = contents()->bounds();
      bounds.set_width(bounds.width() + GetScrollBarWidth());
      contents()->SetBoundsRect(bounds);
    }
  }

  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE {
    gfx::Rect bounds = gfx::Rect(contents()->GetPreferredSize());
    bounds.set_width(std::max(0, width() - GetScrollBarWidth()));
    contents()->SetBoundsRect(bounds);
  }

 private:
  gfx::Size fixed_size_;

  DISALLOW_COPY_AND_ASSIGN(FixedSizedScrollView);
};

// Container for the messages.
class ScrollContentView : public views::View {
 public:
  ScrollContentView() {
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
          0,  /* bottom */
          kMarginBetweenItems - shadow_insets.right() /* right */ ));
    } else {
      views::BoxLayout* layout =
          new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1);
      layout->set_spread_blank_space(true);
      SetLayoutManager(layout);
    }
  }

  virtual ~ScrollContentView() {
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    if (!preferred_size_.IsEmpty())
      return preferred_size_;
    return views::View::GetPreferredSize();
  }

  void set_preferred_size(const gfx::Size& size) { preferred_size_ = size; }

 private:
  gfx::Size preferred_size_;
  DISALLOW_COPY_AND_ASSIGN(ScrollContentView);
};

}  // namespace

// Message Center contents.
class MessageCenterContentsView : public views::View {
 public:
  explicit MessageCenterContentsView(MessageCenterBubble* bubble,
                                     NotificationList::Delegate* list_delegate)
      : list_delegate_(list_delegate),
        bubble_(bubble) {
    int between_child = IsRichNotificationEnabled() ? 0 : 1;
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, between_child));

    scroll_content_ = new ScrollContentView;
    scroller_ = new FixedSizedScrollView;
    scroller_->SetContents(scroll_content_);
    AddChildView(scroller_);

    if (get_use_acceleration_when_possible()) {
      scroller_->SetPaintToLayer(true);
      scroller_->SetFillsBoundsOpaquely(false);
      scroller_->layer()->SetMasksToBounds(true);
    }

    if (IsRichNotificationEnabled())
      button_view_ = new WebNotificationButtonView2(list_delegate);
    else
      button_view_ = new WebNotificationButtonView(list_delegate);
    AddChildView(button_view_);
  }

  void FocusContents() {
    scroller_->RequestFocus();
  }

  void Update(const NotificationList::Notifications& notifications)  {
    scroll_content_->RemoveAllChildViews(true);
    scroll_content_->set_preferred_size(gfx::Size());
    size_t num_children = 0;
    for (NotificationList::Notifications::const_iterator iter =
             notifications.begin(); iter != notifications.end(); ++iter) {
      MessageView* view = NotificationView::Create(*(*iter), list_delegate_);
      view->set_scroller(scroller_);
      scroll_content_->AddChildView(view);
      if (++num_children >=
          NotificationList::kMaxVisibleMessageCenterNotifications) {
        break;
      }
    }
    if (num_children == 0) {
      views::Label* label = new views::Label(l10n_util::GetStringUTF16(
          IDS_MESSAGE_CENTER_NO_MESSAGES));
      label->SetFont(label->font().DeriveFont(1));
      label->SetEnabledColor(SK_ColorGRAY);
      // Set transparent background to ensure that subpixel rendering
      // is disabled. See crbug.com/169056
      label->SetBackgroundColor(kTransparentColor);
      scroll_content_->AddChildView(label);
      button_view_->SetCloseAllVisible(false);
      scroller_->set_focusable(false);
    } else {
      button_view_->SetCloseAllVisible(true);
      scroller_->set_focusable(true);
    }
    SizeScrollContent();
    Layout();
    if (GetWidget())
      GetWidget()->GetRootView()->SchedulePaint();
  }

  size_t NumMessageViews() const {
    return scroll_content_->child_count();
  }

 private:
  void SizeScrollContent() {
    gfx::Size scroll_size = scroll_content_->GetPreferredSize();
    const int button_height = button_view_->GetPreferredSize().height();
    const int min_height = kMessageBubbleBaseMinHeight - button_height;
    const int max_height = bubble_->max_height() - button_height;
    int scroll_height = std::min(std::max(
        scroll_size.height(), min_height), max_height);
    scroll_size.set_height(scroll_height);
    if (scroll_height == min_height)
      scroll_content_->set_preferred_size(scroll_size);
    else
      scroll_content_->set_preferred_size(gfx::Size());
    scroller_->SetFixedSize(scroll_size);
    scroller_->SizeToPreferredSize();
    scroll_content_->InvalidateLayout();
  }

  NotificationList::Delegate* list_delegate_;
  FixedSizedScrollView* scroller_;
  ScrollContentView* scroll_content_;
  WebNotificationButtonViewBase* button_view_;
  MessageCenterBubble* bubble_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterContentsView);
};

// Message Center Bubble.
MessageCenterBubble::MessageCenterBubble(NotificationList::Delegate* delegate)
    : MessageBubbleBase(delegate),
      contents_view_(NULL) {
}

MessageCenterBubble::~MessageCenterBubble() {}

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
  contents_view_ = new MessageCenterContentsView(this, list_delegate());
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
      list_delegate()->GetNotificationList()->GetNotifications();
  contents_view_->Update(notifications);
  bubble_view()->Show();
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
