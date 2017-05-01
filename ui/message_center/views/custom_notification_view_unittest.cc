// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"
#include "ui/message_center/views/custom_notification_view.h"
#include "ui/message_center/views/message_center_controller.h"
#include "ui/message_center/views/message_view_factory.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/views_test_base.h"

namespace message_center {

namespace {

const SkColor kBackgroundColor = SK_ColorGREEN;

std::unique_ptr<ui::GestureEvent> GenerateGestureEvent(ui::EventType type) {
  ui::GestureEventDetails detail(type);
  std::unique_ptr<ui::GestureEvent> event(
      new ui::GestureEvent(0, 0, 0, base::TimeTicks(), detail));
  return event;
}

std::unique_ptr<ui::GestureEvent> GenerateGestureHorizontalScrollUpdateEvent(
    int dx) {
  ui::GestureEventDetails detail(ui::ET_GESTURE_SCROLL_UPDATE, dx, 0);
  std::unique_ptr<ui::GestureEvent> event(
      new ui::GestureEvent(0, 0, 0, base::TimeTicks(), detail));
  return event;
}

class TestCustomView : public views::View {
 public:
  TestCustomView() {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    set_background(views::Background::CreateSolidBackground(kBackgroundColor));
  }
  ~TestCustomView() override {}

  void Reset() {
    mouse_event_count_ = 0;
    keyboard_event_count_ = 0;
  }

  void set_preferred_size(gfx::Size size) { preferred_size_ = size; }

  // views::View
  gfx::Size GetPreferredSize() const override { return preferred_size_; }
  bool OnMousePressed(const ui::MouseEvent& event) override {
    ++mouse_event_count_;
    return true;
  }
  void OnMouseMoved(const ui::MouseEvent& event) override {
    ++mouse_event_count_;
  }
  void OnMouseReleased(const ui::MouseEvent& event) override {
    ++mouse_event_count_;
  }
  bool OnKeyPressed(const ui::KeyEvent& event) override {
    ++keyboard_event_count_;
    return false;
  }

  int mouse_event_count() const { return mouse_event_count_; }
  int keyboard_event_count() const { return keyboard_event_count_; }

 private:
  int mouse_event_count_ = 0;
  int keyboard_event_count_ = 0;
  gfx::Size preferred_size_ = gfx::Size(100, 100);

  DISALLOW_COPY_AND_ASSIGN(TestCustomView);
};

class TestContentViewDelegate : public CustomNotificationContentViewDelegate {
 public:
  bool IsCloseButtonFocused() const override { return false; }
  void RequestFocusOnCloseButton() override {}
  bool IsPinned() const override { return false; }
  void UpdateControlButtonsVisibility() override {}
};

class TestNotificationDelegate : public NotificationDelegate {
 public:
  TestNotificationDelegate() {}

  // NotificateDelegate
  std::unique_ptr<CustomContent> CreateCustomContent() override {
    return base::MakeUnique<CustomContent>(
        base::MakeUnique<TestCustomView>(),
        base::MakeUnique<TestContentViewDelegate>());
  }

 private:
  ~TestNotificationDelegate() override {}

  DISALLOW_COPY_AND_ASSIGN(TestNotificationDelegate);
};

class TestMessageCenterController : public MessageCenterController {
 public:
  TestMessageCenterController() {}

  // MessageCenterController
  void ClickOnNotification(const std::string& notification_id) override {
    // For this test, this method should not be invoked.
    NOTREACHED();
  }

  void RemoveNotification(const std::string& notification_id,
                          bool by_user) override {
    removed_ids_.insert(notification_id);
  }

  std::unique_ptr<ui::MenuModel> CreateMenuModel(
      const NotifierId& notifier_id,
      const base::string16& display_source) override {
    // For this test, this method should not be invoked.
    NOTREACHED();
    return nullptr;
  }

  bool HasClickedListener(const std::string& notification_id) override {
    return false;
  }

  void ClickOnNotificationButton(const std::string& notification_id,
                                 int button_index) override {
    // For this test, this method should not be invoked.
    NOTREACHED();
  }

  void ClickOnSettingsButton(const std::string& notification_id) override {
    // For this test, this method should not be invoked.
    NOTREACHED();
  }

  void UpdateNotificationSize(const std::string& notification_id) override {
    // For this test, this method should not be invoked.
    NOTREACHED();
  }

  bool IsRemoved(const std::string& notification_id) const {
    return (removed_ids_.find(notification_id) != removed_ids_.end());
  }

 private:
  std::set<std::string> removed_ids_;

  DISALLOW_COPY_AND_ASSIGN(TestMessageCenterController);
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

class CustomNotificationViewTest : public views::ViewsTestBase {
 public:
  CustomNotificationViewTest() {}
  ~CustomNotificationViewTest() override {}

  // views::ViewsTestBase
  void SetUp() override {
    views::ViewsTestBase::SetUp();

    notification_delegate_ = new TestNotificationDelegate;

    notification_.reset(new Notification(
        NOTIFICATION_TYPE_CUSTOM, std::string("notification id"),
        base::UTF8ToUTF16("title"), base::UTF8ToUTF16("message"), gfx::Image(),
        base::UTF8ToUTF16("display source"), GURL(),
        NotifierId(NotifierId::APPLICATION, "extension_id"),
        message_center::RichNotificationData(), notification_delegate_.get()));

    notification_view_.reset(static_cast<CustomNotificationView*>(
        MessageViewFactory::Create(controller(), *notification_, true)));
    notification_view_->set_owned_by_client();

    views::Widget::InitParams init_params(
        CreateParams(views::Widget::InitParams::TYPE_POPUP));
    views::Widget* widget = new views::Widget();
    widget->Init(init_params);
    widget->SetContentsView(notification_view_.get());
    widget->SetSize(notification_view_->GetPreferredSize());
  }

  void TearDown() override {
    widget()->Close();
    notification_view_.reset();
    views::ViewsTestBase::TearDown();
  }

  SkColor GetBackgroundColor() const {
    return notification_view_->background_view()->background()->get_color();
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

  void KeyPress(ui::KeyboardCode key_code) {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, key_code, ui::EF_NONE);
    widget()->OnKeyEvent(&event);
  }

  void UpdateNotificationViews() {
    notification_view()->UpdateWithNotification(*notification());
  }

  float GetNotificationScrollAmount() const {
    return notification_view_->GetTransform().To2dTranslation().x();
  }

  TestMessageCenterController* controller() { return &controller_; }
  Notification* notification() { return notification_.get(); }
  TestCustomView* custom_view() {
    return static_cast<TestCustomView*>(notification_view_->contents_view_);
  }
  views::Widget* widget() { return notification_view_->GetWidget(); }
  CustomNotificationView* notification_view() {
    return notification_view_.get();
  }

 private:
  TestMessageCenterController controller_;
  scoped_refptr<TestNotificationDelegate> notification_delegate_;
  std::unique_ptr<Notification> notification_;
  std::unique_ptr<CustomNotificationView> notification_view_;

  DISALLOW_COPY_AND_ASSIGN(CustomNotificationViewTest);
};

TEST_F(CustomNotificationViewTest, Background) {
  EXPECT_EQ(kBackgroundColor, GetBackgroundColor());
}

TEST_F(CustomNotificationViewTest, Events) {
  widget()->Show();
  custom_view()->RequestFocus();

  EXPECT_EQ(0, custom_view()->mouse_event_count());
  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToWidget(custom_view(), &cursor_location);
  PerformClick(cursor_location);
  EXPECT_EQ(2, custom_view()->mouse_event_count());

  ui::MouseEvent move(ui::ET_MOUSE_MOVED, cursor_location, cursor_location,
                      ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  widget()->OnMouseEvent(&move);
  EXPECT_EQ(3, custom_view()->mouse_event_count());

  EXPECT_EQ(0, custom_view()->keyboard_event_count());
  KeyPress(ui::VKEY_A);
  EXPECT_EQ(1, custom_view()->keyboard_event_count());
}

TEST_F(CustomNotificationViewTest, SlideOut) {
  UpdateNotificationViews();
  std::string notification_id = notification()->id();

  auto event_begin = GenerateGestureEvent(ui::ET_GESTURE_SCROLL_BEGIN);
  auto event_scroll10 = GenerateGestureHorizontalScrollUpdateEvent(-10);
  auto event_scroll500 = GenerateGestureHorizontalScrollUpdateEvent(-500);
  auto event_end = GenerateGestureEvent(ui::ET_GESTURE_SCROLL_END);

  notification_view()->OnGestureEvent(event_begin.get());
  notification_view()->OnGestureEvent(event_scroll10.get());
  EXPECT_FALSE(controller()->IsRemoved(notification_id));
  EXPECT_EQ(-10.f, GetNotificationScrollAmount());
  notification_view()->OnGestureEvent(event_end.get());
  EXPECT_FALSE(controller()->IsRemoved(notification_id));
  EXPECT_EQ(0.f, GetNotificationScrollAmount());

  notification_view()->OnGestureEvent(event_begin.get());
  notification_view()->OnGestureEvent(event_scroll500.get());
  EXPECT_FALSE(controller()->IsRemoved(notification_id));
  EXPECT_EQ(-500.f, GetNotificationScrollAmount());
  notification_view()->OnGestureEvent(event_end.get());
  EXPECT_TRUE(controller()->IsRemoved(notification_id));
}

// Pinning notification is ChromeOS only feature.
#if defined(OS_CHROMEOS)

TEST_F(CustomNotificationViewTest, SlideOutPinned) {
  notification()->set_pinned(true);
  UpdateNotificationViews();
  std::string notification_id = notification()->id();

  auto event_begin = GenerateGestureEvent(ui::ET_GESTURE_SCROLL_BEGIN);
  auto event_scroll500 = GenerateGestureHorizontalScrollUpdateEvent(-500);
  auto event_end = GenerateGestureEvent(ui::ET_GESTURE_SCROLL_END);

  notification_view()->OnGestureEvent(event_begin.get());
  notification_view()->OnGestureEvent(event_scroll500.get());
  EXPECT_FALSE(controller()->IsRemoved(notification_id));
  EXPECT_LT(-500.f, GetNotificationScrollAmount());
  notification_view()->OnGestureEvent(event_end.get());
  EXPECT_FALSE(controller()->IsRemoved(notification_id));
}

#endif // defined(OS_CHROMEOS)

TEST_F(CustomNotificationViewTest, PressBackspaceKey) {
  std::string notification_id = notification()->id();
  custom_view()->RequestFocus();

  ui::InputMethod* input_method = custom_view()->GetInputMethod();
  ASSERT_TRUE(input_method);
  TestTextInputClient text_input_client;
  input_method->SetFocusedTextInputClient(&text_input_client);
  ASSERT_EQ(&text_input_client, input_method->GetTextInputClient());

  EXPECT_FALSE(controller()->IsRemoved(notification_id));
  PerformKeyEvents(ui::VKEY_BACK);
  EXPECT_TRUE(controller()->IsRemoved(notification_id));

  input_method->SetFocusedTextInputClient(nullptr);
}

TEST_F(CustomNotificationViewTest, PressBackspaceKeyOnEditBox) {
  std::string notification_id = notification()->id();
  custom_view()->RequestFocus();

  ui::InputMethod* input_method = custom_view()->GetInputMethod();
  ASSERT_TRUE(input_method);
  TestTextInputClient text_input_client;
  input_method->SetFocusedTextInputClient(&text_input_client);
  ASSERT_EQ(&text_input_client, input_method->GetTextInputClient());

  text_input_client.set_text_input_type(ui::TEXT_INPUT_TYPE_TEXT);

  EXPECT_FALSE(controller()->IsRemoved(notification_id));
  PerformKeyEvents(ui::VKEY_BACK);
  EXPECT_FALSE(controller()->IsRemoved(notification_id));

  input_method->SetFocusedTextInputClient(nullptr);
}

TEST_F(CustomNotificationViewTest, ChangeContentHeight) {
  // Default size.
  gfx::Size size = notification_view()->GetPreferredSize();
  size.Enlarge(0, -notification_view()->GetInsets().height());
  EXPECT_EQ("360x100", size.ToString());

  // Allow small notifications.
  custom_view()->set_preferred_size(gfx::Size(10, 10));
  size = notification_view()->GetPreferredSize();
  size.Enlarge(0, -notification_view()->GetInsets().height());
  EXPECT_EQ("360x10", size.ToString());

  // The long notification.
  custom_view()->set_preferred_size(gfx::Size(1000, 1000));
  size = notification_view()->GetPreferredSize();
  size.Enlarge(0, -notification_view()->GetInsets().height());
  EXPECT_EQ("360x1000", size.ToString());
}

}  // namespace message_center
