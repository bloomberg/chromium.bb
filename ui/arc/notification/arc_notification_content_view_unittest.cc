// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "ui/arc/notification/arc_notification_content_view.h"
#include "ui/arc/notification/arc_notification_delegate.h"
#include "ui/arc/notification/arc_notification_item.h"
#include "ui/arc/notification/arc_notification_surface.h"
#include "ui/arc/notification/arc_notification_view.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/views/message_center_controller.h"
#include "ui/message_center/views/message_view_factory.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/views_test_base.h"

namespace arc {

namespace {

constexpr char kNotificationIdPrefix[] = "ARC_NOTIFICATION_";

class MockNotificationSurface : public ArcNotificationSurface {
 public:
  MockNotificationSurface(const std::string& notification_key,
                          std::unique_ptr<aura::Window> window)
      : notification_key_(notification_key), window_(std::move(window)) {}

  gfx::Size GetSize() const override { return gfx::Size(100, 200); }

  void Attach(views::NativeViewHost* nvh) override {
    native_view_host_ = nvh;
    nvh->Attach(window_.get());
  }

  void Detach() override {
    EXPECT_TRUE(native_view_host_);
    EXPECT_EQ(window_.get(), native_view_host_->native_view());
    native_view_host_->Detach();
    native_view_host_ = nullptr;
  }

  bool IsAttached() const override { return native_view_host_; }

  aura::Window* GetWindow() const override { return window_.get(); }
  aura::Window* GetContentWindow() const override { return window_.get(); }

  const std::string& GetNotificationKey() const override {
    return notification_key_;
  }

 private:
  std::string notification_key_;
  std::unique_ptr<aura::Window> window_;
  views::NativeViewHost* native_view_host_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MockNotificationSurface);
};

class TestNotificationSurfaceManager : public ArcNotificationSurfaceManager {
 public:
  TestNotificationSurfaceManager() = default;

  void PrepareSurface(std::string& notification_key) {
    auto surface_window = base::MakeUnique<aura::Window>(&window_delegate_);
    surface_window->SetType(aura::client::WINDOW_TYPE_CONTROL);
    surface_window->Init(ui::LAYER_NOT_DRAWN);
    surface_window->set_owned_by_parent(false);
    surface_window->SetBounds(gfx::Rect(0, 0, 100, 200));

    surface_map_[notification_key] = base::MakeUnique<MockNotificationSurface>(
        notification_key, std::move(surface_window));
  }
  size_t surface_found_count() const { return surface_found_count_; }

  ArcNotificationSurface* GetArcSurface(
      const std::string& notification_key) const override {
    auto it = surface_map_.find(notification_key);
    if (it != surface_map_.end()) {
      ++surface_found_count_;
      return it->second.get();
    }
    return nullptr;
  }
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}

 private:
  // Mutable for modifying in const method.
  mutable int surface_found_count_ = 0;

  aura::test::TestWindowDelegate window_delegate_;
  std::map<std::string, std::unique_ptr<ArcNotificationSurface>> surface_map_;

  DISALLOW_COPY_AND_ASSIGN(TestNotificationSurfaceManager);
};

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
  bool GetPinned() const override { return false; }
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

class TestMessageCenterController
    : public message_center::MessageCenterController {
 public:
  TestMessageCenterController() = default;

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
      const message_center::NotifierId& notifier_id,
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

  void UpdateNotificationSize(const std::string& notification_id) override {}

  bool IsRemoved(const std::string& notification_id) const {
    return (removed_ids_.find(notification_id) != removed_ids_.end());
  }

 private:
  std::set<std::string> removed_ids_;

  DISALLOW_COPY_AND_ASSIGN(TestMessageCenterController);
};

class DummyEvent : public ui::Event {
 public:
  DummyEvent() : Event(ui::ET_UNKNOWN, base::TimeTicks(), 0) {}
  ~DummyEvent() override = default;
};

class ArcNotificationContentViewTest : public views::ViewsTestBase {
 public:
  ArcNotificationContentViewTest() = default;
  ~ArcNotificationContentViewTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();

    surface_manager_ = base::MakeUnique<TestNotificationSurfaceManager>();
  }

  void TearDown() override {
    // Widget and view need to be closed before TearDown() if have been created.
    EXPECT_FALSE(wrapper_widget_);
    EXPECT_FALSE(notification_view_);

    surface_manager_.reset();

    views::ViewsTestBase::TearDown();
  }

  void PressCloseButton() {
    DummyEvent dummy_event;
    views::ImageButton* close_button =
        GetArcNotificationContentView()->close_button_.get();
    ASSERT_NE(nullptr, close_button);
    GetArcNotificationContentView()->ButtonPressed(close_button, dummy_event);
  }

  void CreateAndShowNotificationView(
      const message_center::Notification& notification) {
    DCHECK(!notification_view_);

    notification_view_.reset(static_cast<ArcNotificationView*>(
        message_center::MessageViewFactory::Create(controller(), notification,
                                                   true)));
    notification_view_->set_owned_by_client();
    views::Widget::InitParams params(
        CreateParams(views::Widget::InitParams::TYPE_POPUP));

    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    wrapper_widget_ = base::MakeUnique<views::Widget>();
    wrapper_widget_->Init(params);
    wrapper_widget_->SetContentsView(notification_view_.get());
    wrapper_widget_->SetSize(notification_view_->GetPreferredSize());
  }

  void CloseNotificationView() {
    wrapper_widget_->Close();
    wrapper_widget_.reset();

    notification_view_.reset();
  }

  message_center::Notification CreateNotification(
      MockArcNotificationItem* notification_item) {
    return message_center::Notification(
        message_center::NOTIFICATION_TYPE_CUSTOM,
        notification_item->GetNotificationId(), base::UTF8ToUTF16("title"),
        base::UTF8ToUTF16("message"), gfx::Image(), base::UTF8ToUTF16("arc"),
        GURL(),
        message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                   "ARC_NOTIFICATION"),
        message_center::RichNotificationData(),
        new ArcNotificationDelegate(notification_item->GetWeakPtr()));
  }

  TestMessageCenterController* controller() { return &controller_; }
  TestNotificationSurfaceManager* surface_manager() {
    return surface_manager_.get();
  }
  views::Widget* widget() { return notification_view_->GetWidget(); }

  ArcNotificationContentView* GetArcNotificationContentView() {
    views::View* view = notification_view_->contents_view_;
    EXPECT_EQ(ArcNotificationContentView::kViewClassName, view->GetClassName());
    return static_cast<ArcNotificationContentView*>(view);
  }

  TestNotificationSurfaceManager* surface_manager() const {
    return surface_manager_.get();
  }

 private:
  TestMessageCenterController controller_;
  std::unique_ptr<TestNotificationSurfaceManager> surface_manager_;
  std::unique_ptr<ArcNotificationView> notification_view_;
  std::unique_ptr<views::Widget> wrapper_widget_;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationContentViewTest);
};

TEST_F(ArcNotificationContentViewTest, CreateSurfaceAfterNotification) {
  std::string notification_key("notification id");

  auto notification_item =
      base::MakeUnique<MockArcNotificationItem>(notification_key);
  message_center::Notification notification =
      CreateNotification(notification_item.get());

  surface_manager()->PrepareSurface(notification_key);

  CreateAndShowNotificationView(notification);
  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, CreateSurfaceBeforeNotification) {
  std::string notification_key("notification id");

  surface_manager()->PrepareSurface(notification_key);

  auto notification_item =
      base::MakeUnique<MockArcNotificationItem>(notification_key);
  message_center::Notification notification =
      CreateNotification(notification_item.get());

  CreateAndShowNotificationView(notification);
  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, CreateNotificationWithoutSurface) {
  std::string notification_key("notification id");

  auto notification_item =
      base::MakeUnique<MockArcNotificationItem>(notification_key);
  message_center::Notification notification =
      CreateNotification(notification_item.get());

  CreateAndShowNotificationView(notification);
  CloseNotificationView();
}

TEST_F(ArcNotificationContentViewTest, CloseButton) {
  std::string notification_key("notification id");

  auto notification_item =
      base::MakeUnique<MockArcNotificationItem>(notification_key);
  surface_manager()->PrepareSurface(notification_key);
  message_center::Notification notification =
      CreateNotification(notification_item.get());
  CreateAndShowNotificationView(notification);

  EXPECT_EQ(1u, surface_manager()->surface_found_count());
  EXPECT_FALSE(controller()->IsRemoved(notification_item->GetNotificationId()));
  PressCloseButton();
  EXPECT_TRUE(controller()->IsRemoved(notification_item->GetNotificationId()));

  CloseNotificationView();
}

}  // namespace arc
