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
#include "base/strings/utf_string_conversions.h"
#include "components/exo/buffer.h"
#include "components/exo/keyboard.h"
#include "components/exo/keyboard_delegate.h"
#include "components/exo/notification_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/wm_helper_ash.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/arc/notification/arc_notification_content_view.h"
#include "ui/arc/notification/arc_notification_delegate.h"
#include "ui/arc/notification/arc_notification_item.h"
#include "ui/arc/notification/arc_notification_manager.h"
#include "ui/arc/notification/arc_notification_surface.h"
#include "ui/arc/notification/arc_notification_surface_manager_impl.h"
#include "ui/arc/notification/arc_notification_view.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/views/message_view_delegate.h"
#include "ui/message_center/views/message_view_factory.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/padded_button.h"
#include "ui/views/test/views_test_base.h"

namespace arc {

namespace {

constexpr char kNotificationIdPrefix[] = "ARC_NOTIFICATION_";
constexpr gfx::Rect kNotificationSurfaceBounds(100, 100, 300, 300);

class MockKeyboardDelegate : public exo::KeyboardDelegate {
 public:
  MockKeyboardDelegate() = default;

  // Overridden from KeyboardDelegate:
  MOCK_METHOD1(OnKeyboardDestroying, void(exo::Keyboard*));
  MOCK_CONST_METHOD1(CanAcceptKeyboardEventsForSurface, bool(exo::Surface*));
  MOCK_METHOD2(OnKeyboardEnter,
               void(exo::Surface*, const std::vector<ui::DomCode>&));
  MOCK_METHOD1(OnKeyboardLeave, void(exo::Surface*));
  MOCK_METHOD3(OnKeyboardKey, uint32_t(base::TimeTicks, ui::DomCode, bool));
  MOCK_METHOD1(OnKeyboardModifiers, void(int));
};

class TestWMHelper : public exo::WMHelper {
 public:
  TestWMHelper() = default;
  ~TestWMHelper() override = default;

  const display::ManagedDisplayInfo& GetDisplayInfo(
      int64_t display_id) const override {
    static const display::ManagedDisplayInfo info;
    return info;
  }
  aura::Window* GetPrimaryDisplayContainer(int container_id) override {
    return nullptr;
  }
  aura::Window* GetActiveWindow() const override { return nullptr; }
  aura::Window* GetFocusedWindow() const override { return nullptr; }
  ui::CursorSize GetCursorSize() const override {
    return ui::CursorSize::kNormal;
  }
  const display::Display& GetCursorDisplay() const override {
    static const display::Display display;
    return display;
  }
  void AddPreTargetHandler(ui::EventHandler* handler) override {}
  void PrependPreTargetHandler(ui::EventHandler* handler) override {}
  void RemovePreTargetHandler(ui::EventHandler* handler) override {}
  void AddPostTargetHandler(ui::EventHandler* handler) override {}
  void RemovePostTargetHandler(ui::EventHandler* handler) override {}
  bool IsTabletModeWindowManagerEnabled() const override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWMHelper);
};

aura::Window* GetFocusedWindow() {
  DCHECK(exo::WMHelper::HasInstance());
  return exo::WMHelper::GetInstance()->GetFocusedWindow();
}

}  // anonymous namespace

class MockArcNotificationItem : public ArcNotificationItem {
 public:
  MockArcNotificationItem(const std::string& notification_key)
      : notification_key_(notification_key),
        notification_id_(kNotificationIdPrefix + notification_key),
        weak_factory_(this) {}

  // Methods for testing.
  size_t count_close() { return count_close_; }
  base::WeakPtr<MockArcNotificationItem> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Overriding methods for testing.
  void Close(bool by_user) override { count_close_++; }
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
  bool IsOpeningSettingsSupported() const override { return true; }
  mojom::ArcNotificationExpandState GetExpandState() const override {
    return mojom::ArcNotificationExpandState::FIXED_SIZE;
  }
  mojom::ArcNotificationShownContents GetShownContents() const override {
    return mojom::ArcNotificationShownContents::CONTENTS_SHOWN;
  }
  const base::string16& GetAccessibleName() const override {
    return base::EmptyString16();
  };
  void OnUpdatedFromAndroid(mojom::ArcNotificationDataPtr data) override {}

 private:
  std::string notification_key_;
  std::string notification_id_;
  gfx::ImageSkia snapshot_;
  size_t count_close_ = 0;

  base::WeakPtrFactory<MockArcNotificationItem> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockArcNotificationItem);
};

class TestMessageViewDelegate : public message_center::MessageViewDelegate {
 public:
  TestMessageViewDelegate() = default;

  // MessageViewDelegate
  void ClickOnNotification(const std::string& notification_id) override {
    // For this test, this method should not be invoked.
    NOTREACHED();
  }

  void RemoveNotification(const std::string& notification_id,
                          bool by_user) override {
    removed_ids_.insert(notification_id);
  }

  std::unique_ptr<ui::MenuModel> CreateMenuModel(
      const message_center::Notification& notification) override {
    // For this test, this method should not be invoked.
    NOTREACHED();
    return nullptr;
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

  void UpdateNotificationSize(const std::string& notification_id) override {}

  bool IsRemoved(const std::string& notification_id) const {
    return (removed_ids_.find(notification_id) != removed_ids_.end());
  }

 private:
  std::set<std::string> removed_ids_;

  DISALLOW_COPY_AND_ASSIGN(TestMessageViewDelegate);
};

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

    wm_helper_ = std::make_unique<exo::WMHelperAsh>();
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

  void CreateAndShowNotificationView(
      const message_center::Notification& notification) {
    DCHECK(!notification_view_);

    auto result = CreateNotificationView(notification);
    notification_view_ = std::move(result.first);
    wrapper_widget_ = std::move(result.second);
    wrapper_widget_->Show();
  }

  std::pair<std::unique_ptr<ArcNotificationView>,
            std::unique_ptr<views::Widget>>
  CreateNotificationView(const message_center::Notification& notification) {
    std::unique_ptr<ArcNotificationView> notification_view(
        static_cast<ArcNotificationView*>(
            message_center::MessageViewFactory::Create(controller(),
                                                       notification, true)));
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

  message_center::Notification CreateNotification(
      MockArcNotificationItem* notification_item) {
    return message_center::Notification(
        message_center::NOTIFICATION_TYPE_CUSTOM,
        notification_item->GetNotificationId(), base::UTF8ToUTF16("title"),
        base::UTF8ToUTF16("message"), gfx::Image(), base::UTF8ToUTF16("arc"),
        GURL(),
        message_center::NotifierId(message_center::NotifierId::ARC_APPLICATION,
                                   "ARC_NOTIFICATION"),
        message_center::RichNotificationData(),
        new ArcNotificationDelegate(notification_item->GetWeakPtr()));
  }

  TestMessageViewDelegate* controller() { return &controller_; }
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
  TestMessageViewDelegate controller_;
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
  message_center::Notification notification =
      CreateNotification(notification_item.get());

  PrepareSurface(notification_key);

  CreateAndShowNotificationView(notification);
  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, CreateSurfaceBeforeNotification) {
  std::string notification_key("notification id");

  PrepareSurface(notification_key);

  auto notification_item =
      std::make_unique<MockArcNotificationItem>(notification_key);
  message_center::Notification notification =
      CreateNotification(notification_item.get());

  CreateAndShowNotificationView(notification);
  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, CreateNotificationWithoutSurface) {
  std::string notification_key("notification id");

  auto notification_item =
      std::make_unique<MockArcNotificationItem>(notification_key);
  message_center::Notification notification =
      CreateNotification(notification_item.get());

  CreateAndShowNotificationView(notification);
  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, CloseButton) {
  std::string notification_key("notification id");

  auto notification_item =
      std::make_unique<MockArcNotificationItem>(notification_key);
  PrepareSurface(notification_key);
  message_center::Notification notification =
      CreateNotification(notification_item.get());
  CreateAndShowNotificationView(notification);

  EXPECT_FALSE(controller()->IsRemoved(notification_item->GetNotificationId()));
  PressCloseButton();
  EXPECT_TRUE(controller()->IsRemoved(notification_item->GetNotificationId()));

  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, ReuseSurfaceAfterClosing) {
  std::string notification_key("notification id");

  auto notification_item =
      std::make_unique<MockArcNotificationItem>(notification_key);
  message_center::Notification notification =
      CreateNotification(notification_item.get());

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
  message_center::Notification notification =
      CreateNotification(notification_item.get());

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
  message_center::Notification notification =
      CreateNotification(notification_item.get());

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
  auto keyboard = std::make_unique<exo::Keyboard>(&delegate);

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
  auto keyboard = std::make_unique<exo::Keyboard>(&delegate);

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
