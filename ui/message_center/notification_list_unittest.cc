// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification_list.h"

#include "base/basictypes.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/notification_types.h"

namespace message_center {
namespace test {

class NotificationListTest : public testing::Test {
 public:
  NotificationListTest() {}
  virtual ~NotificationListTest() {}

  virtual void SetUp() {
    notification_list_.reset(new NotificationList());
    counter_ = 0;
  }

 protected:
  // Currently NotificationListTest doesn't care about some fields like title or
  // message, so put a simple template on it. Returns the id of the new
  // notification.
  std::string AddNotification(
      const message_center::RichNotificationData& optional_fields) {
    std::string new_id = base::StringPrintf(kIdFormat, counter_);
    scoped_ptr<Notification> notification(new Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        new_id,
        UTF8ToUTF16(base::StringPrintf(kTitleFormat, counter_)),
        UTF8ToUTF16(base::StringPrintf(kMessageFormat, counter_)),
        gfx::Image(),
        UTF8ToUTF16(kDisplaySource),
        kExtensionId,
        optional_fields,
        NULL));
    notification_list_->AddNotification(notification.Pass());
    counter_++;
    return new_id;
  }

  std::string AddNotification() {
    return AddNotification(message_center::RichNotificationData());
  }

  // Utility methods of AddNotification.
  std::string AddPriorityNotification(NotificationPriority priority) {
    message_center::RichNotificationData optional;
    optional.priority = priority;
    return AddNotification(optional);
  }

  size_t GetPopupCounts() {
    return notification_list()->GetPopupNotifications().size();
  }

  Notification* GetNotification(const std::string& id) {
    NotificationList::Notifications::iterator iter =
        notification_list()->GetNotification(id);
    if (iter == notification_list()->GetNotifications().end())
      return NULL;
    return *iter;
  }

  NotificationList* notification_list() { return notification_list_.get(); }

  static const char kIdFormat[];
  static const char kTitleFormat[];
  static const char kMessageFormat[];
  static const char kDisplaySource[];
  static const char kExtensionId[];

 private:
  scoped_ptr<NotificationList> notification_list_;
  size_t counter_;

  DISALLOW_COPY_AND_ASSIGN(NotificationListTest);
};

bool IsInNotifications(const NotificationList::Notifications& notifications,
                       const std::string& id) {
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    if ((*iter)->id() == id)
      return true;
  }
  return false;
}

const char NotificationListTest::kIdFormat[] = "id%ld";
const char NotificationListTest::kTitleFormat[] = "id%ld";
const char NotificationListTest::kMessageFormat[] = "message%ld";
const char NotificationListTest::kDisplaySource[] = "source";
const char NotificationListTest::kExtensionId[] = "ext";

TEST_F(NotificationListTest, Basic) {
  ASSERT_EQ(0u, notification_list()->NotificationCount());
  ASSERT_EQ(0u, notification_list()->unread_count());

  std::string id0 = AddNotification();
  EXPECT_EQ(1u, notification_list()->NotificationCount());
  std::string id1 = AddNotification();
  EXPECT_EQ(2u, notification_list()->NotificationCount());
  EXPECT_EQ(2u, notification_list()->unread_count());

  EXPECT_TRUE(notification_list()->HasPopupNotifications());
  EXPECT_TRUE(notification_list()->HasNotification(id0));
  EXPECT_TRUE(notification_list()->HasNotification(id1));
  EXPECT_FALSE(notification_list()->HasNotification(id1 + "foo"));

  EXPECT_EQ(2u, GetPopupCounts());

  notification_list()->MarkPopupsAsShown(0);
  EXPECT_EQ(2u, notification_list()->NotificationCount());
  EXPECT_EQ(0u, GetPopupCounts());

  notification_list()->RemoveNotification(id0);
  EXPECT_EQ(1u, notification_list()->NotificationCount());
  EXPECT_EQ(1u, notification_list()->unread_count());

  AddNotification();
  EXPECT_EQ(2u, notification_list()->NotificationCount());

  notification_list()->RemoveAllNotifications();
  EXPECT_EQ(0u, notification_list()->NotificationCount());
  EXPECT_EQ(0u, notification_list()->unread_count());
}

TEST_F(NotificationListTest, MessageCenterVisible) {
  AddNotification();
  EXPECT_EQ(1u, notification_list()->NotificationCount());
  ASSERT_EQ(1u, notification_list()->unread_count());
  ASSERT_EQ(1u, GetPopupCounts());

  // Make the message center visible. It resets the unread count and popup
  // counts.
  notification_list()->SetMessageCenterVisible(true, NULL);
  ASSERT_EQ(0u, notification_list()->unread_count());
  ASSERT_EQ(0u, GetPopupCounts());
}

TEST_F(NotificationListTest, UnreadCount) {
  std::string id0 = AddNotification();
  std::string id1 = AddNotification();
  ASSERT_EQ(2u, notification_list()->unread_count());

  notification_list()->MarkSinglePopupAsDisplayed(id0);
  EXPECT_EQ(1u, notification_list()->unread_count());
  notification_list()->MarkSinglePopupAsDisplayed(id0);
  EXPECT_EQ(1u, notification_list()->unread_count());
  notification_list()->MarkSinglePopupAsDisplayed(id1);
  EXPECT_EQ(0u, notification_list()->unread_count());
}

TEST_F(NotificationListTest, UpdateNotification) {
  std::string id0 = AddNotification();
  std::string replaced = id0 + "_replaced";
  EXPECT_EQ(1u, notification_list()->NotificationCount());
  scoped_ptr<Notification> notification(
      new Notification(message_center::NOTIFICATION_TYPE_SIMPLE,
                       replaced,
                       UTF8ToUTF16("newtitle"),
                       UTF8ToUTF16("newbody"),
                       gfx::Image(),
                       UTF8ToUTF16(kDisplaySource),
                       kExtensionId,
                       message_center::RichNotificationData(),
                       NULL));
  notification_list()->UpdateNotificationMessage(id0, notification.Pass());
  EXPECT_EQ(1u, notification_list()->NotificationCount());
  const NotificationList::Notifications& notifications =
      notification_list()->GetNotifications();
  EXPECT_EQ(replaced, (*notifications.begin())->id());
  EXPECT_EQ(UTF8ToUTF16("newtitle"), (*notifications.begin())->title());
  EXPECT_EQ(UTF8ToUTF16("newbody"), (*notifications.begin())->message());
}

TEST_F(NotificationListTest, GetNotificationsBySourceOrExtensions) {
  scoped_ptr<Notification> notification(
      new Notification(message_center::NOTIFICATION_TYPE_SIMPLE,
                       "id0",
                       UTF8ToUTF16("title0"),
                       UTF8ToUTF16("message0"),
                       gfx::Image(),
                       UTF8ToUTF16("source0"),
                       "ext0",
                       message_center::RichNotificationData(),
                       NULL));
  notification_list()->AddNotification(notification.Pass());
  notification.reset(new Notification(message_center::NOTIFICATION_TYPE_SIMPLE,
                                      "id1",
                                      UTF8ToUTF16("title1"),
                                      UTF8ToUTF16("message1"),
                                      gfx::Image(),
                                      UTF8ToUTF16("source0"),
                                      "ext0",
                                      message_center::RichNotificationData(),
                                      NULL));
  notification_list()->AddNotification(notification.Pass());
  notification.reset(new Notification(message_center::NOTIFICATION_TYPE_SIMPLE,
                                      "id2",
                                      UTF8ToUTF16("title1"),
                                      UTF8ToUTF16("message1"),
                                      gfx::Image(),
                                      UTF8ToUTF16("source1"),
                                      "ext0",
                                      message_center::RichNotificationData(),
                                      NULL));
  notification_list()->AddNotification(notification.Pass());
  notification.reset(new Notification(message_center::NOTIFICATION_TYPE_SIMPLE,
                                      "id3",
                                      UTF8ToUTF16("title1"),
                                      UTF8ToUTF16("message1"),
                                      gfx::Image(),
                                      UTF8ToUTF16("source2"),
                                      "ext1",
                                      message_center::RichNotificationData(),
                                      NULL));
  notification_list()->AddNotification(notification.Pass());

  NotificationList::Notifications by_source =
      notification_list()->GetNotificationsBySource("id0");
  EXPECT_TRUE(IsInNotifications(by_source, "id0"));
  EXPECT_TRUE(IsInNotifications(by_source, "id1"));
  EXPECT_FALSE(IsInNotifications(by_source, "id2"));
  EXPECT_FALSE(IsInNotifications(by_source, "id3"));

  NotificationList::Notifications by_extension =
      notification_list()->GetNotificationsByExtension("id0");
  EXPECT_TRUE(IsInNotifications(by_extension, "id0"));
  EXPECT_TRUE(IsInNotifications(by_extension, "id1"));
  EXPECT_TRUE(IsInNotifications(by_extension, "id2"));
  EXPECT_FALSE(IsInNotifications(by_extension, "id3"));
}

TEST_F(NotificationListTest, OldPopupShouldNotBeHidden) {
  std::vector<std::string> ids;
  for (size_t i = 0; i <= kMaxVisiblePopupNotifications; i++)
    ids.push_back(AddNotification());

  NotificationList::PopupNotifications popups =
      notification_list()->GetPopupNotifications();
  // The popup should contain the oldest kMaxVisiblePopupNotifications. Newer
  // one should come earlier in the popup list. It means, the last element
  // of |popups| should be the firstly added one, and so on.
  EXPECT_EQ(kMaxVisiblePopupNotifications, popups.size());
  NotificationList::PopupNotifications::const_reverse_iterator iter =
      popups.rbegin();
  for (size_t i = 0; i < kMaxVisiblePopupNotifications; ++i, ++iter) {
    EXPECT_EQ(ids[i], (*iter)->id()) << i;
  }

  notification_list()->MarkPopupsAsShown(message_center::DEFAULT_PRIORITY);
  popups.clear();
  popups = notification_list()->GetPopupNotifications();
  EXPECT_EQ(1u, popups.size());
  EXPECT_EQ(ids[ids.size() - 1], (*popups.begin())->id());
}

TEST_F(NotificationListTest, Priority) {
  ASSERT_EQ(0u, notification_list()->NotificationCount());
  ASSERT_EQ(0u, notification_list()->unread_count());

  // Default priority has the limit on the number of the popups.
  for (size_t i = 0; i <= kMaxVisiblePopupNotifications; ++i)
    AddNotification();
  EXPECT_EQ(kMaxVisiblePopupNotifications + 1,
            notification_list()->NotificationCount());
  EXPECT_EQ(kMaxVisiblePopupNotifications, GetPopupCounts());

  // Low priority: not visible to popups.
  notification_list()->SetMessageCenterVisible(true, NULL);
  notification_list()->SetMessageCenterVisible(false, NULL);
  EXPECT_EQ(0u, notification_list()->unread_count());
  AddPriorityNotification(LOW_PRIORITY);
  EXPECT_EQ(kMaxVisiblePopupNotifications + 2,
            notification_list()->NotificationCount());
  EXPECT_EQ(1u, notification_list()->unread_count());
  EXPECT_EQ(0u, GetPopupCounts());

  // Minimum priority: doesn't update the unread count.
  AddPriorityNotification(MIN_PRIORITY);
  EXPECT_EQ(kMaxVisiblePopupNotifications + 3,
            notification_list()->NotificationCount());
  EXPECT_EQ(1u, notification_list()->unread_count());
  EXPECT_EQ(0u, GetPopupCounts());

  notification_list()->RemoveAllNotifications();

  // Higher priority: no limits to the number of popups.
  for (size_t i = 0; i < kMaxVisiblePopupNotifications * 2; ++i)
    AddPriorityNotification(HIGH_PRIORITY);
  for (size_t i = 0; i < kMaxVisiblePopupNotifications * 2; ++i)
    AddPriorityNotification(MAX_PRIORITY);
  EXPECT_EQ(kMaxVisiblePopupNotifications * 4,
            notification_list()->NotificationCount());
  EXPECT_EQ(kMaxVisiblePopupNotifications * 4, GetPopupCounts());
}

TEST_F(NotificationListTest, HasPopupsWithPriority) {
  ASSERT_EQ(0u, notification_list()->NotificationCount());
  ASSERT_EQ(0u, notification_list()->unread_count());

  AddPriorityNotification(MIN_PRIORITY);
  AddPriorityNotification(MAX_PRIORITY);

  EXPECT_EQ(1u, GetPopupCounts());
}

TEST_F(NotificationListTest, HasPopupsWithSystemPriority) {
  ASSERT_EQ(0u, notification_list()->NotificationCount());
  ASSERT_EQ(0u, notification_list()->unread_count());

  std::string normal_id = AddPriorityNotification(DEFAULT_PRIORITY);
  std::string system_id = AddNotification();
  GetNotification(system_id)->SetSystemPriority();

  EXPECT_EQ(2u, GetPopupCounts());

  notification_list()->MarkSinglePopupAsDisplayed(normal_id);
  notification_list()->MarkSinglePopupAsDisplayed(system_id);

  notification_list()->MarkSinglePopupAsShown(normal_id, false);
  notification_list()->MarkSinglePopupAsShown(system_id, false);

  notification_list()->SetMessageCenterVisible(true, NULL);
  notification_list()->SetMessageCenterVisible(false, NULL);
  EXPECT_EQ(1u, GetPopupCounts());

  // Mark as read -- emulation of mouse click.
  notification_list()->MarkSinglePopupAsShown(system_id, true);
  EXPECT_EQ(0u, GetPopupCounts());
}

TEST_F(NotificationListTest, PriorityPromotion) {
  std::string id0 = AddPriorityNotification(LOW_PRIORITY);
  std::string replaced = id0 + "_replaced";
  EXPECT_EQ(1u, notification_list()->NotificationCount());
  EXPECT_EQ(0u, GetPopupCounts());
  message_center::RichNotificationData optional;
  optional.priority = 1;
  scoped_ptr<Notification> notification(
      new Notification(message_center::NOTIFICATION_TYPE_SIMPLE,
                       replaced,
                       UTF8ToUTF16("newtitle"),
                       UTF8ToUTF16("newbody"),
                       gfx::Image(),
                       UTF8ToUTF16(kDisplaySource),
                       kExtensionId,
                       optional,
                       NULL));
  notification_list()->UpdateNotificationMessage(id0, notification.Pass());
  EXPECT_EQ(1u, notification_list()->NotificationCount());
  EXPECT_EQ(1u, GetPopupCounts());
  const NotificationList::Notifications& notifications =
      notification_list()->GetNotifications();
  EXPECT_EQ(replaced, (*notifications.begin())->id());
  EXPECT_EQ(UTF8ToUTF16("newtitle"), (*notifications.begin())->title());
  EXPECT_EQ(UTF8ToUTF16("newbody"), (*notifications.begin())->message());
  EXPECT_EQ(1, (*notifications.begin())->priority());
}

TEST_F(NotificationListTest, PriorityPromotionWithPopups) {
  std::string id0 = AddPriorityNotification(LOW_PRIORITY);
  std::string id1 = AddPriorityNotification(DEFAULT_PRIORITY);
  EXPECT_EQ(1u, GetPopupCounts());
  notification_list()->MarkSinglePopupAsShown(id1, true);
  EXPECT_EQ(0u, GetPopupCounts());

  // id0 promoted to LOW->DEFAULT, it'll appear as toast (popup).
  message_center::RichNotificationData priority;
  priority.priority = DEFAULT_PRIORITY;
  scoped_ptr<Notification> notification(
      new Notification(message_center::NOTIFICATION_TYPE_SIMPLE,
                       id0,
                       UTF8ToUTF16("newtitle"),
                       UTF8ToUTF16("newbody"),
                       gfx::Image(),
                       UTF8ToUTF16(kDisplaySource),
                       kExtensionId,
                       priority,
                       NULL));
  notification_list()->UpdateNotificationMessage(id0, notification.Pass());
  EXPECT_EQ(1u, GetPopupCounts());
  notification_list()->MarkSinglePopupAsShown(id0, true);
  EXPECT_EQ(0u, GetPopupCounts());

  // update with no promotion change for id0, it won't appear as a toast.
  notification.reset(new Notification(message_center::NOTIFICATION_TYPE_SIMPLE,
                                      id0,
                                      UTF8ToUTF16("newtitle2"),
                                      UTF8ToUTF16("newbody2"),
                                      gfx::Image(),
                                      UTF8ToUTF16(kDisplaySource),
                                      kExtensionId,
                                      priority,
                                      NULL));
  notification_list()->UpdateNotificationMessage(id0, notification.Pass());
  EXPECT_EQ(0u, GetPopupCounts());

  // id1 promoted to DEFAULT->HIGH, it'll appear as toast (popup).
  priority.priority = HIGH_PRIORITY;
  notification.reset(new Notification(message_center::NOTIFICATION_TYPE_SIMPLE,
                                      id1,
                                      UTF8ToUTF16("newtitle"),
                                      UTF8ToUTF16("newbody"),
                                      gfx::Image(),
                                      UTF8ToUTF16(kDisplaySource),
                                      kExtensionId,
                                      priority,
                                      NULL));
  notification_list()->UpdateNotificationMessage(id1, notification.Pass());
  EXPECT_EQ(1u, GetPopupCounts());
  notification_list()->MarkSinglePopupAsShown(id1, true);
  EXPECT_EQ(0u, GetPopupCounts());

  // id1 promoted to HIGH->MAX, it'll appear as toast again.
  priority.priority = MAX_PRIORITY;
  notification.reset(new Notification(message_center::NOTIFICATION_TYPE_SIMPLE,
                                      id1,
                                      UTF8ToUTF16("newtitle2"),
                                      UTF8ToUTF16("newbody2"),
                                      gfx::Image(),
                                      UTF8ToUTF16(kDisplaySource),
                                      kExtensionId,
                                      priority,
                                      NULL));
  notification_list()->UpdateNotificationMessage(id1, notification.Pass());
  EXPECT_EQ(1u, GetPopupCounts());
  notification_list()->MarkSinglePopupAsShown(id1, true);
  EXPECT_EQ(0u, GetPopupCounts());

  // id1 demoted to MAX->DEFAULT, no appearing as toast.
  priority.priority = DEFAULT_PRIORITY;
  notification.reset(new Notification(message_center::NOTIFICATION_TYPE_SIMPLE,
                                      id1,
                                      UTF8ToUTF16("newtitle3"),
                                      UTF8ToUTF16("newbody3"),
                                      gfx::Image(),
                                      UTF8ToUTF16(kDisplaySource),
                                      kExtensionId,
                                      priority,
                                      NULL));
  notification_list()->UpdateNotificationMessage(id1, notification.Pass());
  EXPECT_EQ(0u, GetPopupCounts());
}

TEST_F(NotificationListTest, NotificationOrderAndPriority) {
  base::Time now = base::Time::Now();
  message_center::RichNotificationData optional;
  optional.timestamp = now;
  optional.priority = 2;
  std::string max_id = AddNotification(optional);

  now += base::TimeDelta::FromSeconds(1);
  optional.timestamp = now;
  optional.priority = 1;
  std::string high_id = AddNotification(optional);

  now += base::TimeDelta::FromSeconds(1);
  optional.timestamp = now;
  optional.priority = 0;
  std::string default_id = AddNotification(optional);

  {
    // Popups: latest comes first.
    NotificationList::PopupNotifications popups =
        notification_list()->GetPopupNotifications();
    EXPECT_EQ(3u, popups.size());
    NotificationList::PopupNotifications::const_iterator iter = popups.begin();
    EXPECT_EQ(default_id, (*iter)->id());
    iter++;
    EXPECT_EQ(high_id, (*iter)->id());
    iter++;
    EXPECT_EQ(max_id, (*iter)->id());
  }
  {
    // Notifications: high priority comes ealier.
    const NotificationList::Notifications& notifications =
        notification_list()->GetNotifications();
    EXPECT_EQ(3u, notifications.size());
    NotificationList::Notifications::const_iterator iter =
        notifications.begin();
    EXPECT_EQ(max_id, (*iter)->id());
    iter++;
    EXPECT_EQ(high_id, (*iter)->id());
    iter++;
    EXPECT_EQ(default_id, (*iter)->id());
  }
}

TEST_F(NotificationListTest, MarkSinglePopupAsShown) {
  std::string id1 = AddNotification();
  std::string id2 = AddNotification();
  std::string id3 = AddNotification();
  ASSERT_EQ(3u, notification_list()->NotificationCount());
  ASSERT_EQ(std::min(static_cast<size_t>(3u), kMaxVisiblePopupNotifications),
            GetPopupCounts());
  notification_list()->MarkSinglePopupAsDisplayed(id1);
  notification_list()->MarkSinglePopupAsDisplayed(id2);
  notification_list()->MarkSinglePopupAsDisplayed(id3);

  notification_list()->MarkSinglePopupAsShown(id2, true);
  notification_list()->MarkSinglePopupAsShown(id3, false);
  EXPECT_EQ(3u, notification_list()->NotificationCount());
  EXPECT_EQ(1u, notification_list()->unread_count());
  EXPECT_EQ(1u, GetPopupCounts());
  NotificationList::PopupNotifications popups =
      notification_list()->GetPopupNotifications();
  EXPECT_EQ(id1, (*popups.begin())->id());

  // The notifications in the NotificationCenter are unaffected by popups shown.
  NotificationList::Notifications notifications =
      notification_list()->GetNotifications();
  NotificationList::Notifications::const_iterator iter = notifications.begin();
  EXPECT_EQ(id3, (*iter)->id());
  iter++;
  EXPECT_EQ(id2, (*iter)->id());
  iter++;
  EXPECT_EQ(id1, (*iter)->id());

  // Trickier scenario.
  notification_list()->MarkPopupsAsShown(message_center::DEFAULT_PRIORITY);
  std::string id4 = AddNotification();
  std::string id5 = AddNotification();
  std::string id6 = AddNotification();
  notification_list()->MarkSinglePopupAsShown(id5, true);

  {
    popups.clear();
    popups = notification_list()->GetPopupNotifications();
    EXPECT_EQ(2u, popups.size());
    NotificationList::PopupNotifications::const_iterator iter = popups.begin();
    EXPECT_EQ(id6, (*iter)->id());
    iter++;
    EXPECT_EQ(id4, (*iter)->id());
  }

  notifications.clear();
  notifications = notification_list()->GetNotifications();
  EXPECT_EQ(6u, notifications.size());
  iter = notifications.begin();
  EXPECT_EQ(id6, (*iter)->id());
  iter++;
  EXPECT_EQ(id5, (*iter)->id());
  iter++;
  EXPECT_EQ(id4, (*iter)->id());
  iter++;
  EXPECT_EQ(id3, (*iter)->id());
  iter++;
  EXPECT_EQ(id2, (*iter)->id());
  iter++;
  EXPECT_EQ(id1, (*iter)->id());
}

TEST_F(NotificationListTest, UpdateAfterMarkedAsShown) {
  std::string id1 = AddNotification();
  std::string id2 = AddNotification();
  notification_list()->MarkSinglePopupAsDisplayed(id1);
  notification_list()->MarkSinglePopupAsDisplayed(id2);

  EXPECT_EQ(2u, GetPopupCounts());

  const Notification* n1 = GetNotification(id1);
  EXPECT_FALSE(n1->shown_as_popup());
  EXPECT_TRUE(n1->is_read());

  notification_list()->MarkSinglePopupAsShown(id1, true);

  n1 = GetNotification(id1);
  EXPECT_TRUE(n1->shown_as_popup());
  EXPECT_TRUE(n1->is_read());

  const std::string replaced("test-replaced-id");
  scoped_ptr<Notification> notification(
      new Notification(message_center::NOTIFICATION_TYPE_SIMPLE,
                       replaced,
                       UTF8ToUTF16("newtitle"),
                       UTF8ToUTF16("newbody"),
                       gfx::Image(),
                       UTF8ToUTF16(kDisplaySource),
                       kExtensionId,
                       message_center::RichNotificationData(),
                       NULL));
  notification_list()->UpdateNotificationMessage(id1, notification.Pass());
  n1 = GetNotification(id1);
  EXPECT_TRUE(n1 == NULL);
  const Notification* nr = GetNotification(replaced);
  EXPECT_TRUE(nr->shown_as_popup());
  EXPECT_TRUE(nr->is_read());
}

TEST_F(NotificationListTest, QuietMode) {
  notification_list()->SetQuietMode(true);
  AddNotification();
  AddPriorityNotification(HIGH_PRIORITY);
  AddPriorityNotification(MAX_PRIORITY);
  EXPECT_EQ(3u, notification_list()->NotificationCount());
  EXPECT_EQ(0u, GetPopupCounts());

  notification_list()->SetQuietMode(false);
  AddNotification();
  EXPECT_EQ(4u, notification_list()->NotificationCount());
  EXPECT_EQ(1u, GetPopupCounts());

  // TODO(mukai): Add test of quiet mode with expiration.
}

// Verifies that unread_count doesn't become negative.
TEST_F(NotificationListTest, UnreadCountNoNegative) {
  std::string id = AddNotification();
  EXPECT_EQ(1u, notification_list()->unread_count());

  notification_list()->MarkSinglePopupAsDisplayed(id);
  EXPECT_EQ(0u, notification_list()->unread_count());
  notification_list()->MarkSinglePopupAsShown(
      id, false /* mark_notification_as_read */);
  EXPECT_EQ(1u, notification_list()->unread_count());

  // Updates the notification  and verifies unread_count doesn't change.
  scoped_ptr<Notification> updated_notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      id,
      UTF8ToUTF16("updated"),
      UTF8ToUTF16("updated"),
      gfx::Image(),
      base::string16(),
      std::string(),
      RichNotificationData(),
      NULL));
  notification_list()->AddNotification(updated_notification.Pass());
  EXPECT_EQ(1u, notification_list()->unread_count());
}

}  // namespace test
}  // namespace message_center
