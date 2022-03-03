// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/unified_message_list_view.h"

#include "ash/constants/ash_features.h"
#include "ash/system/message_center/message_center_constants.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_view.h"

using message_center::MessageCenter;
using message_center::MessageView;
using message_center::Notification;

namespace ash {

namespace {

class TestNotificationView : public message_center::NotificationView {
 public:
  TestNotificationView(const message_center::Notification& notification)
      : NotificationView(notification) {
    layer()->GetAnimator()->set_preemption_strategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  }

  TestNotificationView(const TestNotificationView&) = delete;
  TestNotificationView& operator=(const TestNotificationView&) = delete;

  ~TestNotificationView() override = default;

  // message_center::NotificationView:
  void UpdateCornerRadius(int top_radius, int bottom_radius) override {
    top_radius_ = top_radius;
    bottom_radius_ = bottom_radius;
    message_center::NotificationViewBase::UpdateCornerRadius(top_radius,
                                                             bottom_radius);
  }

  int top_radius() const { return top_radius_; }
  int bottom_radius() const { return bottom_radius_; }

 private:
  int top_radius_ = 0;
  int bottom_radius_ = 0;
};

class TestUnifiedMessageListView : public UnifiedMessageListView {
 public:
  explicit TestUnifiedMessageListView(UnifiedSystemTrayModel* model)
      : UnifiedMessageListView(nullptr, model) {}

  TestUnifiedMessageListView(const TestUnifiedMessageListView&) = delete;
  TestUnifiedMessageListView& operator=(const TestUnifiedMessageListView&) =
      delete;

  ~TestUnifiedMessageListView() override = default;

  void set_stacked_notification_count(int stacked_notification_count) {
    stacked_notifications_.clear();
    notification_id_list_.clear();
    for (int i = 0; i < stacked_notification_count; i++) {
      std::string id = base::NumberToString(0);
      auto notification = std::make_unique<Notification>(
          message_center::NOTIFICATION_TYPE_BASE_FORMAT, id, u"test title",
          u"test message", gfx::Image(), std::u16string() /* display_source */,
          GURL(), message_center::NotifierId(),
          message_center::RichNotificationData(),
          new message_center::NotificationDelegate());

      stacked_notifications_.push_back(notification.get());
      notification_id_list_.push_back(id);
    }
  }

  // UnifiedMessageListView:
  message_center::MessageView* CreateMessageView(
      const message_center::Notification& notification) override {
    auto* view = new TestNotificationView(notification);
    view->SetIsNested();
    return view;
  }

  std::vector<message_center::Notification*> GetStackedNotifications()
      const override {
    return stacked_notifications_;
  }

  std::vector<std::string> GetNonVisibleNotificationIdsInViewHierarchy()
      const override {
    return notification_id_list_;
  }

 private:
  std::vector<message_center::Notification*> stacked_notifications_;
  std::vector<std::string> notification_id_list_;
};

}  // namespace

// The base test class, has no params so tests with no params can inherit from
// this.
class UnifiedMessageListViewTest : public AshTestBase,
                                   public views::ViewObserver {
 public:
  UnifiedMessageListViewTest() = default;

  UnifiedMessageListViewTest(const UnifiedMessageListViewTest&) = delete;
  UnifiedMessageListViewTest& operator=(const UnifiedMessageListViewTest&) =
      delete;

  ~UnifiedMessageListViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    model_ = base::MakeRefCounted<UnifiedSystemTrayModel>(nullptr);
  }

  void TearDown() override {
    message_list_view_.reset();
    model_.reset();
    AshTestBase::TearDown();
  }

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* view) override {
    view->SetBoundsRect(gfx::Rect(view->GetPreferredSize()));
    view->Layout();
    ++size_changed_count_;
  }

 protected:
  std::string AddNotification(bool pinned = false, bool expandable = false) {
    std::string id = base::NumberToString(id_++);
    // Make the message long enough to be collapsible. Generated by SpaceIpsum.
    auto notification = std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_BASE_FORMAT, id,
        u"Message To Flight Control",
        expandable ? u"From this day forward, Flight Control will be known by "
                     u"two words: "
                     u"‘Tough’ and ‘Competent.’ Tough means we are forever "
                     u"accountable for "
                     u"what we do or what we fail to do. We will never again "
                     u"compromise our "
                     u"responsibilities. Every time we walk into Mission "
                     u"Control we will "
                     u"know what we stand for. Competent means we will never "
                     u"take anything "
                     u"for granted. We will never be found short in our "
                     u"knowledge and in "
                     u"our skills. Mission Control will be perfect."
                   : u"Hey Flight Control, who brought donuts?",
        gfx::Image(), std::u16string() /* display_source */, GURL(),
        message_center::NotifierId(), message_center::RichNotificationData(),
        new message_center::NotificationDelegate());
    notification->set_pinned(pinned);
    MessageCenter::Get()->AddNotification(std::move(notification));
    return id;
  }

  void OffsetNotificationTimestamp(const std::string& id,
                                   const int milliseconds) {
    MessageCenter::Get()->FindVisibleNotificationById(id)->set_timestamp(
        base::Time::Now() - base::Milliseconds(milliseconds));
  }

  void CreateMessageListView() {
    message_list_view_ =
        std::make_unique<TestUnifiedMessageListView>(model_.get());
    message_list_view_->Init();
    message_list_view_->AddObserver(this);
    OnViewPreferredSizeChanged(message_list_view_.get());
    size_changed_count_ = 0;
  }

  void DestroyMessageListView() { message_list_view_.reset(); }

  TestNotificationView* GetMessageViewAt(size_t index) const {
    return static_cast<TestNotificationView*>(
        message_list_view()->children()[index]->children()[1]);
  }

  gfx::Rect GetMessageViewBounds(size_t index) const {
    return message_list_view()->children()[index]->bounds();
  }

  void FinishSlideOutAnimation() { base::RunLoop().RunUntilIdle(); }

  void AnimateToMiddle() {
    EXPECT_TRUE(IsAnimating());
    message_list_view()->animation_->SetCurrentValue(0.5);
    message_list_view()->AnimationProgressed(
        message_list_view()->animation_.get());
  }

  void AnimateToEnd() { message_list_view()->animation_->End(); }

  void AnimateUntilIdle() {
    while (message_list_view()->animation_->is_animating())
      message_list_view()->animation_->End();
  }

  bool IsAnimating() { return message_list_view()->animation_->is_animating(); }

  TestUnifiedMessageListView* message_list_view() const {
    return message_list_view_.get();
  }

  int size_changed_count() const { return size_changed_count_; }

  ui::LayerAnimator* LayerAnimatorAt(int i) {
    return GetMessageViewAt(i)->layer()->GetAnimator();
  }

 private:
  int id_ = 0;
  int size_changed_count_ = 0;

  scoped_refptr<UnifiedSystemTrayModel> model_;
  std::unique_ptr<TestUnifiedMessageListView> message_list_view_;
};

// Tests with NotificationsRefresh enabled and disabled.
class ParameterizedUnifiedMessageListViewTest
    : public UnifiedMessageListViewTest,
      public testing::WithParamInterface<bool> {
 public:
  ParameterizedUnifiedMessageListViewTest() = default;

  ParameterizedUnifiedMessageListViewTest(
      const ParameterizedUnifiedMessageListViewTest&) = delete;
  ParameterizedUnifiedMessageListViewTest& operator=(
      const ParameterizedUnifiedMessageListViewTest&) = delete;

  ~ParameterizedUnifiedMessageListViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatureState(features::kNotificationsRefresh,
                                               IsNotificationsRefreshEnabled());

    UnifiedMessageListViewTest::SetUp();
  }

  bool IsNotificationsRefreshEnabled() const { return GetParam(); }

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ParameterizedUnifiedMessageListViewTest,
                         testing::Bool() /* IsNotificationsRefreshEnabled() */);

TEST_P(ParameterizedUnifiedMessageListViewTest, Open) {
  auto id0 = AddNotification();
  auto id1 = AddNotification();
  auto id2 = AddNotification();
  CreateMessageListView();

  EXPECT_EQ(3u, message_list_view()->children().size());
  EXPECT_EQ(id0, GetMessageViewAt(0)->notification_id());
  EXPECT_EQ(id1, GetMessageViewAt(1)->notification_id());
  EXPECT_EQ(id2, GetMessageViewAt(2)->notification_id());

  EXPECT_FALSE(GetMessageViewAt(0)->IsExpanded());
  EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(2)->IsExpanded());

  // Check the position of notifications within the list. When the new feature
  // is enabled, we have extra spacing between notifications.
  if (IsNotificationsRefreshEnabled()) {
    EXPECT_EQ(
        GetMessageViewBounds(0).bottom() + kMessageListNotificationSpacing,
        GetMessageViewBounds(1).y());
    EXPECT_EQ(
        GetMessageViewBounds(1).bottom() + kMessageListNotificationSpacing,
        GetMessageViewBounds(2).y());
  } else {
    EXPECT_EQ(GetMessageViewBounds(0).bottom(), GetMessageViewBounds(1).y());
    EXPECT_EQ(GetMessageViewBounds(1).bottom(), GetMessageViewBounds(2).y());
  }

  EXPECT_EQ(0, GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(0, GetMessageViewAt(1)->top_radius());
  EXPECT_EQ(0, GetMessageViewAt(2)->top_radius());

  EXPECT_EQ(0, GetMessageViewAt(0)->bottom_radius());
  EXPECT_EQ(0, GetMessageViewAt(1)->bottom_radius());

  // Check rounded corners when the feature is not enabled (when the feature is
  // enabled we round corners in the scroll view).
  if (!IsNotificationsRefreshEnabled())
    EXPECT_EQ(kUnifiedTrayCornerRadius, GetMessageViewAt(2)->bottom_radius());

  EXPECT_LT(0, message_list_view()->GetPreferredSize().height());
}

TEST_P(ParameterizedUnifiedMessageListViewTest, AddNotifications) {
  CreateMessageListView();
  EXPECT_EQ(0, message_list_view()->GetPreferredSize().height());

  auto id0 = AddNotification();
  EXPECT_EQ(1, size_changed_count());
  EXPECT_EQ(1u, message_list_view()->children().size());
  EXPECT_EQ(id0, GetMessageViewAt(0)->notification_id());

  // Check rounded corners when the feature is not enabled (when the feature is
  // enabled we round corners in the scroll view).
  if (!IsNotificationsRefreshEnabled()) {
    EXPECT_EQ(kUnifiedTrayCornerRadius, GetMessageViewAt(0)->top_radius());
    EXPECT_EQ(kUnifiedTrayCornerRadius, GetMessageViewAt(0)->bottom_radius());
  }

  int previous_message_list_view_height =
      message_list_view()->GetPreferredSize().height();
  EXPECT_LT(0, previous_message_list_view_height);

  gfx::Rect previous_bounds = GetMessageViewBounds(0);
  auto id1 = AddNotification();
  EXPECT_EQ(2, size_changed_count());
  EXPECT_EQ(2u, message_list_view()->children().size());
  EXPECT_EQ(id1, GetMessageViewAt(1)->notification_id());

  EXPECT_LT(previous_message_list_view_height,
            message_list_view()->GetPreferredSize().height());

  if (!IsNotificationsRefreshEnabled()) {
    // 1dip larger because now it has separator border.
    previous_bounds.Inset(gfx::Insets(0, 0, -1, 0));
  }
  EXPECT_EQ(previous_bounds, GetMessageViewBounds(0));

  // When the new feature is enabled, we have extra spacing between
  // notifications.
  if (IsNotificationsRefreshEnabled()) {
    EXPECT_EQ(
        GetMessageViewBounds(0).bottom() + kMessageListNotificationSpacing,
        GetMessageViewBounds(1).y());
  } else {
    EXPECT_EQ(GetMessageViewBounds(0).bottom(), GetMessageViewBounds(1).y());
  }

  // The top radius is zero because:
  // - We round corners in the scroll view when the feature is enabled.
  // - The stacking bar is shown when the feature is disabled.
  EXPECT_EQ(0, GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(0, GetMessageViewAt(1)->top_radius());

  EXPECT_EQ(0, GetMessageViewAt(0)->bottom_radius());
  // Check rounded corners when the feature is not enabled (when the feature is
  // enabled, we round corners in the scroll view).
  if (!IsNotificationsRefreshEnabled())
    EXPECT_EQ(kUnifiedTrayCornerRadius, GetMessageViewAt(1)->bottom_radius());
}

TEST_P(ParameterizedUnifiedMessageListViewTest, RemoveNotification) {
  auto id0 = AddNotification();
  auto id1 = AddNotification();

  CreateMessageListView();
  int previous_height = message_list_view()->GetPreferredSize().height();

  EXPECT_EQ(2u, message_list_view()->children().size());
  EXPECT_EQ(0, GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(0, GetMessageViewAt(0)->bottom_radius());

  gfx::Rect previous_bounds = GetMessageViewBounds(0);
  MessageCenter::Get()->RemoveNotification(id0, true /* by_user */);
  FinishSlideOutAnimation();
  AnimateUntilIdle();
  EXPECT_EQ(1u, message_list_view()->children().size());
  EXPECT_EQ(previous_bounds.y(), GetMessageViewBounds(0).y());
  EXPECT_LT(0, message_list_view()->GetPreferredSize().height());
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());

  // Check rounded corners when the feature is not enabled (when the feature is
  // enabled, we round corners in the scroll view).
  if (!IsNotificationsRefreshEnabled()) {
    EXPECT_EQ(kUnifiedTrayCornerRadius, GetMessageViewAt(0)->top_radius());
    EXPECT_EQ(kUnifiedTrayCornerRadius, GetMessageViewAt(0)->bottom_radius());
  }

  MessageCenter::Get()->RemoveNotification(id1, true /* by_user */);
  FinishSlideOutAnimation();
  AnimateUntilIdle();
  EXPECT_EQ(0u, message_list_view()->children().size());
  EXPECT_EQ(0, message_list_view()->GetPreferredSize().height());
}

TEST_P(ParameterizedUnifiedMessageListViewTest, CollapseOlderNotifications) {
  AddNotification();
  CreateMessageListView();
  EXPECT_TRUE(GetMessageViewAt(0)->IsExpanded());

  AddNotification();
  EXPECT_FALSE(GetMessageViewAt(0)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(1)->IsExpanded());

  AddNotification();
  EXPECT_FALSE(GetMessageViewAt(0)->IsExpanded());
  EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(2)->IsExpanded());

  GetMessageViewAt(1)->SetExpanded(true);
  GetMessageViewAt(1)->SetManuallyExpandedOrCollapsed(true);

  AddNotification();
  EXPECT_FALSE(GetMessageViewAt(0)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(1)->IsExpanded());
  EXPECT_FALSE(GetMessageViewAt(2)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(3)->IsExpanded());
}

TEST_P(ParameterizedUnifiedMessageListViewTest, RemovingNotificationAnimation) {
  auto id0 = AddNotification(/*pinned=*/false);
  auto id1 = AddNotification();
  auto id2 = AddNotification();
  CreateMessageListView();
  int previous_height = message_list_view()->GetPreferredSize().height();
  gfx::Rect bounds0 = GetMessageViewBounds(0);
  gfx::Rect bounds1 = GetMessageViewBounds(1);

  MessageCenter::Get()->RemoveNotification(id1, true /* by_user */);
  FinishSlideOutAnimation();
  AnimateToEnd();
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  previous_height = message_list_view()->GetPreferredSize().height();
  if (!IsNotificationsRefreshEnabled()) {
    // Now it lost separator border.
    bounds1.Inset(gfx::Insets(0, 0, 1, 0));
  }
  EXPECT_EQ(bounds0, GetMessageViewBounds(0));
  EXPECT_EQ(bounds1, GetMessageViewBounds(1));

  MessageCenter::Get()->RemoveNotification(id2, true /* by_user */);
  FinishSlideOutAnimation();
  AnimateToEnd();
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  previous_height = message_list_view()->GetPreferredSize().height();
  if (!IsNotificationsRefreshEnabled()) {
    // Now it lost separator border.
    bounds0.Inset(gfx::Insets(0, 0, 1, 0));
  }
  EXPECT_EQ(bounds0, GetMessageViewBounds(0));

  MessageCenter::Get()->RemoveNotification(id0, true /* by_user */);
  FinishSlideOutAnimation();
  AnimateToEnd();
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  previous_height = message_list_view()->GetPreferredSize().height();

  EXPECT_EQ(0, message_list_view()->GetPreferredSize().height());
}

TEST_P(ParameterizedUnifiedMessageListViewTest, ResetAnimation) {
  auto id0 = AddNotification();
  auto id1 = AddNotification();
  CreateMessageListView();

  MessageCenter::Get()->RemoveNotification(id0, true /* by_user */);
  FinishSlideOutAnimation();
  EXPECT_TRUE(IsAnimating());
  AnimateToMiddle();

  // New event resets the animation.
  auto id2 = AddNotification();
  EXPECT_FALSE(IsAnimating());

  EXPECT_EQ(2u, message_list_view()->children().size());
  EXPECT_EQ(id1, GetMessageViewAt(0)->notification_id());
  EXPECT_EQ(id2, GetMessageViewAt(1)->notification_id());
}

TEST_P(ParameterizedUnifiedMessageListViewTest, KeepManuallyExpanded) {
  AddNotification();
  AddNotification();
  CreateMessageListView();

  EXPECT_FALSE(GetMessageViewAt(0)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(1)->IsExpanded());
  EXPECT_FALSE(GetMessageViewAt(0)->IsManuallyExpandedOrCollapsed());
  EXPECT_FALSE(GetMessageViewAt(1)->IsManuallyExpandedOrCollapsed());

  // Manually expand the first notification & manually collapse the second one.
  GetMessageViewAt(0)->SetExpanded(true);
  GetMessageViewAt(0)->SetManuallyExpandedOrCollapsed(true);
  GetMessageViewAt(1)->SetExpanded(false);
  GetMessageViewAt(1)->SetManuallyExpandedOrCollapsed(true);

  DestroyMessageListView();

  // Reopen and confirm the expanded state & manually expanded flags are kept.
  CreateMessageListView();
  EXPECT_TRUE(GetMessageViewAt(0)->IsExpanded());
  EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(0)->IsManuallyExpandedOrCollapsed());
  EXPECT_TRUE(GetMessageViewAt(1)->IsManuallyExpandedOrCollapsed());

  DestroyMessageListView();

  // Add a new notification.
  AddNotification();
  CreateMessageListView();

  // Confirm the new notification isn't affected & others are still kept.
  EXPECT_TRUE(GetMessageViewAt(0)->IsExpanded());
  EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(2)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(0)->IsManuallyExpandedOrCollapsed());
  EXPECT_TRUE(GetMessageViewAt(1)->IsManuallyExpandedOrCollapsed());
  EXPECT_FALSE(GetMessageViewAt(2)->IsManuallyExpandedOrCollapsed());
}

TEST_P(ParameterizedUnifiedMessageListViewTest,
       ClearAllWithOnlyVisibleNotifications) {
  AddNotification();
  AddNotification();
  CreateMessageListView();

  EXPECT_EQ(2u, message_list_view()->children().size());
  int previous_height = message_list_view()->GetPreferredSize().height();
  gfx::Rect previous_bounds = GetMessageViewBounds(0);

  message_list_view()->ClearAllWithAnimation();
  AnimateToMiddle();
  EXPECT_LT(previous_bounds.x(), GetMessageViewBounds(0).x());
  EXPECT_EQ(previous_height, message_list_view()->GetPreferredSize().height());

  AnimateToEnd();
  EXPECT_EQ(1u, message_list_view()->children().size());
  EXPECT_EQ(previous_height, message_list_view()->GetPreferredSize().height());

  previous_bounds = GetMessageViewBounds(0);
  AnimateToMiddle();
  EXPECT_LT(previous_bounds.x(), GetMessageViewBounds(0).x());
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  previous_height = message_list_view()->GetPreferredSize().height();

  AnimateToEnd();
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  previous_height = message_list_view()->GetPreferredSize().height();
  EXPECT_EQ(0u, message_list_view()->children().size());

  AnimateToMiddle();
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  previous_height = message_list_view()->GetPreferredSize().height();

  AnimateToEnd();
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  EXPECT_EQ(0, message_list_view()->GetPreferredSize().height());
  EXPECT_TRUE(MessageCenter::Get()->GetVisibleNotifications().empty());

  EXPECT_FALSE(IsAnimating());
}

TEST_P(ParameterizedUnifiedMessageListViewTest,
       ClearAllWithStackingNotifications) {
  AddNotification();
  AddNotification();
  AddNotification();
  CreateMessageListView();

  message_list_view()->set_stacked_notification_count(2);
  EXPECT_EQ(3u, message_list_view()->children().size());

  message_list_view()->ClearAllWithAnimation();
  EXPECT_EQ(2u, message_list_view()->children().size());

  message_list_view()->set_stacked_notification_count(1);
  int previous_height = message_list_view()->GetPreferredSize().height();
  gfx::Rect previous_bounds = GetMessageViewBounds(1);
  AnimateToMiddle();
  EXPECT_EQ(previous_height, message_list_view()->GetPreferredSize().height());
  EXPECT_EQ(previous_bounds, GetMessageViewBounds(1));
  AnimateToEnd();
  EXPECT_EQ(1u, message_list_view()->children().size());

  message_list_view()->set_stacked_notification_count(0);
  previous_height = message_list_view()->GetPreferredSize().height();
  AnimateToMiddle();
  EXPECT_EQ(previous_height, message_list_view()->GetPreferredSize().height());
  AnimateToEnd();
  EXPECT_EQ(1u, message_list_view()->children().size());

  previous_bounds = GetMessageViewBounds(0);
  AnimateToMiddle();
  EXPECT_LT(previous_bounds.x(), GetMessageViewBounds(0).x());
  AnimateToEnd();
  EXPECT_EQ(0u, message_list_view()->children().size());

  previous_height = message_list_view()->GetPreferredSize().height();
  AnimateToMiddle();
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  previous_height = message_list_view()->GetPreferredSize().height();

  AnimateToEnd();
  EXPECT_EQ(0u, message_list_view()->children().size());
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  EXPECT_EQ(0, message_list_view()->GetPreferredSize().height());

  EXPECT_FALSE(IsAnimating());
}

TEST_P(ParameterizedUnifiedMessageListViewTest, ClearAllClosedInTheMiddle) {
  AddNotification();
  AddNotification();
  AddNotification();
  CreateMessageListView();

  message_list_view()->ClearAllWithAnimation();
  AnimateToMiddle();

  DestroyMessageListView();
  EXPECT_TRUE(MessageCenter::Get()->GetVisibleNotifications().empty());
}

TEST_P(ParameterizedUnifiedMessageListViewTest, ClearAllInterrupted) {
  AddNotification();
  AddNotification();
  AddNotification();
  CreateMessageListView();

  message_list_view()->ClearAllWithAnimation();
  AnimateToMiddle();
  auto new_id = AddNotification();

  EXPECT_EQ(1u, MessageCenter::Get()->GetVisibleNotifications().size());
  EXPECT_TRUE(MessageCenter::Get()->FindVisibleNotificationById(new_id));
}

TEST_P(ParameterizedUnifiedMessageListViewTest,
       ClearAllWithPinnedNotifications) {
  AddNotification(/*pinned=*/true);
  AddNotification();
  AddNotification();
  CreateMessageListView();

  message_list_view()->ClearAllWithAnimation();
  AnimateUntilIdle();
  EXPECT_EQ(1u, message_list_view()->children().size());
}

TEST_P(ParameterizedUnifiedMessageListViewTest, UserSwipesAwayNotification) {
  // Show message list with two notifications.
  AddNotification();
  auto id1 = AddNotification();
  CreateMessageListView();

  // Start swiping the notification away.
  GetMessageViewAt(1)->OnSlideStarted();
  GetMessageViewAt(1)->OnSlideChanged(true);
  EXPECT_EQ(2u, MessageCenter::Get()->GetVisibleNotifications().size());
  EXPECT_EQ(2u, message_list_view()->children().size());

  // Swiping away the notification should remove it both in the MessageCenter
  // and the MessageListView.
  MessageCenter::Get()->RemoveNotification(id1, true /* by_user */);
  FinishSlideOutAnimation();
  EXPECT_EQ(1u, MessageCenter::Get()->GetVisibleNotifications().size());
  EXPECT_EQ(1u, message_list_view()->children().size());

  // The next and only animation should be the move down animation.
  int previous_height = message_list_view()->GetPreferredSize().height();
  AnimateToEnd();
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());
  EXPECT_FALSE(message_list_view()->IsAnimating());
}

TEST_P(ParameterizedUnifiedMessageListViewTest, InitInSortedOrder) {
  // MessageViews should be ordered, from top down: [ id1, id2, id0 ].
  auto id0 = AddNotification(/*pinned=*/true);
  OffsetNotificationTimestamp(id0, 2000 /* milliseconds */);
  auto id1 = AddNotification();
  OffsetNotificationTimestamp(id1, 1000 /* milliseconds */);
  auto id2 = AddNotification();
  CreateMessageListView();

  EXPECT_EQ(3u, message_list_view()->children().size());
  EXPECT_EQ(id1, GetMessageViewAt(0)->notification_id());
  EXPECT_EQ(id2, GetMessageViewAt(1)->notification_id());
  EXPECT_EQ(id0, GetMessageViewAt(2)->notification_id());
}

TEST_P(ParameterizedUnifiedMessageListViewTest,
       NotificationAddedInSortedOrder) {
  auto id0 = AddNotification(/*pinned=*/true);
  OffsetNotificationTimestamp(id0, 3000 /* milliseconds */);
  auto id1 = AddNotification();
  OffsetNotificationTimestamp(id1, 2000 /* milliseconds */);
  auto id2 = AddNotification();
  OffsetNotificationTimestamp(id2, 1000 /* milliseconds */);
  CreateMessageListView();

  // New pinned notification should be added to the end.
  auto id3 = AddNotification(/*pinned=*/true);
  EXPECT_EQ(4u, message_list_view()->children().size());
  EXPECT_EQ(id3, GetMessageViewAt(3)->notification_id());

  // New non-pinned notification should be added before pinned notifications.
  auto id4 = AddNotification();
  EXPECT_EQ(5u, message_list_view()->children().size());

  EXPECT_EQ(id1, GetMessageViewAt(0)->notification_id());
  EXPECT_EQ(id2, GetMessageViewAt(1)->notification_id());
  EXPECT_EQ(id4, GetMessageViewAt(2)->notification_id());
  EXPECT_EQ(id0, GetMessageViewAt(3)->notification_id());
  EXPECT_EQ(id3, GetMessageViewAt(4)->notification_id());
}

// Tests only with NotificationsRefresh enabled.
class RefreshedUnifiedMessageListView : public UnifiedMessageListViewTest {
 public:
  RefreshedUnifiedMessageListView() = default;
  RefreshedUnifiedMessageListView(const RefreshedUnifiedMessageListView&) =
      delete;
  RefreshedUnifiedMessageListView& operator=(
      const RefreshedUnifiedMessageListView&) = delete;
  ~RefreshedUnifiedMessageListView() override = default;

  void SetUp() override {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndEnableFeature(features::kNotificationsRefresh);
    UnifiedMessageListViewTest::SetUp();
  }

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

// Tests that preferred size changes upon toggle of expand/collapse.
TEST_F(RefreshedUnifiedMessageListView, PreferredSizeChangesOnToggle) {
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  CreateMessageListView();
  auto* message_view = GetMessageViewAt(1);
  ASSERT_TRUE(message_view->IsExpanded());
  gfx::Size old_preferred_size =
      message_list_view()->children()[1]->GetPreferredSize();

  EXPECT_FALSE(IsAnimating());

  message_view->SetExpanded(/*expanded=*/false);

  EXPECT_TRUE(IsAnimating());
  EXPECT_TRUE(message_list_view()->IsAnimatingExpandOrCollapseContainer(
      message_list_view()->children()[1]));
  EXPECT_EQ(old_preferred_size.height(),
            message_list_view()->children()[1]->GetPreferredSize().height());

  old_preferred_size = message_list_view()->children()[1]->GetPreferredSize();
  AnimateToMiddle();

  EXPECT_GT(old_preferred_size.height(),
            message_list_view()->children()[1]->GetPreferredSize().height());

  AnimateToEnd();
  FinishSlideOutAnimation();
  EXPECT_FALSE(IsAnimating());
  EXPECT_FALSE(message_list_view()->IsAnimatingExpandOrCollapseContainer(
      message_list_view()->children()[1]));
}

// Tests that expanding a notification while a different notification is
// expanding is handled gracefully.
TEST_F(RefreshedUnifiedMessageListView, TwoExpandsInARow) {
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  CreateMessageListView();

  // First expand the notification in `first_notification_container`.
  auto* first_notification_container = message_list_view()->children()[0];
  auto* message_view = GetMessageViewAt(0);
  ASSERT_FALSE(message_view->IsExpanded());
  message_view->SetExpanded(/*expanded=*/true);
  AnimateToMiddle();
  const gfx::Size first_notification_middle_of_animation_size =
      first_notification_container->GetPreferredSize();

  // Collapse the second notification as `message_view` is still animating.
  auto* second_notification_container = message_list_view()->children()[1];
  const gfx::Size second_notification_initial_size =
      second_notification_container->GetPreferredSize();
  message_view = GetMessageViewAt(1);
  message_view->SetExpanded(/*expanded=*/false);

  EXPECT_TRUE(IsAnimating());
  EXPECT_FALSE(message_list_view()->IsAnimatingExpandOrCollapseContainer(
      first_notification_container));
  EXPECT_TRUE(message_list_view()->IsAnimatingExpandOrCollapseContainer(
      second_notification_container));
  // The originally animating container should have been snapped to its final
  // bounds.
  EXPECT_LT(first_notification_middle_of_animation_size.height(),
            first_notification_container->GetPreferredSize().height());

  AnimateToEnd();
  FinishSlideOutAnimation();

  // `second_notification_container` should animate to its final bounds.
  EXPECT_GT(second_notification_initial_size.height(),
            second_notification_container->GetPreferredSize().height());
}

// Tests that collapsing/expanding is reversible.
TEST_F(RefreshedUnifiedMessageListView, ReverseExpand) {
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  CreateMessageListView();
  auto* message_view = GetMessageViewAt(1);

  auto* second_notification_container = message_list_view()->children()[1];
  message_view->SetExpanded(/*expanded=*/false);
  AnimateToMiddle();
  const gfx::Size middle_of_collapsed_size =
      second_notification_container->GetPreferredSize();

  // Animate to expanded in the middle of the collapse animation. This should
  // stop the collapse animation and set the view to its final bounds, then
  // animate to expanded.
  message_view->SetExpanded(/*expanded=*/true);
  const gfx::Size final_collapsed_size =
      second_notification_container->GetPreferredSize();
  EXPECT_LT(final_collapsed_size.height(), middle_of_collapsed_size.height());

  // Animate to the end. The container view should be fully expanded.
  AnimateToEnd();
  EXPECT_LT(middle_of_collapsed_size.height(),
            second_notification_container->GetPreferredSize().height());
}

// Tests that destroying during a collapse animation does not crash.
TEST_F(RefreshedUnifiedMessageListView, DestroyMessageListViewDuringCollapse) {
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  CreateMessageListView();
  auto* message_view = GetMessageViewAt(1);
  message_view->SetExpanded(/*expanded=*/false);
  AnimateToMiddle();

  DestroyMessageListView();
}

// Tests that closing a notification while its collapse animation is ongoing
// works properly.
TEST_F(RefreshedUnifiedMessageListView, RemoveNotificationDuringCollapse) {
  auto id1 = AddNotification(/*pinned=*/false, /*expandable=*/true);
  CreateMessageListView();
  auto* message_view = GetMessageViewAt(0);
  message_view->SetExpanded(/*expanded=*/false);
  AnimateToMiddle();
  auto* notification_container = message_list_view()->children()[0];
  const gfx::Size middle_of_collapsed_size =
      notification_container->GetPreferredSize();

  // Remove the notification for `message_view`. The view should snap to
  // collapsed bounds, then slide out.
  MessageCenter::Get()->RemoveNotification(id1, /*by_user=*/true);

  EXPECT_LE(notification_container->GetPreferredSize().height(),
            middle_of_collapsed_size.height());
  FinishSlideOutAnimation();
  AnimateUntilIdle();

  EXPECT_EQ(0u, message_list_view()->children().size());
  EXPECT_EQ(0, message_list_view()->GetPreferredSize().height());
}

// Tests that expanding a notification at various stages while it is being
// closed does not result in an animation.
TEST_F(RefreshedUnifiedMessageListView,
       CollapseDuringCloseResultsInNoCollapseAnimation) {
  auto id1 = AddNotification(/*pinned=*/false, /*expandable=*/true);
  AddNotification(/*pinned=*/false, /*expandable=*/true);
  CreateMessageListView();

  auto* notification_container = message_list_view()->children()[0];
  const gfx::Size pre_remove_size = notification_container->GetPreferredSize();
  // Remove the notification, this should activate the "slide out" animation.
  MessageCenter::Get()->RemoveNotification(id1, /*by_user=*/true);
  EXPECT_EQ(notification_container->GetPreferredSize(), pre_remove_size);
  // Removing the notification does not trigger an animation at the level of
  // UnifiedMessageListView
  EXPECT_FALSE(message_list_view()->IsAnimating());
  EXPECT_FALSE(message_list_view()->IsAnimatingExpandOrCollapseContainer(
      notification_container));

  // Trigger the collapse before slide out completes, this should not trigger an
  // animation for UnifiedMessageListView, and no animation should occur.
  // SlideOut animation happens at a lower level. Also, size changes should be
  // ignored when being removed.
  GetMessageViewAt(0)->SetExpanded(/*expanded=*/false);
  EXPECT_FALSE(message_list_view()->IsAnimating());
  EXPECT_FALSE(message_list_view()->IsAnimatingExpandOrCollapseContainer(
      notification_container));
  EXPECT_EQ(pre_remove_size, notification_container->GetPreferredSize());

  // Finish the slide out animation. Then an animation should begin to shrink
  // MessageListView to contain the remaining notifications via
  // State::MOVE_DOWN. Only one notification should remain.
  FinishSlideOutAnimation();
  EXPECT_TRUE(message_list_view()->IsAnimating());
  EXPECT_EQ(1u, message_list_view()->children().size());
}

// Tests that collapsing a notification while it is being moved automatically
// completes both animations.
TEST_F(RefreshedUnifiedMessageListView, CollapseDuringMoveNoAnimation) {
  auto to_be_removed_notification =
      AddNotification(/*pinned=*/false, /*expandable=*/true);
  auto to_be_collapsed_notification =
      AddNotification(/*pinned=*/false, /*expandable=*/true);
  CreateMessageListView();
  auto* to_be_collapsed_message_view_container =
      message_list_view()->children()[1];
  auto* to_be_collapsed_message_view = GetMessageViewAt(1);
  const gfx::Size pre_collapse_size =
      to_be_collapsed_message_view_container->GetPreferredSize();
  ASSERT_TRUE(to_be_collapsed_message_view->IsExpanded());

  // Delete the first notification. This should begin the slide out animation.
  // Let that finish, then State::MOVE_DOWN should begin.
  MessageCenter::Get()->RemoveNotification(to_be_removed_notification,
                                           /*by_user=*/true);
  FinishSlideOutAnimation();
  EXPECT_TRUE(message_list_view()->IsAnimating());
  EXPECT_FALSE(message_list_view()->IsAnimatingExpandOrCollapseContainer(
      to_be_collapsed_message_view_container));

  // Animate to the middle, then attempt to collapse an existing notification.
  // All animations should complete.
  AnimateToMiddle();
  to_be_collapsed_message_view->SetExpanded(false);
  EXPECT_FALSE(message_list_view()->IsAnimating());
  EXPECT_FALSE(message_list_view()->IsAnimatingExpandOrCollapseContainer(
      to_be_collapsed_message_view_container));
  EXPECT_GT(
      pre_collapse_size.height(),
      to_be_collapsed_message_view_container->GetPreferredSize().height());
}

// Tests that moving a notification while it is already collapsing completes
// both animations.
TEST_F(RefreshedUnifiedMessageListView, MoveDuringCollapseNoAnimation) {
  auto to_be_removed_notification =
      AddNotification(/*pinned=*/false, /*expandable=*/true);
  auto to_be_collapsed_notification =
      AddNotification(/*pinned=*/false, /*expandable=*/true);
  CreateMessageListView();
  auto* to_be_collapsed_message_view_container =
      message_list_view()->children()[1];
  auto* to_be_collapsed_message_view = GetMessageViewAt(1);
  const gfx::Size pre_collapse_size =
      to_be_collapsed_message_view_container->GetPreferredSize();
  ASSERT_TRUE(to_be_collapsed_message_view->IsExpanded());

  // Collapse the second notification, then delete the first.
  to_be_collapsed_message_view->SetExpanded(false);
  AnimateToMiddle();
  EXPECT_TRUE(message_list_view()->IsAnimatingExpandOrCollapseContainer(
      to_be_collapsed_message_view_container));
  EXPECT_TRUE(message_list_view()->IsAnimating());
  MessageCenter::Get()->RemoveNotification(to_be_removed_notification,
                                           /*by_user=*/true);

  EXPECT_FALSE(message_list_view()->IsAnimatingExpandOrCollapseContainer(
      to_be_collapsed_message_view_container));
  EXPECT_FALSE(message_list_view()->IsAnimating());
  EXPECT_GT(
      pre_collapse_size.height(),
      to_be_collapsed_message_view_container->GetPreferredSize().height());
}

}  // namespace ash
