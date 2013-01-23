// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_popup_bubble.h"

#include "base/bind.h"
#include "ui/message_center/message_view.h"
#include "ui/message_center/notification_view.h"
#include "ui/notifications/notification_types.h"
#include "ui/views/bubble/tray_bubble_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace message_center {
namespace {

const int kAutocloseHighPriorityDelaySeconds = 25;
const int kAutocloseDefaultDelaySeconds = 8;

std::vector<const NotificationList::Notification*> GetNewNotifications(
    const NotificationList::Notifications& old_list,
    const NotificationList::Notifications& new_list) {
  std::set<std::string> existing_ids;
  std::vector<const NotificationList::Notification*> result;
  for (NotificationList::Notifications::const_iterator iter = old_list.begin();
       iter != old_list.end(); ++iter) {
    existing_ids.insert(iter->id);
  }
  for (NotificationList::Notifications::const_iterator iter = new_list.begin();
       iter != new_list.end(); ++iter) {
    if (existing_ids.find(iter->id) == existing_ids.end())
      result.push_back(&(*iter));
  }
  return result;
}

}  // namespace

// Popup notifications contents.
class PopupBubbleContentsView : public views::View {
 public:
  explicit PopupBubbleContentsView(NotificationList::Delegate* list_delegate)
      : list_delegate_(list_delegate) {
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));

    content_ = new views::View;
    content_->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
    AddChildView(content_);

    if (get_use_acceleration_when_possible()) {
      content_->SetPaintToLayer(true);
      content_->SetFillsBoundsOpaquely(false);
      content_->layer()->SetMasksToBounds(true);
    }
  }

  void Update(const NotificationList::Notifications& popup_notifications) {
    content_->RemoveAllChildViews(true);
    for (NotificationList::Notifications::const_iterator iter =
             popup_notifications.begin();
         iter != popup_notifications.end(); ++iter) {
      MessageView* view =
          NotificationView::ViewForNotification(*iter, list_delegate_);
      view->SetUpView();
      content_->AddChildView(view);
    }
    content_->SizeToPreferredSize();
    content_->InvalidateLayout();
    Layout();
    if (GetWidget())
      GetWidget()->GetRootView()->SchedulePaint();
  }

  size_t NumMessageViews() const {
    return content_->child_count();
  }

 private:
  NotificationList::Delegate* list_delegate_;
  views::View* content_;

  DISALLOW_COPY_AND_ASSIGN(PopupBubbleContentsView);
};

// MessagePopupBubble
MessagePopupBubble::MessagePopupBubble(NotificationList::Delegate* delegate)
    : MessageBubbleBase(delegate),
      contents_view_(NULL),
      should_run_default_timer_(false),
      should_run_high_timer_(false) {
}

MessagePopupBubble::~MessagePopupBubble() {
}

views::TrayBubbleView::InitParams MessagePopupBubble::GetInitParams(
    views::TrayBubbleView::AnchorAlignment anchor_alignment) {
  views::TrayBubbleView::InitParams init_params =
      GetDefaultInitParams(anchor_alignment);
  init_params.arrow_color = kBackgroundColor;
  init_params.close_on_deactivate = false;
  return init_params;
}

void MessagePopupBubble::InitializeContents(
    views::TrayBubbleView* new_bubble_view) {
  set_bubble_view(new_bubble_view);
  contents_view_ = new PopupBubbleContentsView(list_delegate());
  bubble_view()->AddChildView(contents_view_);
  UpdateBubbleView();
}

void MessagePopupBubble::OnBubbleViewDestroyed() {
  contents_view_ = NULL;
}

void MessagePopupBubble::UpdateBubbleView() {
  NotificationList::Notifications new_notifications;
  list_delegate()->GetNotificationList()->GetPopupNotifications(
      &new_notifications);
  if (new_notifications.size() == 0) {
    if (bubble_view())
      bubble_view()->delegate()->HideBubble(bubble_view());  // deletes |this|
    return;
  }
  // Only reset the timer when the number of visible notifications changes.
  std::vector<const NotificationList::Notification*> added =
      GetNewNotifications(popup_notifications_, new_notifications);
  bool run_default = false;
  bool run_high = false;
  for (std::vector<const NotificationList::Notification*>::const_iterator iter =
           added.begin(); iter != added.end(); ++iter) {
    if ((*iter)->priority == ui::notifications::DEFAULT_PRIORITY)
      run_default = true;
    else if ((*iter)->priority >= ui::notifications::HIGH_PRIORITY)
      run_high = true;
  }
  // Currently MAX priority is same as HIGH priority.
  if (run_high)
    StartAutoCloseTimer(ui::notifications::MAX_PRIORITY);
  if (run_default)
    StartAutoCloseTimer(ui::notifications::DEFAULT_PRIORITY);
  should_run_high_timer_ = run_high;
  should_run_default_timer_ = run_default;
  contents_view_->Update(new_notifications);
  popup_notifications_.swap(new_notifications);
  bubble_view()->Show();
  bubble_view()->UpdateBubble();
}

void MessagePopupBubble::OnMouseEnteredView() {
  StopAutoCloseTimer();
}

void MessagePopupBubble::OnMouseExitedView() {
  if (should_run_high_timer_)
    StartAutoCloseTimer(ui::notifications::HIGH_PRIORITY);
  if (should_run_default_timer_)
    StartAutoCloseTimer(ui::notifications::DEFAULT_PRIORITY);
}

void MessagePopupBubble::StartAutoCloseTimer(int priority) {
  base::TimeDelta seconds;
  base::OneShotTimer<MessagePopupBubble>* timer = NULL;
  if (priority == ui::notifications::MAX_PRIORITY) {
    seconds = base::TimeDelta::FromSeconds(kAutocloseHighPriorityDelaySeconds);
    timer = &autoclose_high_;
  } else {
    seconds = base::TimeDelta::FromSeconds(kAutocloseDefaultDelaySeconds);
    timer = &autoclose_default_;
  }

  timer->Start(FROM_HERE,
               seconds,
               base::Bind(&MessagePopupBubble::OnAutoClose,
                          base::Unretained(this), priority));
}

void MessagePopupBubble::StopAutoCloseTimer() {
  autoclose_high_.Stop();
  autoclose_default_.Stop();
}

void MessagePopupBubble::OnAutoClose(int priority) {
  list_delegate()->GetNotificationList()->MarkPopupsAsShown(priority);
  if (priority == ui::notifications::MAX_PRIORITY)
    list_delegate()->GetNotificationList()->MarkPopupsAsShown(
        ui::notifications::HIGH_PRIORITY);

  if (priority >= ui::notifications::MAX_PRIORITY)
    should_run_high_timer_ = false;
  else
    should_run_default_timer_ = false;
  UpdateBubbleView();
}

size_t MessagePopupBubble::NumMessageViewsForTest() const {
  return contents_view_->NumMessageViews();
}

}  // namespace message_center
