// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/unified_message_list_view.h"
#include <string>

#include "ash/bubble/bubble_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/system/message_center/ash_notification_view.h"
#include "ash/system/message_center/message_center_constants.h"
#include "ash/system/message_center/message_center_style.h"
#include "ash/system/message_center/message_center_utils.h"
#include "ash/system/message_center/message_view_factory.h"
#include "ash/system/message_center/metrics_utils.h"
#include "ash/system/message_center/notification_swipe_control_view.h"
#include "ash/system/message_center/unified_message_center_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/auto_reset.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/message_center/notification_view_controller.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

using message_center::MessageCenter;
using message_center::MessageView;
using message_center::Notification;

namespace ash {

namespace {

constexpr base::TimeDelta kClosingAnimationDuration = base::Milliseconds(320);
constexpr base::TimeDelta kClearAllStackedAnimationDuration =
    base::Milliseconds(40);
constexpr base::TimeDelta kClearAllVisibleAnimationDuration =
    base::Milliseconds(160);

constexpr char kMessageViewContainerClassName[] = "MessageViewContainer";

constexpr char kMoveDownAnimationSmoothnessHistogramName[] =
    "Ash.Notification.MoveDown.AnimationSmoothness";
constexpr char kClearAllStackedAnimationSmoothnessHistogramName[] =
    "Ash.Notification.ClearAllStacked.AnimationSmoothness";
constexpr char kClearAllVisibleAnimationSmoothnessHistogramName[] =
    "Ash.Notification.ClearAllVisible.AnimationSmoothness";
constexpr char kExpandOrCollapseAnimationSmoothnessHistogramName[] =
    "Ash.Notification.ExpandOrCollapse.AnimationSmoothness";

void RecordAnimationSmoothness(const std::string& histogram_name,
                               int smoothness) {
  base::UmaHistogramPercentage(histogram_name, smoothness);
}

void SetupThroughputTrackerForAnimationSmoothness(
    views::Widget* widget,
    absl::optional<ui::ThroughputTracker>& tracker,
    const char* histogram_name) {
  // `widget` may not exist in tests.
  if (!widget)
    return;

  tracker.emplace(widget->GetCompositor()->RequestNewThroughputTracker());
  tracker->Start(ash::metrics_util::ForSmoothness(
      base::BindRepeating(&RecordAnimationSmoothness, histogram_name)));
}

}  // namespace

// Container view of notification and swipe control.
// All children of UnifiedMessageListView should be MessageViewContainer.
class UnifiedMessageListView::MessageViewContainer
    : public MessageView::Observer,
      public views::View {
 public:
  MessageViewContainer(MessageView* message_view,
                       UnifiedMessageListView* list_view)
      : message_view_(message_view),
        list_view_(list_view),
        control_view_(new NotificationSwipeControlView(message_view)) {
    message_view_->AddObserver(this);
    if (!features::IsNotificationsRefreshEnabled()) {
      message_view_->SetBackground(
          views::CreateSolidBackground(SK_ColorTRANSPARENT));
    }

    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(control_view_);
    AddChildView(message_view_);
  }

  MessageViewContainer(const MessageViewContainer&) = delete;
  MessageViewContainer& operator=(const MessageViewContainer&) = delete;

  ~MessageViewContainer() override { message_view_->RemoveObserver(this); }

  base::TimeDelta GetBoundsAnimationDuration() const {
    auto* notification = MessageCenter::Get()->FindNotificationById(
        message_view()->notification_id());
    if (!notification)
      return base::Milliseconds(0);
    if (message_view()->GetClassName() == AshNotificationView::kViewClassName) {
      return static_cast<const AshNotificationView*>(message_view())
          ->GetBoundsAnimationDuration(*notification);
    }
    // TODO(crbug/1278483): ARC notifications will require different animation
    // durations. Default to kLargeImageExpandAndCollapseAnimationDuration for
    // now.
    return base::Milliseconds(kLargeImageExpandAndCollapseAnimationDuration);
  }

  // Update the border and background corners based on if the notification is
  // at the top or the bottom. If `force_update` is true, ignore previous states
  // and always update the border.
  void UpdateBorder(bool is_top, bool is_bottom, bool force_update) {
    if (is_top_ == is_top && is_bottom_ == is_bottom && !force_update)
      return;
    is_top_ = is_top;
    is_bottom_ = is_bottom;

    message_view_->SetBorder(
        is_bottom ? views::NullBorder()
                  : views::CreateSolidSidedBorder(
                        0, 0, kUnifiedNotificationSeparatorThickness, 0,
                        message_center_style::kSeperatorColor));
    const int top_radius = is_top ? kBubbleCornerRadius : 0;
    const int bottom_radius = is_bottom ? kBubbleCornerRadius : 0;
    message_view_->UpdateCornerRadius(top_radius, bottom_radius);
    control_view_->UpdateCornerRadius(top_radius, bottom_radius);
  }

  // Reset rounding the corner of the view. This is called when we end a slide.
  void ResetCornerRadius() { message_view_->UpdateCornerRadius(0, 0); }

  // Collapses the notification if its state haven't changed manually by a user.
  void Collapse() {
    if (!message_view_->IsManuallyExpandedOrCollapsed())
      message_view_->SetExpanded(false);
  }

  // Check if the notification is manually expanded / collapsed before and
  // restores the state.
  void LoadExpandedState(UnifiedSystemTrayModel* model, bool is_latest) {
    DCHECK(model);
    base::AutoReset<bool> scoped_reset(&loading_expanded_state_, true);
    absl::optional<bool> manually_expanded =
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
    DCHECK(model);
    if (message_view_->IsManuallyExpandedOrCollapsed()) {
      model->SetNotificationExpanded(GetNotificationId(),
                                     message_view_->IsExpanded());
    }
  }

  void SlideOutAndClose() {
    is_slid_out_programatically = true;
    message_view_->SlideOutAndClose(1 /* direction */);
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

  // Allows UnifiedMessageListView to force preferred size to change during
  // animations.
  void TriggerPreferredSizeChangedForAnimation() {
    views::View::PreferredSizeChanged();
  }

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override {
    // If we've already been removed, ignore new child size changes.
    if (is_removed_)
      return;

    // PreferredSizeChanged will trigger
    // UnifiedMessageListView::ChildPreferredSizeChanged.
    base::ScopedClosureRunner defer_preferred_size_changed(base::BindOnce(
        &MessageViewContainer::PreferredSizeChanged, base::Unretained(this)));

    if (!features::IsNotificationsRefreshEnabled())
      return;

    // Ignore non user triggered expand/collapses.
    if (loading_expanded_state_)
      return;

    auto* notification = MessageCenter::Get()->FindNotificationById(
        message_view()->notification_id());
    if (!notification)
      return;

    needs_bounds_animation_ = true;
  }

  gfx::Size CalculatePreferredSize() const override {
    if (list_view_->IsAnimatingExpandOrCollapseContainer(this)) {
      // Width should never change, only height.
      return gfx::Size(list_view_->message_view_width_,
                       gfx::Tween::IntValueBetween(
                           list_view_->GetCurrentValue(),
                           start_bounds_.height(), target_bounds_.height()));
    }
    return gfx::Size(list_view_->message_view_width_, target_bounds_.height());
  }

  const char* GetClassName() const override {
    return kMessageViewContainerClassName;
  }

  // MessageView::Observer:
  void OnSlideChanged(const std::string& notification_id) override {
    control_view_->UpdateButtonsVisibility();

    if (!features::IsNotificationsRefreshEnabled() ||
        notification_id != GetNotificationId() ||
        message_view_->GetSlideAmount() == 0 || !need_update_corner_radius_)
      return;

    need_update_corner_radius_ = false;

    message_view_->UpdateCornerRadius(kBubbleCornerRadius, kBubbleCornerRadius);

    // Also update `above_view_`'s bottom and `below_view_`'s top corner radius
    // when sliding.
    int index = list_view_->GetIndexOf(this);
    auto list_child_views = list_view_->children();

    above_view_ = (index == 0) ? nullptr : AsMVC(list_child_views[index - 1]);
    if (above_view_)
      above_view_->message_view()->UpdateCornerRadius(0, kBubbleCornerRadius);

    below_view_ = (index == static_cast<int>(list_child_views.size()) - 1)
                      ? nullptr
                      : AsMVC(list_child_views[index + 1]);
    if (below_view_)
      below_view_->message_view()->UpdateCornerRadius(kBubbleCornerRadius, 0);
  }

  void OnSlideEnded(const std::string& notification_id) override {
    if (!features::IsNotificationsRefreshEnabled() ||
        notification_id != GetNotificationId())
      return;

    // Reset the corner radius of views to their normal state.
    ResetCornerRadius();
    if (above_view_ && !above_view_->is_slid_out())
      above_view_->ResetCornerRadius();
    if (below_view_ && !below_view_->is_slid_out())
      below_view_->ResetCornerRadius();

    need_update_corner_radius_ = true;
  }

  void OnPreSlideOut(const std::string& notification_id) override {
    if (!is_slid_out_programatically) {
      metrics_utils::LogClosedByUser(notification_id, /*is_swipe=*/true,
                                     /*is_popup=*/false);
    }
  }

  void OnSlideOut(const std::string& notification_id) override {
    is_slid_out_ = true;
    set_is_removed();
    list_view_->OnNotificationSlidOut();
  }

  gfx::Rect start_bounds() const { return start_bounds_; }
  gfx::Rect target_bounds() const { return target_bounds_; }
  bool is_removed() const { return is_removed_; }

  void ResetNeedsBoundsAnimation() { needs_bounds_animation_ = false; }
  bool needs_bounds_animation() const { return needs_bounds_animation_; }

  void set_start_bounds(const gfx::Rect& start_bounds) {
    start_bounds_ = start_bounds;
  }

  void set_target_bounds(const gfx::Rect& ideal_bounds) {
    target_bounds_ = ideal_bounds;
  }

  void set_is_removed() { is_removed_ = true; }

  bool is_slid_out() { return is_slid_out_; }

  MessageView* message_view() { return message_view_; }
  const MessageView* message_view() const { return message_view_; }

 private:
  // The bounds that the container starts animating from. If not animating, it's
  // ignored.
  gfx::Rect start_bounds_;

  // The final bounds of the container. If not animating, it's same as the
  // actual bounds().
  gfx::Rect target_bounds_;

  // True when the notification is removed and during slide out animation.
  bool is_removed_ = false;

  // True if the notification is slid out completely.
  bool is_slid_out_ = false;

  // True if the notification is slid out through SlideOutAndClose()
  // programagically. False if slid out manually by the user.
  bool is_slid_out_programatically = false;

  // Keeps track if this view is at the top or bottom of `list_view_`. Storing
  // this to prevent unnecessary update.
  bool is_top_ = false;
  bool is_bottom_ = false;

  // Whether expanded state is being set programmatically. Used to prevent
  // animating programmatic expands which occur on open.
  bool loading_expanded_state_ = false;

  // Set to flag the view as requiring an expand or collapse animation.
  bool needs_bounds_animation_ = false;

  // The views directly above or below this view in the list. Used to update
  // corner radius when sliding.
  MessageViewContainer* above_view_ = nullptr;
  MessageViewContainer* below_view_ = nullptr;

  // `need_update_corner_radius_` indicates that we need to update the corner
  // radius of the view when sliding.
  bool need_update_corner_radius_ = true;

  MessageView* const message_view_;
  UnifiedMessageListView* const list_view_;
  NotificationSwipeControlView* const control_view_;
};

UnifiedMessageListView::UnifiedMessageListView(
    UnifiedMessageCenterView* message_center_view,
    scoped_refptr<UnifiedSystemTrayModel> model)
    : views::AnimationDelegateViews(this),
      message_center_view_(message_center_view),
      model_(model),
      animation_(std::make_unique<gfx::LinearAnimation>(this)),
      is_notifications_refresh_enabled_(
          features::IsNotificationsRefreshEnabled()),
      message_view_width_(is_notifications_refresh_enabled_
                              ? kTrayMenuWidth - (2 * kMessageCenterSidePadding)
                              : kTrayMenuWidth) {
  message_center_observation_.Observe(MessageCenter::Get());
  animation_->SetCurrentValue(1.0);

  if (!is_notifications_refresh_enabled_) {
    SetBackground(views::CreateSolidBackground(
        message_center_style::kSwipeControlBackgroundColor));
  }
}

UnifiedMessageListView::~UnifiedMessageListView() {
  DCHECK(model_);
  model_->ClearNotificationChanges();
  for (auto* view : children())
    AsMVC(view)->StoreExpandedState(model_.get());
}

void UnifiedMessageListView::Init() {
  DCHECK(model_);
  bool is_latest = true;
  for (auto* notification :
       message_center_utils::GetSortedNotificationsWithOwnView()) {
    auto* view =
        new MessageViewContainer(CreateMessageView(*notification), this);
    view->LoadExpandedState(model_.get(), is_latest);
    AddChildViewAt(view, 0);
    MessageCenter::Get()->DisplayedNotification(
        notification->id(), message_center::DISPLAY_SOURCE_MESSAGE_CENTER);
    is_latest = false;
  }
  UpdateBorders(/*force_update=*/true);
  UpdateBounds();
}

void UnifiedMessageListView::ClearAllWithAnimation() {
  if (state_ == State::CLEAR_ALL_STACKED || state_ == State::CLEAR_ALL_VISIBLE)
    return;
  ResetBounds();

  UMA_HISTOGRAM_COUNTS_100("ChromeOS.SystemTray.NotificationsRemovedByClearAll",
                           children().size());

  // Record a ClosedByClearAll metric for each notification dismissed.
  for (auto* child : children()) {
    auto* view = AsMVC(child);
    metrics_utils::LogClosedByClearAll(view->GetNotificationId());
  }

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

std::vector<message_center::Notification*>
UnifiedMessageListView::GetAllNotifications() const {
  std::vector<message_center::Notification*> notifications;
  for (views::View* view : children()) {
    // The view may be present in the view hierarchy, but deleted in the message
    // center.
    auto* notification = MessageCenter::Get()->FindVisibleNotificationById(
        AsMVC(view)->GetNotificationId());
    if (notification)
      notifications.insert(notifications.begin(), notification);
  }
  return notifications;
}

std::vector<std::string> UnifiedMessageListView::GetAllNotificationIds() const {
  std::vector<std::string> notifications;
  for (views::View* view : children()) {
    notifications.insert(notifications.begin(),
                         AsMVC(view)->GetNotificationId());
  }
  return notifications;
}

std::vector<message_center::Notification*>
UnifiedMessageListView::GetNotificationsAboveY(int y_offset) const {
  std::vector<message_center::Notification*> notifications;
  for (views::View* view : children()) {
    const int bottom_limit =
        view->bounds().y() + kNotificationIconStackThreshold;
    if (bottom_limit <= y_offset) {
      auto* notification = MessageCenter::Get()->FindVisibleNotificationById(
          AsMVC(view)->GetNotificationId());
      if (notification)
        notifications.insert(notifications.begin(), notification);
    }
  }
  return notifications;
}

std::vector<std::string> UnifiedMessageListView::GetNotificationIdsAboveY(
    int y_offset) const {
  std::vector<std::string> notifications;
  for (views::View* view : children()) {
    const int bottom_limit =
        view->bounds().y() + kNotificationIconStackThreshold;
    if (bottom_limit > y_offset)
      continue;
    notifications.insert(notifications.begin(),
                         AsMVC(view)->GetNotificationId());
  }
  return notifications;
}

std::vector<std::string> UnifiedMessageListView::GetNotificationIdsBelowY(
    int y_offset) const {
  std::vector<std::string> notifications;
  for (views::View* view : children()) {
    const int top_of_notification = view->bounds().y();
    if (top_of_notification < y_offset)
      continue;
    notifications.insert(notifications.begin(),
                         AsMVC(view)->GetNotificationId());
  }
  return notifications;
}

int UnifiedMessageListView::GetTotalNotificationCount() const {
  return static_cast<int>(children().size());
}

int UnifiedMessageListView::GetTotalPinnedNotificationCount() const {
  int count = 0;
  for (auto* child : children()) {
    if (AsMVC(child)->IsPinned())
      count++;
  }
  return count;
}

bool UnifiedMessageListView::IsAnimating() const {
  return animation_->is_animating();
}

bool UnifiedMessageListView::IsAnimatingExpandOrCollapseContainer(
    const views::View* view) const {
  if (!view || !expand_or_collapsing_container_)
    return false;

  DCHECK_EQ(kMessageViewContainerClassName, view->GetClassName())
      << view->GetClassName() << " is not a " << kMessageViewContainerClassName;
  const MessageViewContainer* message_view_container = AsMVC(view);
  return message_view_container == expand_or_collapsing_container_;
}

void UnifiedMessageListView::ChildPreferredSizeChanged(views::View* child) {
  if (ignore_size_change_)
    return;

  // No State::EXPAND_OR_COLLAPSE animation in the old UI.
  if (!features::IsNotificationsRefreshEnabled()) {
    ResetBounds();
    return;
  }

  auto* message_view_container = AsMVC(child);
  // Immediately complete the old expand/collapse animation. It will be snapped
  // to the target bounds when UpdateBounds() is called. If the other animations
  // are occurring, prefer them over expand/collapse.
  if (message_view_container->needs_bounds_animation() &&
      (state_ == State::IDLE || state_ == State::EXPAND_OR_COLLAPSE)) {
    if (animation_->is_animating()) {
      // Finish the previous expand animation instantly.
      animation_->End();
    }
    expand_or_collapsing_container_ = message_view_container;
    expand_or_collapsing_container_->ResetNeedsBoundsAnimation();
    UpdateBounds();
    state_ = State::EXPAND_OR_COLLAPSE;
    StartAnimation();
    return;
  }

  if (state_ == State::EXPAND_OR_COLLAPSE)
    return;

  ResetBounds();
}

void UnifiedMessageListView::PreferredSizeChanged() {
  views::View::PreferredSizeChanged();
  if (message_center_view_)
    message_center_view_->ListPreferredSizeChanged();
}

void UnifiedMessageListView::Layout() {
  for (auto* child : children()) {
    auto* view = AsMVC(child);
    if (state_ == State::IDLE) {
      view->SetBoundsRect(view->target_bounds());
      continue;
    }
    view->SetBoundsRect(gfx::Tween::RectValueBetween(
        GetCurrentValue(), view->start_bounds(), view->target_bounds()));
  }
}

gfx::Rect UnifiedMessageListView::GetNotificationBounds(
    const std::string& notification_id) const {
  const MessageViewContainer* child = nullptr;
  if (!notification_id.empty())
    child = GetNotificationById(notification_id);
  return child ? child->bounds() : GetLastNotificationBounds();
}

gfx::Rect UnifiedMessageListView::GetLastNotificationBounds() const {
  return children().empty() ? gfx::Rect() : children().back()->bounds();
}

gfx::Rect UnifiedMessageListView::GetNotificationBoundsBelowY(
    int y_offset) const {
  const auto it = std::find_if(children().cbegin(), children().cend(),
                               [y_offset](const views::View* v) {
                                 return v->bounds().bottom() >= y_offset;
                               });
  return (it == children().cend()) ? gfx::Rect() : (*it)->bounds();
}

gfx::Size UnifiedMessageListView::CalculatePreferredSize() const {
  if (state_ == State::IDLE)
    return gfx::Size(message_view_width_, target_height_);

  return gfx::Size(message_view_width_,
                   gfx::Tween::IntValueBetween(GetCurrentValue(), start_height_,
                                               target_height_));
}

const char* UnifiedMessageListView::GetClassName() const {
  return "UnifiedMessageListView";
}

message_center::MessageView*
UnifiedMessageListView::GetMessageViewForNotificationId(const std::string& id) {
  auto it =
      std::find_if(children().begin(), children().end(), [&](auto* child) {
        DCHECK(child->GetClassName() == kMessageViewContainerClassName);
        return static_cast<MessageViewContainer*>(child)
                   ->message_view()
                   ->notification_id() == id;
      });

  if (it == children().end())
    return nullptr;
  return static_cast<MessageViewContainer*>(*it)->message_view();
}

void UnifiedMessageListView::ConvertNotificationViewToGroupedNotificationView(
    const std::string& ungrouped_notification_id,
    const std::string& new_grouped_notification_id) {
  GetMessageViewForNotificationId(ungrouped_notification_id)
      ->set_notification_id(new_grouped_notification_id);
}

void UnifiedMessageListView::ConvertGroupedNotificationViewToNotificationView(
    const std::string& grouped_notification_id,
    const std::string& new_single_notification_id) {
  GetMessageViewForNotificationId(grouped_notification_id)
      ->set_notification_id(new_single_notification_id);
}

void UnifiedMessageListView::OnNotificationAdded(const std::string& id) {
  auto* notification = MessageCenter::Get()->FindVisibleNotificationById(id);
  if (!notification)
    return;

  InterruptClearAll();

  // Collapse all notifications before adding new one.
  CollapseAllNotifications();

  // Find the correct index to insert the new notification based on the sorted
  // order.
  auto child_views = children();
  size_t index_to_insert = child_views.size();
  for (size_t i = 0; i < child_views.size(); ++i) {
    MessageViewContainer* message_view =
        static_cast<MessageViewContainer*>(child_views[i]);
    auto* child_notification =
        MessageCenter::Get()->FindVisibleNotificationById(
            message_view->GetNotificationId());
    if (!child_notification)
      break;

    if (!message_center_utils::CompareNotifications(notification,
                                                    child_notification)) {
      index_to_insert = i;
      break;
    }
  }

  auto* view = CreateMessageView(*notification);
  view->SetExpanded(view->IsAutoExpandingAllowed());
  AddChildViewAt(new MessageViewContainer(view, this), index_to_insert);
  UpdateBorders(/*force_update=*/false);
  ResetBounds();
}

void UnifiedMessageListView::OnNotificationRemoved(const std::string& id,
                                                   bool by_user) {
  if (ignore_notification_remove_)
    return;

  // The corresponding MessageView may have already been deleted after being
  // manually slid out.
  auto* child = GetNotificationById(id);
  if (!child)
    return;

  InterruptClearAll();
  ResetBounds();

  child->set_is_removed();

  // If the MessageView is slid out, then do nothing here. The MOVE_DOWN
  // animation will be started in OnNotificationSlidOut().
  if (!child->is_slid_out())
    child->SlideOutAndClose();
}

void UnifiedMessageListView::OnNotificationSlidOut() {
  DeleteRemovedNotifications();

  // |message_center_view_| can be null in tests.
  if (message_center_view_)
    message_center_view_->OnNotificationSlidOut();

  state_ = State::MOVE_DOWN;
  UpdateBounds();
  StartAnimation();
}

void UnifiedMessageListView::OnNotificationUpdated(const std::string& id) {
  auto* notification = MessageCenter::Get()->FindVisibleNotificationById(id);
  if (!notification)
    return;

  InterruptClearAll();

  // The corresponding MessageView may have been slid out and deleted, so just
  // ignore this update as the notification will soon be deleted.
  auto* child = GetNotificationById(id);
  if (!child)
    return;

  child->UpdateWithNotification(*notification);
  ResetBounds();
}

void UnifiedMessageListView::OnSlideStarted(
    const std::string& notification_id) {
  // When the swipe control for |notification_id| is shown, hide all other swipe
  // controls.
  for (auto* child : children()) {
    auto* view = AsMVC(child);
    if (view->GetNotificationId() != notification_id)
      view->CloseSwipeControl();
  }
}

void UnifiedMessageListView::OnCloseButtonPressed(
    const std::string& notification_id) {
  metrics_utils::LogClosedByUser(notification_id, /*is_swipe=*/false,
                                 /*is_popup=*/false);
}

void UnifiedMessageListView::OnSettingsButtonPressed(
    const std::string& notification_id) {
  metrics_utils::LogSettingsShown(notification_id, /*is_slide_controls=*/false,
                                  /*is_popup=*/false);
}

void UnifiedMessageListView::OnSnoozeButtonPressed(
    const std::string& notification_id) {
  metrics_utils::LogSnoozed(notification_id, /*is_slide_controls=*/false,
                            /*is_popup=*/false);
}

void UnifiedMessageListView::AnimationEnded(const gfx::Animation* animation) {
  if (throughput_tracker_) {
    // Reset `throughput_tracker_` to reset animation metrics recording.
    throughput_tracker_->Stop();
    throughput_tracker_.reset();
  }

  // This is also called from AnimationCanceled().
  // TODO(crbug/1272104): Can we do better? If we are interrupting an animation,
  // this does not look good.
  animation_->SetCurrentValue(1.0);
  PreferredSizeChanged();

  switch (state_) {
    case State::IDLE:
    case State::EXPAND_OR_COLLAPSE:
      expand_or_collapsing_container_ = nullptr;
      [[fallthrough]];
    case State::MOVE_DOWN:
      state_ = State::IDLE;
      break;
    case State::CLEAR_ALL_STACKED:
    case State::CLEAR_ALL_VISIBLE:
      DeleteRemovedNotifications();
      UpdateClearAllAnimation();
      break;
  }

  UpdateBorders(/*force_update=*/false);

  if (state_ != State::IDLE)
    StartAnimation();
}

void UnifiedMessageListView::AnimationProgressed(
    const gfx::Animation* animation) {
  if (state_ == State::EXPAND_OR_COLLAPSE)
    expand_or_collapsing_container_->TriggerPreferredSizeChangedForAnimation();

  PreferredSizeChanged();
}

void UnifiedMessageListView::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

MessageView* UnifiedMessageListView::CreateMessageView(
    const Notification& notification) {
  auto* view =
      MessageViewFactory::Create(notification, /*shown_in_popup=*/false)
          .release();
  view->SetIsNested();
  view->AddObserver(this);
  message_center_view_->ConfigureMessageView(view);
  return view;
}

std::vector<message_center::Notification*>
UnifiedMessageListView::GetStackedNotifications() const {
  return message_center_view_->GetStackedNotifications();
}

std::vector<std::string>
UnifiedMessageListView::GetNonVisibleNotificationIdsInViewHierarchy() const {
  return message_center_view_->GetNonVisibleNotificationIdsInViewHierarchy();
}

// static
const UnifiedMessageListView::MessageViewContainer*
UnifiedMessageListView::AsMVC(const views::View* v) {
  return static_cast<const MessageViewContainer*>(v);
}

// static
UnifiedMessageListView::MessageViewContainer* UnifiedMessageListView::AsMVC(
    views::View* v) {
  return static_cast<MessageViewContainer*>(v);
}

const UnifiedMessageListView::MessageViewContainer*
UnifiedMessageListView::GetNotificationById(const std::string& id) const {
  const auto i = std::find_if(
      children().cbegin(), children().cend(),
      [id](const auto* v) { return AsMVC(v)->GetNotificationId() == id; });
  return (i == children().cend()) ? nullptr : AsMVC(*i);
}

UnifiedMessageListView::MessageViewContainer*
UnifiedMessageListView::GetNextRemovableNotification() {
  const auto i =
      std::find_if(children().cbegin(), children().cend(),
                   [](const auto* v) { return !AsMVC(v)->IsPinned(); });
  return (i == children().cend()) ? nullptr : AsMVC(*i);
}

void UnifiedMessageListView::CollapseAllNotifications() {
  base::AutoReset<bool> auto_reset(&ignore_size_change_, true);
  for (auto* child : children())
    AsMVC(child)->Collapse();
}

void UnifiedMessageListView::UpdateBorders(bool force_update) {
  // We do not need individual notifications to have rounded corners
  // on the borders with the new UI. This is because the entire
  // scroll view has rounded corners now.
  if (is_notifications_refresh_enabled_)
    return;

  // The top notification is drawn with rounded corners when the stacking bar is
  // not shown.
  bool is_top = state_ != State::MOVE_DOWN;
  if (!is_notifications_refresh_enabled_)
    is_top = is_top && children().size() == 1;

  for (auto* child : children()) {
    AsMVC(child)->UpdateBorder(is_top, child == children().back(),
                               force_update);
    is_top = false;
  }
}

void UnifiedMessageListView::UpdateBounds() {
  int y = 0;
  for (auto* child : children()) {
    auto* view = AsMVC(child);
    // Height is taken from preferred size, which is calculated based on the
    // tween and animation state when animations are occurring. So views which
    // are animating will provide the correct interpolated height here.
    const int height = view->GetHeightForWidth(message_view_width_);
    const int direction = view->GetSlideDirection();

    if (y > 0 && is_notifications_refresh_enabled_)
      y += kMessageListNotificationSpacing;

    view->set_start_bounds(view->target_bounds());
    view->set_target_bounds(view->is_removed()
                                ? gfx::Rect(message_view_width_ * direction, y,
                                            message_view_width_, height)
                                : gfx::Rect(0, y, message_view_width_, height));
    y += height;
  }

  start_height_ = target_height_;
  target_height_ = y;
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

  for (auto* child : children()) {
    auto* view = AsMVC(child);
    if (!view->IsPinned())
      view->set_is_removed();
  }

  DeleteRemovedNotifications();
}

void UnifiedMessageListView::DeleteRemovedNotifications() {
  DCHECK(model_);
  views::View::Views removed_views;
  std::copy_if(children().cbegin(), children().cend(),
               std::back_inserter(removed_views),
               [](const auto* v) { return AsMVC(v)->is_removed(); });

  {
    base::AutoReset<bool> auto_reset(&is_deleting_removed_notifications_, true);
    for (auto* view : removed_views) {
      model_->RemoveNotificationExpanded(AsMVC(view)->GetNotificationId());
      delete view;
    }
  }

  UpdateBorders(/*force_update=*/false);
}

void UnifiedMessageListView::StartAnimation() {
  DCHECK_NE(state_, State::IDLE);

  base::TimeDelta animation_duration;

  switch (state_) {
    case State::IDLE:
      break;
    case State::MOVE_DOWN:
      SetupThroughputTrackerForAnimationSmoothness(
          GetWidget(), throughput_tracker_,
          kMoveDownAnimationSmoothnessHistogramName);
      animation_duration = kClosingAnimationDuration;
      break;
    case State::CLEAR_ALL_STACKED:
      SetupThroughputTrackerForAnimationSmoothness(
          GetWidget(), throughput_tracker_,
          kClearAllStackedAnimationSmoothnessHistogramName);
      animation_duration = kClearAllStackedAnimationDuration;
      break;
    case State::CLEAR_ALL_VISIBLE:
      SetupThroughputTrackerForAnimationSmoothness(
          GetWidget(), throughput_tracker_,
          kClearAllVisibleAnimationSmoothnessHistogramName);
      animation_duration = kClearAllVisibleAnimationDuration;
      break;
    case State::EXPAND_OR_COLLAPSE:
      SetupThroughputTrackerForAnimationSmoothness(
          GetWidget(), throughput_tracker_,
          kExpandOrCollapseAnimationSmoothnessHistogramName);
      DCHECK(expand_or_collapsing_container_);
      animation_duration =
          expand_or_collapsing_container_
              ? expand_or_collapsing_container_->GetBoundsAnimationDuration()
              : base::Milliseconds(
                    kLargeImageExpandAndCollapseAnimationDuration);
      break;
  }

  animation_->SetDuration(animation_duration);
  animation_->Start();
}

void UnifiedMessageListView::UpdateClearAllAnimation() {
  DCHECK(state_ == State::CLEAR_ALL_STACKED ||
         state_ == State::CLEAR_ALL_VISIBLE);

  auto* view = GetNextRemovableNotification();
  if (view)
    view->set_is_removed();

  if (state_ == State::CLEAR_ALL_STACKED) {
    const auto non_visible_notification_ids =
        GetNonVisibleNotificationIdsInViewHierarchy();
    if (view && non_visible_notification_ids.size() > 0) {
      // Immediately remove all notifications that are outside of the scrollable
      // window.
      for (const auto& id : non_visible_notification_ids) {
        auto* message_view_container = GetNotificationById(id);
        if (message_view_container)
          message_view_container->set_is_removed();
      }

      DeleteRemovedNotifications();
      UpdateBounds();
      start_height_ = target_height_;
      for (auto* child : children()) {
        auto* view = AsMVC(child);
        view->set_start_bounds(view->target_bounds());
      }
      PreferredSizeChanged();
    } else {
      state_ = State::CLEAR_ALL_VISIBLE;
    }
  }

  if (state_ == State::CLEAR_ALL_VISIBLE) {
    UpdateBounds();

    if (view || start_height_ != target_height_)
      state_ = State::CLEAR_ALL_VISIBLE;
    else
      state_ = State::IDLE;
  }
}

double UnifiedMessageListView::GetCurrentValue() const {
  gfx::Tween::Type tween;
  switch (state_) {
    case State::IDLE:
      // No animations are used for State::IDLE.
      NOTREACHED();
      tween = gfx::Tween::LINEAR;
      break;
    case State::CLEAR_ALL_STACKED:
    case State::MOVE_DOWN:
      tween = gfx::Tween::FAST_OUT_SLOW_IN;
      break;
    case State::CLEAR_ALL_VISIBLE:
      tween = gfx::Tween::EASE_IN;
      break;
    case State::EXPAND_OR_COLLAPSE:
      tween = gfx::Tween::FAST_OUT_SLOW_IN_3;
      break;
  }

  return gfx::Tween::CalculateValue(tween, animation_->GetCurrentValue());
}

}  // namespace ash
