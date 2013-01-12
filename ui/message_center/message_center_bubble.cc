// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_bubble.h"

#include "base/command_line.h"
#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/size.h"
#include "ui/message_center/message_center_switches.h"
#include "ui/message_center/message_view.h"
#include "ui/message_center/message_view_factory.h"
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
const int kMarginBetweenItems = 10;
const int kItemShadowHeight = 4;
const SkColor kMessageCenterBackgroundColor = SkColorSetRGB(0xe5, 0xe5, 0xe5);
const SkColor kBorderDarkColor = SkColorSetRGB(0xaa, 0xaa, 0xaa);
const SkColor kMessageItemShadowColorBase = SkColorSetARGB(0.3 * 255, 0, 0, 0);
const SkColor kTransparentColor = SkColorSetARGB(0, 0, 0, 0);

bool UseNewDesign() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNewMessageCenterBubble);
}

// The view for the buttons at the bottom of the web notification tray.
class WebNotificationButtonView : public views::View,
                                  public views::ButtonListener {
 public:
  explicit WebNotificationButtonView(NotificationList::Delegate* list_delegate)
      : list_delegate_(list_delegate),
        close_all_button_(NULL) {
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
    close_all_button_ = new views::TextButton(
        this, rb.GetLocalizedString(IDS_MESSAGE_CENTER_CLEAR_ALL));
    close_all_button_->set_alignment(views::TextButton::ALIGN_CENTER);
    close_all_button_->set_focusable(true);
    close_all_button_->set_request_focus_on_press(false);

    layout->AddPaddingRow(0, 4);
    layout->StartRow(0, 0);
    layout->AddView(close_all_button_);
  }

  virtual ~WebNotificationButtonView() {}

  void SetCloseAllVisible(bool visible) {
    close_all_button_->SetVisible(visible);
  }

  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    if (sender == close_all_button_)
      list_delegate_->SendRemoveAllNotifications();
  }

 private:
  NotificationList::Delegate* list_delegate_;
  views::TextButton* close_all_button_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationButtonView);
};

// A custom scroll-view that has a specified size.
class FixedSizedScrollView : public views::ScrollView {
 public:
  FixedSizedScrollView() {
    set_focusable(true);
    set_notify_enter_exit_on_child(true);
    if (UseNewDesign()) {
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
    if (UseNewDesign()) {
      // Set the margin to 0 for the layout. BoxLayout assumes the same margin
      // for top and bottom, but the bottom margin here should be smaller
      // because of the shadow of message view. Use an empty border instead
      // to provide this margin.
      SetLayoutManager(
          new views::BoxLayout(views::BoxLayout::kVertical,
                               0,
                               0,
                               kMarginBetweenItems - kItemShadowHeight));
      set_background(views::Background::CreateSolidBackground(
          kMessageCenterBackgroundColor));
      set_border(views::Border::CreateEmptyBorder(
          kMarginBetweenItems, /* top */
          kMarginBetweenItems, /* left */
          kMarginBetweenItems - kItemShadowHeight,  /* bottom */
          kMarginBetweenItems /* right */ ));
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

// A view to draw gradient shadow for each MessageView.
class MessageViewShadow : public views::View {
 public:
  MessageViewShadow()
      : painter_(views::Painter::CreateVerticalGradient(
            kMessageItemShadowColorBase, kTransparentColor)) {
  }

 private:
  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    // The preferred size must not be empty. Thus put an arbitrary non-zero
    // width here. It will be just ignored by the vertical box layout.
    return gfx::Size(1, kItemShadowHeight);
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    painter_->Paint(canvas, bounds().size());
  }

  scoped_ptr<views::Painter> painter_;
  DISALLOW_COPY_AND_ASSIGN(MessageViewShadow);
};

}  // namespace

// Message Center contents.
class MessageCenterContentsView : public views::View {
 public:
  explicit MessageCenterContentsView(MessageCenterBubble* bubble,
                                     NotificationList::Delegate* list_delegate)
      : list_delegate_(list_delegate),
        bubble_(bubble) {
    int between_child = UseNewDesign() ? 0 : 1;
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, between_child));

    scroll_content_ = new ScrollContentView;
    scroller_ = new FixedSizedScrollView;
    scroller_->SetContents(scroll_content_);
    AddChildView(scroller_);

    scroller_->SetPaintToLayer(true);
    scroller_->SetFillsBoundsOpaquely(false);
    scroller_->layer()->SetMasksToBounds(true);

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
      MessageView* view =
          MessageViewFactory::ViewForNotification(*iter, list_delegate_);
      view->set_scroller(scroller_);
      view->SetUpView();
      if (UseNewDesign()) {
        views::View* container = new views::View();
        container->SetLayoutManager(
            new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
        container->AddChildView(view);
        container->AddChildView(new MessageViewShadow());
        scroll_content_->AddChildView(container);
      } else {
        scroll_content_->AddChildView(view);
      }
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
  WebNotificationButtonView* button_view_;
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
  if (UseNewDesign()) {
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
  UpdateBubbleView();
  contents_view_->FocusContents();
}

void MessageCenterBubble::OnBubbleViewDestroyed() {
  contents_view_ = NULL;
}

void MessageCenterBubble::UpdateBubbleView() {
  if (!bubble_view())
    return;  // Could get called after view is closed
  NotificationList::Notifications notifications;
  list_delegate()->GetNotificationList()->GetNotifications(&notifications);
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
