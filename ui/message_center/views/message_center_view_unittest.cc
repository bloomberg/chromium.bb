// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_center_view.h"

#include <map>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/views/message_center_controller.h"
#include "ui/message_center/views/message_list_view.h"
#include "ui/message_center/views/notification_view.h"

namespace message_center {

static const char* kNotificationId1 = "notification id 1";
static const char* kNotificationId2 = "notification id 2";

/* Types **********************************************************************/

enum CallType {
  GET_PREFERRED_SIZE,
  GET_HEIGHT_FOR_WIDTH,
  LAYOUT
};

/* Instrumented/Mock NotificationView subclass ********************************/

class MockNotificationView : public NotificationView {
 public:
  class Test {
   public:
    virtual void RegisterCall(CallType type) = 0;
  };

  explicit MockNotificationView(MessageCenterController* controller,
                                const Notification& notification,
                                Test* test);
  ~MockNotificationView() override;

  gfx::Size GetPreferredSize() const override;
  int GetHeightForWidth(int w) const override;
  void Layout() override;

 private:
  Test* test_;

  DISALLOW_COPY_AND_ASSIGN(MockNotificationView);
};

MockNotificationView::MockNotificationView(MessageCenterController* controller,
                                           const Notification& notification,
                                           Test* test)
    : NotificationView(controller, notification),
      test_(test) {
}

MockNotificationView::~MockNotificationView() {
}

gfx::Size MockNotificationView::GetPreferredSize() const {
  test_->RegisterCall(GET_PREFERRED_SIZE);
  DCHECK(child_count() > 0);
  return NotificationView::GetPreferredSize();
}

int MockNotificationView::GetHeightForWidth(int width) const {
  test_->RegisterCall(GET_HEIGHT_FOR_WIDTH);
  DCHECK(child_count() > 0);
  return NotificationView::GetHeightForWidth(width);
}

void MockNotificationView::Layout() {
  test_->RegisterCall(LAYOUT);
  DCHECK(child_count() > 0);
  NotificationView::Layout();
}

class FakeMessageCenterImpl : public FakeMessageCenter {
 public:
  const NotificationList::Notifications& GetVisibleNotifications() override {
    return visible_notifications_;
  }
  void SetVisibleNotifications(NotificationList::Notifications notifications) {
    visible_notifications_ = notifications;
  }
  NotificationList::Notifications visible_notifications_;
};

/* Test fixture ***************************************************************/

class MessageCenterViewTest : public testing::Test,
                              public MockNotificationView::Test,
                              public MessageCenterController {
 public:
  MessageCenterViewTest();
  ~MessageCenterViewTest() override;

  void SetUp() override;
  void TearDown() override;

  MessageCenterView* GetMessageCenterView();
  MessageListView* GetMessageListView();
  NotificationView* GetNotificationView(const std::string& id);
  views::BoundsAnimator* GetAnimator();
  int GetNotificationCount();
  int GetCallCount(CallType type);
  int GetCalculatedMessageListViewHeight();
  void AddNotification(scoped_ptr<Notification> notification);
  void UpdateNotification(const std::string& notification_id,
                          scoped_ptr<Notification> notification);

 // Overridden from MessageCenterController:
  void ClickOnNotification(const std::string& notification_id) override;
  void RemoveNotification(const std::string& notification_id,
                          bool by_user) override;
  scoped_ptr<ui::MenuModel> CreateMenuModel(
      const NotifierId& notifier_id,
      const base::string16& display_source) override;
  bool HasClickedListener(const std::string& notification_id) override;
  void ClickOnNotificationButton(const std::string& notification_id,
                                 int button_index) override;
  void ClickOnSettingsButton(const std::string& notification_id) override;

  // Overridden from MockNotificationView::Test
  void RegisterCall(CallType type) override;

  void FireOnMouseExitedEvent();

  void LogBounds(int depth, views::View* view);

 private:
  views::View* MakeParent(views::View* child1, views::View* child2);

  base::MessageLoopForUI message_loop_;

  NotificationList::Notifications notifications_;
  scoped_ptr<MessageCenterView> message_center_view_;
  FakeMessageCenterImpl message_center_;
  std::map<CallType,int> callCounts_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterViewTest);
};

MessageCenterViewTest::MessageCenterViewTest() {
}

MessageCenterViewTest::~MessageCenterViewTest() {
}

void MessageCenterViewTest::SetUp() {
  // Create a dummy notification.
  Notification* notification1 = new Notification(
      NOTIFICATION_TYPE_SIMPLE, std::string(kNotificationId1),
      base::UTF8ToUTF16("title"), base::UTF8ToUTF16("message1"), gfx::Image(),
      base::UTF8ToUTF16("display source"), GURL(),
      NotifierId(NotifierId::APPLICATION, "extension_id"),
      message_center::RichNotificationData(), NULL);

  Notification* notification2 = new Notification(
      NOTIFICATION_TYPE_SIMPLE, std::string(kNotificationId2),
      base::UTF8ToUTF16("title2"), base::UTF8ToUTF16("message2"), gfx::Image(),
      base::UTF8ToUTF16("display source"), GURL(),
      NotifierId(NotifierId::APPLICATION, "extension_id"),
      message_center::RichNotificationData(), NULL);

  // ...and a list for it.
  notifications_.insert(notification1);
  notifications_.insert(notification2);
  message_center_.SetVisibleNotifications(notifications_);

  // Then create a new MessageCenterView with that single notification.
  base::string16 title;
  message_center_view_.reset(new MessageCenterView(
      &message_center_, NULL, 100, false, /*top_down =*/false, title));
  GetMessageListView()->quit_message_loop_after_animation_for_test_ = true;
  GetMessageCenterView()->SetBounds(0, 0, 380, 600);
  message_center_view_->SetNotifications(notifications_);

  // Wait until the animation finishes if available.
  if (GetAnimator()->IsAnimating())
    base::MessageLoop::current()->Run();
}

void MessageCenterViewTest::TearDown() {
  message_center_view_.reset();
  STLDeleteElements(&notifications_);
}

MessageCenterView* MessageCenterViewTest::GetMessageCenterView() {
  return message_center_view_.get();
}

MessageListView* MessageCenterViewTest::GetMessageListView() {
  return message_center_view_->message_list_view_.get();
}

NotificationView* MessageCenterViewTest::GetNotificationView(
    const std::string& id) {
  return message_center_view_->notification_views_[id];
}

int MessageCenterViewTest::GetCalculatedMessageListViewHeight() {
  return GetMessageListView()->GetHeightForWidth(GetMessageListView()->width());
}

views::BoundsAnimator* MessageCenterViewTest::GetAnimator() {
  return &GetMessageListView()->animator_;
}

int MessageCenterViewTest::GetNotificationCount() {
  return 2;
}

int MessageCenterViewTest::GetCallCount(CallType type) {
  return callCounts_[type];
}

void MessageCenterViewTest::ClickOnNotification(
    const std::string& notification_id) {
  // For this test, this method should not be invoked.
  NOTREACHED();
}

void MessageCenterViewTest::AddNotification(
    scoped_ptr<Notification> notification) {
  std::string notification_id = notification->id();
  notifications_.insert(notification.release());
  message_center_.SetVisibleNotifications(notifications_);
  message_center_view_->OnNotificationAdded(notification_id);
}

void MessageCenterViewTest::UpdateNotification(
    const std::string& notification_id,
    scoped_ptr<Notification> notification) {
  for (auto it = notifications_.begin(); it != notifications_.end(); it++) {
    if ((*it)->id() == notification_id) {
      delete *it;
      notifications_.erase(it);
      break;
    }
  }
  // |notifications| is a "set" container so we don't need to be aware the
  // order.
  notifications_.insert(notification.release());
  message_center_.SetVisibleNotifications(notifications_);
  message_center_view_->OnNotificationUpdated(notification_id);
}

void MessageCenterViewTest::RemoveNotification(
    const std::string& notification_id,
    bool by_user) {
  for (auto it = notifications_.begin(); it != notifications_.end(); it++) {
    if ((*it)->id() == notification_id) {
      delete *it;
      notifications_.erase(it);
      break;
    }
  }
  message_center_.SetVisibleNotifications(notifications_);
  message_center_view_->OnNotificationRemoved(notification_id, by_user);
}

scoped_ptr<ui::MenuModel> MessageCenterViewTest::CreateMenuModel(
    const NotifierId& notifier_id,
    const base::string16& display_source) {
  // For this test, this method should not be invoked.
  NOTREACHED();
  return nullptr;
}

bool MessageCenterViewTest::HasClickedListener(
    const std::string& notification_id) {
  return true;
}

void MessageCenterViewTest::ClickOnNotificationButton(
    const std::string& notification_id,
    int button_index) {
  // For this test, this method should not be invoked.
  NOTREACHED();
}

void MessageCenterViewTest::ClickOnSettingsButton(
    const std::string& notification_id) {
  // For this test, this method should not be invoked.
  NOTREACHED();
}

void MessageCenterViewTest::RegisterCall(CallType type) {
  callCounts_[type] += 1;
}

void MessageCenterViewTest::FireOnMouseExitedEvent() {
  ui::MouseEvent dummy_event(ui::ET_MOUSE_EXITED /* type */,
                             gfx::Point(0,0) /* location */,
                             gfx::Point(0,0) /* root location */,
                             base::TimeDelta() /* time_stamp */,
                             0 /* flags */,
                             0 /*changed_button_flags */);
  message_center_view_->OnMouseExited(dummy_event);
}

void MessageCenterViewTest::LogBounds(int depth, views::View* view) {
  base::string16 inset;
  for (int i = 0; i < depth; ++i)
    inset.append(base::UTF8ToUTF16("  "));
  gfx::Rect bounds = view->bounds();
  DVLOG(0) << inset << bounds.width() << " x " << bounds.height()
           << " @ " << bounds.x() << ", " << bounds.y();
  for (int i = 0; i < view->child_count(); ++i)
    LogBounds(depth + 1, view->child_at(i));
}

/* Unit tests *****************************************************************/

TEST_F(MessageCenterViewTest, CallTest) {
  // Verify that this didn't generate more than 2 Layout() call per descendant
  // NotificationView or more than a total of 20 GetPreferredSize() and
  // GetHeightForWidth() calls per descendant NotificationView. 20 is a very
  // large number corresponding to the current reality. That number will be
  // ratcheted down over time as the code improves.
  EXPECT_LE(GetCallCount(LAYOUT), GetNotificationCount() * 2);
  EXPECT_LE(GetCallCount(GET_PREFERRED_SIZE) +
            GetCallCount(GET_HEIGHT_FOR_WIDTH),
            GetNotificationCount() * 20);
}

TEST_F(MessageCenterViewTest, Size) {
  EXPECT_EQ(2, GetMessageListView()->child_count());
  EXPECT_EQ(GetMessageListView()->height(),
            GetCalculatedMessageListViewHeight());

  int width =
      GetMessageListView()->width() - GetMessageListView()->GetInsets().width();
  EXPECT_EQ(
      GetMessageListView()->height(),
      GetNotificationView(kNotificationId1)->GetHeightForWidth(width) +
          (kMarginBetweenItems - MessageView::GetShadowInsets().bottom()) +
          GetNotificationView(kNotificationId2)->GetHeightForWidth(width) +
          GetMessageListView()->GetInsets().height());
}

TEST_F(MessageCenterViewTest, SizeAfterUpdate) {
  EXPECT_EQ(2, GetMessageListView()->child_count());
  int width =
      GetMessageListView()->width() - GetMessageListView()->GetInsets().width();

  scoped_ptr<Notification> notification(new Notification(
      NOTIFICATION_TYPE_SIMPLE, std::string(kNotificationId2),
      base::UTF8ToUTF16("title2"),
      base::UTF8ToUTF16("message\nwhich\nis\nvertically\nlong\n."),
      gfx::Image(), base::UTF8ToUTF16("display source"), GURL(),
      NotifierId(NotifierId::APPLICATION, "extension_id"),
      message_center::RichNotificationData(), NULL));

  EXPECT_EQ(
      GetMessageListView()->height(),
      GetNotificationView(kNotificationId1)->GetHeightForWidth(width) +
          (kMarginBetweenItems - MessageView::GetShadowInsets().bottom()) +
          GetNotificationView(kNotificationId2)->GetHeightForWidth(width) +
          GetMessageListView()->GetInsets().height());

  int previous_height = GetMessageListView()->height();

  UpdateNotification(kNotificationId2, std::move(notification));

  // Wait until the animation finishes if available.
  if (GetAnimator()->IsAnimating())
    base::MessageLoop::current()->Run();

  EXPECT_EQ(2, GetMessageListView()->child_count());
  EXPECT_EQ(GetMessageListView()->height(),
            GetCalculatedMessageListViewHeight());

  // The size must be changed, since the new string is longer than the old one.
  EXPECT_NE(previous_height, GetMessageListView()->height());

  EXPECT_EQ(
      GetMessageListView()->height(),
      GetNotificationView(kNotificationId1)->GetHeightForWidth(width) +
          (kMarginBetweenItems - MessageView::GetShadowInsets().bottom()) +
          GetNotificationView(kNotificationId2)->GetHeightForWidth(width) +
          GetMessageListView()->GetInsets().height());
}

TEST_F(MessageCenterViewTest, SizeAfterRemove) {
  EXPECT_EQ(2, GetMessageListView()->child_count());
  RemoveNotification(kNotificationId1, false);

  // Wait until the animation finishes if available.
  if (GetAnimator()->IsAnimating())
    base::MessageLoop::current()->Run();

  EXPECT_EQ(1, GetMessageListView()->child_count());

  int width =
      GetMessageListView()->width() - GetMessageListView()->GetInsets().width();
  EXPECT_FALSE(GetNotificationView(kNotificationId1));
  EXPECT_TRUE(GetNotificationView(kNotificationId2));
  EXPECT_EQ(GetMessageListView()->height(),
            GetNotificationView(kNotificationId2)->GetHeightForWidth(width) +
                GetMessageListView()->GetInsets().height());
}

TEST_F(MessageCenterViewTest, PositionAfterUpdate) {
  // Make sure that the notification 2 is placed above the notification 1.
  EXPECT_LT(GetNotificationView(kNotificationId2)->bounds().y(),
            GetNotificationView(kNotificationId1)->bounds().y());

  int previous_vertical_pos_from_bottom =
      GetMessageListView()->height() -
      GetNotificationView(kNotificationId1)->bounds().y();
  GetMessageListView()->SetRepositionTargetForTest(
      GetNotificationView(kNotificationId1)->bounds());

  scoped_ptr<Notification> notification(new Notification(
      NOTIFICATION_TYPE_SIMPLE, std::string(kNotificationId2),
      base::UTF8ToUTF16("title2"),
      base::UTF8ToUTF16("message\nwhich\nis\nvertically\nlong\n."),
      gfx::Image(), base::UTF8ToUTF16("display source"), GURL(),
      NotifierId(NotifierId::APPLICATION, "extension_id"),
      message_center::RichNotificationData(), NULL));
  UpdateNotification(kNotificationId2, std::move(notification));

  // Wait until the animation finishes if available.
  if (GetAnimator()->IsAnimating())
    base::MessageLoop::current()->Run();

  // The vertical position of the target from bottom should be kept over change.
  int current_vertical_pos_from_bottom =
      GetMessageListView()->height() -
      GetNotificationView(kNotificationId1)->bounds().y();
  EXPECT_EQ(previous_vertical_pos_from_bottom,
            current_vertical_pos_from_bottom);
}

TEST_F(MessageCenterViewTest, PositionAfterRemove) {
  // Make sure that the notification 2 is placed above the notification 1.
  EXPECT_LT(GetNotificationView(kNotificationId2)->bounds().y(),
            GetNotificationView(kNotificationId1)->bounds().y());

  GetMessageListView()->SetRepositionTargetForTest(
      GetNotificationView(kNotificationId2)->bounds());
  int previous_height = GetMessageListView()->height();
  int previous_notification2_y =
      GetNotificationView(kNotificationId2)->bounds().y();
  int previous_notification2_height =
      GetNotificationView(kNotificationId2)->bounds().height();

  EXPECT_EQ(2, GetMessageListView()->child_count());
  RemoveNotification(kNotificationId2, false);

  // Wait until the animation finishes if available.
  if (GetAnimator()->IsAnimating())
    base::MessageLoop::current()->Run();

  EXPECT_EQ(1, GetMessageListView()->child_count());

  // Confirm that notification 1 is moved up to the place on which the
  // notification 2 was.
  EXPECT_EQ(previous_notification2_y,
            GetNotificationView(kNotificationId1)->bounds().y());
  // The size should be kept.
  EXPECT_EQ(previous_height, GetMessageListView()->height());

  // Notification 2 is successfully removed.
  EXPECT_FALSE(GetNotificationView(kNotificationId2));
  EXPECT_TRUE(GetNotificationView(kNotificationId1));

  // Mouse cursor moves out of the message center. This resets the reposition
  // target in the message list.
  FireOnMouseExitedEvent();

  // The height should shrink from the height of the removed notification 2.
  EXPECT_EQ(previous_height - previous_notification2_height -
                (kMarginBetweenItems - MessageView::GetShadowInsets().bottom()),
            GetMessageListView()->height());
}

}  // namespace message_center
