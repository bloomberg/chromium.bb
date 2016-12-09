// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/views/message_center_controller.h"
#include "ui/message_center/views/message_list_view.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/views/test/views_test_base.h"

namespace message_center {

static const char* kNotificationId1 = "notification id 1";

namespace {

/* Types **********************************************************************/

enum CallType { GET_PREFERRED_SIZE, GET_HEIGHT_FOR_WIDTH, LAYOUT };

/* Instrumented/Mock NotificationView subclass ********************************/

class MockNotificationView : public NotificationView {
 public:
  class Test {
   public:
    virtual void RegisterCall(CallType type) = 0;
  };

  MockNotificationView(MessageCenterController* controller,
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
    : NotificationView(controller, notification), test_(test) {}

MockNotificationView::~MockNotificationView() {}

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

}  // anonymous namespace

/* Test fixture ***************************************************************/

class MessageListViewTest : public views::ViewsTestBase,
                            public MockNotificationView::Test,
                            public MessageListView::Observer,
                            public MessageCenterController {
 public:
  MessageListViewTest() {}

  ~MessageListViewTest() override {}

  void SetUp() override {
    views::ViewsTestBase::SetUp();

    message_list_view_.reset(new MessageListView());
    message_list_view_->AddObserver(this);
    message_list_view_->set_owned_by_client();

    widget_.reset(new views::Widget());
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_POPUP);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(50, 50, 650, 650);
    widget_->Init(params);
    views::View* root = widget_->GetRootView();
    root->AddChildView(message_list_view_.get());
    widget_->Show();
    widget_->Activate();
  }

  void TearDown() override {
    widget_->CloseNow();
    widget_.reset();

    message_list_view_->RemoveObserver(this);
    message_list_view_.reset();

    views::ViewsTestBase::TearDown();
  }

 protected:
  MessageListView* message_list_view() const {
    return message_list_view_.get();
  }

  MockNotificationView* CreateNotificationView(
      const Notification& notification) {
    return new MockNotificationView(this, notification, this);
  }

 private:
  // MockNotificationView::Test override
  void RegisterCall(CallType type) override {}

  // MessageListView::Observer override
  void OnAllNotificationsCleared() override {}

  // MessageCenterController override:
  void ClickOnNotification(const std::string& notification_id) override {}
  void RemoveNotification(const std::string& notification_id,
                          bool by_user) override {}
  std::unique_ptr<ui::MenuModel> CreateMenuModel(
      const NotifierId& notifier_id,
      const base::string16& display_source) override {
    NOTREACHED();
    return nullptr;
  }
  bool HasClickedListener(const std::string& notification_id) override {
    return false;
  }
  void ClickOnNotificationButton(const std::string& notification_id,
                                 int button_index) override {}
  void ClickOnSettingsButton(const std::string& notification_id) override {}

  // Widget to host a MessageListView.
  std::unique_ptr<views::Widget> widget_;
  // MessageListView to be tested.
  std::unique_ptr<MessageListView> message_list_view_;

  DISALLOW_COPY_AND_ASSIGN(MessageListViewTest);
};

/* Unit tests *****************************************************************/

TEST_F(MessageListViewTest, AddNotification) {
  // Create a dummy notification.
  auto notification_view = CreateNotificationView(
      Notification(NOTIFICATION_TYPE_SIMPLE, std::string(kNotificationId1),
                   base::UTF8ToUTF16("title"), base::UTF8ToUTF16("message1"),
                   gfx::Image(), base::UTF8ToUTF16("display source"), GURL(),
                   NotifierId(NotifierId::APPLICATION, "extension_id"),
                   message_center::RichNotificationData(), nullptr));

  EXPECT_EQ(0, message_list_view()->child_count());
  EXPECT_FALSE(message_list_view()->Contains(notification_view));

  // Add a notification.
  message_list_view()->AddNotificationAt(notification_view, 0);

  EXPECT_EQ(1, message_list_view()->child_count());
  EXPECT_TRUE(message_list_view()->Contains(notification_view));
}

}  // namespace
