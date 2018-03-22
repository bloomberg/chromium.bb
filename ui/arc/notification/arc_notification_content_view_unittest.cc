// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/buffer.h"
#include "components/exo/keyboard.h"
#include "components/exo/keyboard_delegate.h"
#include "components/exo/notification_surface.h"
#include "components/exo/seat.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/wm_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/arc/notification/arc_notification_content_view.h"
#include "ui/arc/notification/arc_notification_delegate.h"
#include "ui/arc/notification/arc_notification_item.h"
#include "ui/arc/notification/arc_notification_manager.h"
#include "ui/arc/notification/arc_notification_surface.h"
#include "ui/arc/notification/arc_notification_surface_manager_impl.h"
#include "ui/arc/notification/arc_notification_view.h"
#include "ui/arc/notification/mock_arc_notification_item.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/message_view_factory.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/padded_button.h"
#include "ui/views/test/views_test_base.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace arc {

namespace {

constexpr gfx::Rect kNotificationSurfaceBounds(100, 100, 300, 300);

class MockKeyboardDelegate : public exo::KeyboardDelegate {
 public:
  MockKeyboardDelegate() = default;

  // Overridden from KeyboardDelegate:
  MOCK_METHOD1(OnKeyboardDestroying, void(exo::Keyboard*));
  MOCK_CONST_METHOD1(CanAcceptKeyboardEventsForSurface, bool(exo::Surface*));
  MOCK_METHOD2(OnKeyboardEnter,
               void(exo::Surface*, const base::flat_set<ui::DomCode>&));
  MOCK_METHOD1(OnKeyboardLeave, void(exo::Surface*));
  MOCK_METHOD3(OnKeyboardKey, uint32_t(base::TimeTicks, ui::DomCode, bool));
  MOCK_METHOD1(OnKeyboardModifiers, void(int));
};

aura::Window* GetFocusedWindow() {
  DCHECK(exo::WMHelper::HasInstance());
  return exo::WMHelper::GetInstance()->GetFocusedWindow();
}

}  // anonymous namespace

class DummyEvent : public ui::Event {
 public:
  DummyEvent() : Event(ui::ET_UNKNOWN, base::TimeTicks(), 0) {}
  ~DummyEvent() override = default;
};

class ArcNotificationContentViewTest : public ash::AshTestBase {
 public:
  ArcNotificationContentViewTest() = default;
  ~ArcNotificationContentViewTest() override = default;

  void SetUp() override {
    ash::AshTestBase::SetUp();

    wm_helper_ = std::make_unique<exo::WMHelper>();
    exo::WMHelper::SetInstance(wm_helper_.get());
    DCHECK(exo::WMHelper::HasInstance());

    surface_manager_ = std::make_unique<ArcNotificationSurfaceManagerImpl>();
    ArcNotificationManager::SetCustomNotificationViewFactory();
  }

  void TearDown() override {
    // Widget and view need to be closed before TearDown() if have been created.
    EXPECT_FALSE(wrapper_widget_);
    EXPECT_FALSE(notification_view_);

    // These may have been initialized in PrepareSurface().
    notification_surface_.reset();
    surface_.reset();
    surface_buffer_.reset();

    surface_manager_.reset();

    DCHECK(exo::WMHelper::HasInstance());
    exo::WMHelper::SetInstance(nullptr);
    wm_helper_.reset();

    ash::AshTestBase::TearDown();
  }

  void PressCloseButton() {
    DummyEvent dummy_event;
    auto* control_buttons_view =
        GetArcNotificationContentView()->control_buttons_view_;
    ASSERT_TRUE(control_buttons_view);
    views::Button* close_button = control_buttons_view->close_button();
    ASSERT_NE(nullptr, close_button);
    control_buttons_view->ButtonPressed(close_button, dummy_event);
  }

  void CreateAndShowNotificationView(const Notification& notification) {
    DCHECK(!notification_view_);

    auto result = CreateNotificationView(notification);
    notification_view_ = std::move(result.first);
    wrapper_widget_ = std::move(result.second);
    wrapper_widget_->Show();
  }

  std::pair<std::unique_ptr<ArcNotificationView>,
            std::unique_ptr<views::Widget>>
  CreateNotificationView(const Notification& notification) {
    std::unique_ptr<ArcNotificationView> notification_view(
        static_cast<ArcNotificationView*>(
            message_center::MessageViewFactory::Create(notification, true)));
    notification_view->set_owned_by_client();
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);

    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.context = ash::Shell::GetPrimaryRootWindow();
    auto wrapper_widget = std::make_unique<views::Widget>();
    wrapper_widget->Init(params);
    wrapper_widget->SetContentsView(notification_view.get());
    wrapper_widget->SetSize(notification_view->GetPreferredSize());

    return std::make_pair(std::move(notification_view),
                          std::move(wrapper_widget));
  }

  void CloseNotificationView() {
    wrapper_widget_->Close();
    wrapper_widget_.reset();

    notification_view_.reset();
  }

  void PrepareSurface(const std::string& notification_key) {
    surface_ = std::make_unique<exo::Surface>();
    notification_surface_ = std::make_unique<exo::NotificationSurface>(
        surface_manager(), surface_.get(), notification_key);

    exo::test::ExoTestHelper exo_test_helper;
    surface_buffer_ =
        std::make_unique<exo::Buffer>(exo_test_helper.CreateGpuMemoryBuffer(
            kNotificationSurfaceBounds.size()));
    surface_->Attach(surface_buffer_.get());

    surface_->Commit();
  }

  Notification CreateNotification(MockArcNotificationItem* notification_item) {
    return Notification(
        message_center::NOTIFICATION_TYPE_CUSTOM,
        notification_item->GetNotificationId(), base::UTF8ToUTF16("title"),
        base::UTF8ToUTF16("message"), gfx::Image(), base::UTF8ToUTF16("arc"),
        GURL(),
        message_center::NotifierId(message_center::NotifierId::ARC_APPLICATION,
                                   "ARC_NOTIFICATION"),
        message_center::RichNotificationData(),
        new ArcNotificationDelegate(notification_item->GetWeakPtr()));
  }

  ArcNotificationSurfaceManagerImpl* surface_manager() {
    return surface_manager_.get();
  }
  views::Widget* widget() { return notification_view_->GetWidget(); }
  exo::Surface* surface() { return surface_.get(); }
  ArcNotificationView* notification_view() { return notification_view_.get(); }

  message_center::NotificationControlButtonsView* GetControlButtonsView()
      const {
    DCHECK(GetArcNotificationContentView());
    DCHECK(GetArcNotificationContentView()->control_buttons_view_);
    return GetArcNotificationContentView()->control_buttons_view_;
  }
  views::Widget* GetControlButtonsWidget() const {
    DCHECK(GetControlButtonsView()->GetWidget());
    return GetControlButtonsView()->GetWidget();
  }

  ArcNotificationContentView* GetArcNotificationContentView() const {
    views::View* view = notification_view_->contents_view_;
    EXPECT_EQ(ArcNotificationContentView::kViewClassName, view->GetClassName());
    return static_cast<ArcNotificationContentView*>(view);
  }
  void ActivateArcNotification() {
    GetArcNotificationContentView()->Activate();
  }

 private:
  std::unique_ptr<exo::WMHelper> wm_helper_;
  std::unique_ptr<ArcNotificationSurfaceManagerImpl> surface_manager_;
  std::unique_ptr<exo::Buffer> surface_buffer_;
  std::unique_ptr<exo::Surface> surface_;
  std::unique_ptr<exo::NotificationSurface> notification_surface_;
  std::unique_ptr<ArcNotificationView> notification_view_;
  std::unique_ptr<views::Widget> wrapper_widget_;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationContentViewTest);
};

TEST_F(ArcNotificationContentViewTest, CreateSurfaceAfterNotification) {
  std::string notification_key("notification id");

  auto notification_item =
      std::make_unique<MockArcNotificationItem>(notification_key);
  Notification notification = CreateNotification(notification_item.get());

  PrepareSurface(notification_key);

  CreateAndShowNotificationView(notification);
  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, CreateSurfaceBeforeNotification) {
  std::string notification_key("notification id");

  PrepareSurface(notification_key);

  auto notification_item =
      std::make_unique<MockArcNotificationItem>(notification_key);
  Notification notification = CreateNotification(notification_item.get());

  CreateAndShowNotificationView(notification);
  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, CreateNotificationWithoutSurface) {
  std::string notification_key("notification id");

  auto notification_item =
      std::make_unique<MockArcNotificationItem>(notification_key);
  Notification notification = CreateNotification(notification_item.get());

  CreateAndShowNotificationView(notification);
  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, CloseButton) {
  std::string notification_key("notification id");

  auto notification_item =
      std::make_unique<MockArcNotificationItem>(notification_key);
  PrepareSurface(notification_key);
  Notification notification = CreateNotification(notification_item.get());
  CreateAndShowNotificationView(notification);

  // Add a notification with the same ID to the message center so we can verify
  // it gets removed when the close button is pressed. It's SIMPLE instead of
  // ARC to avoid surface shutdown issues.
  auto mc_notification = std::make_unique<Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      notification_item->GetNotificationId(), base::UTF8ToUTF16("title"),
      base::UTF8ToUTF16("message"), gfx::Image(), base::UTF8ToUTF16("arc"),
      GURL(),
      message_center::NotifierId(message_center::NotifierId::ARC_APPLICATION,
                                 "ARC_NOTIFICATION"),
      message_center::RichNotificationData(), nullptr);
  MessageCenter::Get()->AddNotification(std::move(mc_notification));

  EXPECT_TRUE(MessageCenter::Get()->FindVisibleNotificationById(
      notification_item->GetNotificationId()));
  PressCloseButton();
  EXPECT_FALSE(MessageCenter::Get()->FindVisibleNotificationById(
      notification_item->GetNotificationId()));

  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, ReuseSurfaceAfterClosing) {
  std::string notification_key("notification id");

  auto notification_item =
      std::make_unique<MockArcNotificationItem>(notification_key);
  Notification notification = CreateNotification(notification_item.get());

  PrepareSurface(notification_key);

  // Use the created surface.
  CreateAndShowNotificationView(notification);
  CloseNotificationView();

  // Reuse.
  CreateAndShowNotificationView(notification);
  CloseNotificationView();

  // Reuse again.
  CreateAndShowNotificationView(notification);
  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, ReuseAndCloseSurfaceBeforeClosing) {
  std::string notification_key("notification id");

  auto notification_item =
      std::make_unique<MockArcNotificationItem>(notification_key);
  Notification notification = CreateNotification(notification_item.get());

  PrepareSurface(notification_key);

  // Create the first view.
  auto result = CreateNotificationView(notification);
  auto notification_view = std::move(result.first);
  auto wrapper_widget = std::move(result.second);
  wrapper_widget->Show();

  // Create the second view.
  CreateAndShowNotificationView(notification);
  // Close second view.
  CloseNotificationView();

  // Close the first view.
  wrapper_widget->Close();
  wrapper_widget.reset();
  notification_view.reset();
}

TEST_F(ArcNotificationContentViewTest, ReuseSurfaceBeforeClosing) {
  std::string notification_key("notification id");

  auto notification_item =
      std::make_unique<MockArcNotificationItem>(notification_key);
  Notification notification = CreateNotification(notification_item.get());

  PrepareSurface(notification_key);

  // Create the first view.
  auto result = CreateNotificationView(notification);
  auto notification_view = std::move(result.first);
  auto wrapper_widget = std::move(result.second);
  wrapper_widget->Show();

  // Create the second view.
  CreateAndShowNotificationView(notification);

  // Close the first view.
  wrapper_widget->Close();
  wrapper_widget.reset();
  notification_view.reset();

  // Close second view.
  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, Activate) {
  std::string key("notification id");
  auto notification_item = std::make_unique<MockArcNotificationItem>(key);
  auto notification = CreateNotification(notification_item.get());

  PrepareSurface(key);
  CreateAndShowNotificationView(notification);

  EXPECT_FALSE(GetFocusedWindow());
  ActivateArcNotification();
  EXPECT_EQ(surface()->window(), GetFocusedWindow());

  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, ActivateOnClick) {
  std::string key("notification id");
  auto notification_item = std::make_unique<MockArcNotificationItem>(key);
  auto notification = CreateNotification(notification_item.get());

  PrepareSurface(key);
  CreateAndShowNotificationView(notification);

  EXPECT_FALSE(GetFocusedWindow());
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow(),
                                     kNotificationSurfaceBounds.CenterPoint());
  generator.PressLeftButton();
  EXPECT_EQ(surface()->window(), GetFocusedWindow());

  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, AcceptInputTextWithActivate) {
  std::string key("notification id");
  auto notification_item = std::make_unique<MockArcNotificationItem>(key);
  auto notification = CreateNotification(notification_item.get());

  PrepareSurface(key);
  CreateAndShowNotificationView(notification);

  EXPECT_FALSE(GetFocusedWindow());
  ActivateArcNotification();
  EXPECT_EQ(surface()->window(), GetFocusedWindow());

  MockKeyboardDelegate delegate;
  EXPECT_CALL(delegate, CanAcceptKeyboardEventsForSurface(surface()))
      .WillOnce(testing::Return(true));
  exo::Seat seat;
  auto keyboard = std::make_unique<exo::Keyboard>(&delegate, &seat);

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_A, true));
  generator.PressKey(ui::VKEY_A, 0);

  keyboard.reset();

  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, NotAcceptInputTextWithoutActivate) {
  std::string key("notification id");
  auto notification_item = std::make_unique<MockArcNotificationItem>(key);
  auto notification = CreateNotification(notification_item.get());

  PrepareSurface(key);
  CreateAndShowNotificationView(notification);
  EXPECT_FALSE(GetFocusedWindow());

  MockKeyboardDelegate delegate;
  EXPECT_CALL(delegate, CanAcceptKeyboardEventsForSurface(surface())).Times(0);
  exo::Seat seat;
  auto keyboard = std::make_unique<exo::Keyboard>(&delegate, &seat);

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, testing::_, testing::_))
      .Times(0);
  generator.PressKey(ui::VKEY_A, 0);

  keyboard.reset();

  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, TraversalFocus) {
  const bool reverse = false;

  std::string key("notification id");
  auto notification_item = std::make_unique<MockArcNotificationItem>(key);
  PrepareSurface(key);
  auto notification = CreateNotification(notification_item.get());
  CreateAndShowNotificationView(notification);

  views::FocusManager* focus_manager = notification_view()->GetFocusManager();

  views::View* view =
      focus_manager->GetNextFocusableView(nullptr, widget(), reverse, true);
  EXPECT_EQ(GetArcNotificationContentView(), view);

  view = focus_manager->GetNextFocusableView(view, nullptr, reverse, true);
  EXPECT_EQ(GetControlButtonsView()->settings_button(), view);

  view = focus_manager->GetNextFocusableView(view, nullptr, reverse, true);
  EXPECT_EQ(GetControlButtonsView()->close_button(), view);

  view = focus_manager->GetNextFocusableView(view, nullptr, reverse, true);
  EXPECT_EQ(nullptr, view);

  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, TraversalFocusReverse) {
  const bool reverse = true;

  std::string key("notification id");
  auto notification_item = std::make_unique<MockArcNotificationItem>(key);
  PrepareSurface(key);
  auto notification = CreateNotification(notification_item.get());
  CreateAndShowNotificationView(notification);

  views::FocusManager* focus_manager = notification_view()->GetFocusManager();

  views::View* view =
      focus_manager->GetNextFocusableView(nullptr, widget(), reverse, true);
  EXPECT_EQ(GetControlButtonsView()->close_button(), view);

  view = focus_manager->GetNextFocusableView(view, nullptr, reverse, true);
  EXPECT_EQ(GetControlButtonsView()->settings_button(), view);

  view = focus_manager->GetNextFocusableView(view, nullptr, reverse, true);
  EXPECT_EQ(GetArcNotificationContentView(), view);

  view = focus_manager->GetNextFocusableView(view, nullptr, reverse, true);
  EXPECT_EQ(nullptr, view);

  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, TraversalFocusByTabKey) {
  const std::string key("notification id");
  auto notification_item = std::make_unique<MockArcNotificationItem>(key);
  PrepareSurface(key);
  auto notification = CreateNotification(notification_item.get());
  CreateAndShowNotificationView(notification);
  ActivateArcNotification();

  views::FocusManager* focus_manager = notification_view()->GetFocusManager();
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());

  generator.PressKey(ui::VKEY_TAB, 0);
  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_EQ(GetArcNotificationContentView(), focus_manager->GetFocusedView());

  generator.PressKey(ui::VKEY_TAB, 0);
  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_EQ(GetControlButtonsView()->settings_button(),
            focus_manager->GetFocusedView());

  generator.PressKey(ui::VKEY_TAB, 0);
  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_EQ(GetControlButtonsView()->close_button(),
            focus_manager->GetFocusedView());

  generator.PressKey(ui::VKEY_TAB, 0);
  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_EQ(GetArcNotificationContentView(), focus_manager->GetFocusedView());

  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, TraversalFocusReverseByShiftTab) {
  std::string key("notification id");

  auto notification_item = std::make_unique<MockArcNotificationItem>(key);
  PrepareSurface(key);
  auto notification = CreateNotification(notification_item.get());
  CreateAndShowNotificationView(notification);
  ActivateArcNotification();

  views::FocusManager* focus_manager = notification_view()->GetFocusManager();
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());

  generator.PressKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_EQ(GetControlButtonsView()->close_button(),
            focus_manager->GetFocusedView());

  generator.PressKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_EQ(GetControlButtonsView()->settings_button(),
            focus_manager->GetFocusedView());

  generator.PressKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_EQ(GetArcNotificationContentView(), focus_manager->GetFocusedView());

  generator.PressKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(focus_manager->GetFocusedView());
  EXPECT_EQ(GetControlButtonsView()->close_button(),
            focus_manager->GetFocusedView());

  CloseNotificationView();
}

}  // namespace arc
