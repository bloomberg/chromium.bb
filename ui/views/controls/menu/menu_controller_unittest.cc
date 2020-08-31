// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_controller.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/ax_event_observer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/menu/menu_controller_delegate.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_host.h"
#include "ui/views/controls/menu/menu_host_root_view.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/test/menu_test_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget_utils.h"

#if defined(USE_AURA)
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_client_observer.h"
#include "ui/aura/null_window_targeter.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/views/controls/menu/menu_pre_target_handler.h"
#endif

#if defined(USE_X11)
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/gfx/x/x11.h"  // nogncheck
#endif

#if defined(OS_CHROMEOS)
#include "ui/base/ui_base_features.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace views {
namespace test {

namespace {

bool ShouldIgnoreScreenBoundsForMenus() {
#if defined(USE_OZONE)
  // Wayland requires placing menus is screen coordinates. See comment in
  // ozone_platform_wayland.cc.
  return ui::OzonePlatform::GetInstance()
      ->GetPlatformProperties()
      .ignore_screen_bounds_for_menus;
#else
  return false;
#endif
}

// Test implementation of MenuControllerDelegate that only reports the values
// called of OnMenuClosed.
class TestMenuControllerDelegate : public internal::MenuControllerDelegate {
 public:
  TestMenuControllerDelegate();
  ~TestMenuControllerDelegate() override = default;

  int on_menu_closed_called() { return on_menu_closed_called_; }

  NotifyType on_menu_closed_notify_type() {
    return on_menu_closed_notify_type_;
  }

  MenuItemView* on_menu_closed_menu() { return on_menu_closed_menu_; }

  int on_menu_closed_mouse_event_flags() {
    return on_menu_closed_mouse_event_flags_;
  }

  // On a subsequent call to OnMenuClosed |controller| will be deleted.
  void set_on_menu_closed_callback(base::RepeatingClosure callback) {
    on_menu_closed_callback_ = std::move(callback);
  }

  // internal::MenuControllerDelegate:
  void OnMenuClosed(NotifyType type,
                    MenuItemView* menu,
                    int mouse_event_flags) override;
  void SiblingMenuCreated(MenuItemView* menu) override;

 private:
  // Number of times OnMenuClosed has been called.
  int on_menu_closed_called_ = 0;

  // The values passed on the last call of OnMenuClosed.
  NotifyType on_menu_closed_notify_type_ = NOTIFY_DELEGATE;
  MenuItemView* on_menu_closed_menu_ = nullptr;
  int on_menu_closed_mouse_event_flags_ = 0;

  // Optional callback triggered during OnMenuClosed
  base::RepeatingClosure on_menu_closed_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestMenuControllerDelegate);
};

TestMenuControllerDelegate::TestMenuControllerDelegate() = default;

void TestMenuControllerDelegate::OnMenuClosed(NotifyType type,
                                              MenuItemView* menu,
                                              int mouse_event_flags) {
  on_menu_closed_called_++;
  on_menu_closed_notify_type_ = type;
  on_menu_closed_menu_ = menu;
  on_menu_closed_mouse_event_flags_ = mouse_event_flags;
  if (!on_menu_closed_callback_.is_null())
    on_menu_closed_callback_.Run();
}

void TestMenuControllerDelegate::SiblingMenuCreated(MenuItemView* menu) {}

class SubmenuViewShown : public SubmenuView {
 public:
  using SubmenuView::SubmenuView;
  ~SubmenuViewShown() override = default;
  bool IsShowing() const override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(SubmenuViewShown);
};

class TestEventHandler : public ui::EventHandler {
 public:
  TestEventHandler() = default;

  void OnTouchEvent(ui::TouchEvent* event) override {
    switch (event->type()) {
      case ui::ET_TOUCH_PRESSED:
        outstanding_touches_++;
        break;
      case ui::ET_TOUCH_RELEASED:
      case ui::ET_TOUCH_CANCELLED:
        outstanding_touches_--;
        break;
      default:
        break;
    }
  }

  int outstanding_touches() const { return outstanding_touches_; }

 private:
  int outstanding_touches_ = 0;
  DISALLOW_COPY_AND_ASSIGN(TestEventHandler);
};

// A test widget that counts gesture events.
class GestureTestWidget : public Widget {
 public:
  GestureTestWidget() = default;

  void OnGestureEvent(ui::GestureEvent* event) override { ++gesture_count_; }

  int gesture_count() const { return gesture_count_; }

 private:
  int gesture_count_ = 0;
  DISALLOW_COPY_AND_ASSIGN(GestureTestWidget);
};

#if defined(USE_AURA)
// A DragDropClient which does not trigger a nested run loop. Instead a
// callback is triggered during StartDragAndDrop in order to allow testing.
class TestDragDropClient : public aura::client::DragDropClient {
 public:
  explicit TestDragDropClient(base::RepeatingClosure callback)
      : start_drag_and_drop_callback_(std::move(callback)),
        drag_in_progress_(false) {}
  ~TestDragDropClient() override = default;

  // aura::client::DragDropClient:
  int StartDragAndDrop(std::unique_ptr<ui::OSExchangeData> data,
                       aura::Window* root_window,
                       aura::Window* source_window,
                       const gfx::Point& screen_location,
                       int operation,
                       ui::DragDropTypes::DragEventSource source) override;
  void DragCancel() override;
  bool IsDragDropInProgress() override;

  void AddObserver(aura::client::DragDropClientObserver* observer) override {}
  void RemoveObserver(aura::client::DragDropClientObserver* observer) override {
  }

 private:
  base::RepeatingClosure start_drag_and_drop_callback_;
  bool drag_in_progress_;

  DISALLOW_COPY_AND_ASSIGN(TestDragDropClient);
};

int TestDragDropClient::StartDragAndDrop(
    std::unique_ptr<ui::OSExchangeData> data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& screen_location,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  drag_in_progress_ = true;
  start_drag_and_drop_callback_.Run();
  return 0;
}

void TestDragDropClient::DragCancel() {
  drag_in_progress_ = false;
}
bool TestDragDropClient::IsDragDropInProgress() {
  return drag_in_progress_;
}

#endif  // defined(USE_AURA)

// View which cancels the menu it belongs to on mouse press.
class CancelMenuOnMousePressView : public View {
 public:
  explicit CancelMenuOnMousePressView(MenuController* controller)
      : controller_(controller) {}

  // View:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    controller_->Cancel(MenuController::ExitType::kAll);
    return true;
  }

  // This is needed to prevent the view from being "squashed" to zero height
  // when the menu which owns it is shown. In such state the logic which
  // determines if the menu contains the mouse press location doesn't work.
  gfx::Size CalculatePreferredSize() const override { return size(); }

 private:
  MenuController* controller_;
};

class TestAXEventObserver : public views::AXEventObserver {
 public:
  TestAXEventObserver() { views::AXEventManager::Get()->AddObserver(this); }
  ~TestAXEventObserver() override {
    views::AXEventManager::Get()->RemoveObserver(this);
  }

  bool saw_selected_children_changed_ = false;

  void OnViewEvent(views::View*, ax::mojom::Event event_type) override {
    if (event_type == ax::mojom::Event::kSelectedChildrenChanged) {
      saw_selected_children_changed_ = true;
    }
  }
};

}  // namespace

class TestMenuItemViewShown : public MenuItemView {
 public:
  explicit TestMenuItemViewShown(MenuDelegate* delegate)
      : MenuItemView(delegate) {
    submenu_ = new SubmenuViewShown(this);
  }
  ~TestMenuItemViewShown() override = default;

  void SetController(MenuController* controller) { set_controller(controller); }

  void AddEmptyMenusForTest() { AddEmptyMenus(); }

  void SetActualMenuPosition(MenuItemView::MenuPosition position) {
    set_actual_menu_position(position);
  }
  MenuItemView::MenuPosition ActualMenuPosition() {
    return actual_menu_position();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMenuItemViewShown);
};

class TestMenuItemViewNotShown : public MenuItemView {
 public:
  explicit TestMenuItemViewNotShown(MenuDelegate* delegate)
      : MenuItemView(delegate) {
    submenu_ = new SubmenuView(this);
  }
  ~TestMenuItemViewNotShown() override = default;

  void SetController(MenuController* controller) { set_controller(controller); }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMenuItemViewNotShown);
};

struct MenuBoundsOptions {
 public:
  gfx::Rect anchor_bounds = gfx::Rect(500, 500, 10, 10);
  gfx::Rect monitor_bounds = gfx::Rect(0, 0, 1000, 1000);
  gfx::Size menu_size = gfx::Size(100, 100);
  MenuAnchorPosition menu_anchor = MenuAnchorPosition::kTopLeft;
  MenuItemView::MenuPosition menu_position =
      MenuItemView::MenuPosition::kBestFit;
};

class MenuControllerTest : public ViewsTestBase,
                           public testing::WithParamInterface<bool> {
 public:
  MenuControllerTest() = default;

  ~MenuControllerTest() override = default;

  // ViewsTestBase:
  void SetUp() override {
    if (testing::UnitTest::GetInstance()->current_test_info()->value_param()) {
      // Setup right to left environment if necessary.
      if (GetParam())
        base::i18n::SetRTLForTesting(true);
    }

    auto test_views_delegate = std::make_unique<ReleaseRefTestViewsDelegate>();
    test_views_delegate_ = test_views_delegate.get();
    // ViewsTestBase takes ownership, destroying during Teardown.
    set_views_delegate(std::move(test_views_delegate));
    ViewsTestBase::SetUp();
    Init();
    ASSERT_TRUE(base::MessageLoopCurrentForUI::IsSet());
  }

  void TearDown() override {
    owner_->CloseNow();
    DestroyMenuController();
    ViewsTestBase::TearDown();
    base::i18n::SetRTLForTesting(false);
  }

  void ReleaseTouchId(int id) { event_generator_->ReleaseTouchId(id); }

  void PressKey(ui::KeyboardCode key_code) {
    event_generator_->PressKey(key_code, 0);
  }

  void DispatchKey(ui::KeyboardCode key_code) {
    ui::KeyEvent event(ui::EventType::ET_KEY_PRESSED, key_code, 0);
    menu_controller_->OnWillDispatchKeyEvent(&event);
  }

  gfx::Rect CalculateMenuBounds(const MenuBoundsOptions& options) {
    SetUpMenuControllerForCalculateBounds(options);
    bool is_leading;
    return menu_controller_->CalculateMenuBounds(menu_item_.get(), true,
                                                 &is_leading);
  }

  gfx::Rect CalculateBubbleMenuBounds(const MenuBoundsOptions& options,
                                      MenuItemView* menu_item) {
    SetUpMenuControllerForCalculateBounds(options);
    bool is_leading;
    return menu_controller_->CalculateBubbleMenuBounds(menu_item, true,
                                                       &is_leading);
  }

  gfx::Rect CalculateBubbleMenuBounds(const MenuBoundsOptions& options) {
    return CalculateBubbleMenuBounds(options, menu_item_.get());
  }

#if defined(USE_AURA)
  // Verifies that a non-nested menu fully closes when receiving an escape key.
  void TestAsyncEscapeKey() {
    ui::KeyEvent event(ui::EventType::ET_KEY_PRESSED, ui::VKEY_ESCAPE, 0);
    menu_controller_->OnWillDispatchKeyEvent(&event);
  }

  // Verifies that an open menu receives a cancel event, and closes.
  void TestCancelEvent() {
    EXPECT_EQ(MenuController::ExitType::kNone, menu_controller_->exit_type());
    ui::CancelModeEvent cancel_event;
    event_generator_->Dispatch(&cancel_event);
    EXPECT_EQ(MenuController::ExitType::kAll, menu_controller_->exit_type());
  }
#endif  // defined(USE_AURA)

  // Verifies the state of the |menu_controller_| before destroying it.
  void VerifyDragCompleteThenDestroy() {
    EXPECT_FALSE(menu_controller()->drag_in_progress());
    EXPECT_EQ(MenuController::ExitType::kAll, menu_controller()->exit_type());
    DestroyMenuController();
  }

  // Setups |menu_controller_delegate_| to be destroyed when OnMenuClosed is
  // called.
  void TestDragCompleteThenDestroyOnMenuClosed() {
    menu_controller_delegate_->set_on_menu_closed_callback(
        base::BindRepeating(&MenuControllerTest::VerifyDragCompleteThenDestroy,
                            base::Unretained(this)));
  }

  // Tests destroying the active |menu_controller_| and replacing it with a new
  // active instance.
  void TestMenuControllerReplacementDuringDrag() {
    DestroyMenuController();
    menu_item()->GetSubmenu()->Close();
    const bool for_drop = false;
    menu_controller_ =
        new MenuController(for_drop, menu_controller_delegate_.get());
    menu_controller_->owner_ = owner_.get();
    menu_controller_->showing_ = true;
  }

  // Tests that the menu does not destroy itself when canceled during a drag.
  void TestCancelAllDuringDrag() {
    menu_controller_->Cancel(MenuController::ExitType::kAll);
    EXPECT_EQ(0, menu_controller_delegate_->on_menu_closed_called());
  }

  // Tests that destroying the menu during ViewsDelegate::ReleaseRef does not
  // cause a crash.
  void TestDestroyedDuringViewsRelease() {
    // |test_views_delegate_| is owned by views::ViewsTestBase and not deleted
    // until TearDown. MenuControllerTest outlives it.
    test_views_delegate_->set_release_ref_callback(base::BindRepeating(
        &MenuControllerTest::DestroyMenuController, base::Unretained(this)));
    menu_controller_->ExitMenu();
  }

  void TestMenuFitsOnScreen(MenuAnchorPosition menu_anchor_position,
                            const gfx::Rect& monitor_bounds) {
    SCOPED_TRACE(base::StringPrintf(
        "MenuAnchorPosition: %d, monitor_bounds: @%s\n", menu_anchor_position,
        monitor_bounds.ToString().c_str()));
    MenuBoundsOptions options;
    options.menu_anchor = menu_anchor_position;
    options.monitor_bounds = monitor_bounds;
    const gfx::Point monitor_center = monitor_bounds.CenterPoint();

    // Simulate a bottom shelf with a tall menu.
    const int button_size = 50;
    options.anchor_bounds =
        gfx::Rect(monitor_center.x(), monitor_bounds.bottom() - button_size,
                  button_size, button_size);
    gfx::Rect final_bounds = CalculateBubbleMenuBounds(options);

    // Adjust the final bounds to not include the shadow and border.
    const gfx::Insets border_and_shadow_insets =
        BubbleBorder::GetBorderAndShadowInsets(
            MenuConfig::instance().touchable_menu_shadow_elevation);
    final_bounds.Inset(border_and_shadow_insets);

    // Test that the menu will show on screen.
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds));

    // Simulate a left shelf with a tall menu.
    options.anchor_bounds = gfx::Rect(monitor_bounds.x(), monitor_center.y(),
                                      button_size, button_size);
    final_bounds = CalculateBubbleMenuBounds(options);

    // Adjust the final bounds to not include the shadow and border.
    final_bounds.Inset(border_and_shadow_insets);

    // Test that the menu will show on screen.
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds));

    // Simulate right shelf with a tall menu.
    options.anchor_bounds =
        gfx::Rect(monitor_bounds.right() - button_size, monitor_center.y(),
                  button_size, button_size);
    final_bounds = CalculateBubbleMenuBounds(options);

    // Adjust the final bounds to not include the shadow and border.
    final_bounds.Inset(border_and_shadow_insets);

    // Test that the menu will show on screen.
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds));
  }

  void TestMenuFitsOnScreenSmallAnchor(MenuAnchorPosition menu_anchor_position,
                                       const gfx::Rect& monitor_bounds) {
    SCOPED_TRACE(base::StringPrintf(
        "MenuAnchorPosition: %d, monitor_bounds: @%s\n", menu_anchor_position,
        monitor_bounds.ToString().c_str()));
    MenuBoundsOptions options;
    options.menu_anchor = menu_anchor_position;
    options.monitor_bounds = monitor_bounds;
    const gfx::Size anchor_size(0, 0);

    // Simulate a click on the top left corner.
    options.anchor_bounds = gfx::Rect(monitor_bounds.origin(), anchor_size);
    gfx::Rect final_bounds = CalculateBubbleMenuBounds(options);

    // Adjust the final bounds to not include the shadow and border.
    const gfx::Insets border_and_shadow_insets =
        BubbleBorder::GetBorderAndShadowInsets(
            MenuConfig::instance().touchable_menu_shadow_elevation);
    final_bounds.Inset(border_and_shadow_insets);

    // Test that the menu is within the monitor bounds.
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds));

    // Simulate a click on the bottom left corner.
    options.anchor_bounds =
        gfx::Rect(monitor_bounds.bottom_left(), anchor_size);
    final_bounds = CalculateBubbleMenuBounds(options);
    // Adjust the final bounds to not include the shadow and border.
    final_bounds.Inset(border_and_shadow_insets);

    // Test that the menu is within the monitor bounds.
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds));

    // Simulate a click on the top right corner.
    options.anchor_bounds = gfx::Rect(monitor_bounds.top_right(), anchor_size);
    final_bounds = CalculateBubbleMenuBounds(options);
    // Adjust the final bounds to not include the shadow and border.
    final_bounds.Inset(border_and_shadow_insets);

    // Test that the menu is within the monitor bounds.
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds));

    // Simulate a click on the bottom right corner.
    options.anchor_bounds =
        gfx::Rect(monitor_bounds.bottom_right(), anchor_size);
    final_bounds = CalculateBubbleMenuBounds(options);
    // Adjust the final bounds to not include the shadow and border.
    final_bounds.Inset(border_and_shadow_insets);

    // Test that the menu is within the monitor bounds.
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds));
  }

  void TestMenuFitsOnSmallScreen(MenuAnchorPosition menu_anchor_position,
                                 const gfx::Rect& monitor_bounds) {
    SCOPED_TRACE(base::StringPrintf(
        "MenuAnchorPosition: %d, monitor_bounds: @%s\n", menu_anchor_position,
        monitor_bounds.ToString().c_str()));
    MenuBoundsOptions options;
    options.menu_anchor = menu_anchor_position;
    options.monitor_bounds = monitor_bounds;
    options.menu_size = monitor_bounds.size();
    options.menu_size.Enlarge(100, 100);
    const gfx::Size anchor_size(0, 0);

    // Adjust the final bounds to not include the shadow and border.
    const gfx::Insets border_and_shadow_insets =
        BubbleBorder::GetBorderAndShadowInsets(
            MenuConfig::instance().touchable_menu_shadow_elevation);

    options.anchor_bounds = gfx::Rect(monitor_bounds.origin(), anchor_size);
    gfx::Rect final_bounds = CalculateBubbleMenuBounds(options);
    final_bounds.Inset(border_and_shadow_insets);
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds))
        << options.monitor_bounds.ToString() << " does not contain "
        << final_bounds.ToString();

    options.anchor_bounds =
        gfx::Rect(monitor_bounds.bottom_left(), anchor_size);
    final_bounds = CalculateBubbleMenuBounds(options);
    final_bounds.Inset(border_and_shadow_insets);
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds))
        << options.monitor_bounds.ToString() << " does not contain "
        << final_bounds.ToString();

    options.anchor_bounds =
        gfx::Rect(monitor_bounds.bottom_right(), anchor_size);
    final_bounds = CalculateBubbleMenuBounds(options);
    final_bounds.Inset(border_and_shadow_insets);
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds))
        << options.monitor_bounds.ToString() << " does not contain "
        << final_bounds.ToString();

    options.anchor_bounds = gfx::Rect(monitor_bounds.top_right(), anchor_size);
    final_bounds = CalculateBubbleMenuBounds(options);
    final_bounds.Inset(border_and_shadow_insets);
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds))
        << options.monitor_bounds.ToString() << " does not contain "
        << final_bounds.ToString();

    options.anchor_bounds =
        gfx::Rect(monitor_bounds.CenterPoint(), anchor_size);
    final_bounds = CalculateBubbleMenuBounds(options);
    final_bounds.Inset(border_and_shadow_insets);
    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds))
        << options.monitor_bounds.ToString() << " does not contain "
        << final_bounds.ToString();
  }

  void TestSubmenuFitsOnScreen(MenuItemView* item,
                               const gfx::Rect& monitor_bounds,
                               const gfx::Rect& parent_bounds) {
    MenuBoundsOptions options;
    options.menu_anchor = MenuAnchorPosition::kBubbleAbove;
    options.monitor_bounds = monitor_bounds;

    // Adjust the final bounds to not include the shadow and border.
    const gfx::Insets border_and_shadow_insets =
        BubbleBorder::GetBorderAndShadowInsets(
            MenuConfig::instance().touchable_menu_shadow_elevation);

    MenuItemView* parent_item = item->GetParentMenuItem();
    SubmenuView* sub_menu = parent_item->GetSubmenu();

    parent_item->SetBoundsRect(parent_bounds);
    sub_menu->ShowAt(owner(), parent_item->bounds(), false);
    gfx::Rect final_bounds = CalculateBubbleMenuBounds(options, item);
    final_bounds.Inset(border_and_shadow_insets);
    sub_menu->Close();

    EXPECT_TRUE(options.monitor_bounds.Contains(final_bounds))
        << options.monitor_bounds.ToString() << " does not contain "
        << final_bounds.ToString();
  }

 protected:
  void SetPendingStateItem(MenuItemView* item) {
    menu_controller_->pending_state_.item = item;
    menu_controller_->pending_state_.submenu_open = true;
  }

  void SetState(MenuItemView* item) {
    menu_controller_->state_.item = item;
    menu_controller_->state_.submenu_open = true;
  }

  void ResetSelection() {
    menu_controller_->SetSelection(
        nullptr, MenuController::SELECTION_EXIT |
                     MenuController::SELECTION_UPDATE_IMMEDIATELY);
  }

  void IncrementSelection() {
    menu_controller_->IncrementSelection(
        MenuController::INCREMENT_SELECTION_DOWN);
  }

  void DecrementSelection() {
    menu_controller_->IncrementSelection(
        MenuController::INCREMENT_SELECTION_UP);
  }

  void DestroyMenuControllerOnMenuClosed(TestMenuControllerDelegate* delegate) {
    // Unretained() is safe here as the test should outlive the delegate. If not
    // we want to know.
    delegate->set_on_menu_closed_callback(base::BindRepeating(
        &MenuControllerTest::DestroyMenuController, base::Unretained(this)));
  }

  MenuItemView* FindInitialSelectableMenuItemDown(MenuItemView* parent) {
    return menu_controller_->FindInitialSelectableMenuItem(
        parent, MenuController::INCREMENT_SELECTION_DOWN);
  }

  MenuItemView* FindInitialSelectableMenuItemUp(MenuItemView* parent) {
    return menu_controller_->FindInitialSelectableMenuItem(
        parent, MenuController::INCREMENT_SELECTION_UP);
  }

  MenuItemView* FindNextSelectableMenuItem(MenuItemView* parent, int index) {
    return menu_controller_->FindNextSelectableMenuItem(
        parent, index, MenuController::INCREMENT_SELECTION_DOWN, false);
  }

  MenuItemView* FindPreviousSelectableMenuItem(MenuItemView* parent,
                                               int index) {
    return menu_controller_->FindNextSelectableMenuItem(
        parent, index, MenuController::INCREMENT_SELECTION_UP, false);
  }

  internal::MenuControllerDelegate* GetCurrentDelegate() {
    return menu_controller_->delegate_;
  }

  bool IsShowing() { return menu_controller_->showing_; }

  MenuHost* GetMenuHost(SubmenuView* submenu) { return submenu->host_; }

  MenuHostRootView* CreateMenuHostRootView(MenuHost* host) {
    return static_cast<MenuHostRootView*>(host->CreateRootView());
  }

  void MenuHostOnDragWillStart(MenuHost* host) { host->OnDragWillStart(); }

  void MenuHostOnDragComplete(MenuHost* host) { host->OnDragComplete(); }

  void SelectByChar(base::char16 character) {
    menu_controller_->SelectByChar(character);
  }

  void SetDropMenuItem(MenuItemView* target,
                       MenuDelegate::DropPosition position) {
    menu_controller_->SetDropMenuItem(target, position);
  }

  void SetComboboxType(MenuController::ComboboxType combobox_type) {
    menu_controller_->set_combobox_type(combobox_type);
  }

  void SetSelectionOnPointerDown(SubmenuView* source,
                                 const ui::LocatedEvent* event) {
    menu_controller_->SetSelectionOnPointerDown(source, event);
  }

  // Note that coordinates of events passed to MenuController must be in that of
  // the MenuScrollViewContainer.
  void ProcessMousePressed(SubmenuView* source, const ui::MouseEvent& event) {
    menu_controller_->OnMousePressed(source, event);
  }

  void ProcessMouseDragged(SubmenuView* source, const ui::MouseEvent& event) {
    menu_controller_->OnMouseDragged(source, event);
  }

  void ProcessMouseMoved(SubmenuView* source, const ui::MouseEvent& event) {
    menu_controller_->OnMouseMoved(source, event);
  }

  void ProcessMouseReleased(SubmenuView* source, const ui::MouseEvent& event) {
    menu_controller_->OnMouseReleased(source, event);
  }

  void Accept(MenuItemView* item, int event_flags) {
    menu_controller_->Accept(item, event_flags);
    views::test::WaitForMenuClosureAnimation();
  }

  // Causes the |menu_controller_| to begin dragging. Use TestDragDropClient to
  // avoid nesting message loops.
  void StartDrag() {
    const gfx::Point location;
    menu_controller_->state_.item = menu_item()->GetSubmenu()->GetMenuItemAt(0);
    menu_controller_->StartDrag(
        menu_item()->GetSubmenu()->GetMenuItemAt(0)->CreateSubmenu(), location);
  }

  void SetUpMenuControllerForCalculateBounds(const MenuBoundsOptions& options) {
    menu_controller_->state_.anchor = options.menu_anchor;
    menu_controller_->state_.initial_bounds = options.anchor_bounds;
    menu_controller_->state_.monitor_bounds = options.monitor_bounds;
    menu_item_->SetActualMenuPosition(options.menu_position);
    menu_item_->GetSubmenu()->GetScrollViewContainer()->SetPreferredSize(
        options.menu_size);
  }

  GestureTestWidget* owner() { return owner_.get(); }
  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }
  TestMenuItemViewShown* menu_item() { return menu_item_.get(); }
  TestMenuDelegate* menu_delegate() { return menu_delegate_.get(); }
  TestMenuControllerDelegate* menu_controller_delegate() {
    return menu_controller_delegate_.get();
  }
  MenuController* menu_controller() { return menu_controller_; }
  const MenuItemView* pending_state_item() const {
    return menu_controller_->pending_state_.item;
  }
  MenuController::ExitType menu_exit_type() const {
    return menu_controller_->exit_type_;
  }

  void AddButtonMenuItems() {
    menu_item()->SetBounds(0, 0, 200, 300);
    MenuItemView* item_view =
        menu_item()->AppendMenuItem(5, base::ASCIIToUTF16("Five"));
    for (size_t i = 0; i < 3; ++i) {
      LabelButton* button =
          new LabelButton(nullptr, base::ASCIIToUTF16("Label"));
      // This is an in-menu button. Hence it must be always focusable.
      button->SetFocusBehavior(View::FocusBehavior::ALWAYS);
      item_view->AddChildView(button);
    }
    menu_item()->GetSubmenu()->ShowAt(owner(), menu_item()->bounds(), false);
  }

  void DestroyMenuItem() { menu_item_.reset(); }

  Button* GetHotButton() { return menu_controller_->hot_button_; }

  void SetHotTrackedButton(Button* hot_button) {
    menu_controller_->SetHotTrackedButton(hot_button);
  }

  void ExitMenuRun() {
    menu_controller_->SetExitType(MenuController::ExitType::kOutermost);
    menu_controller_->ExitTopMostMenu();
  }

  void DestroyMenuController() {
    if (!menu_controller_)
      return;

    if (!owner_->IsClosed())
      owner_->RemoveObserver(menu_controller_);

    menu_controller_->showing_ = false;
    menu_controller_->owner_ = nullptr;
    delete menu_controller_;
    menu_controller_ = nullptr;
  }

  int CountOwnerOnGestureEvent() const { return owner_->gesture_count(); }

  bool SelectionWraps() {
    return MenuConfig::instance().arrow_key_selection_wraps;
  }

  void OpenMenu(MenuItemView* parent) {
    menu_controller_->OpenMenuImpl(parent, true);
  }

 private:
  void Init() {
    owner_ = std::make_unique<GestureTestWidget>();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    owner_->Init(std::move(params));
    event_generator_ =
        std::make_unique<ui::test::EventGenerator>(GetRootWindow(owner()));
    owner_->Show();

    SetupMenuItem();
    SetupMenuController();
  }

  void SetupMenuItem() {
    menu_delegate_ = std::make_unique<TestMenuDelegate>();
    menu_item_ = std::make_unique<TestMenuItemViewShown>(menu_delegate_.get());
    menu_item_->AppendMenuItem(1, base::ASCIIToUTF16("One"));
    menu_item_->AppendMenuItem(2, base::ASCIIToUTF16("Two"));
    menu_item_->AppendMenuItem(3, base::ASCIIToUTF16("Three"));
    menu_item_->AppendMenuItem(4, base::ASCIIToUTF16("Four"));
  }

  void SetupMenuController() {
    menu_controller_delegate_ = std::make_unique<TestMenuControllerDelegate>();
    const bool for_drop = false;
    menu_controller_ =
        new MenuController(for_drop, menu_controller_delegate_.get());
    menu_controller_->owner_ = owner_.get();
    menu_controller_->showing_ = true;
    menu_controller_->SetSelection(
        menu_item_.get(), MenuController::SELECTION_UPDATE_IMMEDIATELY);
    menu_item_->SetController(menu_controller_);
  }

  // Not owned.
  ReleaseRefTestViewsDelegate* test_views_delegate_ = nullptr;

  std::unique_ptr<GestureTestWidget> owner_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  std::unique_ptr<TestMenuItemViewShown> menu_item_;
  std::unique_ptr<TestMenuControllerDelegate> menu_controller_delegate_;
  std::unique_ptr<TestMenuDelegate> menu_delegate_;
  MenuController* menu_controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MenuControllerTest);
};

INSTANTIATE_TEST_SUITE_P(All, MenuControllerTest, testing::Bool());

#if defined(USE_X11)
// Tests that an event targeter which blocks events will be honored by the menu
// event dispatcher.
TEST_F(MenuControllerTest, EventTargeter) {
  {
    // With the aura::NullWindowTargeter instantiated and assigned we expect
    // the menu to not handle the key event.
    aura::ScopedWindowTargeter scoped_targeter(
        GetRootWindow(owner()), std::make_unique<aura::NullWindowTargeter>());
    PressKey(ui::VKEY_ESCAPE);
    EXPECT_EQ(MenuController::ExitType::kNone, menu_exit_type());
  }
  // Now that the targeter has been destroyed, expect to exit the menu
  // normally when hitting escape.
  TestAsyncEscapeKey();
  EXPECT_EQ(MenuController::ExitType::kAll, menu_exit_type());
}

#endif  // defined(USE_X11)

#if defined(USE_X11)
// Tests that touch event ids are released correctly. See crbug.com/439051 for
// details. When the ids aren't managed correctly, we get stuck down touches.
TEST_F(MenuControllerTest, TouchIdsReleasedCorrectly) {
  TestEventHandler test_event_handler;
  GetRootWindow(owner())->AddPreTargetHandler(&test_event_handler);

  std::vector<int> devices;
  devices.push_back(1);
  ui::SetUpTouchDevicesForTest(devices);

  event_generator()->PressTouchId(0);
  event_generator()->PressTouchId(1);
  event_generator()->ReleaseTouchId(0);

  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);

  MenuControllerTest::ReleaseTouchId(1);
  TestAsyncEscapeKey();

  EXPECT_EQ(MenuController::ExitType::kAll, menu_exit_type());
  EXPECT_EQ(0, test_event_handler.outstanding_touches());

  GetRootWindow(owner())->RemovePreTargetHandler(&test_event_handler);
}
#endif  // defined(USE_X11)

// Tests that initial selected menu items are correct when items are enabled or
// disabled.
TEST_F(MenuControllerTest, InitialSelectedItem) {
  // Leave items "Two", "Three", and "Four" enabled.
  menu_item()->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(false);
  // The first selectable item should be item "Two".
  MenuItemView* first_selectable =
      FindInitialSelectableMenuItemDown(menu_item());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(2, first_selectable->GetCommand());
  // The last selectable item should be item "Four".
  MenuItemView* last_selectable = FindInitialSelectableMenuItemUp(menu_item());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(4, last_selectable->GetCommand());

  // Leave items "One" and "Two" enabled.
  menu_item()->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(true);
  menu_item()->GetSubmenu()->GetMenuItemAt(1)->SetEnabled(true);
  menu_item()->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(3)->SetEnabled(false);
  // The first selectable item should be item "One".
  first_selectable = FindInitialSelectableMenuItemDown(menu_item());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(1, first_selectable->GetCommand());
  // The last selectable item should be item "Two".
  last_selectable = FindInitialSelectableMenuItemUp(menu_item());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(2, last_selectable->GetCommand());

  // Leave only a single item "One" enabled.
  menu_item()->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(true);
  menu_item()->GetSubmenu()->GetMenuItemAt(1)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(3)->SetEnabled(false);
  // The first selectable item should be item "One".
  first_selectable = FindInitialSelectableMenuItemDown(menu_item());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(1, first_selectable->GetCommand());
  // The last selectable item should be item "One".
  last_selectable = FindInitialSelectableMenuItemUp(menu_item());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(1, last_selectable->GetCommand());

  // Leave only a single item "Three" enabled.
  menu_item()->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(1)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(true);
  menu_item()->GetSubmenu()->GetMenuItemAt(3)->SetEnabled(false);
  // The first selectable item should be item "Three".
  first_selectable = FindInitialSelectableMenuItemDown(menu_item());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(3, first_selectable->GetCommand());
  // The last selectable item should be item "Three".
  last_selectable = FindInitialSelectableMenuItemUp(menu_item());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(3, last_selectable->GetCommand());

  // Leave only a single item ("Two") selected. It should be the first and the
  // last selectable item.
  menu_item()->GetSubmenu()->GetMenuItemAt(0)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(1)->SetEnabled(true);
  menu_item()->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(false);
  menu_item()->GetSubmenu()->GetMenuItemAt(3)->SetEnabled(false);
  first_selectable = FindInitialSelectableMenuItemDown(menu_item());
  ASSERT_NE(nullptr, first_selectable);
  EXPECT_EQ(2, first_selectable->GetCommand());
  last_selectable = FindInitialSelectableMenuItemUp(menu_item());
  ASSERT_NE(nullptr, last_selectable);
  EXPECT_EQ(2, last_selectable->GetCommand());

  // There should be no next or previous selectable item since there is only a
  // single enabled item in the menu.
  EXPECT_EQ(nullptr, FindNextSelectableMenuItem(menu_item(), 1));
  EXPECT_EQ(nullptr, FindPreviousSelectableMenuItem(menu_item(), 1));

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

// Tests that opening the menu and pressing 'Home' selects the first menu item.
TEST_F(MenuControllerTest, FirstSelectedItem) {
  SetPendingStateItem(menu_item()->GetSubmenu()->GetMenuItemAt(0));
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  // Select the first menu item.
  DispatchKey(ui::VKEY_HOME);
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  // Fake initial root item selection and submenu showing.
  SetPendingStateItem(menu_item());
  EXPECT_EQ(0, pending_state_item()->GetCommand());

  // Select the first menu item.
  DispatchKey(ui::VKEY_HOME);
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  // Select the last item.
  SetPendingStateItem(menu_item()->GetSubmenu()->GetMenuItemAt(3));
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  // Select the first menu item.
  DispatchKey(ui::VKEY_HOME);
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

// Tests that opening the menu and pressing 'End' selects the last enabled menu
// item.
TEST_F(MenuControllerTest, LastSelectedItem) {
  // Fake initial root item selection and submenu showing.
  SetPendingStateItem(menu_item());
  EXPECT_EQ(0, pending_state_item()->GetCommand());

  // Select the last menu item.
  DispatchKey(ui::VKEY_END);
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  // Select the last item.
  SetPendingStateItem(menu_item()->GetSubmenu()->GetMenuItemAt(3));
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  // Select the last menu item.
  DispatchKey(ui::VKEY_END);
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  // Select the first item.
  SetPendingStateItem(menu_item()->GetSubmenu()->GetMenuItemAt(0));
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  // Select the last menu item.
  DispatchKey(ui::VKEY_END);
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

// Tests that opening menu and pressing 'Down' and 'Up' iterates over enabled
// items.
TEST_F(MenuControllerTest, NextSelectedItem) {
  // Disabling the item "Three" gets it skipped when using keyboard to navigate.
  menu_item()->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(false);

  // Fake initial hot selection.
  SetPendingStateItem(menu_item()->GetSubmenu()->GetMenuItemAt(0));
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  // Move down in the menu.
  // Select next item.
  IncrementSelection();
  EXPECT_EQ(2, pending_state_item()->GetCommand());

  // Skip disabled item.
  IncrementSelection();
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  if (SelectionWraps()) {
    // Wrap around.
    IncrementSelection();
    EXPECT_EQ(1, pending_state_item()->GetCommand());

    // Move up in the menu.
    // Wrap around.
    DecrementSelection();
    EXPECT_EQ(4, pending_state_item()->GetCommand());
  } else {
    // Don't wrap.
    IncrementSelection();
    EXPECT_EQ(4, pending_state_item()->GetCommand());
  }

  // Skip disabled item.
  DecrementSelection();
  EXPECT_EQ(2, pending_state_item()->GetCommand());

  // Select previous item.
  DecrementSelection();
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

// Tests that opening menu and pressing 'Up' selects the last enabled menu item.
TEST_F(MenuControllerTest, PreviousSelectedItem) {
  // Disabling the item "Four" gets it skipped when using keyboard to navigate.
  menu_item()->GetSubmenu()->GetMenuItemAt(3)->SetEnabled(false);

  // Fake initial root item selection and submenu showing.
  SetPendingStateItem(menu_item());
  EXPECT_EQ(0, pending_state_item()->GetCommand());

  // Move up and select a previous (in our case the last enabled) item.
  DecrementSelection();
  EXPECT_EQ(3, pending_state_item()->GetCommand());

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

// Tests that opening menu and calling SelectByChar works correctly.
TEST_F(MenuControllerTest, SelectByChar) {
  SetComboboxType(MenuController::ComboboxType::kReadonly);

  // Handle null character should do nothing.
  SelectByChar(0);
  EXPECT_EQ(0, pending_state_item()->GetCommand());

  // Handle searching for 'f'; should find "Four".
  SelectByChar('f');
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

TEST_F(MenuControllerTest, SelectChildButtonView) {
  AddButtonMenuItems();
  View* buttons_view = menu_item()->GetSubmenu()->children()[4];
  ASSERT_NE(nullptr, buttons_view);
  Button* button1 = Button::AsButton(buttons_view->children()[0]);
  ASSERT_NE(nullptr, button1);
  Button* button2 = Button::AsButton(buttons_view->children()[1]);
  ASSERT_NE(nullptr, button2);
  Button* button3 = Button::AsButton(buttons_view->children()[2]);
  ASSERT_NE(nullptr, button2);

  // Handle searching for 'f'; should find "Four".
  SelectByChar('f');
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_FALSE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());

  // Move selection to |button1|.
  IncrementSelection();
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_TRUE(button1->IsHotTracked());
  EXPECT_FALSE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());

  // Move selection to |button2|.
  IncrementSelection();
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_TRUE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());

  // Move selection to |button3|.
  IncrementSelection();
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_FALSE(button2->IsHotTracked());
  EXPECT_TRUE(button3->IsHotTracked());

  // Move a mouse to hot track the |button1|.
  SubmenuView* sub_menu = menu_item()->GetSubmenu();
  gfx::Point location(button1->GetBoundsInScreen().CenterPoint());
  View::ConvertPointFromScreen(sub_menu->GetScrollViewContainer(), &location);
  ui::MouseEvent event(ui::ET_MOUSE_MOVED, location, location,
                       ui::EventTimeForNow(), 0, 0);
  ProcessMouseMoved(sub_menu, event);
  EXPECT_EQ(button1, GetHotButton());
  EXPECT_TRUE(button1->IsHotTracked());

  // Incrementing selection should move hot tracking to the second button (next
  // after the first button).
  IncrementSelection();
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_TRUE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());

  // Increment selection twice to wrap around.
  IncrementSelection();
  IncrementSelection();
  if (SelectionWraps())
    EXPECT_EQ(1, pending_state_item()->GetCommand());
  else
    EXPECT_EQ(5, pending_state_item()->GetCommand());

  // Clear references in menu controller to the menu item that is going away.
  ResetSelection();
}

TEST_F(MenuControllerTest, DeleteChildButtonView) {
  AddButtonMenuItems();

  // Handle searching for 'f'; should find "Four".
  SelectByChar('f');
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  View* buttons_view = menu_item()->GetSubmenu()->children()[4];
  ASSERT_NE(nullptr, buttons_view);
  Button* button1 = Button::AsButton(buttons_view->children()[0]);
  ASSERT_NE(nullptr, button1);
  Button* button2 = Button::AsButton(buttons_view->children()[1]);
  ASSERT_NE(nullptr, button2);
  Button* button3 = Button::AsButton(buttons_view->children()[2]);
  ASSERT_NE(nullptr, button2);
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_FALSE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());

  // Increment twice to move selection to |button2|.
  IncrementSelection();
  IncrementSelection();
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_TRUE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());

  // Delete |button2| while it is hot-tracked.
  // This should update MenuController via ViewHierarchyChanged and reset
  // |hot_button_|.
  delete button2;

  // Incrementing selection should now set hot-tracked item to |button1|.
  // It should not crash.
  IncrementSelection();
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_TRUE(button1->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());
}

// Creates a menu with Button child views, simulates running a nested
// menu and tests that existing the nested run restores hot-tracked child view.
TEST_F(MenuControllerTest, ChildButtonHotTrackedWhenNested) {
  AddButtonMenuItems();

  // Handle searching for 'f'; should find "Four".
  SelectByChar('f');
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  View* buttons_view = menu_item()->GetSubmenu()->children()[4];
  ASSERT_NE(nullptr, buttons_view);
  Button* button1 = Button::AsButton(buttons_view->children()[0]);
  ASSERT_NE(nullptr, button1);
  Button* button2 = Button::AsButton(buttons_view->children()[1]);
  ASSERT_NE(nullptr, button2);
  Button* button3 = Button::AsButton(buttons_view->children()[2]);
  ASSERT_NE(nullptr, button2);
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_FALSE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());

  // Increment twice to move selection to |button2|.
  IncrementSelection();
  IncrementSelection();
  EXPECT_EQ(5, pending_state_item()->GetCommand());
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_TRUE(button2->IsHotTracked());
  EXPECT_FALSE(button3->IsHotTracked());
  EXPECT_EQ(button2, GetHotButton());

  MenuController* controller = menu_controller();
  controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);

  // |button2| should stay in hot-tracked state but menu controller should not
  // track it anymore (preventing resetting hot-tracked state when changing
  // selection while a nested run is active).
  EXPECT_TRUE(button2->IsHotTracked());
  EXPECT_EQ(nullptr, GetHotButton());

  // Setting hot-tracked button while nested should get reverted when nested
  // menu run ends.
  SetHotTrackedButton(button1);
  EXPECT_TRUE(button1->IsHotTracked());
  EXPECT_EQ(button1, GetHotButton());

  ExitMenuRun();
  EXPECT_FALSE(button1->IsHotTracked());
  EXPECT_TRUE(button2->IsHotTracked());
  EXPECT_EQ(button2, GetHotButton());
}

// Tests that a menu opened asynchronously, will notify its
// MenuControllerDelegate when Accept is called.
TEST_F(MenuControllerTest, AsynchronousAccept) {
  views::test::DisableMenuClosureAnimations();

  MenuController* controller = menu_controller();
  controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  EXPECT_EQ(0, delegate->on_menu_closed_called());

  MenuItemView* accepted = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  const int kEventFlags = 42;
  Accept(accepted, kEventFlags);

  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(accepted, delegate->on_menu_closed_menu());
  EXPECT_EQ(kEventFlags, delegate->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            delegate->on_menu_closed_notify_type());
}

// Tests that a menu opened asynchronously, will notify its
// MenuControllerDelegate when CancelAll is called.
TEST_F(MenuControllerTest, AsynchronousCancelAll) {
  MenuController* controller = menu_controller();

  controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  EXPECT_EQ(0, delegate->on_menu_closed_called());

  controller->Cancel(MenuController::ExitType::kAll);
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, delegate->on_menu_closed_menu());
  EXPECT_EQ(0, delegate->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            delegate->on_menu_closed_notify_type());
  EXPECT_EQ(MenuController::ExitType::kAll, controller->exit_type());
}

// Tests that canceling a nested menu restores the previous
// MenuControllerDelegate, and notifies each delegate.
TEST_F(MenuControllerTest, AsynchronousNestedDelegate) {
  MenuController* controller = menu_controller();
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  std::unique_ptr<TestMenuControllerDelegate> nested_delegate(
      new TestMenuControllerDelegate());

  controller->AddNestedDelegate(nested_delegate.get());
  EXPECT_EQ(nested_delegate.get(), GetCurrentDelegate());

  controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);

  controller->Cancel(MenuController::ExitType::kAll);
  EXPECT_EQ(delegate, GetCurrentDelegate());
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(1, nested_delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, nested_delegate->on_menu_closed_menu());
  EXPECT_EQ(0, nested_delegate->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            nested_delegate->on_menu_closed_notify_type());
  EXPECT_EQ(MenuController::ExitType::kAll, controller->exit_type());
}

// Tests that dropping within an asynchronous menu stops the menu from showing
// and does not notify the controller.
TEST_F(MenuControllerTest, AsynchronousPerformDrop) {
  MenuController* controller = menu_controller();
  SubmenuView* source = menu_item()->GetSubmenu();
  MenuItemView* target = source->GetMenuItemAt(0);

  SetDropMenuItem(target, MenuDelegate::DropPosition::kAfter);

  ui::OSExchangeData drop_data;
  gfx::PointF location(target->origin());
  ui::DropTargetEvent target_event(drop_data, location, location,
                                   ui::DragDropTypes::DRAG_MOVE);
  controller->OnPerformDrop(source, target_event);

  TestMenuDelegate* menu_delegate =
      static_cast<TestMenuDelegate*>(target->GetDelegate());
  TestMenuControllerDelegate* controller_delegate = menu_controller_delegate();
  EXPECT_TRUE(menu_delegate->on_perform_drop_called());
  EXPECT_FALSE(IsShowing());
  EXPECT_EQ(0, controller_delegate->on_menu_closed_called());
}

// Tests that dragging within an asynchronous menu notifies the
// MenuControllerDelegate for shutdown.
TEST_F(MenuControllerTest, AsynchronousDragComplete) {
  MenuController* controller = menu_controller();
  TestDragCompleteThenDestroyOnMenuClosed();

  controller->OnDragWillStart();
  controller->OnDragComplete(true);

  TestMenuControllerDelegate* controller_delegate = menu_controller_delegate();
  EXPECT_EQ(1, controller_delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, controller_delegate->on_menu_closed_menu());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            controller_delegate->on_menu_closed_notify_type());
}

// Tests that if Cancel is called during a drag, that OnMenuClosed is still
// notified when the drag completes.
TEST_F(MenuControllerTest, AsynchronousCancelDuringDrag) {
  MenuController* controller = menu_controller();
  TestDragCompleteThenDestroyOnMenuClosed();

  controller->OnDragWillStart();
  controller->Cancel(MenuController::ExitType::kAll);
  controller->OnDragComplete(true);

  TestMenuControllerDelegate* controller_delegate = menu_controller_delegate();
  EXPECT_EQ(1, controller_delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, controller_delegate->on_menu_closed_menu());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            controller_delegate->on_menu_closed_notify_type());
}

// Tests that if a menu is destroyed while drag operations are occurring, that
// the MenuHost does not crash as the drag completes.
TEST_F(MenuControllerTest, AsynchronousDragHostDeleted) {
  SubmenuView* submenu = menu_item()->GetSubmenu();
  submenu->ShowAt(owner(), menu_item()->bounds(), false);
  MenuHost* host = GetMenuHost(submenu);
  MenuHostOnDragWillStart(host);
  submenu->Close();
  DestroyMenuItem();
  MenuHostOnDragComplete(host);
}

// Widget destruction and cleanup occurs on the MessageLoop after the
// MenuController has been destroyed. A MenuHostRootView should not attempt to
// access a destroyed MenuController. This test should not cause a crash.
TEST_F(MenuControllerTest, HostReceivesInputBeforeDestruction) {
  SubmenuView* submenu = menu_item()->GetSubmenu();
  submenu->ShowAt(owner(), menu_item()->bounds(), false);
  gfx::Point location(submenu->bounds().bottom_right());
  location.Offset(1, 1);

  MenuHost* host = GetMenuHost(submenu);
  // Normally created as the full Widget is brought up. Explicitly created here
  // for testing.
  std::unique_ptr<MenuHostRootView> root_view(CreateMenuHostRootView(host));
  DestroyMenuController();

  ui::MouseEvent event(ui::ET_MOUSE_MOVED, location, location,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);

  // This should not attempt to access the destroyed MenuController and should
  // not crash.
  root_view->OnMouseMoved(event);
}

// Tests that an asynchronous menu nested within an asynchronous menu closes
// both menus, and notifies both delegates.
TEST_F(MenuControllerTest, DoubleAsynchronousNested) {
  MenuController* controller = menu_controller();
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  std::unique_ptr<TestMenuControllerDelegate> nested_delegate(
      new TestMenuControllerDelegate());

  // Nested run
  controller->AddNestedDelegate(nested_delegate.get());
  controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);

  controller->Cancel(MenuController::ExitType::kAll);
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(1, nested_delegate->on_menu_closed_called());
}

// Tests that setting send_gesture_events_to_owner flag forwards gesture events
// to owner and the forwarding stops when the current gesture sequence ends.
TEST_F(MenuControllerTest, PreserveGestureForOwner) {
  MenuController* controller = menu_controller();
  MenuItemView* item = menu_item();
  controller->Run(owner(), nullptr, item, gfx::Rect(),
                  MenuAnchorPosition::kBottomCenter, false, false);
  SubmenuView* sub_menu = item->GetSubmenu();
  sub_menu->ShowAt(owner(), gfx::Rect(0, 0, 100, 100), true);

  gfx::Point location(sub_menu->bounds().bottom_left().x(),
                      sub_menu->bounds().bottom_left().y() + 10);
  ui::GestureEvent event(location.x(), location.y(), 0, ui::EventTimeForNow(),
                         ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN));

  // Gesture events should not be forwarded if the flag is not set.
  EXPECT_EQ(CountOwnerOnGestureEvent(), 0);
  EXPECT_FALSE(controller->send_gesture_events_to_owner());
  controller->OnGestureEvent(sub_menu, &event);
  EXPECT_EQ(CountOwnerOnGestureEvent(), 0);

  // The menu's owner should receive gestures triggered outside the menu.
  controller->set_send_gesture_events_to_owner(true);
  controller->OnGestureEvent(sub_menu, &event);
  EXPECT_EQ(CountOwnerOnGestureEvent(), 1);

  ui::GestureEvent event2(location.x(), location.y(), 0, ui::EventTimeForNow(),
                          ui::GestureEventDetails(ui::ET_GESTURE_END));

  controller->OnGestureEvent(sub_menu, &event2);
  EXPECT_EQ(CountOwnerOnGestureEvent(), 2);

  // ET_GESTURE_END resets the |send_gesture_events_to_owner_| flag, so further
  // gesture events should not be sent to the owner.
  controller->OnGestureEvent(sub_menu, &event2);
  EXPECT_EQ(CountOwnerOnGestureEvent(), 2);
}

// Tests that touch outside menu does not closes the menu when forwarding
// gesture events to owner.
TEST_F(MenuControllerTest, NoTouchCloseWhenSendingGesturesToOwner) {
  views::test::DisableMenuClosureAnimations();
  MenuController* controller = menu_controller();

  // Owner wants the gesture events.
  controller->set_send_gesture_events_to_owner(true);

  // Show a sub menu and touch outside of it.
  MenuItemView* item = menu_item();
  SubmenuView* sub_menu = item->GetSubmenu();
  sub_menu->ShowAt(owner(), item->bounds(), false);
  gfx::Point location(sub_menu->bounds().bottom_right());
  location.Offset(1, 1);
  ui::TouchEvent touch_event(
      ui::ET_TOUCH_PRESSED, location, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  controller->OnTouchEvent(sub_menu, &touch_event);

  // Menu should still be visible.
  EXPECT_TRUE(IsShowing());

  // The current gesture sequence ends.
  ui::GestureEvent gesture_end_event(
      location.x(), location.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_END));
  controller->OnGestureEvent(sub_menu, &gesture_end_event);

  // Touch outside again and menu should be closed.
  controller->OnTouchEvent(sub_menu, &touch_event);
  views::test::WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsShowing());
  EXPECT_EQ(MenuController::ExitType::kAll, controller->exit_type());
}

// Tests that a nested menu does not crash when trying to repost events that
// occur outside of the bounds of the menu. Instead a proper shutdown should
// occur.
TEST_F(MenuControllerTest, AsynchronousRepostEvent) {
  views::test::DisableMenuClosureAnimations();
  MenuController* controller = menu_controller();
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  std::unique_ptr<TestMenuControllerDelegate> nested_delegate(
      new TestMenuControllerDelegate());

  controller->AddNestedDelegate(nested_delegate.get());
  EXPECT_EQ(nested_delegate.get(), GetCurrentDelegate());

  MenuItemView* item = menu_item();
  controller->Run(owner(), nullptr, item, gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);

  // Show a sub menu to target with a pointer selection. However have the event
  // occur outside of the bounds of the entire menu.
  SubmenuView* sub_menu = item->GetSubmenu();
  sub_menu->ShowAt(owner(), item->bounds(), false);
  gfx::Point location(sub_menu->bounds().bottom_right());
  location.Offset(1, 1);
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, location, location,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);

  // When attempting to select outside of all menus this should lead to a
  // shutdown. This should not crash while attempting to repost the event.
  SetSelectionOnPointerDown(sub_menu, &event);
  views::test::WaitForMenuClosureAnimation();

  EXPECT_EQ(delegate, GetCurrentDelegate());
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(1, nested_delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, nested_delegate->on_menu_closed_menu());
  EXPECT_EQ(0, nested_delegate->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            nested_delegate->on_menu_closed_notify_type());
  EXPECT_EQ(MenuController::ExitType::kAll, controller->exit_type());
}

// Tests that an asynchronous menu reposts touch events that occur outside of
// the bounds of the menu, and that the menu closes.
TEST_F(MenuControllerTest, AsynchronousTouchEventRepostEvent) {
  views::test::DisableMenuClosureAnimations();
  MenuController* controller = menu_controller();
  TestMenuControllerDelegate* delegate = menu_controller_delegate();

  // Show a sub menu to target with a touch event. However have the event occur
  // outside of the bounds of the entire menu.
  MenuItemView* item = menu_item();
  SubmenuView* sub_menu = item->GetSubmenu();
  sub_menu->ShowAt(owner(), item->bounds(), false);
  gfx::Point location(sub_menu->bounds().bottom_right());
  location.Offset(1, 1);
  ui::TouchEvent event(ui::ET_TOUCH_PRESSED, location, ui::EventTimeForNow(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  controller->OnTouchEvent(sub_menu, &event);
  views::test::WaitForMenuClosureAnimation();

  EXPECT_FALSE(IsShowing());
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, delegate->on_menu_closed_menu());
  EXPECT_EQ(0, delegate->on_menu_closed_mouse_event_flags());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            delegate->on_menu_closed_notify_type());
  EXPECT_EQ(MenuController::ExitType::kAll, controller->exit_type());
}

// Tests that having the MenuController deleted during RepostEvent does not
// cause a crash. ASAN bots should not detect use-after-free in MenuController.
TEST_F(MenuControllerTest, AsynchronousRepostEventDeletesController) {
  views::test::DisableMenuClosureAnimations();
  MenuController* controller = menu_controller();
  std::unique_ptr<TestMenuControllerDelegate> nested_delegate(
      new TestMenuControllerDelegate());

  controller->AddNestedDelegate(nested_delegate.get());
  EXPECT_EQ(nested_delegate.get(), GetCurrentDelegate());

  MenuItemView* item = menu_item();
  controller->Run(owner(), nullptr, item, gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);

  // Show a sub menu to target with a pointer selection. However have the event
  // occur outside of the bounds of the entire menu.
  SubmenuView* sub_menu = item->GetSubmenu();
  sub_menu->ShowAt(owner(), item->bounds(), true);
  gfx::Point location(sub_menu->bounds().bottom_right());
  location.Offset(1, 1);
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, location, location,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);

  // This will lead to MenuController being deleted during the event repost.
  // The remainder of this test, and TearDown should not crash.
  DestroyMenuControllerOnMenuClosed(nested_delegate.get());
  // When attempting to select outside of all menus this should lead to a
  // shutdown. This should not crash while attempting to repost the event.
  SetSelectionOnPointerDown(sub_menu, &event);
  views::test::WaitForMenuClosureAnimation();

  // Close to remove observers before test TearDown
  sub_menu->Close();
  EXPECT_EQ(1, nested_delegate->on_menu_closed_called());
}

// Tests that having the MenuController deleted during OnGestureEvent does not
// cause a crash. ASAN bots should not detect use-after-free in MenuController.
TEST_F(MenuControllerTest, AsynchronousGestureDeletesController) {
  views::test::DisableMenuClosureAnimations();
  MenuController* controller = menu_controller();
  std::unique_ptr<TestMenuControllerDelegate> nested_delegate(
      new TestMenuControllerDelegate());

  controller->AddNestedDelegate(nested_delegate.get());
  EXPECT_EQ(nested_delegate.get(), GetCurrentDelegate());

  MenuItemView* item = menu_item();
  controller->Run(owner(), nullptr, item, gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);

  // Show a sub menu to target with a tap event.
  SubmenuView* sub_menu = item->GetSubmenu();
  sub_menu->ShowAt(owner(), gfx::Rect(0, 0, 100, 100), true);

  gfx::Point location(sub_menu->bounds().CenterPoint());
  ui::GestureEvent event(location.x(), location.y(), 0, ui::EventTimeForNow(),
                         ui::GestureEventDetails(ui::ET_GESTURE_TAP));

  // This will lead to MenuController being deleted during the processing of the
  // gesture event. The remainder of this test, and TearDown should not crash.
  DestroyMenuControllerOnMenuClosed(nested_delegate.get());
  controller->OnGestureEvent(sub_menu, &event);
  views::test::WaitForMenuClosureAnimation();

  // Close to remove observers before test TearDown
  sub_menu->Close();
  EXPECT_EQ(1, nested_delegate->on_menu_closed_called());
}

TEST_F(MenuControllerTest, ArrowKeysAtEnds) {
  menu_item()->GetSubmenu()->GetMenuItemAt(2)->SetEnabled(false);

  SetPendingStateItem(menu_item()->GetSubmenu()->GetMenuItemAt(0));
  EXPECT_EQ(1, pending_state_item()->GetCommand());

  if (SelectionWraps()) {
    DispatchKey(ui::VKEY_UP);
    EXPECT_EQ(4, pending_state_item()->GetCommand());

    DispatchKey(ui::VKEY_DOWN);
    EXPECT_EQ(1, pending_state_item()->GetCommand());
  } else {
    DispatchKey(ui::VKEY_UP);
    EXPECT_EQ(1, pending_state_item()->GetCommand());
  }

  DispatchKey(ui::VKEY_DOWN);
  EXPECT_EQ(2, pending_state_item()->GetCommand());

  DispatchKey(ui::VKEY_DOWN);
  EXPECT_EQ(4, pending_state_item()->GetCommand());

  DispatchKey(ui::VKEY_DOWN);
  if (SelectionWraps())
    EXPECT_EQ(1, pending_state_item()->GetCommand());
  else
    EXPECT_EQ(4, pending_state_item()->GetCommand());
}

// Test that the menu is properly placed where it best fits.
TEST_F(MenuControllerTest, CalculateMenuBoundsBestFitTest) {
  MenuBoundsOptions options;
  gfx::Rect expected;
  const bool ignore_screen_bounds_for_menus =
      ShouldIgnoreScreenBoundsForMenus();

  // Fits in all locations -> placed below.
  options.anchor_bounds =
      gfx::Rect(options.menu_size.width(), options.menu_size.height(), 0, 0);
  options.monitor_bounds =
      gfx::Rect(0, 0, options.anchor_bounds.right() + options.menu_size.width(),
                options.anchor_bounds.bottom() + options.menu_size.height());
  expected =
      gfx::Rect(options.anchor_bounds.x(), options.anchor_bounds.bottom(),
                options.menu_size.width(), options.menu_size.height());
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  // Fits above and to both sides -> placed above.
  options.anchor_bounds =
      gfx::Rect(options.menu_size.width(), options.menu_size.height(), 0, 0);
  options.monitor_bounds =
      gfx::Rect(0, 0, options.anchor_bounds.right() + options.menu_size.width(),
                options.anchor_bounds.bottom());
  if (ignore_screen_bounds_for_menus) {
    expected = gfx::Rect(options.anchor_bounds.origin(), options.menu_size);
  } else {
    expected = gfx::Rect(options.anchor_bounds.x(),
                         options.anchor_bounds.y() - options.menu_size.height(),
                         options.menu_size.width(), options.menu_size.height());
  }
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  // Fits on both sides, prefer right -> placed right.
  options.anchor_bounds = gfx::Rect(options.menu_size.width(),
                                    options.menu_size.height() / 2, 0, 0);
  options.monitor_bounds =
      gfx::Rect(0, 0, options.anchor_bounds.right() + options.menu_size.width(),
                options.menu_size.height());
  if (ignore_screen_bounds_for_menus) {
    expected = gfx::Rect(options.anchor_bounds.origin(), options.menu_size);
  } else {
    expected =
        gfx::Rect(options.anchor_bounds.right(),
                  options.monitor_bounds.bottom() - options.menu_size.height(),
                  options.menu_size.width(), options.menu_size.height());
  }

  EXPECT_EQ(expected, CalculateMenuBounds(options));

  // Fits only on left -> placed left.
  options.anchor_bounds = gfx::Rect(options.menu_size.width(),
                                    options.menu_size.height() / 2, 0, 0);
  options.monitor_bounds = gfx::Rect(0, 0, options.anchor_bounds.right(),
                                     options.menu_size.height());
  if (ignore_screen_bounds_for_menus) {
    expected = gfx::Rect(options.anchor_bounds.origin(), options.menu_size);
  } else {
    expected =
        gfx::Rect(options.anchor_bounds.x() - options.menu_size.width(),
                  options.monitor_bounds.bottom() - options.menu_size.height(),
                  options.menu_size.width(), options.menu_size.height());
  }
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  // Fits on both sides, prefer left -> placed left.
  options.menu_anchor = MenuAnchorPosition::kTopRight;
  options.anchor_bounds = gfx::Rect(options.menu_size.width(),
                                    options.menu_size.height() / 2, 0, 0);
  options.monitor_bounds =
      gfx::Rect(0, 0, options.anchor_bounds.right() + options.menu_size.width(),
                options.menu_size.height());
  if (ignore_screen_bounds_for_menus) {
    expected =
        gfx::Rect({options.anchor_bounds.right() - options.menu_size.width(),
                   options.anchor_bounds.origin().y()},
                  options.menu_size);
  } else {
    expected =
        gfx::Rect(options.anchor_bounds.x() - options.menu_size.width(),
                  options.monitor_bounds.bottom() - options.menu_size.height(),
                  options.menu_size.width(), options.menu_size.height());
  }
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  // Fits only on right -> placed right.
  options.anchor_bounds = gfx::Rect(0, options.menu_size.height() / 2, 0, 0);
  options.monitor_bounds =
      gfx::Rect(0, 0, options.anchor_bounds.right() + options.menu_size.width(),
                options.menu_size.height());
  if (ignore_screen_bounds_for_menus) {
    expected =
        gfx::Rect({options.anchor_bounds.right() - options.menu_size.width(),
                   options.anchor_bounds.origin().y()},
                  options.menu_size);
  } else {
    expected =
        gfx::Rect(options.anchor_bounds.right(),
                  options.monitor_bounds.bottom() - options.menu_size.height(),
                  options.menu_size.width(), options.menu_size.height());
  }
  EXPECT_EQ(expected, CalculateMenuBounds(options));
}

// Tests that the menu is properly placed according to its anchor.
TEST_F(MenuControllerTest, CalculateMenuBoundsAnchorTest) {
  MenuBoundsOptions options;
  gfx::Rect expected;

  options.menu_anchor = MenuAnchorPosition::kTopLeft;
  expected =
      gfx::Rect(options.anchor_bounds.x(), options.anchor_bounds.bottom(),
                options.menu_size.width(), options.menu_size.height());
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  options.menu_anchor = MenuAnchorPosition::kTopRight;
  expected =
      gfx::Rect(options.anchor_bounds.right() - options.menu_size.width(),
                options.anchor_bounds.bottom(), options.menu_size.width(),
                options.menu_size.height());
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  // Menu will be placed above or below with an offset.
  options.menu_anchor = MenuAnchorPosition::kBottomCenter;
  const int kTouchYPadding = 15;

  // Menu fits above -> placed above.
  expected = gfx::Rect(
      options.anchor_bounds.x() +
          (options.anchor_bounds.width() - options.menu_size.width()) / 2,
      options.anchor_bounds.y() - options.menu_size.height() - kTouchYPadding,
      options.menu_size.width(), options.menu_size.height());
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  // Menu does not fit above -> placed below.
  options.anchor_bounds = gfx::Rect(options.menu_size.height() / 2,
                                    options.menu_size.width(), 0, 0);
  if (ShouldIgnoreScreenBoundsForMenus()) {
    expected = gfx::Rect(
        options.anchor_bounds.x() +
            (options.anchor_bounds.width() - options.menu_size.width()) / 2,
        options.anchor_bounds.y() - options.anchor_bounds.bottom() -
            kTouchYPadding,
        options.menu_size.width(), options.menu_size.height());
  } else {
    expected = gfx::Rect(
        options.anchor_bounds.x() +
            (options.anchor_bounds.width() - options.menu_size.width()) / 2,
        options.anchor_bounds.y() + kTouchYPadding, options.menu_size.width(),
        options.menu_size.height());
  }
  EXPECT_EQ(expected, CalculateMenuBounds(options));
}

TEST_F(MenuControllerTest, CalculateMenuBoundsMonitorFitTest) {
  MenuBoundsOptions options;
  gfx::Rect expected;
  options.monitor_bounds = gfx::Rect(0, 0, 100, 100);
  options.anchor_bounds = gfx::Rect();

  options.menu_size = gfx::Size(options.monitor_bounds.width() / 2,
                                options.monitor_bounds.height() * 2);
  expected =
      gfx::Rect(options.anchor_bounds.x(), options.anchor_bounds.bottom(),
                options.menu_size.width(), options.monitor_bounds.height());
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  options.menu_size = gfx::Size(options.monitor_bounds.width() * 2,
                                options.monitor_bounds.height() / 2);
  expected =
      gfx::Rect(options.anchor_bounds.x(), options.anchor_bounds.bottom(),
                options.monitor_bounds.width(), options.menu_size.height());
  EXPECT_EQ(expected, CalculateMenuBounds(options));

  options.menu_size = gfx::Size(options.monitor_bounds.width() * 2,
                                options.monitor_bounds.height() * 2);
  expected = gfx::Rect(
      options.anchor_bounds.x(), options.anchor_bounds.bottom(),
      options.monitor_bounds.width(), options.monitor_bounds.height());
  EXPECT_EQ(expected, CalculateMenuBounds(options));
}

// Test that menus show up on screen with non-zero sized anchors.
TEST_P(MenuControllerTest, TestMenuFitsOnScreen) {
  const int display_size = 500;
  // Simulate multiple display layouts.
  for (int x = -1; x <= 1; x++)
    for (int y = -1; y <= 1; y++) {
      const gfx::Rect monitor_bounds(x * display_size, y * display_size,
                                     display_size, display_size);
      TestMenuFitsOnScreen(MenuAnchorPosition::kBubbleAbove, monitor_bounds);
      TestMenuFitsOnScreen(MenuAnchorPosition::kBubbleLeft, monitor_bounds);
      TestMenuFitsOnScreen(MenuAnchorPosition::kBubbleRight, monitor_bounds);
    }
}

// Test that menus show up on screen with zero sized anchors.
TEST_P(MenuControllerTest, TestMenuFitsOnScreenSmallAnchor) {
  const int display_size = 500;
  // Simulate multiple display layouts.
  for (int x = -1; x <= 1; x++)
    for (int y = -1; y <= 1; y++) {
      const gfx::Rect monitor_bounds(x * display_size, y * display_size,
                                     display_size, display_size);
      TestMenuFitsOnScreenSmallAnchor(MenuAnchorPosition::kBubbleAbove,
                                      monitor_bounds);
      TestMenuFitsOnScreenSmallAnchor(MenuAnchorPosition::kBubbleLeft,
                                      monitor_bounds);
      TestMenuFitsOnScreenSmallAnchor(MenuAnchorPosition::kBubbleRight,
                                      monitor_bounds);
    }
}

// Test that menus fit a small screen.
TEST_P(MenuControllerTest, TestMenuFitsOnSmallScreen) {
  const int display_size = 500;

  // Simulate multiple display layouts.
  for (int x = -1; x <= 1; x++)
    for (int y = -1; y <= 1; y++) {
      const gfx::Rect monitor_bounds(x * display_size, y * display_size,
                                     display_size, display_size);
      TestMenuFitsOnSmallScreen(MenuAnchorPosition::kBubbleAbove,
                                monitor_bounds);
      TestMenuFitsOnSmallScreen(MenuAnchorPosition::kBubbleLeft,
                                monitor_bounds);
      TestMenuFitsOnSmallScreen(MenuAnchorPosition::kBubbleRight,
                                monitor_bounds);
    }
}

// Test that submenus are displayed within the screen bounds on smaller screens.
TEST_P(MenuControllerTest, TestSubmenuFitsOnScreen) {
  menu_controller()->set_use_touchable_layout(true);
  MenuItemView* sub_item = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  sub_item->AppendMenuItem(11, base::ASCIIToUTF16("Subitem.One"));

  const int menu_width = MenuConfig::instance().touchable_menu_width;
  const gfx::Size parent_size(menu_width, menu_width);
  const gfx::Size parent_size_wide(menu_width * 2, menu_width);

  const int kDisplayWidth = parent_size.width() * 3;
  const int kDisplayHeight = parent_size.height() * 3;

  // Simulate multiple display layouts.
  for (int x = -1; x <= 1; x++)
    for (int y = -1; y <= 1; y++) {
      const gfx::Rect monitor_bounds(x * kDisplayWidth, y * kDisplayHeight,
                                     kDisplayWidth, kDisplayHeight);

      const int x_min = monitor_bounds.x();
      const int x_max = monitor_bounds.right() - parent_size.width();
      const int x_mid = (x_min + x_max) / 2;
      const int x_qtr = x_min + (x_max - x_min) / 4;

      const int y_min = monitor_bounds.y();
      const int y_max = monitor_bounds.bottom() - parent_size.height();
      const int y_mid = (y_min + y_max) / 2;

      TestSubmenuFitsOnScreen(sub_item, monitor_bounds,
                              gfx::Rect(gfx::Point(x_min, y_min), parent_size));
      TestSubmenuFitsOnScreen(sub_item, monitor_bounds,
                              gfx::Rect(gfx::Point(x_max, y_min), parent_size));
      TestSubmenuFitsOnScreen(sub_item, monitor_bounds,
                              gfx::Rect(gfx::Point(x_mid, y_min), parent_size));
      TestSubmenuFitsOnScreen(sub_item, monitor_bounds,
                              gfx::Rect(gfx::Point(x_min, y_mid), parent_size));
      TestSubmenuFitsOnScreen(sub_item, monitor_bounds,
                              gfx::Rect(gfx::Point(x_min, y_max), parent_size));

      // Extra wide menu: test with insufficient room on both sides.
      TestSubmenuFitsOnScreen(
          sub_item, monitor_bounds,
          gfx::Rect(gfx::Point(x_qtr, y_min), parent_size_wide));
    }
}

// Test that a menu that was originally drawn below the anchor does not get
// squished or move above the anchor when it grows vertically and horizontally
// beyond the monitor bounds.
TEST_F(MenuControllerTest, GrowingMenuMovesLaterallyNotVertically) {
  // We can't know the position of windows in Wayland. Thus, this case is not
  // valid for Wayland.
  if (ShouldIgnoreScreenBoundsForMenus())
    return;

  MenuBoundsOptions options;
  options.monitor_bounds = gfx::Rect(0, 0, 100, 100);
  // The anchor should be near the bottom right side of the screen.
  options.anchor_bounds = gfx::Rect(80, 70, 15, 10);
  // The menu should fit the available space, below the anchor.
  options.menu_size = gfx::Size(20, 20);

  // Ensure the menu is initially drawn below the bounds, and the MenuPosition
  // is set to MenuPosition::kBelowBounds;
  const gfx::Rect first_drawn_expected(80, 80, 20, 20);
  EXPECT_EQ(first_drawn_expected, CalculateMenuBounds(options));
  EXPECT_EQ(MenuItemView::MenuPosition::kBelowBounds,
            menu_item()->ActualMenuPosition());

  options.menu_position = MenuItemView::MenuPosition::kBelowBounds;

  // The menu bounds are larger than the remaining space on the monitor. This
  // simulates the case where the menu has been grown vertically and
  // horizontally to where it would no longer fit on the screen.
  options.menu_size = gfx::Size(50, 50);

  // The menu bounds should move left to show the wider menu, and grow to fill
  // the remaining vertical space without moving upwards.
  const gfx::Rect final_expected(50, 80, 50, 20);
  EXPECT_EQ(final_expected, CalculateMenuBounds(options));
}

#if defined(USE_AURA)
// This tests that mouse moved events from the initial position of the mouse
// when the menu was shown don't select the menu item at the mouse position.
TEST_F(MenuControllerTest, MouseAtMenuItemOnShow) {
  // Most tests create an already shown menu but this test needs one that's
  // not shown, so it can show it. The mouse position is remembered when
  // the menu is shown.
  std::unique_ptr<TestMenuItemViewNotShown> menu_item(
      new TestMenuItemViewNotShown(menu_delegate()));
  MenuItemView* first_item =
      menu_item->AppendMenuItem(1, base::ASCIIToUTF16("One"));
  menu_item->AppendMenuItem(2, base::ASCIIToUTF16("Two"));
  menu_item->SetController(menu_controller());

  // Move the mouse to where the first menu item will be shown,
  // and show the menu.
  gfx::Size item_size = first_item->CalculatePreferredSize();
  gfx::Point location(item_size.width() / 2, item_size.height() / 2);
  GetRootWindow(owner())->MoveCursorTo(location);
  menu_controller()->Run(owner(), nullptr, menu_item.get(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);

  EXPECT_EQ(0, pending_state_item()->GetCommand());

  // Synthesize an event at the mouse position when the menu was opened.
  // It should be ignored, and selected item shouldn't change.
  SubmenuView* sub_menu = menu_item->GetSubmenu();
  View::ConvertPointFromScreen(sub_menu->GetScrollViewContainer(), &location);
  ui::MouseEvent event(ui::ET_MOUSE_MOVED, location, location,
                       ui::EventTimeForNow(), 0, 0);
  ProcessMouseMoved(sub_menu, event);
  EXPECT_EQ(0, pending_state_item()->GetCommand());
  // Synthesize an event at a slightly different mouse position. It
  // should cause the item under the cursor to be selected.
  location.Offset(0, 1);
  ui::MouseEvent second_event(ui::ET_MOUSE_MOVED, location, location,
                              ui::EventTimeForNow(), 0, 0);
  ProcessMouseMoved(sub_menu, second_event);
  EXPECT_EQ(1, pending_state_item()->GetCommand());
}

// Tests that when an asynchronous menu receives a cancel event, that it closes.
TEST_F(MenuControllerTest, AsynchronousCancelEvent) {
  ExitMenuRun();
  MenuController* controller = menu_controller();
  controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);
  EXPECT_EQ(MenuController::ExitType::kNone, controller->exit_type());
  ui::CancelModeEvent cancel_event;
  event_generator()->Dispatch(&cancel_event);
  EXPECT_EQ(MenuController::ExitType::kAll, controller->exit_type());
}

// Tests that menus without parent widgets do not crash in MenuPreTargetHandler.
TEST_F(MenuControllerTest, RunWithoutWidgetDoesntCrash) {
  ExitMenuRun();
  MenuController* controller = menu_controller();
  controller->Run(nullptr, nullptr, menu_item(), gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);
}

// Tests that if a MenuController is destroying during drag/drop, and another
// MenuController becomes active, that the exiting of drag does not cause a
// crash.
TEST_F(MenuControllerTest, MenuControllerReplacedDuringDrag) {
  // Build the menu so that the appropriate root window is available to set the
  // drag drop client on.
  AddButtonMenuItems();
  TestDragDropClient drag_drop_client(base::BindRepeating(
      &MenuControllerTest::TestMenuControllerReplacementDuringDrag,
      base::Unretained(this)));
  aura::client::SetDragDropClient(
      GetRootWindow(menu_item()->GetSubmenu()->GetWidget()), &drag_drop_client);
  StartDrag();
}

// Tests that if a CancelAll is called during drag-and-drop that it does not
// destroy the MenuController. On Windows and Linux this destruction also
// destroys the Widget used for drag-and-drop, thereby ending the drag.
TEST_F(MenuControllerTest, CancelAllDuringDrag) {
  // Build the menu so that the appropriate root window is available to set the
  // drag drop client on.
  AddButtonMenuItems();
  TestDragDropClient drag_drop_client(base::BindRepeating(
      &MenuControllerTest::TestCancelAllDuringDrag, base::Unretained(this)));
  aura::client::SetDragDropClient(
      GetRootWindow(menu_item()->GetSubmenu()->GetWidget()), &drag_drop_client);
  StartDrag();
}

// Tests that when releasing the ref on ViewsDelegate and MenuController is
// deleted, that shutdown occurs without crashing.
TEST_F(MenuControllerTest, DestroyedDuringViewsRelease) {
  ExitMenuRun();
  MenuController* controller = menu_controller();
  controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);
  TestDestroyedDuringViewsRelease();
}

// Tests that when a context menu is opened above an empty menu item, and a
// right-click occurs over the empty item, that the bottom menu is not hidden,
// that a request to relaunch the context menu is received, and that
// subsequently pressing ESC does not crash the browser.
TEST_F(MenuControllerTest, RepostEventToEmptyMenuItem) {
  // Setup a submenu. Additionally hook up appropriate Widget and View
  // containers, with bounds, so that hit testing works.
  MenuController* controller = menu_controller();
  MenuItemView* base_menu = menu_item();
  base_menu->SetBounds(0, 0, 200, 200);
  SubmenuView* base_submenu = base_menu->GetSubmenu();
  base_submenu->SetBounds(0, 0, 200, 200);
  base_submenu->ShowAt(owner(), gfx::Rect(0, 0, 200, 200), false);
  GetMenuHost(base_submenu)
      ->SetContentsView(base_submenu->GetScrollViewContainer());

  // Build the submenu to have an empty menu item. Additionally hook up
  // appropriate Widget and View containers with bounds, so that hit testing
  // works.
  std::unique_ptr<TestMenuDelegate> sub_menu_item_delegate =
      std::make_unique<TestMenuDelegate>();
  std::unique_ptr<TestMenuItemViewShown> sub_menu_item =
      std::make_unique<TestMenuItemViewShown>(sub_menu_item_delegate.get());
  sub_menu_item->AddEmptyMenusForTest();
  sub_menu_item->SetController(controller);
  sub_menu_item->SetBounds(0, 50, 50, 50);
  base_submenu->AddChildView(sub_menu_item.get());
  SubmenuView* sub_menu_view = sub_menu_item->GetSubmenu();
  sub_menu_view->SetBounds(0, 50, 50, 50);
  sub_menu_view->ShowAt(owner(), gfx::Rect(0, 50, 50, 50), false);
  GetMenuHost(sub_menu_view)
      ->SetContentsView(sub_menu_view->GetScrollViewContainer());

  // Set that the last selection target was the item which launches the submenu,
  // as the empty item can never become a target.
  SetPendingStateItem(sub_menu_item.get());

  // Nest a context menu.
  std::unique_ptr<TestMenuDelegate> nested_menu_delegate_1 =
      std::make_unique<TestMenuDelegate>();
  std::unique_ptr<TestMenuItemViewShown> nested_menu_item_1 =
      std::make_unique<TestMenuItemViewShown>(nested_menu_delegate_1.get());
  nested_menu_item_1->SetBounds(0, 0, 100, 100);
  nested_menu_item_1->SetController(controller);
  std::unique_ptr<TestMenuControllerDelegate> nested_controller_delegate_1 =
      std::make_unique<TestMenuControllerDelegate>();
  controller->AddNestedDelegate(nested_controller_delegate_1.get());
  controller->Run(owner(), nullptr, nested_menu_item_1.get(),
                  gfx::Rect(150, 50, 100, 100), MenuAnchorPosition::kTopLeft,
                  true, false);

  SubmenuView* nested_menu_submenu = nested_menu_item_1->GetSubmenu();
  nested_menu_submenu->SetBounds(0, 0, 100, 100);
  nested_menu_submenu->ShowAt(owner(), gfx::Rect(0, 0, 100, 100), false);
  GetMenuHost(nested_menu_submenu)
      ->SetContentsView(nested_menu_submenu->GetScrollViewContainer());

  // Press down outside of the context menu, and within the empty menu item.
  // This should close the first context menu.
  gfx::Point press_location(sub_menu_view->bounds().CenterPoint());
  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, press_location,
                             press_location, ui::EventTimeForNow(),
                             ui::EF_RIGHT_MOUSE_BUTTON, 0);
  ProcessMousePressed(nested_menu_submenu, press_event);
  EXPECT_EQ(nested_controller_delegate_1->on_menu_closed_called(), 1);
  EXPECT_EQ(menu_controller_delegate(), GetCurrentDelegate());

  // While the current state is the menu item which launched the sub menu, cause
  // a drag in the empty menu item. This should not hide the menu.
  SetState(sub_menu_item.get());
  press_location.Offset(-5, 0);
  ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, press_location,
                            press_location, ui::EventTimeForNow(),
                            ui::EF_RIGHT_MOUSE_BUTTON, 0);
  ProcessMouseDragged(sub_menu_view, drag_event);
  EXPECT_EQ(menu_delegate()->will_hide_menu_count(), 0);

  // Release the mouse in the empty menu item, triggering a context menu
  // request.
  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, press_location,
                               press_location, ui::EventTimeForNow(),
                               ui::EF_RIGHT_MOUSE_BUTTON, 0);
  ProcessMouseReleased(sub_menu_view, release_event);
  EXPECT_EQ(sub_menu_item_delegate->show_context_menu_count(), 1);
  EXPECT_EQ(sub_menu_item_delegate->show_context_menu_source(),
            sub_menu_item.get());

  // Nest a context menu.
  std::unique_ptr<TestMenuDelegate> nested_menu_delegate_2 =
      std::make_unique<TestMenuDelegate>();
  std::unique_ptr<TestMenuItemViewShown> nested_menu_item_2 =
      std::make_unique<TestMenuItemViewShown>(nested_menu_delegate_2.get());
  nested_menu_item_2->SetBounds(0, 0, 100, 100);
  nested_menu_item_2->SetController(controller);

  std::unique_ptr<TestMenuControllerDelegate> nested_controller_delegate_2 =
      std::make_unique<TestMenuControllerDelegate>();
  controller->AddNestedDelegate(nested_controller_delegate_2.get());
  controller->Run(owner(), nullptr, nested_menu_item_2.get(),
                  gfx::Rect(150, 50, 100, 100), MenuAnchorPosition::kTopLeft,
                  true, false);

  // The escape key should only close the nested menu. SelectByChar should not
  // crash.
  TestAsyncEscapeKey();
  EXPECT_EQ(nested_controller_delegate_2->on_menu_closed_called(), 1);
  EXPECT_EQ(menu_controller_delegate(), GetCurrentDelegate());
}

// Drag the mouse from an external view into a menu
// When the mouse leaves the menu while still in the process of dragging
// the menu item view highlight should turn off
TEST_F(MenuControllerTest, DragFromViewIntoMenuAndExit) {
  SubmenuView* sub_menu = menu_item()->GetSubmenu();
  MenuItemView* first_item = sub_menu->GetMenuItemAt(0);

  std::unique_ptr<View> drag_view = std::make_unique<View>();
  drag_view->SetBoundsRect(gfx::Rect(0, 500, 100, 100));
  sub_menu->ShowAt(owner(), gfx::Rect(0, 0, 100, 100), false);
  gfx::Point press_location(drag_view->bounds().CenterPoint());
  gfx::Point drag_location(first_item->bounds().CenterPoint());
  gfx::Point release_location(200, 50);

  // Begin drag on an external view
  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, press_location,
                             press_location, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  drag_view->OnMousePressed(press_event);

  // Drag into a menu item
  ui::MouseEvent drag_event_enter(ui::ET_MOUSE_DRAGGED, drag_location,
                                  drag_location, ui::EventTimeForNow(),
                                  ui::EF_LEFT_MOUSE_BUTTON, 0);
  ProcessMouseDragged(sub_menu, drag_event_enter);
  EXPECT_TRUE(first_item->IsSelected());

  // Drag out of the menu item
  ui::MouseEvent drag_event_exit(ui::ET_MOUSE_DRAGGED, release_location,
                                 release_location, ui::EventTimeForNow(),
                                 ui::EF_LEFT_MOUSE_BUTTON, 0);
  ProcessMouseDragged(sub_menu, drag_event_exit);
  EXPECT_FALSE(first_item->IsSelected());

  // Complete drag with release
  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, release_location,
                               release_location, ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  ProcessMouseReleased(sub_menu, release_event);
}

#endif  // defined(USE_AURA)

// Tests that having the MenuController deleted during OnMousePressed does not
// cause a crash. ASAN bots should not detect use-after-free in MenuController.
TEST_F(MenuControllerTest, NoUseAfterFreeWhenMenuCanceledOnMousePress) {
  MenuController* controller = menu_controller();
  DestroyMenuControllerOnMenuClosed(menu_controller_delegate());

  // Creating own MenuItem for a minimal test environment.
  auto item = std::make_unique<TestMenuItemViewNotShown>(menu_delegate());
  item->SetController(controller);
  item->SetBounds(0, 0, 50, 50);

  SubmenuView* sub_menu = item->CreateSubmenu();
  auto* canceling_view = new CancelMenuOnMousePressView(controller);
  sub_menu->AddChildView(canceling_view);
  canceling_view->SetBoundsRect(item->bounds());

  controller->Run(owner(), nullptr, item.get(), item->bounds(),
                  MenuAnchorPosition::kTopLeft, false, false);
  sub_menu->ShowAt(owner(), item->bounds(), true);

  // Simulate a mouse press in the middle of the |closing_widget|.
  gfx::Point location(canceling_view->bounds().CenterPoint());
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, location, location,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  EXPECT_TRUE(controller->OnMousePressed(sub_menu, event));

  // Close to remove observers before test TearDown.
  sub_menu->Close();
}

TEST_F(MenuControllerTest, SetSelectionIndices_MenuItemsOnly) {
  MenuItemView* const item1 = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  MenuItemView* const item2 = menu_item()->GetSubmenu()->GetMenuItemAt(1);
  MenuItemView* const item3 = menu_item()->GetSubmenu()->GetMenuItemAt(2);
  MenuItemView* const item4 = menu_item()->GetSubmenu()->GetMenuItemAt(3);
  OpenMenu(menu_item());

  ui::AXNodeData data;
  item1->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(1, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(4, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item2->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(2, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(4, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item3->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(3, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(4, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item4->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(4, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(4, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
}

TEST_F(MenuControllerTest,
       SetSelectionIndices_MenuItemsOnly_SkipHiddenAndDisabled) {
  MenuItemView* const item1 = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  item1->SetEnabled(false);
  MenuItemView* const item2 = menu_item()->GetSubmenu()->GetMenuItemAt(1);
  MenuItemView* const item3 = menu_item()->GetSubmenu()->GetMenuItemAt(2);
  item3->SetVisible(false);
  MenuItemView* const item4 = menu_item()->GetSubmenu()->GetMenuItemAt(3);
  OpenMenu(menu_item());

  ui::AXNodeData data;
  item2->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(1, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(2, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item4->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(2, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(2, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
}

TEST_F(MenuControllerTest, SetSelectionIndices_Buttons) {
  AddButtonMenuItems();
  MenuItemView* const item1 = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  MenuItemView* const item2 = menu_item()->GetSubmenu()->GetMenuItemAt(1);
  MenuItemView* const item3 = menu_item()->GetSubmenu()->GetMenuItemAt(2);
  MenuItemView* const item4 = menu_item()->GetSubmenu()->GetMenuItemAt(3);
  MenuItemView* const item5 = menu_item()->GetSubmenu()->GetMenuItemAt(4);
  Button* const button1 = Button::AsButton(item5->children()[0]);
  Button* const button2 = Button::AsButton(item5->children()[1]);
  Button* const button3 = Button::AsButton(item5->children()[2]);
  OpenMenu(menu_item());

  ui::AXNodeData data;
  item1->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(1, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(7, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item2->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(2, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(7, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item3->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(3, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(7, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item4->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(4, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(7, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  button1->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(7, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  button2->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(6, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(7, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  button3->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(7, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(7, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
}

TEST_F(MenuControllerTest, SetSelectionIndices_Buttons_SkipHiddenAndDisabled) {
  AddButtonMenuItems();
  MenuItemView* const item1 = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  MenuItemView* const item2 = menu_item()->GetSubmenu()->GetMenuItemAt(1);
  MenuItemView* const item3 = menu_item()->GetSubmenu()->GetMenuItemAt(2);
  MenuItemView* const item4 = menu_item()->GetSubmenu()->GetMenuItemAt(3);
  MenuItemView* const item5 = menu_item()->GetSubmenu()->GetMenuItemAt(4);
  Button* const button1 = Button::AsButton(item5->children()[0]);
  button1->SetEnabled(false);
  Button* const button2 = Button::AsButton(item5->children()[1]);
  button2->SetVisible(false);
  Button* const button3 = Button::AsButton(item5->children()[2]);
  OpenMenu(menu_item());

  ui::AXNodeData data;
  item1->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(1, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item2->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(2, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item3->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(3, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item4->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(4, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  button3->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
}

TEST_F(MenuControllerTest, SetSelectionIndices_NestedButtons) {
  MenuItemView* const item1 = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  MenuItemView* const item2 = menu_item()->GetSubmenu()->GetMenuItemAt(1);
  MenuItemView* const item3 = menu_item()->GetSubmenu()->GetMenuItemAt(2);
  MenuItemView* const item4 = menu_item()->GetSubmenu()->GetMenuItemAt(3);

  // This simulates how buttons are nested in views in the main app menu.
  View* const container_view = new View();
  container_view->GetViewAccessibility().OverrideRole(ax::mojom::Role::kMenu);
  item4->AddChildView(container_view);

  // There's usually a label before the traversable elements.
  container_view->AddChildView(new Label());

  // Add two focusable buttons (buttons in menus are always focusable).
  Button* const button1 = new LabelButton(nullptr, base::string16());
  button1->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  button1->GetViewAccessibility().OverrideRole(ax::mojom::Role::kMenuItem);
  container_view->AddChildView(button1);
  Button* const button2 = new LabelButton(nullptr, base::string16());
  button2->GetViewAccessibility().OverrideRole(ax::mojom::Role::kMenuItem);
  button2->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  container_view->AddChildView(button2);

  OpenMenu(menu_item());

  ui::AXNodeData data;
  item1->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(1, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item2->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(2, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  item3->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(3, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  data = ui::AXNodeData();
  button1->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(4, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  data = ui::AXNodeData();
  button2->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(5, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
}

// Tests that a menu opened asynchronously, will notify its
// MenuControllerDelegate when accessibility performs a do default action.
TEST_F(MenuControllerTest, AccessibilityDoDefaultCallsAccept) {
  views::test::DisableMenuClosureAnimations();

  MenuController* controller = menu_controller();
  controller->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                  MenuAnchorPosition::kTopLeft, false, false);
  TestMenuControllerDelegate* delegate = menu_controller_delegate();
  EXPECT_EQ(0, delegate->on_menu_closed_called());

  MenuItemView* accepted = menu_item()->GetSubmenu()->GetMenuItemAt(0);
  ui::AXActionData data;
  data.action = ax::mojom::Action::kDoDefault;
  accepted->HandleAccessibleAction(data);
  views::test::WaitForMenuClosureAnimation();

  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(accepted, delegate->on_menu_closed_menu());
  EXPECT_EQ(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
            delegate->on_menu_closed_notify_type());
}

// Test that the kSelectedChildrenChanged event is emitted on
// the root menu item when the selected menu item changes.
TEST_F(MenuControllerTest, AccessibilityEmitsSelectChildrenChanged) {
  TestAXEventObserver observer;
  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);

  // Arrow down to select an item checking the event has been emitted.
  EXPECT_EQ(observer.saw_selected_children_changed_, false);
  DispatchKey(ui::VKEY_DOWN);
  EXPECT_EQ(observer.saw_selected_children_changed_, true);

  observer.saw_selected_children_changed_ = false;
  DispatchKey(ui::VKEY_DOWN);
  EXPECT_EQ(observer.saw_selected_children_changed_, true);
}

#if defined(OS_MACOSX)
// This test exercises a Mac-specific behavior, by which hotkeys using modifiers
// cause menus to close and the hotkeys to be handled by the browser window.
// This specific test case tries using cmd-ctrl-f, which normally means
// "Fullscreen".
TEST_F(MenuControllerTest, BrowserHotkeysCancelMenusAndAreRedispatched) {
  menu_controller()->Run(owner(), nullptr, menu_item(), gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, false, false);

  int options = ui::EF_COMMAND_DOWN;
  ui::KeyEvent press_cmd(ui::ET_KEY_PRESSED, ui::VKEY_COMMAND, options);
  menu_controller()->OnWillDispatchKeyEvent(&press_cmd);
  EXPECT_TRUE(IsShowing());  // ensure the command press itself doesn't cancel

  options |= ui::EF_CONTROL_DOWN;
  ui::KeyEvent press_ctrl(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, options);
  menu_controller()->OnWillDispatchKeyEvent(&press_ctrl);
  EXPECT_TRUE(IsShowing());

  ui::KeyEvent press_f(ui::ET_KEY_PRESSED, ui::VKEY_F, options);
  menu_controller()->OnWillDispatchKeyEvent(&press_f);  // to pay respects
  EXPECT_FALSE(IsShowing());
  EXPECT_FALSE(press_f.handled());
  EXPECT_FALSE(press_f.stopped_propagation());
}
#endif

}  // namespace test
}  // namespace views
