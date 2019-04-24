// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/unified_message_list_view.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/system/message_center/notification_swipe_control_view.h"
#include "ash/system/message_center/unified_message_center_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/auto_reset.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_view_factory.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

using message_center::Notification;
using message_center::MessageCenter;
using message_center::MessageView;

namespace ash {

namespace {

constexpr base::TimeDelta kClosingAnimationDuration =
    base::TimeDelta::FromMilliseconds(330);
constexpr base::TimeDelta kClearAllStackedAnimationDuration =
    base::TimeDelta::FromMilliseconds(40);
constexpr base::TimeDelta kClearAllVisibleAnimationDuration =
    base::TimeDelta::FromMilliseconds(160);

}  // namespace

// Container view of notification and swipe control.
// All children of UnifiedMessageListView should be MessageViewContainer.
class UnifiedMessageListView::MessageViewContainer
    : public views::View,
      public MessageView::SlideObserver {
 public:
  explicit MessageViewContainer(MessageView* message_view)
      : message_view_(message_view),
        control_view_(new NotificationSwipeControlView(message_view)) {
    message_view_->AddSlideObserver(this);

    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(control_view_);
    AddChildView(message_view_);
  }

  ~MessageViewContainer() override = default;

  // Update the border and background corners based on if the notification is
  // at the top or the bottom.
  void UpdateBorder(bool is_top, bool is_bottom) {
    message_view_->SetBorder(
        is_bottom ? views::NullBorder()
                  : views::CreateSolidSidedBorder(
                        0, 0, kUnifiedNotificationSeparatorThickness, 0,
                        kUnifiedNotificationSeparatorColor));
    const int top_radius = is_top ? kUnifiedTrayCornerRadius : 0;
    const int bottom_radius = is_bottom ? kUnifiedTrayCornerRadius : 0;
    message_view_->UpdateCornerRadius(top_radius, bottom_radius);
    control_view_->UpdateCornerRadius(top_radius, bottom_radius);
  }

  // Collapses the notification if its state haven't changed manually by a user.
  void Collapse() {
    if (!message_view_->IsManuallyExpandedOrCollapsed())
      message_view_->SetExpanded(false);
  }

  // Check if the notification is manually expanded / collapsed before and
  // restores the state.
  void LoadExpandedState(UnifiedSystemTrayModel* model, bool is_latest) {
    base::Optional<bool> manually_expanded =
        model->GetNotificationExpanded(GetNotificationId());
    if (manually_expanded.has_value()) {
      message_view_->SetExpanded(manually_expanded.value());
      message_view_->SetManuallyExpandedOrCollapsed(true);
    } else {
      // Expand the latest notification, and collapse all other notifications.
      message_view_->SetExpanded(is_latest &&
                                 message_view_->IsAutoExpandingAllowed());
    }
  }

  // Stores if the notification is manually expanded or collapsed so that we can
  // restore that when UnifiedSystemTray is reopened.
  void StoreExpandedState(UnifiedSystemTrayModel* model) {
    if (message_view_->IsManuallyExpandedOrCollapsed()) {
      model->SetNotificationExpanded(GetNotificationId(),
                                     message_view_->IsExpanded());
    }
  }

  std::string GetNotificationId() const {
    return message_view_->notification_id();
  }

  void UpdateWithNotification(const Notification& notification) {
    message_view_->UpdateWithNotification(notification);
  }

  void CloseSwipeControl() { message_view_->CloseSwipeControl(); }

  // Returns if the notification is pinned i.e. can be removed manually.
  bool IsPinned() const {
    return message_view_->GetMode() == MessageView::Mode::PINNED;
  }

  // Returns the direction that the notification is swiped out. If swiped to the
  // left, it returns -1 and if sipwed to the right, it returns 1. By default
  // (i.e. the notification is removed but not by touch gesture), it returns 1.
  int GetSlideDirection() const {
    return message_view_->GetSlideAmount() < 0 ? -1 : 1;
  }

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  // MessageView::SlideObserver:
  void OnSlideChanged(const std::string& notification_id) override {
    control_view_->UpdateButtonsVisibility();
  }

  gfx::Rect start_bounds() const { return start_bounds_; }
  gfx::Rect ideal_bounds() const { return ideal_bounds_; }
  bool is_removed() const { return is_removed_; }

  void set_start_bounds(const gfx::Rect& start_bounds) {
    start_bounds_ = start_bounds;
  }

  void set_ideal_bounds(const gfx::Rect& ideal_bounds) {
    ideal_bounds_ = ideal_bounds;
  }

  void set_is_removed() { is_removed_ = true; }

 private:
  // The bounds that the container starts animating from. If not animating, it's
  // ignored.
  gfx::Rect start_bounds_;

  // The final bounds of the container. If not animating, it's same as the
  // actual bounds().
  gfx::Rect ideal_bounds_;

  // True when the notification is removed and during SLIDE_OUT animation.
  // Unused if |state_| is not SLIDE_OUT.
  bool is_removed_ = false;

  MessageView* const message_view_;
  NotificationSwipeControlView* const control_view_;

  DISALLOW_COPY_AND_ASSIGN(MessageViewContainer);
};

UnifiedMessageListView::UnifiedMessageListView(
    UnifiedMessageCenterView* message_center_view,
    UnifiedSystemTrayModel* model)
    : message_center_view_(message_center_view),
      model_(model),
      animation_(std::make_unique<gfx::LinearAnimation>(this)) {
  MessageCenter::Get()->AddObserver(this);
  animation_->SetCurrentValue(1.0);
}

UnifiedMessageListView::~UnifiedMessageListView() {
  MessageCenter::Get()->RemoveObserver(this);

  model_->ClearNotificationChanges();
  for (int i = 0; i < child_count(); ++i)
    GetContainer(i)->StoreExpandedState(model_);
}

void UnifiedMessageListView::Init() {
  bool is_latest = true;
  for (auto* notification : MessageCenter::Get()->GetVisibleNotifications()) {
    auto* view = new MessageViewContainer(CreateMessageView(*notification));
    view->LoadExpandedState(model_, is_latest);
    AddChildViewAt(view, 0);
    MessageCenter::Get()->DisplayedNotification(
        notification->id(), message_center::DISPLAY_SOURCE_MESSAGE_CENTER);
    is_latest = false;
  }
  UpdateBorders();
  UpdateBounds();
}

void UnifiedMessageListView::ClearAllWithAnimation() {
  if (state_ == State::CLEAR_ALL_STACKED || state_ == State::CLEAR_ALL_VISIBLE)
    return;
  ResetBounds();

  {
    base::AutoReset<bool> auto_reset(&ignore_notification_remove_, true);
    message_center::MessageCenter::Get()->RemoveAllNotifications(
        true /* by_user */,
        message_center::MessageCenter::RemoveType::NON_PINNED);
  }

  state_ = State::CLEAR_ALL_STACKED;
  UpdateClearAllAnimation();
  if (state_ != State::IDLE)
    StartAnimation();
}

int UnifiedMessageListView::CountNotificationsAboveY(int y_offset) const {
  for (int i = 0; i < child_count(); ++i) {
    if (child_at(i)->bounds().bottom() > y_offset)
      return i;
  }
  return child_count();
}

int UnifiedMessageListView::GetTotalNotificationCount() const {
  return child_count();
}

void UnifiedMessageListView::ChildPreferredSizeChanged(views::View* child) {
  if (ignore_size_change_)
    return;
  ResetBounds();
}

void UnifiedMessageListView::PreferredSizeChanged() {
  views::View::PreferredSizeChanged();
  if (message_center_view_)
    message_center_view_->ListPreferredSizeChanged();
}

void UnifiedMessageListView::Layout() {
  for (int i = 0; i < child_count(); ++i) {
    auto* view = GetContainer(i);
    view->SetBoundsRect(gfx::Tween::RectValueBetween(
        GetCurrentValue(), view->start_bounds(), view->ideal_bounds()));
  }
}

gfx::Rect UnifiedMessageListView::GetNotificationBounds(
    const std::string& notification_id) const {
  if (!notification_id.empty()) {
    for (int i = 0; i < child_count(); i++) {
      if (GetContainer(i)->GetNotificationId() == notification_id)
        return GetContainer(i)->bounds();
    }
  }

  // Fallback to the last notification.
  return GetLastNotificationBounds();
}

gfx::Rect UnifiedMessageListView::GetLastNotificationBounds() const {
  if (child_count() == 0)
    return gfx::Rect();

  return GetContainer(child_count() - 1)->bounds();
}

gfx::Rect UnifiedMessageListView::GetNotificationBoundsBelowY(
    int y_offset) const {
  for (int i = 0; i < child_count(); ++i) {
    auto* view = GetContainer(i);
    if (view->bounds().bottom() >= y_offset)
      return view->bounds();
  }
  return gfx::Rect();
}

gfx::Size UnifiedMessageListView::CalculatePreferredSize() const {
  return gfx::Size(kTrayMenuWidth,
                   gfx::Tween::IntValueBetween(GetCurrentValue(), start_height_,
                                               ideal_height_));
}

void UnifiedMessageListView::OnNotificationAdded(const std::string& id) {
  auto* notification = MessageCenter::Get()->FindVisibleNotificationById(id);
  if (!notification)
    return;

  InterruptClearAll();

  // Collapse all notifications before adding new one.
  CollapseAllNotifications();

  auto* view = CreateMessageView(*notification);
  // Expand the latest notification.
  view->SetExpanded(view->IsAutoExpandingAllowed());
  AddChildView(new MessageViewContainer(view));
  UpdateBorders();
  ResetBounds();
}

void UnifiedMessageListView::OnNotificationRemoved(const std::string& id,
                                                   bool by_user) {
  if (ignore_notification_remove_)
    return;
  InterruptClearAll();
  ResetBounds();

  for (int i = 0; i < child_count(); ++i) {
    auto* view = GetContainer(i);
    if (view->GetNotificationId() == id) {
      view->set_is_removed();
      break;
    }
  }

  UpdateBounds();

  state_ = State::SLIDE_OUT;
  StartAnimation();
}

void UnifiedMessageListView::OnNotificationUpdated(const std::string& id) {
  auto* notification = MessageCenter::Get()->FindVisibleNotificationById(id);
  if (!notification)
    return;

  InterruptClearAll();

  for (int i = 0; i < child_count(); ++i) {
    auto* view = GetContainer(i);
    if (view->GetNotificationId() == id) {
      view->UpdateWithNotification(*notification);
      break;
    }
  }

  ResetBounds();
}

void UnifiedMessageListView::OnSlideStarted(
    const std::string& notification_id) {
  // When the swipe control for |notification_id| is shown, hide all other swipe
  // controls.
  for (int i = 0; i < child_count(); ++i) {
    auto* view = GetContainer(i);
    if (view->GetNotificationId() == notification_id)
      continue;
    view->CloseSwipeControl();
  }
}

void UnifiedMessageListView::AnimationEnded(const gfx::Animation* animation) {
  // This is also called from AnimationCanceled().
  animation_->SetCurrentValue(1.0);
  PreferredSizeChanged();

  if (state_ == State::SLIDE_OUT) {
    DeleteRemovedNotifications();
    UpdateBounds();

    state_ = State::MOVE_DOWN;
  } else if (state_ == State::MOVE_DOWN) {
    state_ = State::IDLE;
  } else if (state_ == State::CLEAR_ALL_STACKED ||
             state_ == State::CLEAR_ALL_VISIBLE) {
    DeleteRemovedNotifications();
    UpdateClearAllAnimation();
  }

  if (state_ != State::IDLE)
    StartAnimation();
}

void UnifiedMessageListView::AnimationProgressed(
    const gfx::Animation* animation) {
  PreferredSizeChanged();
}

void UnifiedMessageListView::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

MessageView* UnifiedMessageListView::CreateMessageView(
    const Notification& notification) {
  auto* view = message_center::MessageViewFactory::Create(notification);
  view->SetIsNested();
  view->AddSlideObserver(this);
  message_center_view_->ConfigureMessageView(view);
  return view;
}

int UnifiedMessageListView::GetStackedNotificationCount() const {
  return message_center_view_->GetStackedNotificationCount();
}

UnifiedMessageListView::MessageViewContainer*
UnifiedMessageListView::GetContainer(int index) {
  return const_cast<MessageViewContainer*>(
      const_cast<const UnifiedMessageListView*>(this)->GetContainer(index));
}

const UnifiedMessageListView::MessageViewContainer*
UnifiedMessageListView::GetContainer(int index) const {
  return static_cast<const MessageViewContainer*>(child_at(index));
}

UnifiedMessageListView::MessageViewContainer*
UnifiedMessageListView::GetNextRemovableNotification() {
  for (int i = 0; i < child_count(); ++i) {
    auto* view = GetContainer(i);
    if (!view->IsPinned())
      return view;
  }
  return nullptr;
}

void UnifiedMessageListView::CollapseAllNotifications() {
  base::AutoReset<bool> auto_reset(&ignore_size_change_, true);
  for (int i = 0; i < child_count(); ++i)
    GetContainer(i)->Collapse();
}

void UnifiedMessageListView::UpdateBorders() {
  for (int i = 0; i < child_count(); ++i) {
    bool is_top = i == 0;
    // When the stacking bar is shown, there should never be a top notification.
    if (features::IsNotificationStackingBarRedesignEnabled() &&
        GetTotalNotificationCount() > 1) {
      is_top = false;
    }
    const bool is_bottom = i == child_count() - 1;
    GetContainer(i)->UpdateBorder(is_top, is_bottom);
  }
}

void UnifiedMessageListView::UpdateBounds() {
  int y = 0;
  for (int i = 0; i < child_count(); ++i) {
    auto* view = GetContainer(i);
    const int height = view->GetHeightForWidth(kTrayMenuWidth);
    const int direction = view->GetSlideDirection();
    view->set_start_bounds(view->ideal_bounds());
    view->set_ideal_bounds(
        view->is_removed()
            ? gfx::Rect(kTrayMenuWidth * direction, y, kTrayMenuWidth, height)
            : gfx::Rect(0, y, kTrayMenuWidth, height));
    y += height;
  }

  start_height_ = ideal_height_;
  ideal_height_ = y;
}

void UnifiedMessageListView::ResetBounds() {
  DeleteRemovedNotifications();
  UpdateBounds();

  state_ = State::IDLE;
  if (animation_->is_animating())
    animation_->End();
  else
    PreferredSizeChanged();
}

void UnifiedMessageListView::InterruptClearAll() {
  if (state_ != State::CLEAR_ALL_STACKED && state_ != State::CLEAR_ALL_VISIBLE)
    return;

  for (int i = 0; i < child_count(); ++i) {
    auto* view = GetContainer(i);
    if (!view->IsPinned())
      view->set_is_removed();
  }

  DeleteRemovedNotifications();
}

void UnifiedMessageListView::DeleteRemovedNotifications() {
  std::vector<MessageViewContainer*> removed_views;
  for (int i = 0; i < child_count(); ++i) {
    auto* view = GetContainer(i);
    if (view->is_removed())
      removed_views.push_back(view);
  }

  {
    base::AutoReset<bool> auto_reset(&is_deleting_removed_notifications_, true);
    for (auto* view : removed_views) {
      model_->RemoveNotificationExpanded(view->GetNotificationId());
      delete view;
    }
  }

  UpdateBorders();
}

void UnifiedMessageListView::StartAnimation() {
  DCHECK_NE(state_, State::IDLE);

  switch (state_) {
    case State::IDLE:
      break;
    case State::SLIDE_OUT:
      FALLTHROUGH;
    case State::MOVE_DOWN:
      animation_->SetDuration(kClosingAnimationDuration);
      animation_->Start();
      break;
    case State::CLEAR_ALL_STACKED:
      animation_->SetDuration(kClearAllStackedAnimationDuration);
      animation_->Start();
      break;
    case State::CLEAR_ALL_VISIBLE:
      animation_->SetDuration(kClearAllVisibleAnimationDuration);
      animation_->Start();
      break;
  }
}

void UnifiedMessageListView::UpdateClearAllAnimation() {
  DCHECK(state_ == State::CLEAR_ALL_STACKED ||
         state_ == State::CLEAR_ALL_VISIBLE);

  auto* view = GetNextRemovableNotification();
  if (view)
    view->set_is_removed();

  if (state_ == State::CLEAR_ALL_STACKED) {
    if (view && GetStackedNotificationCount() > 0) {
      DeleteRemovedNotifications();
      UpdateBounds();
      start_height_ = ideal_height_;
      for (int i = 0; i < child_count(); ++i) {
        auto* view = GetContainer(i);
        view->set_start_bounds(view->ideal_bounds());
      }

      PreferredSizeChanged();

      state_ = State::CLEAR_ALL_STACKED;
    } else {
      state_ = State::CLEAR_ALL_VISIBLE;
    }
  }

  if (state_ == State::CLEAR_ALL_VISIBLE) {
    UpdateBounds();

    if (view || start_height_ != ideal_height_)
      state_ = State::CLEAR_ALL_VISIBLE;
    else
      state_ = State::IDLE;
  }
}

double UnifiedMessageListView::GetCurrentValue() const {
  return gfx::Tween::CalculateValue(
      state_ == State::SLIDE_OUT || state_ == State::CLEAR_ALL_VISIBLE
          ? gfx::Tween::EASE_IN
          : gfx::Tween::FAST_OUT_SLOW_IN,
      animation_->GetCurrentValue());
}

}  // namespace ash
