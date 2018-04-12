// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/arc/notification/arc_notification_content_view.h"
#include "ui/arc/notification/arc_notification_item.h"
#include "ui/arc/notification/arc_notification_view.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/message_view_factory.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/views_test_base.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace arc {

namespace {

constexpr char kNotificationIdPrefix[] = "ARC_NOTIFICATION_";

class MockArcNotificationItem : public ArcNotificationItem {
 public:
  MockArcNotificationItem(const std::string& notification_key)
      : notification_key_(notification_key),
        notification_id_(kNotificationIdPrefix + notification_key) {}

  // Overriding methods for testing.
  void Close(bool by_user) override {}
  const gfx::ImageSkia& GetSnapshot() const override { return snapshot_; }
  const std::string& GetNotificationKey() const override {
    return notification_key_;
  }
  const std::string& GetNotificationId() const override {
    return notification_id_;
  }

  // Overriding methods for returning dummy data or doing nothing.
  void OnClosedFromAndroid() override {}
  void Click() override {}
  void ToggleExpansion() override {}
  void OpenSettings() override {}
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  void IncrementWindowRefCount() override {}
  void DecrementWindowRefCount() override {}
  mojom::ArcNotificationType GetNotificationType() const override {
    return mojom::ArcNotificationType::SIMPLE;
  }
  mojom::ArcNotificationExpandState GetExpandState() const override {
    return mojom::ArcNotificationExpandState::FIXED_SIZE;
  }
  mojom::ArcNotificationShownContents GetShownContents() const override {
    return mojom::ArcNotificationShownContents::CONTENTS_SHOWN;
  }
  gfx::Rect GetSwipeInputRect() const override { return gfx::Rect(); }
  void OnUpdatedFromAndroid(mojom::ArcNotificationDataPtr data,
                            const std::string& app_id) override {}
  bool IsManuallyExpandedOrCollapsed() const override { return false; }

 private:
  std::string notification_key_;
  std::string notification_id_;
  gfx::ImageSkia snapshot_;

  DISALLOW_COPY_AND_ASSIGN(MockArcNotificationItem);
};

class TestTextInputClient : public ui::DummyTextInputClient {
 public:
  TestTextInputClient() : ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_TEXT) {}

  ui::TextInputType GetTextInputType() const override { return type_; }

  void set_text_input_type(ui::TextInputType type) { type_ = type; }

 private:
  ui::TextInputType type_ = ui::TEXT_INPUT_TYPE_NONE;

  DISALLOW_COPY_AND_ASSIGN(TestTextInputClient);
};

}  // namespace

class ArcNotificationViewTest : public ash::AshTestBase {
 public:
  ArcNotificationViewTest() = default;
  ~ArcNotificationViewTest() override = default;

  // views::ViewsTestBase
  void SetUp() override {
    ash::AshTestBase::SetUp();

    const std::string notification_id("notification id");
    item_ = std::make_unique<MockArcNotificationItem>(notification_id);
    message_center::MessageViewFactory::SetCustomNotificationViewFactory(
        base::BindRepeating(
            &ArcNotificationViewTest::CreateCustomMessageViewForTest,
            base::Unretained(this), item_.get()));

    notification_ = std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_CUSTOM, notification_id,
        base::UTF8ToUTF16("title"), base::UTF8ToUTF16("message"), gfx::Image(),
        base::UTF8ToUTF16("display source"), GURL(),
        message_center::NotifierId(message_center::NotifierId::ARC_APPLICATION,
                                   "test_app_id"),
        message_center::RichNotificationData(), nullptr);

    notification_view_.reset(static_cast<ArcNotificationView*>(
        message_center::MessageViewFactory::Create(*notification_, true)));
    notification_view_->set_owned_by_client();
    UpdateNotificationViews();

    views::Widget::InitParams init_params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    init_params.context = CurrentContext();
    init_params.parent = ash::Shell::GetPrimaryRootWindow()->GetChildById(
        ash::kShellWindowId_DefaultContainer);
    init_params.ownership =
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    views::Widget* widget = new views::Widget();
    widget->Init(init_params);
    widget->SetContentsView(notification_view_.get());
    widget->SetSize(notification_view_->GetPreferredSize());
    widget->Show();
    EXPECT_EQ(widget, notification_view_->GetWidget());
  }

  void TearDown() override {
    widget()->Close();
    notification_view_.reset();
    ash::AshTestBase::TearDown();
  }

  void PerformClick(const gfx::Point& point) {
    ui::MouseEvent pressed_event = ui::MouseEvent(
        ui::ET_MOUSE_PRESSED, point, point, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    widget()->OnMouseEvent(&pressed_event);
    ui::MouseEvent released_event = ui::MouseEvent(
        ui::ET_MOUSE_RELEASED, point, point, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    widget()->OnMouseEvent(&released_event);
  }

  void PerformKeyEvents(ui::KeyboardCode code) {
    ui::KeyEvent event1 = ui::KeyEvent(ui::ET_KEY_PRESSED, code, ui::EF_NONE);
    widget()->OnKeyEvent(&event1);
    ui::KeyEvent event2 = ui::KeyEvent(ui::ET_KEY_RELEASED, code, ui::EF_NONE);
    widget()->OnKeyEvent(&event2);
  }

  void UpdateNotificationViews() {
    MessageCenter::Get()->AddNotification(
        std::make_unique<Notification>(*notification()));
    notification_view()->UpdateWithNotification(*notification());
  }

  float GetNotificationSlideAmount() const {
    return notification_view_->GetSlideOutLayer()
        ->transform()
        .To2dTranslation()
        .x();
  }

  bool IsRemoved(const std::string& notification_id) const {
    return !MessageCenter::Get()->FindVisibleNotificationById(notification_id);
  }

  void DispatchGesture(const ui::GestureEventDetails& details) {
    ui::GestureEvent event2(0, 0, 0, ui::EventTimeForNow(), details);
    widget()->OnGestureEvent(&event2);
  }

  void BeginScroll() {
    DispatchGesture(ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN));
  }

  void EndScroll() {
    DispatchGesture(ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));
  }

  void ScrollBy(int dx) {
    DispatchGesture(
        ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, dx, 0));
  }

  Notification* notification() { return notification_.get(); }
  ArcNotificationContentView* content_view() {
    return notification_view_->content_view_;
  }
  views::Widget* widget() { return notification_view_->GetWidget(); }
  ArcNotificationView* notification_view() { return notification_view_.get(); }

 private:
  std::unique_ptr<message_center::MessageView> CreateCustomMessageViewForTest(
      ArcNotificationItem* item,
      const Notification& notification) {
    auto message_view =
        std::make_unique<ArcNotificationView>(item, notification);
    message_view->content_view_->SetPreferredSize(gfx::Size(100, 100));
    return message_view;
  }

  std::unique_ptr<Notification> notification_;
  std::unique_ptr<ArcNotificationView> notification_view_;

  std::unique_ptr<MockArcNotificationItem> item_;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationViewTest);
};

TEST_F(ArcNotificationViewTest, Events) {
  widget()->Show();

  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToWidget(content_view(), &cursor_location);
  EXPECT_EQ(content_view(),
            widget()->GetRootView()->GetEventHandlerForPoint(cursor_location));

  content_view()->RequestFocus();
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(content_view(),
            static_cast<ui::EventTargeter*>(
                widget()->GetRootView()->GetEffectiveViewTargeter())
                ->FindTargetForEvent(widget()->GetRootView(), &key_event));
}

TEST_F(ArcNotificationViewTest, SlideOut) {
  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  std::string notification_id = notification()->id();

  BeginScroll();
  EXPECT_EQ(0.f, GetNotificationSlideAmount());
  ScrollBy(-10);
  EXPECT_FALSE(IsRemoved(notification_id));
  EXPECT_EQ(-10.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_FALSE(IsRemoved(notification_id));
  EXPECT_EQ(0.f, GetNotificationSlideAmount());

  BeginScroll();
  EXPECT_EQ(0.f, GetNotificationSlideAmount());
  ScrollBy(-200);
  EXPECT_FALSE(IsRemoved(notification_id));
  EXPECT_EQ(-200.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_TRUE(IsRemoved(notification_id));
}

TEST_F(ArcNotificationViewTest, SlideOutNested) {
  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  notification_view()->SetIsNested();
  std::string notification_id = notification()->id();

  BeginScroll();
  EXPECT_EQ(0.f, GetNotificationSlideAmount());
  ScrollBy(-10);
  EXPECT_FALSE(IsRemoved(notification_id));
  EXPECT_EQ(-10.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_FALSE(IsRemoved(notification_id));
  EXPECT_EQ(0.f, GetNotificationSlideAmount());

  BeginScroll();
  EXPECT_EQ(0.f, GetNotificationSlideAmount());
  ScrollBy(-200);
  EXPECT_FALSE(IsRemoved(notification_id));
  EXPECT_EQ(-200.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_TRUE(IsRemoved(notification_id));
}

// Pinning notification is ChromeOS only feature.
#if defined(OS_CHROMEOS)

TEST_F(ArcNotificationViewTest, SlideOutPinned) {
  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  notification()->set_pinned(true);
  notification_view()->SetIsNested();
  UpdateNotificationViews();
  std::string notification_id = notification()->id();

  BeginScroll();
  EXPECT_EQ(0.f, GetNotificationSlideAmount());
  ScrollBy(-200);
  EXPECT_FALSE(IsRemoved(notification_id));
  EXPECT_LT(-200.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_EQ(0.f, GetNotificationSlideAmount());
  EXPECT_FALSE(IsRemoved(notification_id));
}

#endif  // defined(OS_CHROMEOS)

TEST_F(ArcNotificationViewTest, PressBackspaceKey) {
  std::string notification_id = notification()->id();
  content_view()->RequestFocus();

  ui::InputMethod* input_method = content_view()->GetInputMethod();
  ASSERT_TRUE(input_method);
  TestTextInputClient text_input_client;
  input_method->SetFocusedTextInputClient(&text_input_client);
  ASSERT_EQ(&text_input_client, input_method->GetTextInputClient());

  EXPECT_FALSE(IsRemoved(notification_id));
  PerformKeyEvents(ui::VKEY_BACK);
  EXPECT_TRUE(IsRemoved(notification_id));

  input_method->SetFocusedTextInputClient(nullptr);
}

TEST_F(ArcNotificationViewTest, PressBackspaceKeyOnEditBox) {
  std::string notification_id = notification()->id();
  content_view()->RequestFocus();

  ui::InputMethod* input_method = content_view()->GetInputMethod();
  ASSERT_TRUE(input_method);
  TestTextInputClient text_input_client;
  input_method->SetFocusedTextInputClient(&text_input_client);
  ASSERT_EQ(&text_input_client, input_method->GetTextInputClient());

  text_input_client.set_text_input_type(ui::TEXT_INPUT_TYPE_TEXT);

  EXPECT_FALSE(IsRemoved(notification_id));
  PerformKeyEvents(ui::VKEY_BACK);
  EXPECT_FALSE(IsRemoved(notification_id));

  input_method->SetFocusedTextInputClient(nullptr);
}

TEST_F(ArcNotificationViewTest, ChangeContentHeight) {
  // Default size.
  gfx::Size size = notification_view()->GetPreferredSize();
  size.Enlarge(0, -notification_view()->GetInsets().height());
  EXPECT_EQ("360x100", size.ToString());

  // Allow small notifications.
  content_view()->SetPreferredSize(gfx::Size(10, 10));
  size = notification_view()->GetPreferredSize();
  size.Enlarge(0, -notification_view()->GetInsets().height());
  EXPECT_EQ("360x10", size.ToString());

  // The long notification.
  content_view()->SetPreferredSize(gfx::Size(1000, 1000));
  size = notification_view()->GetPreferredSize();
  size.Enlarge(0, -notification_view()->GetInsets().height());
  EXPECT_EQ("360x1000", size.ToString());
}

}  // namespace arc
