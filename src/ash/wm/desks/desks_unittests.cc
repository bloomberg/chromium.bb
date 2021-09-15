// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/sticky_keys/sticky_keys_controller.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/multi_user/multi_user_window_manager_impl.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/public/cpp/event_rewriter_controller.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/multi_user_window_manager_delegate.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/close_desk_button.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_animation_base.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_restore_util.h"
#include "ash/wm/desks/desks_test_api.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/expanded_state_new_desk_button.h"
#include "ash/wm/desks/persistent_desks_bar_button.h"
#include "ash/wm/desks/persistent_desks_bar_context_menu.h"
#include "ash/wm/desks/persistent_desks_bar_controller.h"
#include "ash/wm/desks/persistent_desks_bar_view.h"
#include "ash/wm/desks/root_window_desk_switch_animator_test_api.h"
#include "ash/wm/desks/scroll_arrow_button.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/ime/chromeos/fake_ime_keyboard.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/events/event_rewriter_chromeos.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/client_view.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/window_modality_controller.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

class TestShelfItemDelegate : public ShelfItemDelegate {
 public:
  explicit TestShelfItemDelegate(const ShelfID& shelf_id)
      : ShelfItemDelegate(shelf_id) {}
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override {}
  void Close() override {}
};

void NewDesk() {
  // Create a desk through keyboard. Do not use |kButton| to avoid empty name.
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kKeyboard);
}

std::unique_ptr<aura::Window> CreateTransientWindow(
    aura::Window* transient_parent,
    const gfx::Rect& bounds) {
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr, aura::client::WINDOW_TYPE_POPUP);
  window->Init(ui::LAYER_NOT_DRAWN);
  window->SetBounds(bounds);
  ::wm::AddTransientChild(transient_parent, window.get());
  aura::client::ParentWindowWithContext(
      window.get(), transient_parent->GetRootWindow(), bounds);
  window->Show();
  return window;
}

std::unique_ptr<aura::Window> CreateTransientModalChildWindow(
    aura::Window* transient_parent) {
  auto child =
      CreateTransientWindow(transient_parent, gfx::Rect(20, 30, 200, 150));
  child->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
  ::wm::SetModalParent(child.get(), transient_parent);
  return child;
}

bool DoesActiveDeskContainWindow(aura::Window* window) {
  return base::Contains(DesksController::Get()->active_desk()->windows(),
                        window);
}

OverviewGrid* GetOverviewGridForRoot(aura::Window* root) {
  DCHECK(root->IsRootWindow());

  auto* overview_controller = Shell::Get()->overview_controller();
  DCHECK(overview_controller->InOverviewSession());

  return overview_controller->overview_session()->GetGridWithRootWindow(root);
}

void CloseDeskFromMiniView(const DeskMiniView* desk_mini_view,
                           ui::test::EventGenerator* event_generator) {
  DCHECK(desk_mini_view);

  // Move to the center of the mini view so that the close button shows up.
  const gfx::Point mini_view_center =
      desk_mini_view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(mini_view_center);
  EXPECT_TRUE(desk_mini_view->close_desk_button()->GetVisible());
  // Move to the center of the close button and click.
  event_generator->MoveMouseTo(
      desk_mini_view->close_desk_button()->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
}

void ClickOnView(const views::View* view,
                 ui::test::EventGenerator* event_generator) {
  DCHECK(view);

  const gfx::Point view_center = view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(view_center);
  event_generator->ClickLeftButton();
}

void WaitForMilliseconds(int milliseconds) {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(milliseconds));
  run_loop.Run();
}

void LongGestureTap(const gfx::Point& screen_location,
                    ui::test::EventGenerator* event_generator,
                    bool release_touch = true) {
  // Temporarily reconfigure gestures so that the long tap takes 2 milliseconds.
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  const int old_long_press_time_in_ms = gesture_config->long_press_time_in_ms();
  const int old_show_press_delay_in_ms =
      gesture_config->show_press_delay_in_ms();
  gesture_config->set_long_press_time_in_ms(1);
  gesture_config->set_show_press_delay_in_ms(1);

  event_generator->set_current_screen_location(screen_location);
  event_generator->PressTouch();
  WaitForMilliseconds(2);

  gesture_config->set_long_press_time_in_ms(old_long_press_time_in_ms);
  gesture_config->set_show_press_delay_in_ms(old_show_press_delay_in_ms);

  if (release_touch)
    event_generator->ReleaseTouch();
}

void GestureTapOnView(const views::View* view,
                      ui::test::EventGenerator* event_generator) {
  event_generator->GestureTapAt(view->GetBoundsInScreen().CenterPoint());
}

// If |drop| is false, the dragged |item| won't be dropped; giving the caller
// a chance to do some validations before the item is dropped.
void DragItemToPoint(OverviewItem* item,
                     const gfx::Point& screen_location,
                     ui::test::EventGenerator* event_generator,
                     bool by_touch_gestures = false,
                     bool drop = true) {
  DCHECK(item);

  const gfx::Point item_center =
      gfx::ToRoundedPoint(item->target_bounds().CenterPoint());
  event_generator->set_current_screen_location(item_center);
  if (by_touch_gestures) {
    event_generator->PressTouch();
    // Move the touch by an enough amount in X to engage in the normal drag mode
    // rather than the drag to close mode.
    event_generator->MoveTouchBy(50, 0);
    event_generator->MoveTouch(screen_location);
    if (drop)
      event_generator->ReleaseTouch();
  } else {
    event_generator->PressLeftButton();
    Shell::Get()->cursor_manager()->SetDisplay(
        display::Screen::GetScreen()->GetDisplayNearestPoint(screen_location));
    event_generator->MoveMouseTo(screen_location);
    if (drop)
      event_generator->ReleaseLeftButton();
  }
}

BackdropController* GetDeskBackdropController(const Desk* desk,
                                              aura::Window* root) {
  auto* workspace_controller =
      GetWorkspaceController(desk->GetDeskContainerForRoot(root));
  WorkspaceLayoutManager* layout_manager =
      workspace_controller->layout_manager();
  return layout_manager->backdrop_controller();
}

// Returns true if |win1| is stacked (not directly) below |win2|.
bool IsStackedBelow(aura::Window* win1, aura::Window* win2) {
  DCHECK_NE(win1, win2);
  DCHECK_EQ(win1->parent(), win2->parent());

  const auto& children = win1->parent()->children();
  auto win1_iter = std::find(children.begin(), children.end(), win1);
  auto win2_iter = std::find(children.begin(), children.end(), win2);
  DCHECK(win1_iter != children.end());
  DCHECK(win2_iter != children.end());
  return win1_iter < win2_iter;
}

// Simulate pressing on a desk preview.
void LongTapOnDeskPreview(const DeskMiniView* desk_mini_view,
                          ui::test::EventGenerator* event_generator) {
  DCHECK(desk_mini_view);

  gfx::Point desk_preview_center =
      desk_mini_view->GetPreviewBoundsInScreen().CenterPoint();

  LongGestureTap(desk_preview_center, event_generator, /*release_touch=*/false);
}

// Simulate drag on a desk preview.
void StartDragDeskPreview(const DeskMiniView* desk_mini_view,
                          ui::test::EventGenerator* event_generator) {
  DCHECK(desk_mini_view);

  gfx::Point desk_preview_center =
      desk_mini_view->GetPreviewBoundsInScreen().CenterPoint();
  event_generator->set_current_screen_location(desk_preview_center);

  event_generator->PressLeftButton();
  event_generator->MoveMouseBy(0, 50);
}

// Defines an observer to test DesksController notifications.
class TestObserver : public DesksController::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  const std::vector<const Desk*>& desks() const { return desks_; }
  int desk_name_changed_notify_counts() const {
    return desk_name_changed_notify_counts_;
  }

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk) override {
    desks_.emplace_back(desk);
    EXPECT_TRUE(DesksController::Get()->AreDesksBeingModified());
  }
  void OnDeskRemoved(const Desk* desk) override {
    base::Erase(desks_, desk);
    EXPECT_TRUE(DesksController::Get()->AreDesksBeingModified());
  }
  void OnDeskReordered(int old_index, int new_index) override {}
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override {
    EXPECT_TRUE(DesksController::Get()->AreDesksBeingModified());
  }
  void OnDeskSwitchAnimationLaunching() override {}
  void OnDeskSwitchAnimationFinished() override {
    EXPECT_FALSE(DesksController::Get()->AreDesksBeingModified());
  }
  void OnDeskNameChanged(const Desk* desk,
                         const std::u16string& new_name) override {
    ++desk_name_changed_notify_counts_;
  }

 private:
  std::vector<const Desk*> desks_;

  int desk_name_changed_notify_counts_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class TestDeskObserver : public Desk::Observer {
 public:
  TestDeskObserver() = default;
  ~TestDeskObserver() override = default;

  int notify_counts() const { return notify_counts_; }

  // Desk::Observer:
  void OnContentChanged() override { ++notify_counts_; }
  void OnDeskDestroyed(const Desk* desk) override {}
  void OnDeskNameChanged(const std::u16string& new_name) override {}

 private:
  int notify_counts_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestDeskObserver);
};

// Defines a test fixture to test Virtual Desks behavior, parameterized to run
// some window drag tests with both touch gestures and with mouse events.
class DesksTest : public AshTestBase,
                  public ::testing::WithParamInterface<bool> {
 public:
  DesksTest() = default;
  explicit DesksTest(base::test::TaskEnvironment::TimeSource time)
      : AshTestBase(time) {}
  ~DesksTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    SetVirtualKeyboardEnabled(true);
  }

  void SendKey(ui::KeyboardCode key_code, int flags = ui::EF_NONE) {
    PressAndReleaseKey(key_code, flags);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DesksTest);
};

TEST_F(DesksTest, DesksCreationAndRemoval) {
  TestObserver observer;
  auto* controller = DesksController::Get();
  controller->AddObserver(&observer);

  // There's always a default pre-existing desk that cannot be removed.
  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_FALSE(controller->CanRemoveDesks());
  EXPECT_TRUE(controller->CanCreateDesks());

  // Add desks until no longer possible.
  while (controller->CanCreateDesks())
    NewDesk();

  // Expect we've reached the max number of desks, and we've been notified only
  // with the newly created desks.
  EXPECT_EQ(desks_util::kMaxNumberOfDesks, controller->desks().size());
  EXPECT_EQ(desks_util::kMaxNumberOfDesks - 1, observer.desks().size());
  EXPECT_TRUE(controller->CanRemoveDesks());

  // Remove all desks until no longer possible, and expect that there's always
  // one default desk remaining.
  while (controller->CanRemoveDesks())
    RemoveDesk(observer.desks().back());

  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_FALSE(controller->CanRemoveDesks());
  EXPECT_TRUE(controller->CanCreateDesks());
  EXPECT_TRUE(observer.desks().empty());

  controller->RemoveObserver(&observer);
}

// Verifies that desk's name change notifies |DesksController::Observer|.
TEST_F(DesksTest, OnDeskNameChanged) {
  TestObserver observer;
  auto* controller = DesksController::Get();
  controller->AddObserver(&observer);

  NewDesk();
  controller->desks()[0]->SetName(u"test1", /*set_by_user=*/true);
  controller->desks()[1]->SetName(u"test2", /*set_by_user=*/true);

  // Verify that desk name change will trigger
  // |TestObserver::OnDeskNameChanged()|. Notice when creating a new desk
  // and setting its name, it also triggers the call. Thus
  // |desk_name_changed_notify_counts_| is 3.
  ASSERT_EQ(3, observer.desk_name_changed_notify_counts());
  controller->RemoveObserver(&observer);
}

TEST_F(DesksTest, DesksBarViewDeskCreation) {
  auto* controller = DesksController::Get();

  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());

  const auto* desks_bar_view = overview_grid->desks_bar_view();
  EXPECT_FALSE(overview_grid->IsDesksBarViewActive());

  DCHECK(desks_bar_view);
  EXPECT_TRUE(desks_bar_view->mini_views().empty());

  auto* event_generator = GetEventGenerator();
  ClickOnView(desks_bar_view->zero_state_default_desk_button(),
              event_generator);
  EXPECT_FALSE(desks_bar_view->IsZeroState());

  auto* new_desk_button =
      desks_bar_view->expanded_state_new_desk_button()->new_desk_button();
  EXPECT_TRUE(new_desk_button->GetEnabled());
  // Click many times on the expanded new desk button and expect only the max
  // number of desks will be created, and the button is no longer enabled.
  for (size_t i = 0; i < desks_util::kMaxNumberOfDesks + 2; ++i)
    ClickOnView(new_desk_button, event_generator);

  EXPECT_TRUE(overview_grid->IsDesksBarViewActive());
  EXPECT_EQ(desks_util::kMaxNumberOfDesks, controller->desks().size());
  EXPECT_EQ(controller->desks().size(), desks_bar_view->mini_views().size());
  EXPECT_FALSE(controller->CanCreateDesks());
  EXPECT_TRUE(controller->CanRemoveDesks());
  EXPECT_FALSE(new_desk_button->GetEnabled());
  EXPECT_EQ(views::Button::STATE_DISABLED, new_desk_button->GetState());

  // Hover over one of the mini_views, and expect that the close button
  // becomes visible.
  const auto* mini_view = desks_bar_view->mini_views().front();
  EXPECT_FALSE(mini_view->close_desk_button()->GetVisible());
  const gfx::Point mini_view_center =
      mini_view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(mini_view_center);
  EXPECT_TRUE(mini_view->close_desk_button()->GetVisible());

  // Use the close button to close the desk.
  event_generator->MoveMouseTo(
      mini_view->close_desk_button()->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();

  // The new desk button is now enabled again.
  EXPECT_EQ(desks_util::kMaxNumberOfDesks - 1, controller->desks().size());
  EXPECT_EQ(controller->desks().size(), desks_bar_view->mini_views().size());
  EXPECT_TRUE(controller->CanCreateDesks());
  EXPECT_TRUE(new_desk_button->GetEnabled());
  EXPECT_EQ(views::Button::STATE_NORMAL, new_desk_button->GetState());

  // Exit overview mode and re-enter. Since we have more than one pre-existing
  // desks, their mini_views should be created upon construction of the desks
  // bar.
  ExitOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Get the new grid and the new desk_bar_view.
  overview_grid =
      overview_controller->overview_session()->GetGridWithRootWindow(
          Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(overview_grid->IsDesksBarViewActive());
  desks_bar_view = overview_grid->desks_bar_view();

  DCHECK(desks_bar_view);
  EXPECT_EQ(controller->desks().size(), desks_bar_view->mini_views().size());
  EXPECT_TRUE(desks_bar_view->expanded_state_new_desk_button()
                  ->new_desk_button()
                  ->GetEnabled());
}

TEST_F(DesksTest, RemoveDeskWithEmptyName) {
  auto* controller = DesksController::Get();

  EnterOverview();
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());

  const auto* desks_bar_view = overview_grid->desks_bar_view();

  auto* event_generator = GetEventGenerator();

  // Create a new desk by using button with empty name.
  controller->NewDesk(DesksCreationRemovalSource::kButton);
  EXPECT_EQ(2u, controller->desks().size());

  // Close the newly created desk with the close button.
  auto* mini_view = desks_bar_view->mini_views().back();
  CloseDeskFromMiniView(mini_view, event_generator);
  EXPECT_EQ(1u, controller->desks().size());
}

// Tests that removing a non-active desk updates the window workspaces for
// desks restore correctly.
TEST_F(DesksTest, RemovingNonActiveDeskUpdatesWindowWorkspaces) {
  auto* controller = DesksController::Get();

  // Create two new desks.
  NewDesk();
  NewDesk();
  EXPECT_EQ(3u, controller->desks().size());

  // Create one window in each desk.
  std::vector<std::unique_ptr<aura::Window>> windows;
  for (int i = 0; i < 3; i++) {
    windows.push_back(CreateAppWindow());
    controller->SendToDeskAtIndex(windows[i].get(), i);
    EXPECT_EQ(i, windows[i]->GetProperty(aura::client::kWindowWorkspaceKey));
  }

  // Switch to the third desk.
  ActivateDesk(controller->desks()[2].get());

  // Close the second desk.
  RemoveDesk(controller->desks()[1].get());
  EXPECT_EQ(2u, controller->desks().size());

  // The window in the first desk remain unaffected by the second desk removal.
  EXPECT_EQ(0, windows[0]->GetProperty(aura::client::kWindowWorkspaceKey));
  // The window in the removed second desk should move to the active desk, the
  // third desk, which just becomes the second desk after the removal.
  EXPECT_EQ(1, windows[1]->GetProperty(aura::client::kWindowWorkspaceKey));
  // The window in the third desk update its workspace to index - 1.
  EXPECT_EQ(1, windows[2]->GetProperty(aura::client::kWindowWorkspaceKey));
}

// Tests that removing an active desk updates the window workspaces for desks
// restore correctly.
TEST_F(DesksTest, RemovingActiveDeskUpdatesWindowWorkspaces) {
  auto* controller = DesksController::Get();

  // Create three new desks.
  NewDesk();
  NewDesk();
  NewDesk();
  EXPECT_EQ(4u, controller->desks().size());

  // Create one window in each desk.
  std::vector<std::unique_ptr<aura::Window>> windows;
  for (int i = 0; i < 4; i++) {
    windows.push_back(CreateAppWindow());
    controller->SendToDeskAtIndex(windows[i].get(), i);
    EXPECT_EQ(i, windows[i]->GetProperty(aura::client::kWindowWorkspaceKey));
  }

  // Switch to the second desk.
  const Desk* desk_2 = controller->desks()[1].get();
  ActivateDesk(desk_2);

  // Close the second desk.
  RemoveDesk(desk_2);
  EXPECT_EQ(3u, controller->desks().size());

  // The previous desk (first) becomes active after closing the second desk.
  EXPECT_EQ(controller->desks()[0].get(), controller->active_desk());

  // The window in the first desk remain unaffected by the second desk removal.
  EXPECT_EQ(0, windows[0]->GetProperty(aura::client::kWindowWorkspaceKey));
  // The window from the removed second desk moves to the active, first desk.
  EXPECT_EQ(0, windows[1]->GetProperty(aura::client::kWindowWorkspaceKey));
  // The desk indices of the third and forth desks are decreased by one, so
  // the workspace values of windows in those desks are reduced by one.
  EXPECT_EQ(1, windows[2]->GetProperty(aura::client::kWindowWorkspaceKey));
  EXPECT_EQ(2, windows[3]->GetProperty(aura::client::kWindowWorkspaceKey));
}

// Test that gesture taps do not reset the button state to normal when the
// button is disabled. https://crbug.com/1084241.
TEST_F(DesksTest, GestureTapOnNewDeskButton) {
  EnterOverview();
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());

  const auto* desks_bar_view = overview_grid->desks_bar_view();
  auto* event_generator = GetEventGenerator();
  DCHECK(desks_bar_view);
  ClickOnView(desks_bar_view->zero_state_default_desk_button(),
              event_generator);
  auto* new_desk_button =
      desks_bar_view->expanded_state_new_desk_button()->new_desk_button();
  EXPECT_TRUE(new_desk_button->GetEnabled());

  // Gesture tap multiple times on the new desk button until it's disabled,
  // and verify the button state.
  for (size_t i = 0; i < desks_util::kMaxNumberOfDesks + 2; ++i)
    GestureTapOnView(new_desk_button, event_generator);

  EXPECT_FALSE(new_desk_button->GetEnabled());
  EXPECT_EQ(views::Button::STATE_DISABLED, new_desk_button->GetState());
}

TEST_F(DesksTest, DeskActivation) {
  auto* controller = DesksController::Get();
  ASSERT_EQ(1u, controller->desks().size());
  const Desk* desk_1 = controller->desks()[0].get();
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(desk_1->is_active());

  auto* root = Shell::GetPrimaryRootWindow();
  EXPECT_TRUE(desk_1->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_EQ(desks_util::GetActiveDeskContainerForRoot(root),
            desk_1->GetDeskContainerForRoot(root));

  // Create three new desks, and activate one of the middle ones.
  NewDesk();
  NewDesk();
  NewDesk();
  ASSERT_EQ(4u, controller->desks().size());
  const Desk* desk_2 = controller->desks()[1].get();
  const Desk* desk_3 = controller->desks()[2].get();
  const Desk* desk_4 = controller->desks()[3].get();
  EXPECT_FALSE(controller->AreDesksBeingModified());
  ActivateDesk(desk_2);
  EXPECT_FALSE(controller->AreDesksBeingModified());
  EXPECT_EQ(desk_2, controller->active_desk());
  EXPECT_FALSE(desk_1->is_active());
  EXPECT_TRUE(desk_2->is_active());
  EXPECT_FALSE(desk_3->is_active());
  EXPECT_FALSE(desk_4->is_active());
  EXPECT_FALSE(desk_1->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_TRUE(desk_2->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_3->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(root)->IsVisible());

  // Remove the active desk, which is in the middle, activation should move to
  // the left, so desk 1 should be activated.
  EXPECT_FALSE(controller->AreDesksBeingModified());
  RemoveDesk(desk_2);
  EXPECT_FALSE(controller->AreDesksBeingModified());
  ASSERT_EQ(3u, controller->desks().size());
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_FALSE(desk_3->is_active());
  EXPECT_FALSE(desk_4->is_active());
  EXPECT_TRUE(desk_1->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_3->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(root)->IsVisible());

  // Remove the active desk, it's the first one on the left, so desk_3 (on the
  // right) will be activated.
  EXPECT_FALSE(controller->AreDesksBeingModified());
  RemoveDesk(desk_1);
  EXPECT_FALSE(controller->AreDesksBeingModified());
  ASSERT_EQ(2u, controller->desks().size());
  EXPECT_EQ(desk_3, controller->active_desk());
  EXPECT_TRUE(desk_3->is_active());
  EXPECT_FALSE(desk_4->is_active());
  EXPECT_TRUE(desk_3->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(root)->IsVisible());
}

TEST_F(DesksTest, TestWindowPositioningPaused) {
  auto* controller = DesksController::Get();
  NewDesk();

  // Create two windows whose window positioning is managed.
  const auto win0_bounds = gfx::Rect{10, 20, 250, 100};
  const auto win1_bounds = gfx::Rect{50, 50, 200, 200};
  auto win0 = CreateAppWindow(win0_bounds);
  auto win1 = CreateAppWindow(win1_bounds);
  WindowState* window_state = WindowState::Get(win0.get());
  window_state->SetWindowPositionManaged(true);
  window_state = WindowState::Get(win1.get());
  window_state->SetWindowPositionManaged(true);
  EXPECT_EQ(win0_bounds, win0->GetBoundsInScreen());
  EXPECT_EQ(win1_bounds, win1->GetBoundsInScreen());

  // Moving one window to the second desk should not affect the bounds of either
  // windows.
  Desk* desk_2 = controller->desks()[1].get();
  controller->MoveWindowFromActiveDeskTo(
      win1.get(), desk_2, win1->GetRootWindow(),
      DesksMoveWindowFromActiveDeskSource::kDragAndDrop);
  EXPECT_EQ(win0_bounds, win0->GetBoundsInScreen());
  EXPECT_EQ(win1_bounds, win1->GetBoundsInScreen());

  // Removing a desk, which results in moving its windows to another desk should
  // not affect the positions of those managed windows.
  RemoveDesk(desk_2);
  EXPECT_EQ(win0_bounds, win0->GetBoundsInScreen());
  EXPECT_EQ(win1_bounds, win1->GetBoundsInScreen());
}

// This test makes sure we have coverage for that desk switch animation when run
// with multiple displays.
TEST_F(DesksTest, DeskActivationDualDisplay) {
  UpdateDisplay("600x600,400x500");

  auto* controller = DesksController::Get();
  ASSERT_EQ(1u, controller->desks().size());
  const Desk* desk_1 = controller->desks()[0].get();
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(desk_1->is_active());

  // Create three new desks, and activate one of the middle ones.
  NewDesk();
  NewDesk();
  NewDesk();
  ASSERT_EQ(4u, controller->desks().size());
  const Desk* desk_2 = controller->desks()[1].get();
  const Desk* desk_3 = controller->desks()[2].get();
  const Desk* desk_4 = controller->desks()[3].get();
  EXPECT_FALSE(controller->AreDesksBeingModified());
  ActivateDesk(desk_2);
  EXPECT_FALSE(controller->AreDesksBeingModified());
  EXPECT_EQ(desk_2, controller->active_desk());
  EXPECT_FALSE(desk_1->is_active());
  EXPECT_TRUE(desk_2->is_active());
  EXPECT_FALSE(desk_3->is_active());
  EXPECT_FALSE(desk_4->is_active());

  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());
  EXPECT_FALSE(desk_1->GetDeskContainerForRoot(roots[0])->IsVisible());
  EXPECT_FALSE(desk_1->GetDeskContainerForRoot(roots[1])->IsVisible());
  EXPECT_TRUE(desk_2->GetDeskContainerForRoot(roots[0])->IsVisible());
  EXPECT_TRUE(desk_2->GetDeskContainerForRoot(roots[1])->IsVisible());
  EXPECT_FALSE(desk_3->GetDeskContainerForRoot(roots[0])->IsVisible());
  EXPECT_FALSE(desk_3->GetDeskContainerForRoot(roots[1])->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(roots[0])->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(roots[1])->IsVisible());
}

TEST_F(DesksTest, TransientWindows) {
  auto* controller = DesksController::Get();
  ASSERT_EQ(1u, controller->desks().size());
  const Desk* desk_1 = controller->desks()[0].get();
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(desk_1->is_active());

  // Create two windows, one is a transient child of the other.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateTransientWindow(win0.get(), gfx::Rect(100, 100, 100, 100));

  EXPECT_EQ(2u, desk_1->windows().size());
  EXPECT_TRUE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win1.get()));

  auto* root = Shell::GetPrimaryRootWindow();
  EXPECT_EQ(desks_util::GetActiveDeskContainerForRoot(root),
            desks_util::GetDeskContainerForContext(win0.get()));
  EXPECT_EQ(desks_util::GetActiveDeskContainerForRoot(root),
            desks_util::GetDeskContainerForContext(win1.get()));

  // Create a new desk and activate it.
  NewDesk();
  const Desk* desk_2 = controller->desks()[1].get();
  EXPECT_TRUE(desk_2->windows().empty());
  ActivateDesk(desk_2);
  EXPECT_FALSE(desk_1->is_active());
  EXPECT_TRUE(desk_2->is_active());

  // Create another transient child of the earlier transient child, and confirm
  // it's tracked in desk_1 (even though desk_2 is the currently active one).
  // This is because the transient parent exists in desk_1.
  auto win2 = CreateTransientWindow(win1.get(), gfx::Rect(100, 100, 50, 50));
  EXPECT_FALSE(controller->AreDesksBeingModified());
  EXPECT_EQ(3u, desk_1->windows().size());
  EXPECT_TRUE(desk_2->windows().empty());
  EXPECT_FALSE(DoesActiveDeskContainWindow(win2.get()));
  auto* desk_1_container = desk_1->GetDeskContainerForRoot(root);
  EXPECT_EQ(win0.get(), desk_1_container->children()[0]);
  EXPECT_EQ(win1.get(), desk_1_container->children()[1]);
  EXPECT_EQ(win2.get(), desk_1_container->children()[2]);

  // Remove the inactive desk 1, and expect that its windows, including
  // transient will move to desk 2.
  RemoveDesk(desk_1);
  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_EQ(desk_2, controller->active_desk());
  EXPECT_EQ(3u, desk_2->windows().size());
  auto* desk_2_container = desk_2->GetDeskContainerForRoot(root);
  EXPECT_EQ(win0.get(), desk_2_container->children()[0]);
  EXPECT_EQ(win1.get(), desk_2_container->children()[1]);
  EXPECT_EQ(win2.get(), desk_2_container->children()[2]);
}

TEST_F(DesksTest, WindowStackingAfterWindowMoveToAnotherDesk) {
  auto* controller = DesksController::Get();
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));

  NewDesk();
  Desk* desk_2 = controller->desks()[1].get();
  ActivateDesk(desk_2);

  auto win1 = CreateAppWindow(gfx::Rect(10, 10, 250, 100));
  auto win2 = CreateTransientWindow(win1.get(), gfx::Rect(100, 100, 100, 100));
  wm::ActivateWindow(win2.get());
  auto win3 = CreateAppWindow(gfx::Rect(20, 20, 250, 100));

  // Move back to desk_1, so |win0| becomes the most recent.
  Desk* desk_1 = controller->desks()[0].get();
  ActivateDesk(desk_1);
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());

  // The global MRU order should be {win0, win3, win2, win1}.
  auto* mru_tracker = Shell::Get()->mru_window_tracker();
  EXPECT_EQ(std::vector<aura::Window*>(
                {win0.get(), win3.get(), win2.get(), win1.get()}),
            mru_tracker->BuildMruWindowList(DesksMruType::kAllDesks));

  // Now move back to desk_2, and move its windows to desk_1. Their window
  // stacking should match their order in the MRU.
  ActivateDesk(desk_2);
  // The global MRU order is updated to be {win3, win0, win2, win1}.
  EXPECT_EQ(std::vector<aura::Window*>(
                {win3.get(), win0.get(), win2.get(), win1.get()}),
            mru_tracker->BuildMruWindowList(DesksMruType::kAllDesks));

  // Moving |win2| should be enough to get its transient parent |win1| moved as
  // well.
  desk_2->MoveWindowToDesk(win2.get(), desk_1, win1->GetRootWindow());
  desk_2->MoveWindowToDesk(win3.get(), desk_1, win1->GetRootWindow());
  EXPECT_TRUE(IsStackedBelow(win1.get(), win2.get()));
  EXPECT_TRUE(IsStackedBelow(win2.get(), win0.get()));
  EXPECT_TRUE(IsStackedBelow(win0.get(), win3.get()));
}

TEST_F(DesksTest, TransientModalChildren) {
  auto* controller = DesksController::Get();
  NewDesk();
  NewDesk();
  ASSERT_EQ(3u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  Desk* desk_2 = controller->desks()[1].get();
  Desk* desk_3 = controller->desks()[2].get();
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(desk_1->is_active());

  // Create three windows, one of them is a modal transient child.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateTransientModalChildWindow(win0.get());
  EXPECT_EQ(win1.get(), ::wm::GetModalTransient(win0.get()));
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 200, 100));
  ASSERT_EQ(3u, desk_1->windows().size());
  auto* root = Shell::GetPrimaryRootWindow();
  auto* desk_1_container = desk_1->GetDeskContainerForRoot(root);
  EXPECT_EQ(win0.get(), desk_1_container->children()[0]);
  EXPECT_EQ(win1.get(), desk_1_container->children()[1]);
  EXPECT_EQ(win2.get(), desk_1_container->children()[2]);

  // Remove desk_1, and expect that all its windows (including the transient
  // modal child and its parent) are moved to desk_2, and that their z-order
  // within the container is preserved.
  RemoveDesk(desk_1);
  EXPECT_EQ(desk_2, controller->active_desk());
  ASSERT_EQ(3u, desk_2->windows().size());
  auto* desk_2_container = desk_2->GetDeskContainerForRoot(root);
  EXPECT_EQ(win0.get(), desk_2_container->children()[0]);
  EXPECT_EQ(win1.get(), desk_2_container->children()[1]);
  EXPECT_EQ(win2.get(), desk_2_container->children()[2]);
  EXPECT_EQ(win1.get(), ::wm::GetModalTransient(win0.get()));

  // Move only the modal child window to desk_3, and expect that its parent will
  // move along with it, and their z-order is preserved.
  controller->MoveWindowFromActiveDeskTo(
      win1.get(), desk_3, win1->GetRootWindow(),
      DesksMoveWindowFromActiveDeskSource::kDragAndDrop);
  ASSERT_EQ(1u, desk_2->windows().size());
  ASSERT_EQ(2u, desk_3->windows().size());
  EXPECT_EQ(win2.get(), desk_2_container->children()[0]);
  auto* desk_3_container = desk_3->GetDeskContainerForRoot(root);
  EXPECT_EQ(win0.get(), desk_3_container->children()[0]);
  EXPECT_EQ(win1.get(), desk_3_container->children()[1]);

  // The modality remains the same.
  ActivateDesk(desk_3);
  EXPECT_EQ(win1.get(), ::wm::GetModalTransient(win0.get()));
}

TEST_F(DesksTest, WindowActivation) {
  // Create three windows.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto win2 = CreateAppWindow(gfx::Rect(100, 100, 100, 100));

  EXPECT_TRUE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win1.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win2.get()));

  // Activate win0 and expects that it remains activated until we switch desks.
  wm::ActivateWindow(win0.get());

  // Create a new desk and activate it. Expect it's not tracking any windows
  // yet.
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  const Desk* desk_2 = controller->desks()[1].get();
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_EQ(3u, desk_1->windows().size());
  EXPECT_TRUE(desk_2->windows().empty());
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());

  // Activate the newly-added desk. Expect that the tracked windows per each
  // desk will remain the same.
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, controller->active_desk());
  EXPECT_EQ(3u, desk_1->windows().size());
  EXPECT_TRUE(desk_2->windows().empty());

  // `desk_2` has no windows, so now no window should be active. However,
  // windows on `desk_1` are activateable.
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());
  EXPECT_TRUE(wm::CanActivateWindow(win0.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win1.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win2.get()));

  // Create two new windows, they should now go to desk_2.
  auto win3 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  auto win4 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));
  wm::ActivateWindow(win3.get());
  EXPECT_EQ(2u, desk_2->windows().size());
  EXPECT_TRUE(DoesActiveDeskContainWindow(win3.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win4.get()));
  EXPECT_FALSE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_FALSE(DoesActiveDeskContainWindow(win1.get()));
  EXPECT_FALSE(DoesActiveDeskContainWindow(win2.get()));
  EXPECT_EQ(win3.get(), window_util::GetActiveWindow());

  // Delete `win0` and expect that `desk_1`'s windows will be updated.
  win0.reset();
  EXPECT_EQ(2u, desk_1->windows().size());
  EXPECT_EQ(2u, desk_2->windows().size());
  // No change in the activation.
  EXPECT_EQ(win3.get(), window_util::GetActiveWindow());

  // Switch back to `desk_1`. Now we can activate its windows.
  ActivateDesk(desk_1);
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(wm::CanActivateWindow(win1.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win2.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win3.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win4.get()));

  // After `win0` has been deleted, `win2` is next on the MRU list.
  EXPECT_EQ(win2.get(), window_util::GetActiveWindow());

  // Remove `desk_2` and expect that its windows will be moved to the active
  // desk.
  TestDeskObserver observer;
  desk_1->AddObserver(&observer);
  RemoveDesk(desk_2);
  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_EQ(4u, desk_1->windows().size());
  // Even though two new windows have been added to desk_1, observers should be
  // notified only once.
  EXPECT_EQ(1, observer.notify_counts());
  desk_1->RemoveObserver(&observer);
  EXPECT_TRUE(DoesActiveDeskContainWindow(win3.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win4.get()));

  // `desk_2`'s windows moved to `desk_1`, but that should not change the
  // already active window.
  EXPECT_EQ(win2.get(), window_util::GetActiveWindow());

  // Moved windows can still be activated.
  EXPECT_TRUE(wm::CanActivateWindow(win3.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win4.get()));
}

TEST_F(DesksTest, ActivateDeskFromOverview) {
  auto* controller = DesksController::Get();

  // Create three desks other than the default initial desk.
  NewDesk();
  NewDesk();
  NewDesk();
  ASSERT_EQ(4u, controller->desks().size());

  // Create two windows on desk_1.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win1.get());
  EXPECT_EQ(win1.get(), window_util::GetActiveWindow());

  // Enter overview mode, and expect the desk bar is shown with exactly four
  // desks mini views, and there are exactly two windows in the overview mode
  // grid.
  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(4u, desks_bar_view->mini_views().size());
  EXPECT_EQ(2u, overview_grid->window_list().size());

  // Activate desk_4 (last one on the right) by clicking on its mini view.
  const Desk* desk_4 = controller->desks()[3].get();
  EXPECT_FALSE(desk_4->is_active());
  auto* mini_view = desks_bar_view->mini_views().back();
  EXPECT_EQ(desk_4, mini_view->desk());
  EXPECT_FALSE(mini_view->close_desk_button()->GetVisible());
  const gfx::Point mini_view_center =
      mini_view->GetBoundsInScreen().CenterPoint();
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(mini_view_center);
  DeskSwitchAnimationWaiter waiter;
  event_generator->ClickLeftButton();
  waiter.Wait();

  // Expect that desk_4 is now active, and overview mode exited.
  EXPECT_TRUE(desk_4->is_active());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  // Exiting overview mode should not restore focus to a window on a
  // now-inactive desk. Run a loop since the overview session is destroyed async
  // and until that happens, focus will be on the dummy
  // "OverviewModeFocusedWidget".
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());

  // Create one window in desk_4 and enter overview mode. Expect the grid is
  // showing exactly one window.
  auto win2 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win2.get());
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, overview_grid->window_list().size());

  // When exiting overview mode without changing desks, the focus should be
  // restored to the same window.
  ExitOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  // Run a loop since the overview session is destroyed async and until that
  // happens, focus will be on the dummy "OverviewModeFocusedWidget".
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(win2.get(), window_util::GetActiveWindow());
}

// This test makes sure we have coverage for that desk switch animation when run
// with multiple displays while overview mode is active.
TEST_F(DesksTest, ActivateDeskFromOverviewDualDisplay) {
  UpdateDisplay("600x600,400x500");

  auto* controller = DesksController::Get();

  // Create three desks other than the default initial desk.
  NewDesk();
  NewDesk();
  NewDesk();
  ASSERT_EQ(4u, controller->desks().size());

  // Enter overview mode.
  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());
  // Use secondary display grid.
  const auto* overview_grid = GetOverviewGridForRoot(roots[1]);
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(4u, desks_bar_view->mini_views().size());

  // Activate desk_4 (last one on the right) by clicking on its mini view.
  const Desk* desk_4 = controller->desks()[3].get();
  EXPECT_FALSE(desk_4->is_active());
  const auto* mini_view = desks_bar_view->mini_views().back();
  const gfx::Point mini_view_center =
      mini_view->GetBoundsInScreen().CenterPoint();
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(mini_view_center);
  DeskSwitchAnimationWaiter waiter;
  event_generator->ClickLeftButton();
  waiter.Wait();

  // Expect that desk_4 is now active, and overview mode exited.
  EXPECT_TRUE(desk_4->is_active());
  EXPECT_FALSE(overview_controller->InOverviewSession());
}

TEST_F(DesksTest, RemoveInactiveDeskFromOverview) {
  auto* controller = DesksController::Get();

  // Create three desks other than the default initial desk.
  NewDesk();
  NewDesk();
  NewDesk();
  ASSERT_EQ(4u, controller->desks().size());

  // Create 3 windows on desk_1.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto win2 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win0.get());
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());

  auto* mru_tracker = Shell::Get()->mru_window_tracker();
  EXPECT_EQ(std::vector<aura::Window*>({win0.get(), win2.get(), win1.get()}),
            mru_tracker->BuildMruWindowList(DesksMruType::kActiveDesk));

  // Active desk_4 and enter overview mode, and add a single window.
  Desk* desk_4 = controller->desks()[3].get();
  ActivateDesk(desk_4);
  auto* overview_controller = Shell::Get()->overview_controller();
  auto win3 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, overview_grid->window_list().size());

  // Remove desk_1 using the close button on its mini view. desk_1 is currently
  // inactive. Its windows should be moved to desk_4 and added to the overview
  // grid in the MRU order (win0, win2, and win1) at the end after desk_4's
  // existing window (win3).
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(4u, desks_bar_view->mini_views().size());
  Desk* desk_1 = controller->desks()[0].get();
  auto* mini_view = desks_bar_view->mini_views().front();
  EXPECT_EQ(desk_1, mini_view->desk());

  // Setup observers of both the active and inactive desks to make sure
  // refreshing the mini_view is requested only *once* for the active desk, and
  // never for the to-be-removed inactive desk.
  TestDeskObserver desk_4_observer;
  desk_4->AddObserver(&desk_4_observer);
  TestDeskObserver desk_1_observer;
  desk_1->AddObserver(&desk_1_observer);

  CloseDeskFromMiniView(mini_view, GetEventGenerator());

  EXPECT_EQ(0, desk_1_observer.notify_counts());
  EXPECT_EQ(1, desk_4_observer.notify_counts());

  ASSERT_EQ(3u, desks_bar_view->mini_views().size());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ASSERT_EQ(4u, overview_grid->window_list().size());
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win0.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win1.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win2.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win3.get()));
  // Expected order of items: win3, win0, win2, win1.
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win3.get()),
            overview_grid->window_list()[0].get());
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win0.get()),
            overview_grid->window_list()[1].get());
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win2.get()),
            overview_grid->window_list()[2].get());
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win1.get()),
            overview_grid->window_list()[3].get());

  // Make sure overview mode remains active.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Removing a desk with no windows should not result in any new mini_views
  // updates.
  mini_view = desks_bar_view->mini_views().front();
  EXPECT_TRUE(mini_view->desk()->windows().empty());
  CloseDeskFromMiniView(mini_view, GetEventGenerator());
  EXPECT_EQ(1, desk_4_observer.notify_counts());

  // Exiting overview mode should not cause any mini_views refreshes, since the
  // destroyed overview-specific windows do not show up in the mini_view.
  ExitOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(1, desk_4_observer.notify_counts());
  desk_4->RemoveObserver(&desk_4_observer);

  // Verify that the stacking order is correct (top-most comes last, and
  // top-most is the same as MRU).
  EXPECT_EQ(std::vector<aura::Window*>(
                {win3.get(), win0.get(), win2.get(), win1.get()}),
            mru_tracker->BuildMruWindowList(DesksMruType::kActiveDesk));
  EXPECT_EQ(std::vector<aura::Window*>(
                {win1.get(), win2.get(), win0.get(), win3.get()}),
            desk_4->GetDeskContainerForRoot(Shell::GetPrimaryRootWindow())
                ->children());
}

TEST_F(DesksTest, RemoveActiveDeskFromOverview) {
  auto* controller = DesksController::Get();

  // Create one desk other than the default initial desk.
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());

  // Create two windows on desk_1.
  Desk* desk_1 = controller->desks()[0].get();
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win0.get());
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());

  // Activate desk_2 and create one more window.
  Desk* desk_2 = controller->desks()[1].get();
  ActivateDesk(desk_2);
  auto win2 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto win3 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win2.get());
  EXPECT_EQ(win2.get(), window_util::GetActiveWindow());

  // The MRU across all desks is now {win2, win3, win0, win1}.
  auto* mru_tracker = Shell::Get()->mru_window_tracker();
  EXPECT_EQ(std::vector<aura::Window*>(
                {win2.get(), win3.get(), win0.get(), win1.get()}),
            mru_tracker->BuildMruWindowList(DesksMruType::kAllDesks));

  // Enter overview mode, and remove desk_2 from its mini-view close button.
  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(2u, overview_grid->window_list().size());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
  auto* mini_view = desks_bar_view->mini_views().back();
  EXPECT_EQ(desk_2, mini_view->desk());

  // Setup observers of both the active and inactive desks to make sure
  // refreshing the mini_view is requested only *once* for the soon-to-be active
  // desk_1, and never for the to-be-removed currently active desk_2.
  TestDeskObserver desk_1_observer;
  desk_1->AddObserver(&desk_1_observer);
  TestDeskObserver desk_2_observer;
  desk_2->AddObserver(&desk_2_observer);

  CloseDeskFromMiniView(mini_view, GetEventGenerator());

  EXPECT_EQ(1, desk_1_observer.notify_counts());
  EXPECT_EQ(0, desk_2_observer.notify_counts());

  // Make sure that the desks_bar_view window is still visible, i.e. it moved to
  // the newly-activated desk's container.
  EXPECT_TRUE(desks_bar_view->GetWidget()->IsVisible());
  EXPECT_TRUE(DoesActiveDeskContainWindow(
      desks_bar_view->GetWidget()->GetNativeWindow()));

  // desk_1 will become active, and windows from desk_2 will move to desk_1 such
  // that they become last in MRU order, and therefore appended at the end of
  // the overview grid. Desks bar should go back to zero state since there is a
  // single desk after removing.
  ASSERT_EQ(1u, controller->desks().size());
  ASSERT_EQ(0u, desks_bar_view->mini_views().size());
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(4u, overview_grid->window_list().size());
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win0.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win1.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win2.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win3.get()));

  // The new MRU order is {win0, win1, win2, win3}.
  EXPECT_EQ(std::vector<aura::Window*>(
                {win0.get(), win1.get(), win2.get(), win3.get()}),
            mru_tracker->BuildMruWindowList(DesksMruType::kActiveDesk));
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win0.get()),
            overview_grid->window_list()[0].get());
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win1.get()),
            overview_grid->window_list()[1].get());
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win2.get()),
            overview_grid->window_list()[2].get());
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win3.get()),
            overview_grid->window_list()[3].get());

  // Make sure overview mode remains active.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Exiting overview mode should not cause any mini_views refreshes, since the
  // destroyed overview-specific windows do not show up in the mini_view.
  ExitOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(1, desk_1_observer.notify_counts());
  desk_1->RemoveObserver(&desk_1_observer);
}

TEST_F(DesksTest, ActivateActiveDeskFromOverview) {
  auto* controller = DesksController::Get();

  // Create one more desk other than the default initial desk, so the desks bar
  // shows up in overview mode.
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());

  // Enter overview mode, and click on `desk_1`'s mini_view, and expect that
  // overview mode exits since this is the already active desk.
  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  const Desk* desk_1 = controller->desks()[0].get();
  const auto* mini_view = desks_bar_view->mini_views().front();
  ClickOnView(mini_view, GetEventGenerator());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_EQ(desk_1, controller->active_desk());
}

TEST_F(DesksTest, MinimizedWindow) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_1 = controller->desks()[0].get();
  const Desk* desk_2 = controller->desks()[1].get();

  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(win0.get());
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());

  auto* window_state = WindowState::Get(win0.get());
  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());

  // Minimized windows on the active desk show up in the MRU list...
  EXPECT_EQ(1u, Shell::Get()
                    ->mru_window_tracker()
                    ->BuildMruWindowList(kActiveDesk)
                    .size());

  ActivateDesk(desk_2);

  // ... But they don't once their desk is inactive.
  EXPECT_TRUE(Shell::Get()
                  ->mru_window_tracker()
                  ->BuildMruWindowList(kActiveDesk)
                  .empty());

  // Switching back to their desk should neither activate them nor unminimize
  // them.
  ActivateDesk(desk_1);
  EXPECT_TRUE(window_state->IsMinimized());
  EXPECT_NE(win0.get(), window_util::GetActiveWindow());
}

// Tests that the app list stays open when switching desks. Regression test for
// http://crbug.com/1138982.
TEST_F(DesksTest, AppListStaysOpenInClamshell) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());

  // Create one app window on each desk.
  auto win0 = CreateAppWindow(gfx::Rect(400, 400));
  ActivateDesk(controller->desks()[1].get());
  auto win1 = CreateAppWindow(gfx::Rect(400, 400));

  // Open the app list.
  auto* app_list_controller = Shell::Get()->app_list_controller();
  app_list_controller->ShowAppList();
  ASSERT_TRUE(app_list_controller->IsVisible());

  // Switch back to desk 1. Test that the app list is still open.
  ActivateDesk(controller->desks()[0].get());
  EXPECT_TRUE(app_list_controller->IsVisible());
}

// Tests that the app list correctly loses focus in tablet mode when switching
// desks. Regression test for https://crbug.com/1206030.
TEST_F(DesksTest, AppListActivationInTablet) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());

  // Create one app window on desk 1.
  auto window = CreateAppWindow(gfx::Rect(400, 400));
  ASSERT_EQ(window.get(), window_util::GetActiveWindow());

  // Enter tablet mode and switch to desk 2. Verify the app list has activation
  // as there are no app windows.
  TabletModeControllerTestApi().EnterTabletMode();
  ActivateDesk(controller->desks()[1].get());
  auto* app_list_controller = Shell::Get()->app_list_controller();
  ASSERT_EQ(app_list_controller->GetWindow(), window_util::GetActiveWindow());

  // Switch back to desk 1. `window` should have activation now.
  ActivateDesk(controller->desks()[0].get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());
}

TEST_P(DesksTest, DragWindowToDesk) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_1 = controller->desks()[0].get();
  const Desk* desk_2 = controller->desks()[1].get();

  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 200, 150));
  wm::ActivateWindow(win1.get());
  EXPECT_EQ(win1.get(), window_util::GetActiveWindow());

  ui::Shadow* shadow = ::wm::ShadowController::GetShadowForWindow(win1.get());
  ASSERT_TRUE(shadow);
  ASSERT_TRUE(shadow->layer());
  EXPECT_TRUE(shadow->layer()->GetTargetVisibility());

  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(2u, overview_grid->size());

  // While in overview mode, the window's shadow is hidden.
  EXPECT_FALSE(shadow->layer()->GetTargetVisibility());

  auto* overview_session = overview_controller->overview_session();
  auto* overview_item = overview_session->GetOverviewItemForWindow(win1.get());
  ASSERT_TRUE(overview_item);
  const gfx::RectF target_bounds_before_drag = overview_item->target_bounds();

  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
  auto* desk_1_mini_view = desks_bar_view->mini_views()[0];
  EXPECT_EQ(desk_1, desk_1_mini_view->desk());
  // Drag it and drop it on its same desk's mini_view. Nothing happens, it
  // should be returned back to its original target bounds.
  auto* event_generator = GetEventGenerator();
  DragItemToPoint(overview_item,
                  desk_1_mini_view->GetBoundsInScreen().CenterPoint(),
                  event_generator,
                  /*by_touch_gestures=*/GetParam());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(2u, overview_grid->size());
  EXPECT_EQ(target_bounds_before_drag, overview_item->target_bounds());
  EXPECT_TRUE(DoesActiveDeskContainWindow(win1.get()));

  // Now drag it to desk_2's mini_view. The overview grid should now have only
  // `win2`, and `win1` should move to desk_2.
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1];
  EXPECT_EQ(desk_2, desk_2_mini_view->desk());
  DragItemToPoint(overview_item,
                  desk_2_mini_view->GetBoundsInScreen().CenterPoint(),
                  event_generator,
                  /*by_touch_gestures=*/GetParam());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(1u, overview_grid->size());
  EXPECT_FALSE(DoesActiveDeskContainWindow(win1.get()));
  EXPECT_TRUE(base::Contains(desk_2->windows(), win1.get()));
  EXPECT_FALSE(overview_grid->drop_target_widget());

  // After dragging an item outside of overview to another desk, the focus
  // should not be given to another window in overview, and should remain on the
  // OverviewModeFocusedWidget.
  EXPECT_EQ(overview_session->GetOverviewFocusWindow(),
            window_util::GetActiveWindow());

  // It is possible to select the remaining window which will activate it and
  // exit overview.
  overview_item = overview_session->GetOverviewItemForWindow(win2.get());
  ASSERT_TRUE(overview_item);
  const auto window_center =
      gfx::ToFlooredPoint(overview_item->target_bounds().CenterPoint());
  if (GetParam() /* uses gestures */) {
    event_generator->GestureTapAt(window_center);
  } else {
    event_generator->MoveMouseTo(window_center);
    event_generator->ClickLeftButton();
  }
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(win2.get(), window_util::GetActiveWindow());

  // After the window is dropped onto another desk, its shadow should be
  // restored properly.
  EXPECT_TRUE(shadow->layer()->GetTargetVisibility());
}

TEST_P(DesksTest, DragMinimizedWindowToDesk) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_2 = controller->desks()[1].get();

  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Minimize the window before entering Overview Mode.
  auto* window_state = WindowState::Get(window.get());
  window_state->Minimize();
  ASSERT_TRUE(window_state->IsMinimized());

  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());

  // Drag the window to desk_2's mini_view and activate desk_2. Expect that the
  // window will be in an unminimized state and all its visibility and layer
  // opacity attributes are correct.
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1];
  EXPECT_EQ(desk_2, desk_2_mini_view->desk());
  DragItemToPoint(overview_item,
                  desk_2_mini_view->GetBoundsInScreen().CenterPoint(),
                  GetEventGenerator(),
                  /*by_touch_gestures=*/GetParam());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_grid->empty());
  EXPECT_TRUE(base::Contains(desk_2->windows(), window.get()));
  EXPECT_FALSE(overview_grid->drop_target_widget());
  DeskSwitchAnimationWaiter waiter;
  ClickOnView(desk_2_mini_view, GetEventGenerator());
  waiter.Wait();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(desk_2->is_active());

  EXPECT_FALSE(window_state->IsMinimized());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->layer()->GetTargetVisibility());
  EXPECT_EQ(1.f, window->layer()->GetTargetOpacity());
}

TEST_P(DesksTest, DragAllOverviewWindowsToOtherDesksNotEndOverview) {
  NewDesk();
  ASSERT_EQ(2u, DesksController::Get()->desks().size());
  auto win = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto* overview_controller = Shell::Get()->overview_controller();
  ASSERT_TRUE(EnterOverview());
  auto* overview_session = overview_controller->overview_session();
  DragItemToPoint(overview_session->GetOverviewItemForWindow(win.get()),
                  GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())
                      ->desks_bar_view()
                      ->mini_views()[1]
                      ->GetBoundsInScreen()
                      .CenterPoint(),
                  GetEventGenerator(),
                  /*by_touch_gestures=*/GetParam());
  EXPECT_FALSE(DoesActiveDeskContainWindow(win.get()));
  ASSERT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_session->IsEmpty());
}

TEST_P(DesksTest, DragWindowToNonMiniViewPoints) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());

  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, overview_grid->size());

  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);
  const gfx::RectF target_bounds_before_drag = overview_item->target_bounds();

  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);

  // Drag it and drop it on the new desk button. Nothing happens, it should be
  // returned back to its original target bounds.
  DragItemToPoint(overview_item,
                  desks_bar_view->expanded_state_new_desk_button()
                      ->new_desk_button()
                      ->GetBoundsInScreen()
                      .CenterPoint(),
                  GetEventGenerator(),
                  /*by_touch_gestures=*/GetParam());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(1u, overview_grid->size());
  EXPECT_EQ(target_bounds_before_drag, overview_item->target_bounds());
  EXPECT_TRUE(DoesActiveDeskContainWindow(window.get()));

  // Drag it and drop it on the center of the bottom of the display. Also,
  // nothing should happen.
  DragItemToPoint(overview_item,
                  window->GetRootWindow()->GetBoundsInScreen().bottom_center(),
                  GetEventGenerator(),
                  /*by_touch_gestures=*/GetParam());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(1u, overview_grid->size());
  EXPECT_EQ(target_bounds_before_drag, overview_item->target_bounds());
  EXPECT_TRUE(DoesActiveDeskContainWindow(window.get()));
}

TEST_F(DesksTest, MruWindowTracker) {
  // Create two desks with two windows in each.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_2 = controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, controller->active_desk());
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  auto win3 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));

  // Build active desk's MRU window list.
  auto* mru_window_tracker = Shell::Get()->mru_window_tracker();
  auto window_list = mru_window_tracker->BuildWindowForCycleList(kActiveDesk);
  ASSERT_EQ(2u, window_list.size());
  EXPECT_EQ(win3.get(), window_list[0]);
  EXPECT_EQ(win2.get(), window_list[1]);

  // Build the global MRU window list.
  window_list = mru_window_tracker->BuildWindowForCycleList(kAllDesks);
  ASSERT_EQ(4u, window_list.size());
  EXPECT_EQ(win3.get(), window_list[0]);
  EXPECT_EQ(win2.get(), window_list[1]);
  EXPECT_EQ(win1.get(), window_list[2]);
  EXPECT_EQ(win0.get(), window_list[3]);

  // Switch back to desk_1 and test both MRU list types.
  Desk* desk_1 = controller->desks()[0].get();
  ActivateDesk(desk_1);
  window_list = mru_window_tracker->BuildWindowForCycleList(kActiveDesk);
  ASSERT_EQ(2u, window_list.size());
  EXPECT_EQ(win1.get(), window_list[0]);
  EXPECT_EQ(win0.get(), window_list[1]);
  // TODO(afakhry): Check with UX if we should favor active desk's windows in
  // the global MRU list (i.e. put them first in the list).
  window_list = mru_window_tracker->BuildWindowForCycleList(kAllDesks);
  ASSERT_EQ(4u, window_list.size());
  EXPECT_EQ(win1.get(), window_list[0]);
  EXPECT_EQ(win3.get(), window_list[1]);
  EXPECT_EQ(win2.get(), window_list[2]);
  EXPECT_EQ(win0.get(), window_list[3]);
}

TEST_F(DesksTest, NextActivatable) {
  // Create two desks with two windows in each.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_2 = controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, controller->active_desk());
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  auto win3 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));
  EXPECT_EQ(win3.get(), window_util::GetActiveWindow());

  // When deactivating a window, the next activatable window should be on the
  // same desk.
  wm::DeactivateWindow(win3.get());
  EXPECT_EQ(win2.get(), window_util::GetActiveWindow());
  wm::DeactivateWindow(win2.get());
  EXPECT_EQ(win3.get(), window_util::GetActiveWindow());

  // Similarly for desk_1.
  Desk* desk_1 = controller->desks()[0].get();
  ActivateDesk(desk_1);
  EXPECT_EQ(win1.get(), window_util::GetActiveWindow());
  win1.reset();
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());
  win0.reset();
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());
}

TEST_F(DesksTest, NoMiniViewsUpdateOnOverviewEnter) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  auto* desk_1 = controller->desks()[0].get();
  auto* desk_2 = controller->desks()[1].get();

  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win1.get());
  EXPECT_EQ(win1.get(), window_util::GetActiveWindow());

  TestDeskObserver desk_1_observer;
  TestDeskObserver desk_2_observer;
  desk_1->AddObserver(&desk_1_observer);
  desk_2->AddObserver(&desk_2_observer);
  EXPECT_EQ(0, desk_1_observer.notify_counts());
  EXPECT_EQ(0, desk_2_observer.notify_counts());

  // The widgets created by overview mode, whose windows are added to the active
  // desk's container, should never result in mini_views updates since they're
  // not mirrored there at all.
  EnterOverview();
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  EXPECT_EQ(0, desk_1_observer.notify_counts());
  EXPECT_EQ(0, desk_2_observer.notify_counts());

  desk_1->RemoveObserver(&desk_1_observer);
  desk_2->RemoveObserver(&desk_2_observer);
}

// Tests that the consecutive daily visits metric is properly recorded.
TEST_F(DesksTest, ConsecutiveDailyVisitsMetric) {
  constexpr char kConsecutiveDailyVisitsHistogram[] =
      "Ash.Desks.ConsecutiveDailyVisits";
  auto* desks_controller = DesksController::Get();
  base::HistogramTester histogram_tester;
  base::SimpleTestClock test_clock;

  // Set the time to 00:00:00 local time the next day, override the current
  // desk's clock and reset its visited metrics.
  test_clock.SetNow(base::Time::Now().LocalMidnight());
  test_clock.Advance(base::TimeDelta::FromHours(1));
  auto* active_desk = desks_controller->active_desk();
  desks_restore_util::OverrideClockForTesting(&test_clock);
  DesksTestApi::ResetDeskVisitedMetrics(const_cast<Desk*>(active_desk));
  EXPECT_EQ(
      0u,
      histogram_tester.GetAllSamples(kConsecutiveDailyVisitsHistogram).size());

  // Create a new desk and don't visit it.
  active_desk = desks_controller->active_desk();
  NewDesk();
  ASSERT_EQ(active_desk, desks_controller->active_desk());

  // Fast forward by two days then remove the active desk. This should record an
  // entry for two days since we stayed on the active desk the whole time.
  // Additionally, there shouldn't be a record for the desk we switch to since
  // we haven't visited it yet.
  test_clock.Advance(base::TimeDelta::FromDays(2));
  RemoveDesk(active_desk);
  histogram_tester.ExpectBucketCount(kConsecutiveDailyVisitsHistogram, 3, 1);
  EXPECT_EQ(
      1u,
      histogram_tester.GetAllSamples(kConsecutiveDailyVisitsHistogram).size());

  // Create a new desk and remove the active desk. This should record an entry
  // for one day since we visited the active desk before removing it.
  active_desk = desks_controller->active_desk();
  NewDesk();
  RemoveDesk(active_desk);
  histogram_tester.ExpectBucketCount(kConsecutiveDailyVisitsHistogram, 1, 1);
  EXPECT_EQ(
      2u,
      histogram_tester.GetAllSamples(kConsecutiveDailyVisitsHistogram).size());

  // Create a new desk and switch to it. Then fast forward two days and revisit
  // the previous desk. Since it's been more than one day since the last visit,
  // a one day entry should be recorded for the previous desk.
  NewDesk();
  ActivateDesk(desks_controller->GetNextDesk());
  test_clock.Advance(base::TimeDelta::FromDays(2));
  ActivateDesk(desks_controller->GetPreviousDesk());
  histogram_tester.ExpectBucketCount(kConsecutiveDailyVisitsHistogram, 1, 2);
  EXPECT_EQ(
      2u,
      histogram_tester.GetAllSamples(kConsecutiveDailyVisitsHistogram).size());

  // Go back in time to simulate a user switching timezones and then switch to
  // the next desk. Since the current time is before the |last_day_visited_|
  // field of the next desk, its visited fields should be reset.
  test_clock.Advance(base::TimeDelta::FromDays(-2));
  ActivateDesk(desks_controller->GetNextDesk());
  active_desk = desks_controller->active_desk();
  const int current_date = desks_restore_util::GetDaysFromLocalEpoch();
  EXPECT_EQ(current_date, active_desk->first_day_visited());
  EXPECT_EQ(current_date, active_desk->last_day_visited());
  desks_restore_util::OverrideClockForTesting(nullptr);
}

// Tests that the new desk button's state and color are as expected.
TEST_F(DesksTest, NewDeskButtonStateAndColor) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());

  EnterOverview();
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  const auto* new_desk_button =
      desks_bar_view->expanded_state_new_desk_button()->new_desk_button();

  // Tests that with one or two desks, the new desk button has an enabled state
  // and color.
  const SkColor background_color =
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  const SkColor disabled_background_color =
      AshColorProvider::GetDisabledColor(background_color);
  EXPECT_TRUE(new_desk_button->GetEnabled());
  EXPECT_EQ(background_color, DesksTestApi::GetNewDeskButtonBackgroundColor());

  auto* event_generator = GetEventGenerator();
  ClickOnView(new_desk_button, event_generator);
  EXPECT_TRUE(new_desk_button->GetEnabled());
  EXPECT_EQ(background_color, DesksTestApi::GetNewDeskButtonBackgroundColor());

  // Tests that adding desks until we reach the desks limit should change the
  // state and color of the new desk button.
  size_t prev_size = controller->desks().size();
  while (controller->CanCreateDesks()) {
    ClickOnView(new_desk_button, event_generator);
    EXPECT_EQ(prev_size + 1, controller->desks().size());
    prev_size = controller->desks().size();
  }
  EXPECT_FALSE(new_desk_button->GetEnabled());
  EXPECT_EQ(disabled_background_color,
            DesksTestApi::GetNewDeskButtonBackgroundColor());
}

class DesksWithMultiDisplayOverview : public AshTestBase {
 public:
  DesksWithMultiDisplayOverview() = default;
  ~DesksWithMultiDisplayOverview() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Start the test with two displays and two desks.
    UpdateDisplay("600x600,400x500");
    NewDesk();
  }
};

TEST_F(DesksWithMultiDisplayOverview, DropOnSameDeskInOtherDisplay) {
  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());

  auto win = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_session = overview_controller->overview_session();
  auto* overview_item = overview_session->GetOverviewItemForWindow(win.get());
  ASSERT_TRUE(overview_item);

  // The window should exist on the grid of the first display.
  auto* grid1 = GetOverviewGridForRoot(roots[0]);
  auto* grid2 = GetOverviewGridForRoot(roots[1]);
  EXPECT_EQ(1u, grid1->size());
  EXPECT_EQ(grid1, overview_item->overview_grid());
  EXPECT_EQ(0u, grid2->size());

  // Drag the item and drop it on the mini view of the same desk (i.e. desk 1)
  // on the second display. The window should not change desks, but it should
  // change displays (i.e. it should be removed from |grid1| and added to
  // |grid2|).
  const auto* desks_bar_view = grid2->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
  auto* desk_1_mini_view = desks_bar_view->mini_views()[0];
  auto* event_generator = GetEventGenerator();
  DragItemToPoint(overview_item,
                  desk_1_mini_view->GetBoundsInScreen().CenterPoint(),
                  event_generator,
                  /*by_touch_gestures=*/false);
  // A new item should have been created for the window on the second grid.
  EXPECT_TRUE(overview_controller->InOverviewSession());
  overview_item = overview_session->GetOverviewItemForWindow(win.get());
  ASSERT_TRUE(overview_item);
  EXPECT_EQ(0u, grid1->size());
  EXPECT_EQ(grid2, overview_item->overview_grid());
  EXPECT_EQ(1u, grid2->size());
  EXPECT_TRUE(DoesActiveDeskContainWindow(win.get()));
  EXPECT_EQ(roots[1], win->GetRootWindow());
}

TEST_F(DesksWithMultiDisplayOverview, DropOnOtherDeskInOtherDisplay) {
  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());

  auto win = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_session = overview_controller->overview_session();
  auto* overview_item = overview_session->GetOverviewItemForWindow(win.get());
  ASSERT_TRUE(overview_item);

  // The window should exist on the grid of the first display.
  auto* grid1 = GetOverviewGridForRoot(roots[0]);
  auto* grid2 = GetOverviewGridForRoot(roots[1]);
  EXPECT_EQ(1u, grid1->size());
  EXPECT_EQ(grid1, overview_item->overview_grid());
  EXPECT_EQ(0u, grid2->size());

  // Drag the item and drop it on the mini view of the second desk on the second
  // display. The window should change desks as well as displays.
  const auto* desks_bar_view = grid2->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1];
  auto* event_generator = GetEventGenerator();
  DragItemToPoint(overview_item,
                  desk_2_mini_view->GetBoundsInScreen().CenterPoint(),
                  event_generator,
                  /*by_touch_gestures=*/false);
  // The item should no longer be in any grid, since it moved to an inactive
  // desk.
  EXPECT_TRUE(overview_controller->InOverviewSession());
  overview_item = overview_session->GetOverviewItemForWindow(win.get());
  ASSERT_FALSE(overview_item);
  EXPECT_EQ(0u, grid1->size());
  EXPECT_EQ(0u, grid2->size());
  EXPECT_FALSE(DoesActiveDeskContainWindow(win.get()));
  EXPECT_EQ(roots[1], win->GetRootWindow());
  EXPECT_FALSE(win->IsVisible());
  auto* controller = DesksController::Get();
  const Desk* desk_2 = controller->desks()[1].get();
  EXPECT_TRUE(base::Contains(desk_2->windows(), win.get()));
}

namespace {

PrefService* GetPrimaryUserPrefService() {
  return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
}

// Verifies that the desks restore prefs in the given |user_prefs| matches the
// given list of |desks_names|.
void VerifyDesksRestoreData(PrefService* user_prefs,
                            const std::vector<std::string>& desks_names) {
  const base::ListValue* desks_restore_names =
      user_prefs->GetList(prefs::kDesksNamesList);
  ASSERT_EQ(desks_names.size(), desks_restore_names->GetSize());

  size_t index = 0;
  for (const auto& value : desks_restore_names->GetList())
    EXPECT_EQ(desks_names[index++], value.GetString());
}

}  // namespace

class DesksEditableNamesTest : public DesksTest {
 public:
  DesksEditableNamesTest() = default;
  DesksEditableNamesTest(const DesksEditableNamesTest&) = delete;
  DesksEditableNamesTest& operator=(const DesksEditableNamesTest&) = delete;
  ~DesksEditableNamesTest() override = default;

  DesksController* controller() { return controller_; }
  OverviewGrid* overview_grid() { return overview_grid_; }
  const DesksBarView* desks_bar_view() { return desks_bar_view_; }

  // DesksTest:
  void SetUp() override {
    DesksTest::SetUp();

    // Begin all tests with two desks and start overview.
    NewDesk();
    controller_ = DesksController::Get();

    EnterOverview();
    overview_grid_ = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
    desks_bar_view_ = overview_grid_->desks_bar_view();
    ASSERT_TRUE(desks_bar_view_);
  }

  void ClickOnDeskNameViewAtIndex(size_t index) {
    ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
    ASSERT_LT(index, desks_bar_view_->mini_views().size());

    auto* desk_name_view =
        desks_bar_view_->mini_views()[index]->desk_name_view();
    auto* generator = GetEventGenerator();
    generator->MoveMouseTo(desk_name_view->GetBoundsInScreen().CenterPoint());
    generator->ClickLeftButton();
  }

 private:
  DesksController* controller_ = nullptr;
  OverviewGrid* overview_grid_ = nullptr;
  const DesksBarView* desks_bar_view_ = nullptr;
};

TEST_F(DesksEditableNamesTest, DefaultNameChangeAborted) {
  ASSERT_EQ(2u, controller()->desks().size());
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Click on the desk name view of the second mini view.
  auto* desk_name_view_2 = desks_bar_view()->mini_views()[1]->desk_name_view();
  EXPECT_FALSE(overview_grid()->IsDeskNameBeingModified());
  ClickOnDeskNameViewAtIndex(1);
  EXPECT_TRUE(overview_grid()->IsDeskNameBeingModified());
  EXPECT_TRUE(desk_name_view_2->HasFocus());

  // Pressing enter now without making any changes should not change the
  // `is_name_set_by_user_` bit.
  SendKey(ui::VKEY_RETURN);
  EXPECT_FALSE(overview_grid()->IsDeskNameBeingModified());
  EXPECT_FALSE(desk_name_view_2->HasFocus());
  auto* desk_2 = controller()->desks()[1].get();
  EXPECT_FALSE(desk_2->is_name_set_by_user());

  // Desks restore data should reflect two default-named desks.
  VerifyDesksRestoreData(GetPrimaryUserPrefService(),
                         {std::string(), std::string()});
}

TEST_F(DesksEditableNamesTest, NamesSetByUsersAreNotOverwritten) {
  ASSERT_EQ(2u, controller()->desks().size());
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Change the name of the first desk. Adding/removing desks or
  // exiting/reentering overview should not cause changes to the desk's name.
  // All other names should be the default.
  ClickOnDeskNameViewAtIndex(0);
  // Select all and delete.
  SendKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  SendKey(ui::VKEY_BACK);
  // Type " code  " (with one space at the beginning and two spaces at the end)
  // and hit enter.
  SendKey(ui::VKEY_SPACE);
  SendKey(ui::VKEY_C);
  SendKey(ui::VKEY_O);
  SendKey(ui::VKEY_D);
  SendKey(ui::VKEY_E);
  SendKey(ui::VKEY_SPACE);
  SendKey(ui::VKEY_SPACE);
  SendKey(ui::VKEY_RETURN);

  // Extra whitespace should be trimmed.
  auto* desk_1 = controller()->desks()[0].get();
  auto* desk_2 = controller()->desks()[1].get();
  EXPECT_EQ(u"code", desk_1->name());
  EXPECT_EQ(u"Desk 2", desk_2->name());
  EXPECT_TRUE(desk_1->is_name_set_by_user());
  EXPECT_FALSE(desk_2->is_name_set_by_user());

  // Renaming desks via the mini views trigger an update to the desks restore
  // prefs.
  VerifyDesksRestoreData(GetPrimaryUserPrefService(),
                         {std::string("code"), std::string()});

  // Add a third desk and remove the second. Both operations should not affect
  // the user-modified desk names.
  NewDesk();
  auto* desk_3 = controller()->desks()[2].get();
  EXPECT_EQ(u"Desk 3", desk_3->name());
  EXPECT_TRUE(desk_1->is_name_set_by_user());
  EXPECT_FALSE(desk_2->is_name_set_by_user());
  EXPECT_FALSE(desk_3->is_name_set_by_user());

  // Adding a desk triggers an update to the restore prefs.
  VerifyDesksRestoreData(GetPrimaryUserPrefService(),
                         {std::string("code"), std::string(), std::string()});

  RemoveDesk(desk_2);
  EXPECT_TRUE(desk_1->is_name_set_by_user());
  EXPECT_FALSE(desk_3->is_name_set_by_user());
  // Desk 3 will now be renamed to "Desk 2".
  EXPECT_EQ(u"Desk 2", desk_3->name());
  VerifyDesksRestoreData(GetPrimaryUserPrefService(),
                         {std::string("code"), std::string()});

  ExitOverview();
  EnterOverview();
  EXPECT_TRUE(desk_1->is_name_set_by_user());
  EXPECT_FALSE(desk_3->is_name_set_by_user());
  EXPECT_EQ(u"code", desk_1->name());
  EXPECT_EQ(u"Desk 2", desk_3->name());
}

TEST_F(DesksEditableNamesTest, DontAllowEmptyNames) {
  ASSERT_EQ(2u, controller()->desks().size());
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Change the name of a desk to an empty string.
  ClickOnDeskNameViewAtIndex(0);
  // Select all and delete.
  SendKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  SendKey(ui::VKEY_BACK);
  // At this point the desk's name is empty, but editing hasn't committed yet,
  // so it's ok.
  auto* desk_1 = controller()->desks()[0].get();
  EXPECT_TRUE(desk_1->name().empty());
  // Committing also works with the ESC key.
  SendKey(ui::VKEY_ESCAPE);
  // The name should now revert back to the default value.
  EXPECT_FALSE(desk_1->name().empty());
  EXPECT_FALSE(desk_1->is_name_set_by_user());
  EXPECT_EQ(u"Desk 1", desk_1->name());
  VerifyDesksRestoreData(GetPrimaryUserPrefService(),
                         {std::string(), std::string()});
}

TEST_F(DesksEditableNamesTest, SelectAllOnFocus) {
  ASSERT_EQ(2u, controller()->desks().size());
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  ClickOnDeskNameViewAtIndex(0);

  auto* desk_name_view = desks_bar_view()->mini_views()[0]->desk_name_view();
  EXPECT_TRUE(desk_name_view->HasFocus());
  EXPECT_TRUE(desk_name_view->HasSelection());
  auto* desk_1 = controller()->desks()[0].get();
  EXPECT_EQ(desk_1->name(), desk_name_view->GetSelectedText());
}

TEST_F(DesksEditableNamesTest, EventsThatCommitChanges) {
  ASSERT_EQ(2u, controller()->desks().size());
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  ClickOnDeskNameViewAtIndex(0);
  auto* desk_name_view = desks_bar_view()->mini_views()[0]->desk_name_view();
  EXPECT_TRUE(desk_name_view->HasFocus());

  // Creating a new desk commits the changes.
  auto* new_desk_button =
      desks_bar_view()->expanded_state_new_desk_button()->new_desk_button();
  auto* event_generator = GetEventGenerator();
  ClickOnView(new_desk_button, event_generator);
  ASSERT_EQ(3u, controller()->desks().size());
  EXPECT_FALSE(desk_name_view->HasFocus());

  // Deleting a desk commits the changes.
  ClickOnDeskNameViewAtIndex(0);
  EXPECT_TRUE(desk_name_view->HasFocus());
  CloseDeskFromMiniView(desks_bar_view()->mini_views()[2], event_generator);
  ASSERT_EQ(2u, controller()->desks().size());
  EXPECT_FALSE(desk_name_view->HasFocus());

  // Clicking in the empty area of the desks bar also commits the changes.
  ClickOnDeskNameViewAtIndex(0);
  EXPECT_TRUE(desk_name_view->HasFocus());
  event_generator->MoveMouseTo(gfx::Point(2, 2));
  event_generator->ClickLeftButton();
  EXPECT_FALSE(desk_name_view->HasFocus());
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
}

TEST_F(DesksEditableNamesTest, MaxLength) {
  ASSERT_EQ(2u, controller()->desks().size());
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  ClickOnDeskNameViewAtIndex(0);
  // Select all and delete.
  SendKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  SendKey(ui::VKEY_BACK);

  // Simulate user is typing text beyond the max length.
  std::u16string expected_desk_name(DeskNameView::kMaxLength, L'a');
  for (size_t i = 0; i < DeskNameView::kMaxLength + 10; ++i)
    SendKey(ui::VKEY_A);
  SendKey(ui::VKEY_RETURN);

  // Desk name has been trimmed.
  auto* desk_1 = controller()->desks()[0].get();
  EXPECT_EQ(DeskNameView::kMaxLength, desk_1->name().size());
  EXPECT_EQ(expected_desk_name, desk_1->name());
  EXPECT_TRUE(desk_1->is_name_set_by_user());

  // Test that pasting a large amount of text is trimmed at the max length.
  std::u16string clipboard_text(DeskNameView::kMaxLength + 10, L'b');
  expected_desk_name = std::u16string(DeskNameView::kMaxLength, L'b');
  EXPECT_GT(clipboard_text.size(), DeskNameView::kMaxLength);
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(clipboard_text);

  ClickOnDeskNameViewAtIndex(0);
  // Select all and delete.
  SendKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  SendKey(ui::VKEY_BACK);

  // Paste text.
  SendKey(ui::VKEY_V, ui::EF_CONTROL_DOWN);
  SendKey(ui::VKEY_RETURN);
  EXPECT_EQ(DeskNameView::kMaxLength, desk_1->name().size());
  EXPECT_EQ(expected_desk_name, desk_1->name());
}

class TabletModeDesksTest : public DesksTest {
 public:
  TabletModeDesksTest() = default;
  ~TabletModeDesksTest() override = default;

  // DesksTest:
  void SetUp() override {
    DesksTest::SetUp();

    // Enter tablet mode. Avoid TabletModeController::OnGetSwitchStates() from
    // disabling tablet mode.
    base::RunLoop().RunUntilIdle();
    TabletModeControllerTestApi().EnterTabletMode();
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabletModeDesksTest);
};

TEST_F(TabletModeDesksTest, Backdrops) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_1 = controller->desks()[0].get();
  const Desk* desk_2 = controller->desks()[1].get();

  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Enter tablet mode and expect that the backdrop is created only for desk_1,
  // since it's the one that has a window in it.
  auto* desk_1_backdrop_controller =
      GetDeskBackdropController(desk_1, Shell::GetPrimaryRootWindow());
  auto* desk_2_backdrop_controller =
      GetDeskBackdropController(desk_2, Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
  EXPECT_TRUE(desk_1_backdrop_controller->backdrop_window()->IsVisible());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window());

  // Enter overview and expect that the backdrop is still present for desk_1 but
  // hidden.
  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window()->IsVisible());

  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, overview_grid->size());

  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);

  // Now drag it to desk_2's mini_view, so that it moves to desk_2. Expect that
  // desk_1's backdrop is destroyed, while created (but still hidden) for
  // desk_2.
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1];
  EXPECT_EQ(desk_2, desk_2_mini_view->desk());
  DragItemToPoint(overview_item,
                  desk_2_mini_view->GetBoundsInScreen().CenterPoint(),
                  GetEventGenerator());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(base::Contains(desk_2->windows(), window.get()));
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window());
  ASSERT_TRUE(desk_2_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window()->IsVisible());

  // Exit overview, and expect that desk_2's backdrop remains hidden since the
  // desk is not activated yet.
  ExitOverview(OverviewEnterExitType::kImmediateExit);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window());
  ASSERT_TRUE(desk_2_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window()->IsVisible());

  // Activate desk_2 and expect that its backdrop is now visible.
  ActivateDesk(desk_2);
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window());
  ASSERT_TRUE(desk_2_backdrop_controller->backdrop_window());
  EXPECT_TRUE(desk_2_backdrop_controller->backdrop_window()->IsVisible());

  // No backdrops after exiting tablet mode.
  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window());
}

TEST_F(TabletModeDesksTest,
       BackdropStackingAndMiniviewsUpdatesWithOverviewDragDrop) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  Desk* desk_2 = controller->desks()[1].get();
  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());
  auto* desk_1_backdrop_controller =
      GetDeskBackdropController(desk_1, Shell::GetPrimaryRootWindow());
  auto* desk_2_backdrop_controller =
      GetDeskBackdropController(desk_2, Shell::GetPrimaryRootWindow());

  // Enter overview and expect that |desk_1| has a backdrop stacked under
  // |window| while desk_2 has none.
  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window());
  EXPECT_EQ(window.get(), desk_1_backdrop_controller->window_having_backdrop());
  EXPECT_FALSE(desk_2_backdrop_controller->window_having_backdrop());
  EXPECT_TRUE(IsStackedBelow(desk_1_backdrop_controller->backdrop_window(),
                             window.get()));

  // Prepare to drag and drop |window| on desk_2's mini view.
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, overview_grid->size());
  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1];

  // Observe how many times a drag and drop operation updates the mini views.
  TestDeskObserver observer1;
  TestDeskObserver observer2;
  desk_1->AddObserver(&observer1);
  desk_2->AddObserver(&observer2);
  {
    // For this test to fail the stacking test below, we need to drag and drop
    // while animations are enabled. https://crbug.com/1055732.
    ui::ScopedAnimationDurationScaleMode normal_anim(
        ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
    DragItemToPoint(overview_item,
                    desk_2_mini_view->GetBoundsInScreen().CenterPoint(),
                    GetEventGenerator());
  }
  // The backdrop should be destroyed for |desk_1|, and a new one should be
  // created for |window| in desk_2 and should be stacked below it.
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(base::Contains(desk_2->windows(), window.get()));
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window());
  ASSERT_TRUE(desk_2_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_1_backdrop_controller->window_having_backdrop());
  EXPECT_EQ(window.get(), desk_2_backdrop_controller->window_having_backdrop());
  EXPECT_TRUE(IsStackedBelow(desk_2_backdrop_controller->backdrop_window(),
                             window.get()));

  // The mini views should only be updated once for both desks.
  EXPECT_EQ(1, observer1.notify_counts());
  EXPECT_EQ(1, observer2.notify_counts());
  desk_1->RemoveObserver(&observer1);
  desk_2->RemoveObserver(&observer2);
}

TEST_F(TabletModeDesksTest, NoDesksBarInTabletModeWithOneDesk) {
  // Initially there's only one desk.
  auto* controller = DesksController::Get();
  ASSERT_EQ(1u, controller->desks().size());

  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Enter overview and expect that the DesksBar widget won't be created.
  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_FALSE(desks_bar_view);

  // It's possible to drag the window without any crashes.
  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  DragItemToPoint(overview_item, window->GetBoundsInScreen().CenterPoint(),
                  GetEventGenerator(), /*by_touch_gestures=*/true);

  // Exit overview and add a new desk, then re-enter overview. Expect that now
  // the desks bar is visible.
  ExitOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  NewDesk();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
}

TEST_F(TabletModeDesksTest, DesksCreationRemovalCycle) {
  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Create and remove desks in a cycle while in overview mode. Expect as the
  // containers are reused for new desks, their backdrop state are always
  // correct, and there are no crashes as desks are removed.
  auto* desks_controller = DesksController::Get();
  for (size_t i = 0; i < 2 * desks_util::kMaxNumberOfDesks; ++i) {
    NewDesk();
    ASSERT_EQ(2u, desks_controller->desks().size());
    const Desk* desk_1 = desks_controller->desks()[0].get();
    const Desk* desk_2 = desks_controller->desks()[1].get();
    auto* desk_1_backdrop_controller =
        GetDeskBackdropController(desk_1, Shell::GetPrimaryRootWindow());
    auto* desk_2_backdrop_controller =
        GetDeskBackdropController(desk_2, Shell::GetPrimaryRootWindow());
    {
      SCOPED_TRACE("Check backdrops after desk creation");
      ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
      EXPECT_TRUE(desk_1_backdrop_controller->backdrop_window()->IsVisible());
      EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window());
    }
    // Remove the active desk, and expect that now desk_2 should have a hidden
    // backdrop, while the container of the removed desk_1 should have none.
    RemoveDesk(desk_1);
    {
      SCOPED_TRACE("Check backdrops after desk removal");
      EXPECT_TRUE(desk_2->is_active());
      EXPECT_TRUE(DoesActiveDeskContainWindow(window.get()));
      EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window());
      ASSERT_TRUE(desk_2_backdrop_controller->backdrop_window());
      EXPECT_TRUE(desk_2_backdrop_controller->backdrop_window()->IsVisible());
    }
  }
}

TEST_F(TabletModeDesksTest, RestoreSplitViewOnDeskSwitch) {
  // Create two desks with two snapped windows in each.
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  Desk* desk_1 = desks_controller->desks()[0].get();
  Desk* desk_2 = desks_controller->desks()[1].get();

  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(win2.get(), SplitViewController::RIGHT);
  EXPECT_EQ(win1.get(), split_view_controller()->left_window());
  EXPECT_EQ(win2.get(), split_view_controller()->right_window());

  // Desk 2 has no windows, so the SplitViewController should be tracking no
  // windows.
  ActivateDesk(desk_2);
  EXPECT_EQ(nullptr, split_view_controller()->left_window());
  EXPECT_EQ(nullptr, split_view_controller()->right_window());
  // However, the snapped windows on desk 1 should retain their snapped state.
  EXPECT_TRUE(WindowState::Get(win1.get())->IsSnapped());
  EXPECT_TRUE(WindowState::Get(win2.get())->IsSnapped());

  // Snap two other windows in desk 2.
  auto win3 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win4 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win3.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(win4.get(), SplitViewController::RIGHT);
  EXPECT_EQ(win3.get(), split_view_controller()->left_window());
  EXPECT_EQ(win4.get(), split_view_controller()->right_window());

  // Switch back to desk 1, and expect the snapped windows are restored.
  ActivateDesk(desk_1);
  EXPECT_EQ(win1.get(), split_view_controller()->left_window());
  EXPECT_EQ(win2.get(), split_view_controller()->right_window());
  EXPECT_TRUE(WindowState::Get(win3.get())->IsSnapped());
  EXPECT_TRUE(WindowState::Get(win4.get())->IsSnapped());
}

TEST_F(TabletModeDesksTest, SnappedStateRetainedOnSwitchingDesksFromOverview) {
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(win2.get(), SplitViewController::RIGHT);
  EXPECT_EQ(win1.get(), split_view_controller()->left_window());
  EXPECT_EQ(win2.get(), split_view_controller()->right_window());

  // Enter overview and switch to desk_2 using its mini_view. Overview should
  // end, but TabletModeWindowManager should not maximize the snapped windows
  // and they should retain their snapped state.
  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  auto* desks_bar_view = overview_grid->desks_bar_view();
  auto* mini_view = desks_bar_view->mini_views()[1];
  Desk* desk_2 = desks_controller->desks()[1].get();
  EXPECT_EQ(desk_2, mini_view->desk());
  {
    DeskSwitchAnimationWaiter waiter;
    ClickOnView(mini_view, GetEventGenerator());
    waiter.Wait();
  }
  EXPECT_TRUE(WindowState::Get(win1.get())->IsSnapped());
  EXPECT_TRUE(WindowState::Get(win2.get())->IsSnapped());
  EXPECT_FALSE(overview_controller->InOverviewSession());

  // Snap two other windows in desk_2, and switch back to desk_1 from overview.
  // The snapped state should be retained for windows in both source and
  // destination desks.
  auto win3 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win4 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win3.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(win4.get(), SplitViewController::RIGHT);
  EXPECT_EQ(win3.get(), split_view_controller()->left_window());
  EXPECT_EQ(win4.get(), split_view_controller()->right_window());
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  desks_bar_view = overview_grid->desks_bar_view();
  mini_view = desks_bar_view->mini_views()[0];
  Desk* desk_1 = desks_controller->desks()[0].get();
  EXPECT_EQ(desk_1, mini_view->desk());
  {
    DeskSwitchAnimationWaiter waiter;
    ClickOnView(mini_view, GetEventGenerator());
    waiter.Wait();
  }
  EXPECT_TRUE(WindowState::Get(win1.get())->IsSnapped());
  EXPECT_TRUE(WindowState::Get(win2.get())->IsSnapped());
  EXPECT_TRUE(WindowState::Get(win3.get())->IsSnapped());
  EXPECT_TRUE(WindowState::Get(win4.get())->IsSnapped());
  EXPECT_FALSE(overview_controller->InOverviewSession());
}

TEST_F(
    TabletModeDesksTest,
    SnappedStateRetainedOnSwitchingDesksWithOverviewFullOfUnsnappableWindows) {
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          Shell::GetPrimaryRootWindow());
  const gfx::Size big(work_area.width() * 2 / 3, work_area.height() * 2 / 3);
  const gfx::Size small(250, 100);
  std::unique_ptr<aura::Window> win1 = CreateTestWindow(gfx::Rect(small));
  aura::test::TestWindowDelegate win2_delegate;
  win2_delegate.set_minimum_size(big);
  std::unique_ptr<aura::Window> win2(CreateTestWindowInShellWithDelegate(
      &win2_delegate, /*id=*/-1, gfx::Rect(big)));
  aura::test::TestWindowDelegate win3_delegate;
  win3_delegate.set_minimum_size(big);
  std::unique_ptr<aura::Window> win3(CreateTestWindowInShellWithDelegate(
      &win3_delegate, /*id=*/-1, gfx::Rect(big)));
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(EnterOverview());
  split_view_controller()->SnapWindow(win1.get(), SplitViewController::LEFT);
  EXPECT_EQ(win1.get(), split_view_controller()->left_window());
  EXPECT_FALSE(split_view_controller()->CanSnapWindow(win2.get()));
  EXPECT_FALSE(split_view_controller()->CanSnapWindow(win3.get()));

  // Switch to |desk_2| using its |mini_view|. Split view and overview should
  // end, but |win1| should retain its snapped state.
  ASSERT_TRUE(overview_controller->InOverviewSession());
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  auto* desks_bar_view = overview_grid->desks_bar_view();
  auto* mini_view = desks_bar_view->mini_views()[1];
  Desk* desk_2 = desks_controller->desks()[1].get();
  EXPECT_EQ(desk_2, mini_view->desk());
  {
    DeskSwitchAnimationWaiter waiter;
    ClickOnView(mini_view, GetEventGenerator());
    waiter.Wait();
  }
  EXPECT_TRUE(WindowState::Get(win1.get())->IsSnapped());
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller()->state());
  EXPECT_FALSE(overview_controller->InOverviewSession());

  // Switch back to |desk_1| and verify that split view is arranged as before.
  EXPECT_TRUE(EnterOverview());
  ASSERT_TRUE(overview_controller->InOverviewSession());
  overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  desks_bar_view = overview_grid->desks_bar_view();
  mini_view = desks_bar_view->mini_views()[0];
  Desk* desk_1 = desks_controller->desks()[0].get();
  EXPECT_EQ(desk_1, mini_view->desk());
  {
    DeskSwitchAnimationWaiter waiter;
    ClickOnView(mini_view, GetEventGenerator());
    waiter.Wait();
  }
  EXPECT_TRUE(WindowState::Get(win1.get())->IsSnapped());
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  EXPECT_EQ(win1.get(), split_view_controller()->left_window());
  EXPECT_TRUE(overview_controller->InOverviewSession());
}

TEST_F(TabletModeDesksTest, OverviewStateOnSwitchToDeskWithSplitView) {
  // Setup two desks, one (desk_1) with two snapped windows, and the other
  // (desk_2) with only one snapped window.
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  Desk* desk_1 = desks_controller->desks()[0].get();
  Desk* desk_2 = desks_controller->desks()[1].get();
  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(win2.get(), SplitViewController::RIGHT);
  EXPECT_EQ(win1.get(), split_view_controller()->left_window());
  EXPECT_EQ(win2.get(), split_view_controller()->right_window());
  auto* overview_controller = Shell::Get()->overview_controller();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  ActivateDesk(desk_2);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  auto win3 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win3.get(), SplitViewController::LEFT);
  EXPECT_EQ(win3.get(), split_view_controller()->left_window());
  EXPECT_EQ(nullptr, split_view_controller()->right_window());

  // Switching to the desk that has only one snapped window to be restored in
  // SplitView should enter overview mode, whereas switching to one that has two
  // snapped windows should exit overview.
  ActivateDesk(desk_1);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  ActivateDesk(desk_2);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ActivateDesk(desk_1);
  EXPECT_FALSE(overview_controller->InOverviewSession());
}

TEST_F(TabletModeDesksTest, RemovingDesksWithSplitView) {
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  Desk* desk_2 = desks_controller->desks()[1].get();
  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win1.get(), SplitViewController::LEFT);
  EXPECT_EQ(win1.get(), split_view_controller()->left_window());
  EXPECT_EQ(nullptr, split_view_controller()->right_window());
  ActivateDesk(desk_2);
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win2.get(), SplitViewController::RIGHT);
  EXPECT_EQ(nullptr, split_view_controller()->left_window());
  EXPECT_EQ(win2.get(), split_view_controller()->right_window());

  // Removing desk_2 will cause both snapped windows to merge in SplitView.
  RemoveDesk(desk_2);
  EXPECT_EQ(win1.get(), split_view_controller()->left_window());
  EXPECT_EQ(win2.get(), split_view_controller()->right_window());
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
}

TEST_F(TabletModeDesksTest, RemoveDeskWithMaximizedWindowAndMergeWithSnapped) {
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  Desk* desk_2 = desks_controller->desks()[1].get();
  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win1.get(), SplitViewController::LEFT);
  EXPECT_EQ(win1.get(), split_view_controller()->left_window());
  EXPECT_EQ(nullptr, split_view_controller()->right_window());
  ActivateDesk(desk_2);
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  EXPECT_EQ(nullptr, split_view_controller()->left_window());
  EXPECT_EQ(nullptr, split_view_controller()->right_window());
  EXPECT_TRUE(WindowState::Get(win2.get())->IsMaximized());

  // Removing desk_2 will cause us to enter overview mode without any crashes.
  // SplitView will remain left snapped.
  RemoveDesk(desk_2);
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(win1.get(), split_view_controller()->left_window());
  EXPECT_EQ(nullptr, split_view_controller()->right_window());
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
}

TEST_F(TabletModeDesksTest, BackdropsStacking) {
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  Desk* desk_1 = desks_controller->desks()[0].get();
  Desk* desk_2 = desks_controller->desks()[1].get();

  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(win2.get(), SplitViewController::RIGHT);
  auto* desk_1_backdrop_controller =
      GetDeskBackdropController(desk_1, Shell::GetPrimaryRootWindow());
  auto* desk_2_backdrop_controller =
      GetDeskBackdropController(desk_2, Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window());

  // The backdrop window should be stacked below both snapped windows.
  auto* desk_1_backdrop = desk_1_backdrop_controller->backdrop_window();
  EXPECT_TRUE(IsStackedBelow(desk_1_backdrop, win1.get()));
  EXPECT_TRUE(IsStackedBelow(desk_1_backdrop, win2.get()));

  // Switching to another desk doesn't change the backdrop state of the inactive
  // desk.
  ActivateDesk(desk_2);
  ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window());
  EXPECT_TRUE(IsStackedBelow(desk_1_backdrop, win1.get()));
  EXPECT_TRUE(IsStackedBelow(desk_1_backdrop, win2.get()));

  // Snapping new windows in desk_2 should update the backdrop state of desk_2,
  // but should not affect desk_1.
  auto win3 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win4 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win3.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(win4.get(), SplitViewController::RIGHT);
  ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
  ASSERT_TRUE(desk_2_backdrop_controller->backdrop_window());
  auto* desk_2_backdrop = desk_2_backdrop_controller->backdrop_window();
  EXPECT_TRUE(IsStackedBelow(desk_2_backdrop, win3.get()));
  EXPECT_TRUE(IsStackedBelow(desk_2_backdrop, win4.get()));
}

TEST_F(TabletModeDesksTest, HotSeatStateAfterMovingAWindowToAnotherDesk) {
  // Create 2 desks, and 2 windows, one of them is minimized and the other is
  // normal. Test that dragging and dropping them to another desk does not hide
  // the hotseat. https://crbug.com/1063536.
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto* window_state = WindowState::Get(win0.get());
  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());
  wm::ActivateWindow(win1.get());
  EXPECT_EQ(win1.get(), window_util::GetActiveWindow());

  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_session = overview_controller->overview_session();

  HotseatWidget* hotseat_widget =
      Shelf::ForWindow(win1.get())->hotseat_widget();
  EXPECT_EQ(HotseatState::kExtended, hotseat_widget->state());

  const struct {
    aura::Window* window;
    const char* trace_message;
  } kTestTable[] = {{win0.get(), "Minimized window"},
                    {win1.get(), "Normal window"}};

  for (const auto& test_case : kTestTable) {
    SCOPED_TRACE(test_case.trace_message);
    auto* win = test_case.window;
    auto* overview_item = overview_session->GetOverviewItemForWindow(win);
    ASSERT_TRUE(overview_item);

    auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
    const auto* desks_bar_view = overview_grid->desks_bar_view();
    ASSERT_TRUE(desks_bar_view);
    ASSERT_EQ(2u, desks_bar_view->mini_views().size());
    auto* desk_2_mini_view = desks_bar_view->mini_views()[1];
    auto* desk_2 = controller->desks()[1].get();
    EXPECT_EQ(desk_2, desk_2_mini_view->desk());
    auto* event_generator = GetEventGenerator();

    {
      // For this test to fail without the fix, we need to test while animations
      // are not disabled.
      ui::ScopedAnimationDurationScaleMode normal_anim(
          ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
      DragItemToPoint(overview_item,
                      desk_2_mini_view->GetBoundsInScreen().CenterPoint(),
                      event_generator,
                      /*by_touch_gestures=*/false);
    }

    EXPECT_TRUE(overview_controller->InOverviewSession());
    EXPECT_FALSE(DoesActiveDeskContainWindow(win));
    EXPECT_EQ(HotseatState::kExtended, hotseat_widget->state());
  }
}

TEST_F(TabletModeDesksTest, RestoringUnsnappableWindowsInSplitView) {
  UpdateDisplay("600x400");
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();

  // Setup an app window that cannot be snapped in landscape orientation, but
  // can be snapped in portrait orientation.
  auto window = CreateAppWindow(gfx::Rect(350, 350));
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window.get());
  widget->widget_delegate()->GetContentsView()->SetPreferredSize(
      gfx::Size(350, 100));
  EXPECT_FALSE(split_view_controller()->CanSnapWindow(window.get()));

  // Change to a portrait orientation and expect it's possible to snap the
  // window.
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kPortraitPrimary);
  EXPECT_TRUE(split_view_controller()->CanSnapWindow(window.get()));

  // Snap the window in this orientation.
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  EXPECT_EQ(window.get(), split_view_controller()->left_window());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  // Create a second desk, switch to it, and change back the orientation to
  // landscape, in which the window is not snappable. The window still exists on
  // the first desk, so nothing should change.
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_2 = controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, controller->active_desk());
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kLandscapePrimary);

  // Switch back to the first desk, and expect that SplitView is not restored,
  // since the only available window on that desk is not snappable.
  const Desk* desk_1 = controller->desks()[0].get();
  ActivateDesk(desk_1);
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());
}

TEST_F(DesksTest, MiniViewsTouchGestures) {
  auto* controller = DesksController::Get();
  NewDesk();
  NewDesk();
  ASSERT_EQ(3u, controller->desks().size());
  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(3u, desks_bar_view->mini_views().size());
  auto* desk_1_mini_view = desks_bar_view->mini_views()[0];
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1];
  auto* desk_3_mini_view = desks_bar_view->mini_views()[2];

  // Long gesture tapping on one mini_view shows its close button, and hides
  // those of other mini_views.
  auto* event_generator = GetEventGenerator();
  LongGestureTap(desk_1_mini_view->GetBoundsInScreen().CenterPoint(),
                 event_generator);
  EXPECT_TRUE(desk_1_mini_view->close_desk_button()->GetVisible());
  EXPECT_FALSE(desk_2_mini_view->close_desk_button()->GetVisible());
  EXPECT_FALSE(desk_3_mini_view->close_desk_button()->GetVisible());
  LongGestureTap(desk_2_mini_view->GetBoundsInScreen().CenterPoint(),
                 event_generator);
  EXPECT_FALSE(desk_1_mini_view->close_desk_button()->GetVisible());
  EXPECT_TRUE(desk_2_mini_view->close_desk_button()->GetVisible());
  EXPECT_FALSE(desk_3_mini_view->close_desk_button()->GetVisible());

  // Tapping on the visible close button, closes the desk rather than switches
  // to that desk.
  GestureTapOnView(desk_2_mini_view->close_desk_button(), event_generator);
  ASSERT_EQ(2u, controller->desks().size());
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Tapping on the invisible close button should not result in closing that
  // desk; rather activating that desk.
  EXPECT_FALSE(desk_1_mini_view->close_desk_button()->GetVisible());
  GestureTapOnView(desk_1_mini_view->close_desk_button(), event_generator);
  ASSERT_EQ(2u, controller->desks().size());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(controller->desks()[0]->is_active());
}

TEST_F(DesksTest, AutohiddenShelfAnimatesAfterDeskSwitch) {
  Shelf* shelf = GetPrimaryShelf();
  ShelfWidget* shelf_widget = shelf->shelf_widget();
  const gfx::Rect shown_shelf_bounds = shelf_widget->GetWindowBoundsInScreen();

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  // Enable animations so that we can make sure that they occur.
  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  NewDesk();

  // Create a window on the first desk so that the shelf will auto-hide there.
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  widget->Maximize();
  // LayoutShelf() forces the animation to completion, at which point the
  // shelf should go off the screen.
  shelf->shelf_layout_manager()->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  const gfx::Rect hidden_shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  EXPECT_NE(shown_shelf_bounds, hidden_shelf_bounds);

  // Go to the second desk.
  ActivateDesk(DesksController::Get()->desks()[1].get());
  // The shelf should now want to show itself.
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Since the layer transform animation is just starting, the transformed
  // bounds should still be hidden. If this fails, the change was not animated.
  gfx::RectF transformed_bounds(shelf_widget->GetWindowBoundsInScreen());
  shelf_widget->GetLayer()->transform().TransformRect(&transformed_bounds);
  EXPECT_EQ(gfx::ToEnclosedRect(transformed_bounds), hidden_shelf_bounds);
  EXPECT_EQ(shelf_widget->GetWindowBoundsInScreen(), shown_shelf_bounds);

  // Let's wait until the shelf animates to a fully shown state.
  while (shelf_widget->GetLayer()->transform() != gfx::Transform())
    WaitForMilliseconds(200);

  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
}

TEST_F(DesksTest, SwitchToDeskWithSnappedActiveWindow) {
  auto* desks_controller = DesksController::Get();
  auto* overview_controller = Shell::Get()->overview_controller();

  // Two virtual desks: |desk_1| (active) and |desk_2|.
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  Desk* desk_1 = desks_controller->desks()[0].get();
  Desk* desk_2 = desks_controller->desks()[1].get();

  // Two windows on |desk_1|: |win0| (snapped) and |win1|.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  WindowState* win0_state = WindowState::Get(win0.get());
  WMEvent snap_to_left(WM_EVENT_CYCLE_SNAP_PRIMARY);
  win0_state->OnWMEvent(&snap_to_left);
  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            win0_state->GetStateType());

  // Switch to |desk_2| and then back to |desk_1|. Verify that neither split
  // view nor overview arises.
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  EXPECT_FALSE(split_view_controller->InSplitViewMode());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  ActivateDesk(desk_2);
  EXPECT_FALSE(split_view_controller->InSplitViewMode());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  ActivateDesk(desk_1);
  EXPECT_FALSE(split_view_controller->InSplitViewMode());
  EXPECT_FALSE(overview_controller->InOverviewSession());
}

TEST_F(DesksTest, SuccessfulDragToDeskRemovesSplitViewIndicators) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());

  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());

  // Drag it to desk_2's mini_view. The overview grid should now show the
  // "no-windows" widget, and the window should move to desk_2.
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1];
  DragItemToPoint(overview_item,
                  desk_2_mini_view->GetBoundsInScreen().CenterPoint(),
                  GetEventGenerator(),
                  /*by_touch_gestures=*/false,
                  /*drop=*/false);
  // Validate that before dropping, the SplitView indicators and the drop target
  // widget are created.
  EXPECT_TRUE(overview_grid->drop_target_widget());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromOverview,
            overview_session->grid_list()[0]
                ->split_view_drag_indicators()
                ->current_window_dragging_state());
  // Now drop the window, and validate the indicators and the drop target were
  // removed.
  GetEventGenerator()->ReleaseLeftButton();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_grid->empty());
  EXPECT_FALSE(DoesActiveDeskContainWindow(window.get()));
  EXPECT_TRUE(overview_session->no_windows_widget_for_testing());
  EXPECT_FALSE(overview_grid->drop_target_widget());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            overview_session->grid_list()[0]
                ->split_view_drag_indicators()
                ->current_window_dragging_state());
}

TEST_F(DesksTest, DragAllOverviewWindowsToOtherDesksNotEndClamshellSplitView) {
  // Two virtual desks.
  NewDesk();
  ASSERT_EQ(2u, DesksController::Get()->desks().size());

  // Two windows: |win0| (in clamshell split view) and |win1| (in overview).
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto* overview_controller = Shell::Get()->overview_controller();
  ASSERT_TRUE(EnterOverview());
  auto* overview_session = overview_controller->overview_session();
  auto* generator = GetEventGenerator();
  DragItemToPoint(overview_session->GetOverviewItemForWindow(win0.get()),
                  gfx::Point(0, 0), generator);
  ASSERT_TRUE(overview_controller->InOverviewSession());
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(split_view_controller->InSplitViewMode());

  // Drag |win1| to the other desk.
  DragItemToPoint(overview_session->GetOverviewItemForWindow(win1.get()),
                  GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())
                      ->desks_bar_view()
                      ->mini_views()[1]
                      ->GetBoundsInScreen()
                      .CenterPoint(),
                  generator);
  EXPECT_FALSE(DoesActiveDeskContainWindow(win1.get()));

  // Overview should now be empty, but split view should still be active.
  ASSERT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_session->IsEmpty());
  EXPECT_TRUE(split_view_controller->InSplitViewMode());
}

TEST_F(DesksTest, DeskTraversalNonTouchpadMetrics) {
  NewDesk();
  NewDesk();
  NewDesk();
  ASSERT_EQ(4u, DesksController::Get()->desks().size());

  constexpr char kDeskTraversalsHistogramName[] =
      "Ash.Desks.NumberOfDeskTraversals";

  base::HistogramTester histogram_tester;
  auto* controller = DesksController::Get();
  const auto& desks = controller->desks();
  ASSERT_EQ(controller->active_desk(), desks[0].get());

  // Move 5 desks. There is nothing recorded at the end since the timer is still
  // running.
  ActivateDesk(desks[1].get());
  ActivateDesk(desks[0].get());
  ActivateDesk(desks[1].get());
  ActivateDesk(desks[2].get());
  ActivateDesk(desks[3].get());
  histogram_tester.ExpectBucketCount(kDeskTraversalsHistogramName, 5, 0);

  // Simulate advancing the time to end the timer. There should be 5 desks
  // recorded.
  controller->FireMetricsTimerForTesting();
  histogram_tester.ExpectBucketCount(kDeskTraversalsHistogramName, 5, 1);
}

// Tests that clipping is unchanged when removing a desk in overview. Regression
// test for https://crbug.com/1166300.
TEST_F(DesksTest, RemoveDeskPreservesOverviewClipping) {
  // Three virtual desks.
  NewDesk();
  NewDesk();

  auto* controller = DesksController::Get();
  Desk* desk2 = controller->desks()[1].get();
  Desk* desk3 = controller->desks()[2].get();
  ActivateDesk(desk3);

  // Create a window on |desk3| with a header.
  const int header_height = 32;
  auto win0 = CreateAppWindow(gfx::Rect(200, 200));
  win0->SetProperty(aura::client::kTopViewInset, header_height);
  EXPECT_EQ(desk3->GetDeskContainerForRoot(Shell::GetPrimaryRootWindow()),
            win0->parent());

  ASSERT_TRUE(EnterOverview());
  const gfx::Rect expected_clip = win0->layer()->GetTargetClipRect();

  // Remove |desk3|. |win0| is now a child of |desk2|.
  RemoveDesk(desk3);
  ASSERT_EQ(desk2->GetDeskContainerForRoot(Shell::GetPrimaryRootWindow()),
            win0->parent());

  // Tests that the clip is the same after the desk removal.
  EXPECT_EQ(expected_clip, win0->layer()->GetTargetClipRect());
}

namespace {

constexpr char kUser1Email[] = "user1@desks";
constexpr char kUser2Email[] = "user2@desks";

}  // namespace

class DesksMultiUserTest : public NoSessionAshTestBase,
                           public MultiUserWindowManagerDelegate {
 public:
  DesksMultiUserTest() = default;
  ~DesksMultiUserTest() override = default;

  MultiUserWindowManager* multi_user_window_manager() {
    return multi_user_window_manager_.get();
  }
  TestingPrefServiceSimple* user_1_prefs() { return user_1_prefs_; }
  TestingPrefServiceSimple* user_2_prefs() { return user_2_prefs_; }

  // AshTestBase:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    TestSessionControllerClient* session_controller =
        GetSessionControllerClient();
    session_controller->Reset();

    // Inject our own PrefServices for each user which enables us to setup the
    // desks restore data before the user signs in.
    auto user_1_prefs = std::make_unique<TestingPrefServiceSimple>();
    user_1_prefs_ = user_1_prefs.get();
    RegisterUserProfilePrefs(user_1_prefs_->registry(), /*for_test=*/true);
    auto user_2_prefs = std::make_unique<TestingPrefServiceSimple>();
    user_2_prefs_ = user_2_prefs.get();
    RegisterUserProfilePrefs(user_2_prefs_->registry(), /*for_test=*/true);
    session_controller->AddUserSession(kUser1Email,
                                       user_manager::USER_TYPE_REGULAR,
                                       /*provide_pref_service=*/false);
    session_controller->SetUserPrefService(GetUser1AccountId(),
                                           std::move(user_1_prefs));
    session_controller->AddUserSession(kUser2Email,
                                       user_manager::USER_TYPE_REGULAR,
                                       /*provide_pref_service=*/false);
    session_controller->SetUserPrefService(GetUser2AccountId(),
                                           std::move(user_2_prefs));
  }

  void TearDown() override {
    multi_user_window_manager_.reset();
    NoSessionAshTestBase::TearDown();
  }

  // MultiUserWindowManagerDelegate:
  void OnWindowOwnerEntryChanged(aura::Window* window,
                                 const AccountId& account_id,
                                 bool was_minimized,
                                 bool teleported) override {}
  void OnTransitionUserShelfToNewAccount() override {}

  AccountId GetUser1AccountId() const {
    return AccountId::FromUserEmail(kUser1Email);
  }

  AccountId GetUser2AccountId() const {
    return AccountId::FromUserEmail(kUser2Email);
  }

  void SwitchActiveUser(const AccountId& account_id) {
    GetSessionControllerClient()->SwitchActiveUser(account_id);
  }

  // Initializes the given |prefs| with a desks restore data of 3 desks, with
  // the third desk named "code", and the rest are default-named.
  void InitPrefsWithDesksRestoreData(PrefService* prefs) {
    InitPrefsWithDesksRestoreData(
        prefs, {std::string(), std::string(), std::string("code")});
  }

  // Initializes the given |prefs| with a desks restore data from the
  // |desk_names| list.
  void InitPrefsWithDesksRestoreData(PrefService* prefs,
                                     std::vector<std::string> desk_names) {
    DCHECK(prefs);
    ListPrefUpdate update(prefs, prefs::kDesksNamesList);
    base::ListValue* pref_data = update.Get();
    ASSERT_TRUE(pref_data->GetList().empty());
    for (auto desk_name : desk_names)
      pref_data->Append(desk_name);
  }

  void SimulateUserLogin(const AccountId& account_id) {
    SwitchActiveUser(account_id);
    multi_user_window_manager_ =
        MultiUserWindowManager::Create(this, account_id);
    MultiUserWindowManagerImpl::Get()->SetAnimationSpeedForTest(
        MultiUserWindowManagerImpl::ANIMATION_SPEED_DISABLED);
    GetSessionControllerClient()->SetSessionState(
        session_manager::SessionState::ACTIVE);
  }

 private:
  std::unique_ptr<MultiUserWindowManager> multi_user_window_manager_;

  TestingPrefServiceSimple* user_1_prefs_ = nullptr;
  TestingPrefServiceSimple* user_2_prefs_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DesksMultiUserTest);
};

TEST_F(DesksMultiUserTest, SwitchUsersBackAndForth) {
  SimulateUserLogin(GetUser1AccountId());
  auto* controller = DesksController::Get();
  NewDesk();
  NewDesk();
  ASSERT_EQ(3u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  Desk* desk_2 = controller->desks()[1].get();
  Desk* desk_3 = controller->desks()[2].get();
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  multi_user_window_manager()->SetWindowOwner(win0.get(), GetUser1AccountId());
  EXPECT_TRUE(win0->IsVisible());
  ActivateDesk(desk_2);
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  multi_user_window_manager()->SetWindowOwner(win1.get(), GetUser1AccountId());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_TRUE(win1->IsVisible());

  // Switch to user_2 and expect no windows from user_1 is visible regardless of
  // the desk.
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_FALSE(win1->IsVisible());

  // Since this is the first time this user logs in, desk_1 will be activated
  // for this user.
  EXPECT_TRUE(desk_1->is_active());

  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win2.get(), GetUser2AccountId());
  EXPECT_TRUE(win2->IsVisible());
  ActivateDesk(desk_3);
  auto win3 = CreateAppWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win3.get(), GetUser2AccountId());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_FALSE(win1->IsVisible());
  EXPECT_FALSE(win2->IsVisible());
  EXPECT_TRUE(win3->IsVisible());

  // When switching back to user_1, the active desk should be restored to
  // desk_2.
  SwitchActiveUser(GetUser1AccountId());
  EXPECT_TRUE(desk_2->is_active());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_TRUE(win1->IsVisible());
  EXPECT_FALSE(win2->IsVisible());
  EXPECT_FALSE(win3->IsVisible());

  // When switching to user_2, the active desk should be restored to desk_3.
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_TRUE(desk_3->is_active());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_FALSE(win1->IsVisible());
  EXPECT_FALSE(win2->IsVisible());
  EXPECT_TRUE(win3->IsVisible());
}

TEST_F(DesksMultiUserTest, RemoveDesks) {
  SimulateUserLogin(GetUser1AccountId());
  // Create two desks with several windows with different app types that
  // belong to different users.
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  Desk* desk_2 = controller->desks()[1].get();
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  multi_user_window_manager()->SetWindowOwner(win0.get(), GetUser1AccountId());
  EXPECT_TRUE(win0->IsVisible());
  ActivateDesk(desk_2);
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto win2 = CreateAppWindow(gfx::Rect(50, 50, 200, 200), AppType::ARC_APP);
  // Non-app window.
  auto win3 = CreateAppWindow(gfx::Rect(50, 50, 200, 200), AppType::NON_APP);
  multi_user_window_manager()->SetWindowOwner(win2.get(), GetUser1AccountId());
  multi_user_window_manager()->SetWindowOwner(win1.get(), GetUser1AccountId());
  multi_user_window_manager()->SetWindowOwner(win3.get(), GetUser1AccountId());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_TRUE(win1->IsVisible());
  EXPECT_TRUE(win2->IsVisible());
  EXPECT_TRUE(win3->IsVisible());

  // Switch to user_2 and expect no windows from user_1 is visible regardless of
  // the desk.
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_FALSE(win1->IsVisible());
  EXPECT_FALSE(win2->IsVisible());
  EXPECT_FALSE(win3->IsVisible());

  auto win4 = CreateAppWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win4.get(), GetUser2AccountId());
  EXPECT_TRUE(win4->IsVisible());
  ActivateDesk(desk_2);
  auto win5 = CreateAppWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win5.get(), GetUser2AccountId());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_FALSE(win1->IsVisible());
  EXPECT_FALSE(win2->IsVisible());
  EXPECT_FALSE(win3->IsVisible());
  EXPECT_FALSE(win4->IsVisible());
  EXPECT_TRUE(win5->IsVisible());

  // Delete desk_2, and expect all app windows move to desk_1.
  RemoveDesk(desk_2);
  EXPECT_TRUE(desk_1->is_active());
  auto* desk_1_container =
      desk_1->GetDeskContainerForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(desk_1_container, win0->parent());
  EXPECT_EQ(desk_1_container, win1->parent());
  EXPECT_EQ(desk_1_container, win2->parent());
  // The non-app window didn't move.
  EXPECT_NE(desk_1_container, win3->parent());
  EXPECT_EQ(desk_1_container, win4->parent());
  EXPECT_EQ(desk_1_container, win5->parent());

  // Only user_2's window are visible.
  EXPECT_TRUE(win4->IsVisible());
  EXPECT_TRUE(win5->IsVisible());

  // The non-app window will always be hidden.
  EXPECT_FALSE(win3->IsVisible());

  // Switch to user_1 and expect the correct windows' visibility.
  SwitchActiveUser(GetUser1AccountId());
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_TRUE(win0->IsVisible());
  EXPECT_TRUE(win1->IsVisible());
  EXPECT_TRUE(win2->IsVisible());
  EXPECT_FALSE(win3->IsVisible());

  // Create two more desks, switch to user_2, and activate the third desk.
  NewDesk();
  NewDesk();
  ASSERT_EQ(3u, controller->desks().size());
  desk_2 = controller->desks()[1].get();
  Desk* desk_3 = controller->desks()[2].get();
  SwitchActiveUser(GetUser2AccountId());
  ActivateDesk(desk_3);
  auto win6 = CreateAppWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win5.get(), GetUser2AccountId());

  // Switch back to user_1, and remove the first desk. When switching back to
  // user_2 after that, we should see that what used to be the third desk is now
  // active.
  SwitchActiveUser(GetUser1AccountId());
  EXPECT_TRUE(desk_1->is_active());
  RemoveDesk(desk_1);
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_TRUE(desk_3->is_active());
  EXPECT_TRUE(win6->IsVisible());
}

TEST_F(DesksMultiUserTest, SwitchingUsersEndsOverview) {
  SimulateUserLogin(GetUser1AccountId());
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(EnterOverview());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_FALSE(overview_controller->InOverviewSession());
}

using DesksRestoreMultiUserTest = DesksMultiUserTest;

TEST_F(DesksRestoreMultiUserTest, DesksRestoredFromPrimaryUserPrefsOnly) {
  constexpr int kDefaultActiveDesk = 0;
  constexpr int kUser1StoredActiveDesk = 2;
  InitPrefsWithDesksRestoreData(user_1_prefs());

  // Set the primary user1's active desk prefs to kUser1StoredActiveDesk.
  user_1_prefs()->SetInteger(prefs::kDesksActiveDesk, kUser1StoredActiveDesk);

  SimulateUserLogin(GetUser1AccountId());
  // User 1 is the first to login, hence the primary user.
  auto* controller = DesksController::Get();
  const auto& desks = controller->desks();

  auto verify_desks = [&](const std::string& trace_name) {
    SCOPED_TRACE(trace_name);
    EXPECT_EQ(3u, desks.size());
    EXPECT_EQ(u"Desk 1", desks[0]->name());
    EXPECT_EQ(u"Desk 2", desks[1]->name());
    EXPECT_EQ(u"code", desks[2]->name());
    // Restored non-default names should be marked as `set_by_user`.
    EXPECT_FALSE(desks[0]->is_name_set_by_user());
    EXPECT_FALSE(desks[1]->is_name_set_by_user());
    EXPECT_TRUE(desks[2]->is_name_set_by_user());
  };

  verify_desks("Before switching users");
  // The primary user1 should restore the saved active desk from its pref.
  EXPECT_EQ(desks[kUser1StoredActiveDesk]->container_id(),
            desks_util::GetActiveDeskContainerId());

  // Switching users should not change anything as restoring happens only at
  // the time when the first user signs in.
  SwitchActiveUser(GetUser2AccountId());
  verify_desks("After switching users");
  // The secondary user2 should start with a default active desk.
  EXPECT_EQ(desks[kDefaultActiveDesk]->container_id(),
            desks_util::GetActiveDeskContainerId());

  // Activating the second desk in the secondary user session should not
  // affect the primary user1's active desk pref. Moreover, switching back to
  // user1 session should activate the user1's previously active desk
  // correctly.
  ActivateDesk(desks[1].get());
  EXPECT_EQ(user_1_prefs()->GetInteger(prefs::kDesksActiveDesk),
            kUser1StoredActiveDesk);
  SwitchActiveUser(GetUser1AccountId());
  EXPECT_EQ(desks[kUser1StoredActiveDesk]->container_id(),
            desks_util::GetActiveDeskContainerId());
}

TEST_F(DesksRestoreMultiUserTest,
       ChangesMadeBySecondaryUserAffectsOnlyPrimaryUserPrefs) {
  InitPrefsWithDesksRestoreData(user_1_prefs());
  SimulateUserLogin(GetUser1AccountId());
  // Switch to user 2 (secondary) and make some desks changes. Those changes
  // should be persisted to user 1's prefs only.
  SwitchActiveUser(GetUser2AccountId());

  auto* controller = DesksController::Get();
  const auto& desks = controller->desks();
  ASSERT_EQ(3u, desks.size());

  // Create a fourth desk.
  NewDesk();
  VerifyDesksRestoreData(user_1_prefs(), {std::string(), std::string(),
                                          std::string("code"), std::string()});
  // User 2's prefs are unaffected (empty list of desks).
  VerifyDesksRestoreData(user_2_prefs(), {});

  // Delete the second desk.
  RemoveDesk(desks[1].get());
  VerifyDesksRestoreData(user_1_prefs(),
                         {std::string(), std::string("code"), std::string()});
  VerifyDesksRestoreData(user_2_prefs(), {});

  // Move the third desk to the second to test desks reordering.
  controller->ReorderDesk(/*old_index=*/2, /*new_index=*/1);
  VerifyDesksRestoreData(user_1_prefs(),
                         {std::string(), std::string(), std::string("code")});
  VerifyDesksRestoreData(user_2_prefs(), {});
}

// Tests that desks reordering updates workspaces of all windows in affected
// desks for all simultaneously logged-in users.
TEST_F(DesksRestoreMultiUserTest,
       DeskIndexChangesMadeByActiveUserAffectsAllUsers) {
  // Setup two users and four desks with one window in each desk --------------
  // Create user1 and user2 with four shared desks named numerically by their
  // initial order. Set the user1 active desk to the fourth desk.
  const int n_desks_per_user = 4;
  int user_1_active_desk_index = 3;
  user_1_prefs()->SetInteger(prefs::kDesksActiveDesk, user_1_active_desk_index);
  InitPrefsWithDesksRestoreData(user_1_prefs(),
                                std::vector<std::string>{"0", "1", "2", "3"});
  SimulateUserLogin(GetUser1AccountId());
  auto* controller = DesksController::Get();
  const auto& desks = controller->desks();
  ASSERT_EQ(4u, desks.size());

  // Switch to user2 and activate the third desk. Thus, the desks initial state
  // is [0, 1, 2**, 3*] where * and ** label active desks of user1 and user2.
  EXPECT_EQ(desks[user_1_active_desk_index]->container_id(),
            desks_util::GetActiveDeskContainerId());
  int user_2_active_desk_index = 2;
  SwitchActiveUser(GetUser2AccountId());
  ActivateDesk(desks[user_2_active_desk_index].get());
  EXPECT_EQ(desks[user_2_active_desk_index]->container_id(),
            desks_util::GetActiveDeskContainerId());
  EXPECT_EQ(user_1_active_desk_index,
            user_1_prefs()->GetInteger(prefs::kDesksActiveDesk));

  // For each user's desk, create a window and set window workspace property.
  std::vector<std::unique_ptr<aura::Window>> user1_windows;
  std::vector<std::unique_ptr<aura::Window>> user2_windows;
  for (int i = 0; i < n_desks_per_user; i++) {
    user1_windows.push_back(CreateAppWindow());
    user2_windows.push_back(CreateAppWindow());
    multi_user_window_manager()->SetWindowOwner(user1_windows[i].get(),
                                                GetUser1AccountId());
    multi_user_window_manager()->SetWindowOwner(user2_windows[i].get(),
                                                GetUser2AccountId());
    controller->SendToDeskAtIndex(user1_windows[i].get(), i);
    controller->SendToDeskAtIndex(user2_windows[i].get(), i);
    EXPECT_EQ(i,
              user1_windows[i]->GetProperty(aura::client::kWindowWorkspaceKey));
    EXPECT_EQ(i,
              user2_windows[i]->GetProperty(aura::client::kWindowWorkspaceKey));
  }
  // Done setup four desks with the numerical desk names ----------------------

  // |desk_names_as_ints| represents the numerical desk names in desks order
  // after a series of desks reordering. The initial order is [0, 1, 2, 3].
  auto check_window_workspaces =
      [&](const std::vector<int> desk_names_as_ints) {
        DCHECK(n_desks_per_user == desk_names_as_ints.size());
        for (int desk_index = 0; desk_index < n_desks_per_user; desk_index++) {
          int desk_name_as_int = desk_names_as_ints[desk_index];
          EXPECT_EQ(desk_index, user1_windows[desk_name_as_int]->GetProperty(
                                    aura::client::kWindowWorkspaceKey));
          EXPECT_EQ(desk_index, user2_windows[desk_name_as_int]->GetProperty(
                                    aura::client::kWindowWorkspaceKey));
        }
      };

  // 1. Test that desks reordering from the secondary user updates both users'
  // saved active desks correctly. Move the fourth desk to the second, so the
  // desks state become [0, 3*, 1, 2**].
  controller->ReorderDesk(/*old_index=*/3, /*new_index=*/1);
  user_1_active_desk_index = 1;
  user_2_active_desk_index = 3;
  check_window_workspaces(std::vector<int>{0, 3, 1, 2});
  VerifyDesksRestoreData(user_1_prefs(),
                         std::vector<std::string>{"0", "3", "1", "2"});
  VerifyDesksRestoreData(user_2_prefs(), {});
  EXPECT_EQ(user_1_active_desk_index,
            user_1_prefs()->GetInteger(prefs::kDesksActiveDesk));
  EXPECT_EQ(desks[user_2_active_desk_index]->container_id(),
            desks_util::GetActiveDeskContainerId());

  // Switch to the primary user1.
  SwitchActiveUser(GetUser1AccountId());
  EXPECT_EQ(desks[user_1_active_desk_index]->container_id(),
            desks_util::GetActiveDeskContainerId());

  // 2. Test that desks reordering from the primary user updates both users'
  // saved active desks correctly. Move the first desk to the fourth, so the
  // desks state become [3*, 1, 2**, 0].
  controller->ReorderDesk(/*old_index=*/0, /*new_index=*/3);
  user_1_active_desk_index = 0;
  user_2_active_desk_index = 2;
  check_window_workspaces(std::vector<int>{3, 1, 2, 0});
  VerifyDesksRestoreData(user_1_prefs(),
                         std::vector<std::string>{"3", "1", "2", "0"});
  VerifyDesksRestoreData(user_2_prefs(), {});
  EXPECT_EQ(user_1_active_desk_index,
            user_1_prefs()->GetInteger(prefs::kDesksActiveDesk));
  EXPECT_EQ(desks[user_1_active_desk_index]->container_id(),
            desks_util::GetActiveDeskContainerId());
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_EQ(desks[user_2_active_desk_index]->container_id(),
            desks_util::GetActiveDeskContainerId());
}

}  // namespace

// Simulates the same behavior of event rewriting that key presses go through.
class DesksAcceleratorsTest : public DesksTest,
                              public ui::EventRewriterChromeOS::Delegate {
 public:
  DesksAcceleratorsTest() = default;
  ~DesksAcceleratorsTest() override = default;

  // DesksTest:
  void SetUp() override {
    DesksTest::SetUp();

    auto* event_rewriter_controller = EventRewriterController::Get();
    auto event_rewriter = std::make_unique<ui::EventRewriterChromeOS>(
        this, Shell::Get()->sticky_keys_controller(), false,
        &fake_ime_keyboard_);
    event_rewriter_controller->AddEventRewriter(std::move(event_rewriter));
  }

  // ui::EventRewriterChromeOS::Delegate:
  bool RewriteModifierKeys() override { return true; }
  bool GetKeyboardRemappedPrefValue(const std::string& pref_name,
                                    int* result) const override {
    return false;
  }
  bool TopRowKeysAreFunctionKeys() const override { return false; }
  bool IsExtensionCommandRegistered(ui::KeyboardCode key_code,
                                    int flags) const override {
    return false;
  }
  bool IsSearchKeyAcceleratorReserved() const override { return true; }
  bool NotifyDeprecatedRightClickRewrite() override { return false; }
  bool NotifyDeprecatedFKeyRewrite() override { return false; }
  bool NotifyDeprecatedSixPackKeyRewrite(ui::KeyboardCode key_code) override {
    return false;
  }

  void SendAccelerator(ui::KeyboardCode key_code, int flags) {
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->PressKey(key_code, flags);
    generator->ReleaseKey(key_code, flags);
  }

  // Moves the overview highlight to the next item.
  void MoveOverviewHighlighter(OverviewSession* session) {
    session->Move(/*reverse=*/false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DesksAcceleratorsTest);

  chromeos::input_method::FakeImeKeyboard fake_ime_keyboard_;
};

namespace {

TEST_F(DesksAcceleratorsTest, NewDesk) {
  auto* controller = DesksController::Get();
  // It's possible to add up to kMaxNumberOfDesks desks using the
  // shortcut.
  const int flags = ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN;
  for (size_t num_desks = 1; num_desks < desks_util::kMaxNumberOfDesks;
       ++num_desks) {
    DeskSwitchAnimationWaiter waiter;
    SendAccelerator(ui::VKEY_OEM_PLUS, flags);
    waiter.Wait();
    // The newly created desk should be activated.
    ASSERT_EQ(num_desks + 1, controller->desks().size());
    EXPECT_TRUE(controller->desks().back()->is_active());
  }

  // When we reach the limit, the shortcut does nothing.
  EXPECT_EQ(desks_util::kMaxNumberOfDesks, controller->desks().size());
  SendAccelerator(ui::VKEY_OEM_PLUS, flags);
  EXPECT_EQ(desks_util::kMaxNumberOfDesks, controller->desks().size());
}

TEST_F(DesksAcceleratorsTest, CannotRemoveLastDesk) {
  auto* controller = DesksController::Get();
  // Removing the last desk is not possible.
  ASSERT_EQ(1u, controller->desks().size());
  const int flags = ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN;
  SendAccelerator(ui::VKEY_OEM_MINUS, flags);
  ASSERT_EQ(1u, controller->desks().size());
}

TEST_F(DesksAcceleratorsTest, RemoveDesk) {
  auto* controller = DesksController::Get();
  // Create a few desks and remove them outside and inside overview using the
  // shortcut.
  NewDesk();
  NewDesk();
  ASSERT_EQ(3u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  Desk* desk_2 = controller->desks()[1].get();
  Desk* desk_3 = controller->desks()[2].get();
  EXPECT_TRUE(desk_1->is_active());
  const int flags = ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN;
  DeskSwitchAnimationWaiter waiter;
  SendAccelerator(ui::VKEY_OEM_MINUS, flags);
  waiter.Wait();
  ASSERT_EQ(2u, controller->desks().size());
  EXPECT_TRUE(desk_2->is_active());

  // Using the accelerator doesn't result in exiting overview.
  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  SendAccelerator(ui::VKEY_OEM_MINUS, flags);
  ASSERT_EQ(1u, controller->desks().size());
  EXPECT_TRUE(desk_3->is_active());
  EXPECT_TRUE(overview_controller->InOverviewSession());
}

TEST_F(DesksAcceleratorsTest, RemoveRightmostDesk) {
  auto* controller = DesksController::Get();
  // Create a few desks and remove them outside and inside overview using the
  // shortcut.
  NewDesk();
  NewDesk();
  ASSERT_EQ(3u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  Desk* desk_2 = controller->desks()[1].get();
  Desk* desk_3 = controller->desks()[2].get();
  ActivateDesk(desk_3);
  EXPECT_TRUE(desk_3->is_active());
  const int flags = ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN;
  {
    DeskSwitchAnimationWaiter waiter;
    SendAccelerator(ui::VKEY_OEM_MINUS, flags);
    waiter.Wait();
  }
  ASSERT_EQ(2u, controller->desks().size());
  EXPECT_TRUE(desk_2->is_active());
  {
    DeskSwitchAnimationWaiter waiter;
    SendAccelerator(ui::VKEY_OEM_MINUS, flags);
    waiter.Wait();
  }
  ASSERT_EQ(1u, controller->desks().size());
  EXPECT_TRUE(desk_1->is_active());
}

TEST_F(DesksAcceleratorsTest, LeftRightDeskActivation) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  Desk* desk_2 = controller->desks()[1].get();
  EXPECT_TRUE(desk_1->is_active());
  // No desk on left, nothing should happen.
  const int flags = ui::EF_COMMAND_DOWN;
  SendAccelerator(ui::VKEY_OEM_4, flags);
  EXPECT_TRUE(desk_1->is_active());

  // Go right until there're no more desks on the right.
  {
    DeskSwitchAnimationWaiter waiter;
    SendAccelerator(ui::VKEY_OEM_6, flags);
    waiter.Wait();
    EXPECT_TRUE(desk_2->is_active());
  }

  // Nothing happens.
  SendAccelerator(ui::VKEY_OEM_6, flags);
  EXPECT_TRUE(desk_2->is_active());

  // Go back left.
  {
    DeskSwitchAnimationWaiter waiter;
    SendAccelerator(ui::VKEY_OEM_4, flags);
    waiter.Wait();
    EXPECT_TRUE(desk_1->is_active());
  }
}

TEST_F(DesksAcceleratorsTest, MoveWindowLeftRightDesk) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  Desk* desk_2 = controller->desks()[1].get();
  EXPECT_TRUE(desk_1->is_active());

  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Moving window left when this is the left-most desk. Nothing happens.
  const int flags = ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN;
  SendAccelerator(ui::VKEY_OEM_4, flags);
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());
  EXPECT_TRUE(DoesActiveDeskContainWindow(window.get()));

  // Move window right, it should be deactivated.
  SendAccelerator(ui::VKEY_OEM_6, flags);
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());
  EXPECT_TRUE(desk_1->windows().empty());
  EXPECT_TRUE(base::Contains(desk_2->windows(), window.get()));

  // No more active windows on this desk, nothing happens.
  SendAccelerator(ui::VKEY_OEM_6, flags);
  EXPECT_TRUE(desk_1->windows().empty());
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());

  // Activate desk 2, and do the same set of tests.
  ActivateDesk(desk_2);
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Move right does nothing.
  SendAccelerator(ui::VKEY_OEM_6, flags);
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  SendAccelerator(ui::VKEY_OEM_4, flags);
  EXPECT_TRUE(desk_2->windows().empty());
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());
  EXPECT_TRUE(base::Contains(desk_1->windows(), window.get()));
}

TEST_F(DesksAcceleratorsTest, MoveWindowLeftRightDeskOverview) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  Desk* desk_2 = controller->desks()[1].get();
  EXPECT_TRUE(desk_1->is_active());

  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(win0.get());
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());

  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const int flags = ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN;
  // In overview, while no window is highlighted, nothing should happen.
  const size_t num_windows_before = desk_1->windows().size();
  EXPECT_TRUE(desk_2->windows().empty());
  SendAccelerator(ui::VKEY_OEM_6, flags);
  ASSERT_EQ(num_windows_before, desk_1->windows().size());
  EXPECT_TRUE(desk_2->windows().empty());

  auto* overview_session = overview_controller->overview_session();
  ASSERT_TRUE(overview_session);
  // It's possible to move the highlighted window. |Move()| will cycle through
  // the desk items first, so call it until we are highlighting an OverviewItem.
  while (!overview_session->GetHighlightedWindow())
    MoveOverviewHighlighter(overview_session);
  EXPECT_EQ(win0.get(), overview_session->GetHighlightedWindow());
  SendAccelerator(ui::VKEY_OEM_6, flags);
  EXPECT_FALSE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_TRUE(base::Contains(desk_2->windows(), win0.get()));
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // The highlight widget should move to the next window if we call
  // |MoveOverviewHighlighter()| again.
  MoveOverviewHighlighter(overview_session);
  EXPECT_EQ(win1.get(), overview_session->GetHighlightedWindow());
  SendAccelerator(ui::VKEY_OEM_6, flags);
  EXPECT_FALSE(DoesActiveDeskContainWindow(win1.get()));
  EXPECT_TRUE(base::Contains(desk_2->windows(), win1.get()));
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // No more highlighted windows.
  EXPECT_FALSE(overview_session->GetHighlightedWindow());
}

TEST_F(DesksAcceleratorsTest, CannotMoveAlwaysOnTopWindows) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  Desk* desk_2 = controller->desks()[1].get();
  EXPECT_TRUE(desk_1->is_active());

  // An always-on-top window does not belong to any desk and hence cannot be
  // removed.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  win0->SetProperty(aura::client::kZOrderingKey,
                    ui::ZOrderLevel::kFloatingWindow);
  wm::ActivateWindow(win0.get());
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());
  EXPECT_FALSE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_FALSE(controller->MoveWindowFromActiveDeskTo(
      win0.get(), desk_2, win0->GetRootWindow(),
      DesksMoveWindowFromActiveDeskSource::kDragAndDrop));
  const int flags = ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN;
  SendAccelerator(ui::VKEY_OEM_4, flags);
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());
  EXPECT_TRUE(win0->IsVisible());

  // It remains visible even after switching desks.
  ActivateDesk(desk_2);
  EXPECT_TRUE(win0->IsVisible());
}

// Tests that hitting an acclerator to switch desks does not cause a crash if we
// are already at an edge desk. Regression test for https://crbug.com/1159068.
TEST_F(DesksAcceleratorsTest, HitAcceleratorWhenAlreadyAtEdge) {
  NewDesk();

  // Enable animations so that we can make sure that they occur.
  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // First go right. Wait until the ending screenshot is taken.
  const int flags = ui::EF_COMMAND_DOWN;
  SendAccelerator(ui::VKEY_OEM_6, flags);

  DeskAnimationBase* animation = DesksController::Get()->animation();
  ASSERT_TRUE(animation);
  base::RunLoop run_loop;
  auto* desk_switch_animator =
      animation->GetDeskSwitchAnimatorAtIndexForTesting(0);
  ASSERT_TRUE(desk_switch_animator);
  RootWindowDeskSwitchAnimatorTestApi(desk_switch_animator)
      .SetOnEndingScreenshotTakenCallback(run_loop.QuitClosure());
  run_loop.Run();

  // Tap the accelerator to go left to desk 1, then tap again. There should be
  // no crash.
  SendAccelerator(ui::VKEY_OEM_4, flags);
  SendAccelerator(ui::VKEY_OEM_4, flags);
}

class PerDeskShelfTest : public AshTestBase,
                         public ::testing::WithParamInterface<bool> {
 public:
  PerDeskShelfTest() = default;
  PerDeskShelfTest(const PerDeskShelfTest&) = delete;
  PerDeskShelfTest& operator=(const PerDeskShelfTest&) = delete;
  ~PerDeskShelfTest() override = default;

  // AshTestBase:
  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(features::kPerDeskShelf);
    } else {
      scoped_feature_list_.InitAndDisableFeature(features::kPerDeskShelf);
    }

    AshTestBase::SetUp();
  }

  // Creates and returns an app window that is asscoaited with a shelf item with
  // |type|.
  std::unique_ptr<aura::Window> CreateAppWithShelfItem(ShelfItemType type) {
    auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
    const ash::ShelfID shelf_id(base::StringPrintf("%d", current_shelf_id_++));
    window->SetProperty(ash::kShelfIDKey, shelf_id.Serialize());
    window->SetProperty(ash::kAppIDKey, shelf_id.app_id);
    window->SetProperty<int>(ash::kShelfItemTypeKey, type);
    ShelfItem item;
    item.status = ShelfItemStatus::STATUS_RUNNING;
    item.type = type;
    item.id = shelf_id;
    ShelfModel::Get()->Add(item,
                           std::make_unique<TestShelfItemDelegate>(item.id));
    return window;
  }

  bool IsPerDeskShelfEnabled() const { return GetParam(); }

  ShelfView* GetShelfView() const {
    return GetPrimaryShelf()->GetShelfViewForTesting();
  }

  // Returns the index of the shelf item associated with the given |window| or
  // -1 if no such item exists.
  int GetShelfItemIndexForWindow(aura::Window* window) const {
    const auto shelf_id =
        ShelfID::Deserialize(window->GetProperty(kShelfIDKey));
    EXPECT_FALSE(shelf_id.IsNull());
    return ShelfModel::Get()->ItemIndexByID(shelf_id);
  }

  // Verifies that the visibility of the shelf item view associated with the
  // given |window| is equal to the given |expected_visibility|.
  void VerifyViewVisibility(aura::Window* window,
                            bool expected_visibility) const {
    const int index = GetShelfItemIndexForWindow(window);
    EXPECT_GE(index, 0);
    auto* shelf_view = GetShelfView();
    auto* view_model = shelf_view->view_model();

    views::View* item_view = view_model->view_at(index);
    const bool contained_in_visible_indices =
        base::Contains(shelf_view->visible_views_indices(), index);

    EXPECT_EQ(expected_visibility, item_view->GetVisible());
    EXPECT_EQ(expected_visibility, contained_in_visible_indices);
  }

  void MoveWindowFromActiveDeskTo(aura::Window* window,
                                  Desk* target_desk) const {
    DesksController::Get()->MoveWindowFromActiveDeskTo(
        window, target_desk, window->GetRootWindow(),
        DesksMoveWindowFromActiveDeskSource::kDragAndDrop);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  int current_shelf_id_ = 0;
};

TEST_P(PerDeskShelfTest, MoveWindowOutOfActiveDesk) {
  auto* controller = DesksController::Get();
  NewDesk();

  // Create three app windows; a browser window, a pinned app, and a normal app.
  auto win0 = CreateAppWithShelfItem(ShelfItemType::TYPE_BROWSER_SHORTCUT);
  aura::Window* browser = win0.get();
  auto win1 = CreateAppWithShelfItem(ShelfItemType::TYPE_PINNED_APP);
  aura::Window* pinned = win1.get();
  auto win2 = CreateAppWithShelfItem(ShelfItemType::TYPE_APP);
  aura::Window* app = win2.get();

  // All items should be visible.
  VerifyViewVisibility(browser, true);
  VerifyViewVisibility(pinned, true);
  VerifyViewVisibility(app, true);

  // Move the app window, it should be removed from the shelf if the feature is
  // enabled.
  const bool visible_in_per_desk_shelf = IsPerDeskShelfEnabled() ? false : true;
  Desk* desk_2 = controller->desks()[1].get();
  MoveWindowFromActiveDeskTo(app, desk_2);
  VerifyViewVisibility(browser, true);
  VerifyViewVisibility(pinned, true);
  VerifyViewVisibility(app, visible_in_per_desk_shelf);

  // Move the pinned app and the browser window, they should remain visible on
  // the shelf even though they're on an inactive desk now.
  MoveWindowFromActiveDeskTo(pinned, desk_2);
  MoveWindowFromActiveDeskTo(browser, desk_2);
  VerifyViewVisibility(browser, true);
  VerifyViewVisibility(pinned, true);
  VerifyViewVisibility(app, visible_in_per_desk_shelf);
}

TEST_P(PerDeskShelfTest, DeskSwitching) {
  // Create two more desks, so total is three.
  auto* controller = DesksController::Get();
  NewDesk();
  NewDesk();

  // On desk_1, create a browser and a normal app.
  auto win0 = CreateAppWithShelfItem(ShelfItemType::TYPE_BROWSER_SHORTCUT);
  aura::Window* browser = win0.get();
  auto win1 = CreateAppWithShelfItem(ShelfItemType::TYPE_APP);
  aura::Window* app1 = win1.get();

  // Switch to desk_2, only the browser app should be visible on the shelf if
  // the feature is enabled.
  const bool visible_in_per_desk_shelf = IsPerDeskShelfEnabled() ? false : true;
  Desk* desk_2 = controller->desks()[1].get();
  ActivateDesk(desk_2);
  VerifyViewVisibility(browser, true);
  VerifyViewVisibility(app1, visible_in_per_desk_shelf);

  // On desk_2, create a pinned app.
  auto win2 = CreateAppWithShelfItem(ShelfItemType::TYPE_PINNED_APP);
  aura::Window* pinned = win2.get();
  VerifyViewVisibility(pinned, true);

  // Switch to desk_3, only the browser and the pinned app should be visible on
  // the shelf if the feature is enabled.
  Desk* desk_3 = controller->desks()[2].get();
  ActivateDesk(desk_3);
  VerifyViewVisibility(browser, true);
  VerifyViewVisibility(app1, visible_in_per_desk_shelf);
  VerifyViewVisibility(pinned, true);

  // On desk_3, create a normal app, then switch back to desk_1. app1 should
  // show again on the shelf.
  auto win3 = CreateAppWithShelfItem(ShelfItemType::TYPE_APP);
  aura::Window* app2 = win3.get();
  Desk* desk_1 = controller->desks()[0].get();
  ActivateDesk(desk_1);
  VerifyViewVisibility(browser, true);
  VerifyViewVisibility(app1, true);
  VerifyViewVisibility(pinned, true);
  VerifyViewVisibility(app2, visible_in_per_desk_shelf);
}

TEST_P(PerDeskShelfTest, RemoveInactiveDesk) {
  auto* controller = DesksController::Get();
  NewDesk();

  // On desk_1, create two apps.
  auto win0 = CreateAppWithShelfItem(ShelfItemType::TYPE_APP);
  aura::Window* app1 = win0.get();
  auto win1 = CreateAppWithShelfItem(ShelfItemType::TYPE_APP);
  aura::Window* app2 = win1.get();

  // Switch to desk_2, no apps should show on the shelf if the feature is
  // enabled.
  const bool visible_in_per_desk_shelf = IsPerDeskShelfEnabled() ? false : true;
  Desk* desk_2 = controller->desks()[1].get();
  ActivateDesk(desk_2);
  VerifyViewVisibility(app1, visible_in_per_desk_shelf);
  VerifyViewVisibility(app2, visible_in_per_desk_shelf);

  // Remove desk_1 (inactive), apps should now show on the shelf.
  Desk* desk_1 = controller->desks()[0].get();
  RemoveDesk(desk_1);
  VerifyViewVisibility(app1, true);
  VerifyViewVisibility(app2, true);
}

TEST_P(PerDeskShelfTest, RemoveActiveDesk) {
  auto* controller = DesksController::Get();
  NewDesk();

  // On desk_1, create an app.
  auto win0 = CreateAppWithShelfItem(ShelfItemType::TYPE_APP);
  aura::Window* app1 = win0.get();

  // Switch to desk_2, no apps should show on the shelf if the feature is
  // enabled.
  const bool visible_in_per_desk_shelf = IsPerDeskShelfEnabled() ? false : true;
  Desk* desk_2 = controller->desks()[1].get();
  ActivateDesk(desk_2);
  VerifyViewVisibility(app1, visible_in_per_desk_shelf);

  // Create another app on desk_2.
  auto win1 = CreateAppWithShelfItem(ShelfItemType::TYPE_APP);
  aura::Window* app2 = win1.get();

  // Remove desk_2 (active), all apps should now show on the shelf.
  RemoveDesk(desk_2);
  VerifyViewVisibility(app1, true);
  VerifyViewVisibility(app2, true);
}

// Tests desks name nudges, i.e. when a user creates a new desk, focus + clear
// the new desk's renaming textfield.
TEST_F(DesksTest, NameNudges) {
  // Make sure the display is large enough to hold the max number of desks.
  UpdateDisplay("1200x800");
  auto* controller = DesksController::Get();

  // Start overview.
  EnterOverview();
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Hover over the new desk button.
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view->IsZeroState());

  auto* event_generator = GetEventGenerator();
  ClickOnView(desks_bar_view->zero_state_default_desk_button(),
              event_generator);
  EXPECT_EQ(1u, desks_bar_view->mini_views().size());

  auto* new_desk_button =
      desks_bar_view->expanded_state_new_desk_button()->new_desk_button();
  EXPECT_TRUE(new_desk_button->GetEnabled());

  // Click on the new desk button until the max number of desks is created. Each
  // time a new desk is created the new desk's name view should have focus, be
  // empty and have its accessible name set to the default desk name. Also, the
  // previous desk should be left with a default name.
  for (size_t i = 1; i < desks_util::kMaxNumberOfDesks; ++i) {
    ClickOnView(new_desk_button, event_generator);
    auto* desk_name_view = desks_bar_view->mini_views()[i]->desk_name_view();
    EXPECT_TRUE(desk_name_view->HasFocus());
    EXPECT_EQ(std::u16string(), controller->desks()[i]->name());
    EXPECT_EQ(DesksController::GetDeskDefaultName(i),
              desk_name_view->GetAccessibleName());
    EXPECT_EQ(DesksController::GetDeskDefaultName(i - 1),
              controller->desks()[i - 1]->name());
  }
}

// Tests that name nudges works with multiple displays. When a user
// clicks/touches the new desk button, the newly created DeskNameView that
// resides on the same DesksBarView as the clicked button should be focused.
// See crbug.com/1206013.
TEST_F(DesksTest, NameNudgesMultiDisplay) {
  UpdateDisplay("800x800,800x800");

  // Start overview.
  EnterOverview();
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Retrieve the desks bar view for each root window.
  auto root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  const auto* desks_bar_view_1 =
      GetOverviewGridForRoot(root_windows[0])->desks_bar_view();
  const auto* desks_bar_view_2 =
      GetOverviewGridForRoot(root_windows[1])->desks_bar_view();
  ASSERT_TRUE(desks_bar_view_1->IsZeroState());
  ASSERT_TRUE(desks_bar_view_2->IsZeroState());

  // Click on the zero state default desk button for the second root window.
  auto* zero_state_default_desk_button_2 =
      desks_bar_view_2->zero_state_default_desk_button();
  EXPECT_TRUE(zero_state_default_desk_button_2->GetEnabled());
  auto* event_generator = GetEventGenerator();
  ClickOnView(zero_state_default_desk_button_2, event_generator);

  // The desk bar should not be in the zero state anymore and the existing
  // desk's name view should be focused, but not cleared.
  EXPECT_FALSE(desks_bar_view_2->IsZeroState());
  EXPECT_EQ(1u, desks_bar_view_2->mini_views().size());
  auto* desk_name_view_2 = desks_bar_view_2->mini_views()[0]->desk_name_view();
  EXPECT_TRUE(desk_name_view_2->HasFocus());
  EXPECT_EQ(DesksController::GetDeskDefaultName(/*desk_index=*/0),
            desk_name_view_2->GetText());

  // Restart overview to reset the zero state.
  ExitOverview();
  EnterOverview();
  desks_bar_view_1 = GetOverviewGridForRoot(root_windows[0])->desks_bar_view();
  desks_bar_view_2 = GetOverviewGridForRoot(root_windows[1])->desks_bar_view();
  ASSERT_TRUE(desks_bar_view_1->IsZeroState());
  ASSERT_TRUE(desks_bar_view_2->IsZeroState());

  // Click on the new desk button on the first root window.
  auto* new_desk_button_1 = desks_bar_view_1->zero_state_new_desk_button();
  EXPECT_TRUE(new_desk_button_1->GetEnabled());
  ClickOnView(new_desk_button_1, event_generator);

  // There should be 2 desks now and the name view on the first root window
  // should be focused. Each new name view on the 2 root windows should be
  // empty.
  EXPECT_EQ(2u, desks_bar_view_1->mini_views().size());
  auto* desk_name_view_1 = desks_bar_view_1->mini_views()[1]->desk_name_view();
  desk_name_view_2 = desks_bar_view_2->mini_views()[1]->desk_name_view();
  EXPECT_TRUE(desk_name_view_1->HasFocus());
  EXPECT_FALSE(desk_name_view_2->HasFocus());
  EXPECT_EQ(std::u16string(), desk_name_view_1->GetText());
  EXPECT_EQ(std::u16string(), desk_name_view_2->GetText());

  // Tap on the new desk button on the second root window.
  auto* new_desk_button_2 = desks_bar_view_2->expanded_state_new_desk_button();
  EXPECT_TRUE(new_desk_button_2->GetEnabled());
  GestureTapOnView(new_desk_button_2, event_generator);

  // There should be 3 desks now and the name view on the second root window
  // should be focused. Each new name view on the 2 root windows should be
  // empty.
  EXPECT_EQ(3u, desks_bar_view_1->mini_views().size());
  desk_name_view_1 = desks_bar_view_1->mini_views()[2]->desk_name_view();
  desk_name_view_2 = desks_bar_view_2->mini_views()[2]->desk_name_view();
  EXPECT_FALSE(desk_name_view_1->HasFocus());
  EXPECT_TRUE(desk_name_view_2->HasFocus());
  EXPECT_EQ(std::u16string(), desk_name_view_1->GetText());
  EXPECT_EQ(std::u16string(), desk_name_view_2->GetText());
}

// Tests that when a user has a `DeskNameView` focused and clicks within the
// overview grid, the `DeskNameView` loses focus and the overview grid is not
// closed.
TEST_F(DesksTest, ClickingOverviewGridUnfocusesDeskNameView) {
  // Create a second desk so we don't start in zero state.
  NewDesk();

  // Start overview.
  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Focus on a `DeskNameView`.
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
  auto* desk_name_view = desks_bar_view->mini_views()[0]->desk_name_view();
  desk_name_view->RequestFocus();
  ASSERT_TRUE(desk_name_view->HasFocus());

  // Click the center of the overview grid. This should not close overview mode
  // and should remove focus from the focused `desk_name_view`.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(overview_grid->bounds().CenterPoint());
  event_generator->ClickLeftButton();
  EXPECT_FALSE(desk_name_view->HasFocus());
  EXPECT_TRUE(overview_controller->InOverviewSession());
}

TEST_F(DesksTest, ScrollableDesks) {
  UpdateDisplay("201x400");
  EnterOverview();
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  auto* root_window = Shell::GetPrimaryRootWindow();
  const auto* desks_bar_view =
      GetOverviewGridForRoot(root_window)->desks_bar_view();
  ASSERT_TRUE(desks_bar_view->IsZeroState());
  auto* event_generator = GetEventGenerator();
  ClickOnView(desks_bar_view->zero_state_default_desk_button(),
              event_generator);
  EXPECT_EQ(1u, desks_bar_view->mini_views().size());

  auto* new_desk_button =
      desks_bar_view->expanded_state_new_desk_button()->new_desk_button();

  // Set the scroll delta large enough to make sure the desks bar can be
  // scrolled to the end each time.
  const int x_scroll_delta = 200;
  gfx::Rect display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window);
  for (size_t i = 1; i < desks_util::kMaxNumberOfDesks; i++) {
    gfx::Rect new_desk_button_bounds = new_desk_button->GetBoundsInScreen();
    EXPECT_TRUE(display_bounds.Contains(new_desk_button_bounds));
    ClickOnView(new_desk_button, event_generator);
    // Scroll right to make sure the new desk button is always inside the
    // display.
    event_generator->MoveMouseWheel(-x_scroll_delta, 0);
  }

  auto* controller = DesksController::Get();
  EXPECT_EQ(desks_util::kMaxNumberOfDesks, controller->desks().size());
  EXPECT_FALSE(controller->CanCreateDesks());

  EXPECT_TRUE(display_bounds.Contains(new_desk_button->GetBoundsInScreen()));
  EXPECT_FALSE(display_bounds.Contains(
      desks_bar_view->mini_views()[0]->GetBoundsInScreen()));
  event_generator->MoveMouseWheel(x_scroll_delta, 0);
  // Tests that scroll to the left will put the first desk inside the display.
  EXPECT_TRUE(display_bounds.Contains(
      desks_bar_view->mini_views()[0]->GetBoundsInScreen()));
  EXPECT_FALSE(display_bounds.Contains(new_desk_button->GetBoundsInScreen()));
}

// Tests the visibility of the scroll buttons and behavior while clicking the
// corresponding scroll button.
TEST_F(DesksTest, ScrollButtonsVisibility) {
  UpdateDisplay("501x600");
  for (size_t i = 1; i < desks_util::kMaxNumberOfDesks; i++)
    NewDesk();

  EXPECT_EQ(DesksController::Get()->desks().size(),
            desks_util::kMaxNumberOfDesks);
  auto window = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  TabletModeControllerTestApi().EnterTabletMode();
  // Snap the window to left and then right side of the display should enter
  // overview mode.
  auto* root_window = Shell::GetPrimaryRootWindow();
  SplitViewController::Get(root_window)
      ->SnapWindow(window.get(), SplitViewController::LEFT);
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  auto* desks_bar = GetOverviewGridForRoot(root_window)->desks_bar_view();
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(desks_bar->GetBoundsInScreen().CenterPoint());

  // Set the scroll delta large enough to make sure the desks bar can be
  // scrolled to the end each time.
  const int x_scroll_delta = 500;
  // Left scroll button should be hidden and right scroll button should be
  // visible while at the start position.
  event_generator->MoveMouseWheel(x_scroll_delta, 0);
  EXPECT_FALSE(DesksTestApi::GetDesksBarLeftScrollButton()->GetVisible());
  EXPECT_TRUE(DesksTestApi::GetDesksBarRightScrollButton()->GetVisible());

  // Click the right scroll button should scroll to the next page. And left and
  // right scroll buttons should both be visible while at the middle position.
  ClickOnView(DesksTestApi::GetDesksBarRightScrollButton(), event_generator);
  EXPECT_TRUE(DesksTestApi::GetDesksBarLeftScrollButton()->GetVisible());
  EXPECT_TRUE(DesksTestApi::GetDesksBarRightScrollButton()->GetVisible());

  // Click the left scroll button should scroll to the previous page. In this
  // case, it will scroll back to the start position and left scroll button
  // should be hidden and right scroll button should be visible.
  ClickOnView(DesksTestApi::GetDesksBarLeftScrollButton(), event_generator);
  EXPECT_FALSE(DesksTestApi::GetDesksBarLeftScrollButton()->GetVisible());
  EXPECT_TRUE(DesksTestApi::GetDesksBarRightScrollButton()->GetVisible());

  // Left scroll button should be visible and right scroll button should be
  // hidden while at the end position.
  event_generator->MoveMouseTo(desks_bar->GetBoundsInScreen().CenterPoint());
  event_generator->MoveMouseWheel(-x_scroll_delta, 0);
  EXPECT_TRUE(DesksTestApi::GetDesksBarLeftScrollButton()->GetVisible());
  EXPECT_FALSE(DesksTestApi::GetDesksBarRightScrollButton()->GetVisible());
}

TEST_F(DesksTest, GradientsVisibility) {
  // Set a flat display size to make sure there are multiple pages in the desks
  // bar with maximum number of desks.
  UpdateDisplay("800x150");
  const size_t max_desks_size = desks_util::kMaxNumberOfDesks;
  for (size_t i = 1; i < max_desks_size; i++)
    NewDesk();

  EnterOverview();
  auto* desks_bar =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();

  auto* left_button = DesksTestApi::GetDesksBarLeftScrollButton();
  auto* right_button = DesksTestApi::GetDesksBarRightScrollButton();

  // Only right graident is visible while at the first page.
  auto* scroll_view = DesksTestApi::GetDesksBarScrollView();
  EXPECT_EQ(0, scroll_view->GetVisibleRect().x());
  EXPECT_FALSE(left_button->GetVisible());
  EXPECT_FALSE(DesksTestApi::IsDesksBarLeftGradientVisible());
  EXPECT_TRUE(right_button->GetVisible());
  EXPECT_TRUE(DesksTestApi::IsDesksBarRightGradientVisible());

  // Both left and right gradients should be visible while during scroll.
  const gfx::Point center_point = desks_bar->bounds().CenterPoint();
  ui::GestureEvent scroll_begin(
      center_point.x(), center_point.y(), ui::EF_NONE, base::TimeTicks::Now(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 1, 0));
  scroll_view->OnGestureEvent(&scroll_begin);
  ui::GestureEvent scroll_update(
      center_point.x(), center_point.y(), ui::EF_NONE, base::TimeTicks::Now(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, -100, 0));
  scroll_view->OnGestureEvent(&scroll_update);
  EXPECT_TRUE(scroll_view->is_scrolling());
  EXPECT_TRUE(left_button->GetVisible());
  EXPECT_TRUE(DesksTestApi::IsDesksBarLeftGradientVisible());
  EXPECT_TRUE(right_button->GetVisible());
  EXPECT_TRUE(DesksTestApi::IsDesksBarRightGradientVisible());

  // The gradient should be hidden if the corresponding scroll button is
  // invisible even though it is during scroll.
  ui::GestureEvent second_scroll_update(
      center_point.x() - 100, center_point.y(), ui::EF_NONE,
      base::TimeTicks::Now(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 100, 0));
  scroll_view->OnGestureEvent(&second_scroll_update);
  EXPECT_TRUE(scroll_view->is_scrolling());
  EXPECT_FALSE(left_button->GetVisible());
  EXPECT_FALSE(DesksTestApi::IsDesksBarLeftGradientVisible());
  EXPECT_TRUE(right_button->GetVisible());
  EXPECT_TRUE(DesksTestApi::IsDesksBarRightGradientVisible());

  ui::GestureEvent scroll_end(
      center_point.x(), center_point.y(), ui::EF_NONE, base::TimeTicks::Now(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));
  scroll_view->OnGestureEvent(&scroll_end);
  EXPECT_FALSE(scroll_view->is_scrolling());
  EXPECT_FALSE(left_button->GetVisible());
  EXPECT_FALSE(DesksTestApi::IsDesksBarLeftGradientVisible());
  EXPECT_TRUE(right_button->GetVisible());
  EXPECT_TRUE(DesksTestApi::IsDesksBarRightGradientVisible());

  // Only right gradient should be shown at the middle page when it is not
  // during scroll even though the left scroll button is visible.
  auto* event_generator = GetEventGenerator();
  ClickOnView(right_button, event_generator);
  EXPECT_TRUE(left_button->GetVisible());
  EXPECT_FALSE(DesksTestApi::IsDesksBarLeftGradientVisible());
  EXPECT_TRUE(right_button->GetVisible());
  EXPECT_TRUE(DesksTestApi::IsDesksBarRightGradientVisible());

  // Only the left gradient should be shown at the last page.
  while (right_button->GetVisible())
    ClickOnView(right_button, event_generator);

  EXPECT_EQ(scroll_view->contents()->bounds().width() - scroll_view->width(),
            scroll_view->GetVisibleRect().x());
  EXPECT_TRUE(left_button->GetVisible());
  EXPECT_TRUE(DesksTestApi::IsDesksBarLeftGradientVisible());
  EXPECT_FALSE(right_button->GetVisible());
  EXPECT_FALSE(DesksTestApi::IsDesksBarRightGradientVisible());
}

// Tests the behavior when long press on the scroll buttons.
TEST_F(DesksTest, ContinueScrollBar) {
  // Make a flat long window to generate multiple pages on desks bar.
  UpdateDisplay("800x150");
  const size_t max_desks_size = desks_util::kMaxNumberOfDesks;
  for (size_t i = 1; i < max_desks_size; i++)
    NewDesk();

  auto* desks_controller = DesksController::Get();
  EXPECT_EQ(desks_controller->desks().size(), max_desks_size);
  EnterOverview();
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  auto* desks_bar =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();

  views::ScrollView* scroll_view = DesksTestApi::GetDesksBarScrollView();
  const int page_size = scroll_view->width();
  const auto mini_views = desks_bar->mini_views();
  const int mini_view_width = mini_views[0]->bounds().width();
  int desks_in_one_page = page_size / mini_view_width;
  float fractional_page = static_cast<float>(page_size % mini_view_width) /
                          static_cast<float>(mini_view_width);
  if (fractional_page > 0.5)
    desks_in_one_page++;

  int current_index = 0;
  ScrollArrowButton* left_button = DesksTestApi::GetDesksBarLeftScrollButton();
  ScrollArrowButton* right_button =
      DesksTestApi::GetDesksBarRightScrollButton();

  // At first, left scroll button is hidden and right scroll button is visible.
  EXPECT_FALSE(left_button->GetVisible());
  EXPECT_TRUE(right_button->GetVisible());

  // Press on the right scroll button by mouse should scroll to the next page.
  // And the final scroll position should be adjusted to make sure the desk
  // preview will not be cropped.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(right_button->GetBoundsInScreen().CenterPoint());
  event_generator->PressLeftButton();
  current_index += desks_in_one_page;
  EXPECT_EQ(scroll_view->GetVisibleRect().x(),
            mini_views[current_index]->bounds().x());

  // Both scroll buttons should be visible.
  EXPECT_TRUE(left_button->GetVisible());
  EXPECT_TRUE(right_button->GetVisible());

  // Wait for 1s, there will be another scroll.
  WaitForMilliseconds(1000);
  current_index += desks_in_one_page;
  EXPECT_EQ(scroll_view->GetVisibleRect().x(),
            mini_views[current_index]->bounds().x());

  // Wait for 1s, it will scroll to the maximum offset. Scroll ends.
  WaitForMilliseconds(1000);
  EXPECT_EQ(scroll_view->GetVisibleRect().x(),
            scroll_view->contents()->width() - page_size);

  // Left scroll button should be visible and right scroll button should be
  // hidden.
  EXPECT_TRUE(left_button->GetVisible());
  EXPECT_FALSE(right_button->GetVisible());

  event_generator->ReleaseLeftButton();

  // Press on left scroll button by gesture should scroll to the previous page.
  // And the final scroll position should also be adjusted while scrolling to
  // previous page to make sure the desk preview will not be cropped.
  event_generator->MoveTouch(left_button->GetBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  current_index -= desks_in_one_page;
  EXPECT_EQ(scroll_view->GetVisibleRect().x(),
            mini_views[current_index]->bounds().x());

  // Wait for 1s, there is another scroll.
  WaitForMilliseconds(1000);
  current_index -= desks_in_one_page;
  EXPECT_EQ(scroll_view->GetVisibleRect().x(),
            mini_views[current_index]->bounds().x());

  event_generator->ReleaseTouch();
}

// Tests that change the focused mini view should scroll the desks bar and put
// the focused mini view inside the visible bounds.
TEST_F(DesksTest, FocusedMiniViewIsVisible) {
  UpdateDisplay("501x600");
  for (size_t i = 1; i < desks_util::kMaxNumberOfDesks; i++)
    NewDesk();

  EXPECT_EQ(DesksController::Get()->desks().size(),
            desks_util::kMaxNumberOfDesks);
  auto window = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  TabletModeControllerTestApi().EnterTabletMode();
  // Snap the window to left and then right side of the display should enter
  // overview mode.
  auto* root_window = Shell::GetPrimaryRootWindow();
  SplitViewController::Get(root_window)
      ->SnapWindow(window.get(), SplitViewController::LEFT);
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  auto* desks_bar = GetOverviewGridForRoot(root_window)->desks_bar_view();
  auto mini_views = desks_bar->mini_views();
  ASSERT_EQ(mini_views.size(), desks_util::kMaxNumberOfDesks);
  // Traverse all the desks mini views from left to right.
  for (size_t i = 0; i < desks_util::kMaxNumberOfDesks; i++) {
    // Move the focus to mini view.
    SendKey(ui::VKEY_TAB);
    EXPECT_TRUE(
        DesksTestApi::GetDesksBarScrollView()->GetVisibleRect().Contains(
            mini_views[i]->bounds()));
    // Move the focus to the mini view's associated name view.
    SendKey(ui::VKEY_TAB);
  }

  // Traverse from all the desk mini views from right to left.
  for (size_t i = desks_util::kMaxNumberOfDesks - 1; i > 0; i--) {
    // Move the focus from desk name view to the associated mini view.
    SendKey(ui::VKEY_LEFT);
    // Move the focus to previous mini view's name view.
    SendKey(ui::VKEY_LEFT);
    EXPECT_TRUE(
        DesksTestApi::GetDesksBarScrollView()->GetVisibleRect().Contains(
            mini_views[i - 1]->bounds()));
  }
}

// Tests that the bounds of a window that is visible on all desks is shared
// across desks.
TEST_F(DesksTest, VisibleOnAllDesksGlobalBounds) {
  auto* controller = DesksController::Get();
  NewDesk();
  const Desk* desk_1 = controller->desks()[0].get();
  const Desk* desk_2 = controller->desks()[1].get();
  auto* root = Shell::GetPrimaryRootWindow();
  const gfx::Rect window_initial_bounds(1, 1, 200, 200);
  const gfx::Rect window_moved_bounds(200, 200, 250, 250);

  auto window = CreateAppWindow(window_initial_bounds);
  auto* widget = views::Widget::GetWidgetForNativeWindow(window.get());
  ASSERT_EQ(window_initial_bounds, window->bounds());

  // Assign |window| to all desks. It shouldn't change bounds.
  widget->SetVisibleOnAllWorkspaces(true);
  ASSERT_TRUE(window->GetProperty(aura::client::kVisibleOnAllWorkspacesKey));
  EXPECT_EQ(window_initial_bounds, window->bounds());
  EXPECT_EQ(1u, controller->visible_on_all_desks_windows().size());

  // Move to desk 2. The only window on the new desk should be |window|
  // and it should have the same bounds.
  ActivateDesk(desk_2);
  auto desk_2_children = desk_2->GetDeskContainerForRoot(root)->children();
  EXPECT_EQ(1u, desk_2_children.size());
  EXPECT_EQ(window.get(), desk_2_children[0]);
  EXPECT_EQ(window_initial_bounds, window->bounds());

  // Change |window|'s bounds and move to desk 1. It should retain its moved
  // bounds.
  window->SetBounds(window_moved_bounds);
  EXPECT_EQ(window_moved_bounds, window->bounds());
  ActivateDesk(desk_1);
  auto desk_1_children = desk_1->GetDeskContainerForRoot(root)->children();
  EXPECT_EQ(1u, desk_1_children.size());
  EXPECT_EQ(window.get(), desk_1_children[0]);
  EXPECT_EQ(window_moved_bounds, window->bounds());
}

// Tests that the z-ordering of windows that are visible on all desks respects
// its global MRU ordering.
TEST_F(DesksTest, VisibleOnAllDesksGlobalZOrder) {
  auto* controller = DesksController::Get();
  NewDesk();
  const Desk* desk_1 = controller->desks()[0].get();
  const Desk* desk_2 = controller->desks()[1].get();
  auto* root = Shell::GetPrimaryRootWindow();

  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  auto win1 = CreateAppWindow(gfx::Rect(1, 1, 150, 150));
  auto win2 = CreateAppWindow(gfx::Rect(2, 2, 200, 200));
  auto* widget0 = views::Widget::GetWidgetForNativeWindow(win0.get());
  auto* widget1 = views::Widget::GetWidgetForNativeWindow(win1.get());
  auto* widget2 = views::Widget::GetWidgetForNativeWindow(win2.get());
  ASSERT_TRUE(IsStackedBelow(win0.get(), win1.get()));
  ASSERT_TRUE(IsStackedBelow(win1.get(), win2.get()));

  // Assign |win1| to all desks. It shouldn't change stacking order.
  widget1->SetVisibleOnAllWorkspaces(true);
  ASSERT_TRUE(win1->GetProperty(aura::client::kVisibleOnAllWorkspacesKey));
  EXPECT_TRUE(IsStackedBelow(win0.get(), win1.get()));
  EXPECT_TRUE(IsStackedBelow(win1.get(), win2.get()));
  EXPECT_EQ(1u, controller->visible_on_all_desks_windows().size());

  // Move to desk 2. The only window on the new desk should be |win1|.
  ActivateDesk(desk_2);
  auto desk_2_children = desk_2->GetDeskContainerForRoot(root)->children();
  EXPECT_EQ(1u, desk_2_children.size());
  EXPECT_EQ(win1.get(), desk_2_children[0]);

  // Move to desk 1. Since |win1|  was activated last, |win1| should be on top
  // of the stacking order.
  ActivateDesk(desk_1);
  auto desk_1_children = desk_1->GetDeskContainerForRoot(root)->children();
  EXPECT_EQ(3u, desk_1_children.size());
  EXPECT_TRUE(IsStackedBelow(win0.get(), win2.get()));
  EXPECT_TRUE(IsStackedBelow(win2.get(), win1.get()));

  // Assign all the other windows and rearrange the order by activating the
  // windows.
  widget0->SetVisibleOnAllWorkspaces(true);
  widget2->SetVisibleOnAllWorkspaces(true);
  ASSERT_TRUE(win0->GetProperty(aura::client::kVisibleOnAllWorkspacesKey));
  ASSERT_TRUE(win1->GetProperty(aura::client::kVisibleOnAllWorkspacesKey));
  wm::ActivateWindow(win2.get());
  wm::ActivateWindow(win1.get());
  wm::ActivateWindow(win0.get());
  EXPECT_TRUE(IsStackedBelow(win2.get(), win1.get()));
  EXPECT_TRUE(IsStackedBelow(win1.get(), win0.get()));
  EXPECT_EQ(3u, controller->visible_on_all_desks_windows().size());

  // Move to desk 2. All the windows should move to the new desk and maintain
  // their order.
  ActivateDesk(desk_2);
  desk_2_children = desk_2->GetDeskContainerForRoot(root)->children();
  EXPECT_EQ(3u, desk_2_children.size());
  EXPECT_TRUE(IsStackedBelow(win2.get(), win1.get()));
  EXPECT_TRUE(IsStackedBelow(win1.get(), win0.get()));
}

// Tests the behavior of windows that are visible on all desks when the active
// desk is removed.
TEST_F(DesksTest, VisibleOnAllDesksActiveDeskRemoval) {
  auto* controller = DesksController::Get();
  NewDesk();
  const Desk* desk_1 = controller->desks()[0].get();
  const Desk* desk_2 = controller->desks()[1].get();
  auto* root = Shell::GetPrimaryRootWindow();

  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  auto win1 = CreateAppWindow(gfx::Rect(1, 1, 150, 150));
  auto* widget0 = views::Widget::GetWidgetForNativeWindow(win0.get());
  auto* widget1 = views::Widget::GetWidgetForNativeWindow(win1.get());

  // Assign |win0| and |win1| to all desks.
  widget0->SetVisibleOnAllWorkspaces(true);
  widget1->SetVisibleOnAllWorkspaces(true);
  ASSERT_TRUE(win0->GetProperty(aura::client::kVisibleOnAllWorkspacesKey));
  ASSERT_TRUE(win1->GetProperty(aura::client::kVisibleOnAllWorkspacesKey));

  // Remove the active desk. The visible on all desks windows should be on
  // |desk_2|.
  RemoveDesk(desk_1);
  auto desk_2_children = desk_2->GetDeskContainerForRoot(root)->children();
  EXPECT_EQ(2u, desk_2_children.size());
  EXPECT_TRUE(IsStackedBelow(win0.get(), win1.get()));
  EXPECT_EQ(2u, controller->visible_on_all_desks_windows().size());
}

// Tests the behavior of a minimized window that is visible on all desks.
TEST_F(DesksTest, VisibleOnAllDesksMinimizedWindow) {
  auto* controller = DesksController::Get();
  NewDesk();
  const Desk* desk_2 = controller->desks()[1].get();
  auto* root = Shell::GetPrimaryRootWindow();
  auto window = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  auto* widget = views::Widget::GetWidgetForNativeWindow(window.get());

  // Minimize |window| and then assign it to all desks. This shouldn't
  // unminimize it.
  auto* window_state = WindowState::Get(window.get());
  window_state->Minimize();
  ASSERT_TRUE(window_state->IsMinimized());
  widget->SetVisibleOnAllWorkspaces(true);
  ASSERT_TRUE(window->GetProperty(aura::client::kVisibleOnAllWorkspacesKey));
  EXPECT_TRUE(window_state->IsMinimized());

  // Switch desks. |window| should be on the newly active desk and should still
  // be minimized.
  ActivateDesk(desk_2);
  auto desk_2_children = desk_2->GetDeskContainerForRoot(root)->children();
  EXPECT_EQ(1u, desk_2_children.size());
  EXPECT_EQ(window.get(), desk_2_children[0]);
  EXPECT_TRUE(window_state->IsMinimized());
}

// Tests the behavior of a window that is visible on all desks when a user tries
// to move it to another desk using drag and drop (overview mode).
TEST_F(DesksTest, VisibleOnAllDesksMoveWindowToDeskViaDragAndDrop) {
  auto* controller = DesksController::Get();
  auto* root = Shell::GetPrimaryRootWindow();
  NewDesk();
  const Desk* desk_1 = controller->desks()[0].get();
  const Desk* desk_2 = controller->desks()[1].get();

  auto window = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  auto* widget = views::Widget::GetWidgetForNativeWindow(window.get());

  // Assign |window| to all desks.
  widget->SetVisibleOnAllWorkspaces(true);
  ASSERT_TRUE(window->GetProperty(aura::client::kVisibleOnAllWorkspacesKey));

  // Try to move |window| to |desk_2| via drag and drop. It should not be moved.
  EXPECT_FALSE(controller->MoveWindowFromActiveDeskTo(
      window.get(), const_cast<Desk*>(desk_2), root,
      DesksMoveWindowFromActiveDeskSource::kDragAndDrop));
  EXPECT_TRUE(desks_util::BelongsToActiveDesk(window.get()));
  EXPECT_EQ(1u, controller->visible_on_all_desks_windows().size());
  EXPECT_TRUE(window->GetProperty(aura::client::kVisibleOnAllWorkspacesKey));
  EXPECT_TRUE(base::Contains(desk_1->windows(), window.get()));
}

// Tests the behavior of a window that is visible on all desks when a user tries
// to move it to another desk using keyboard shortcuts.
TEST_F(DesksTest, VisibleOnAllDesksMoveWindowToDeskViaShortcuts) {
  auto* controller = DesksController::Get();
  auto* root = Shell::GetPrimaryRootWindow();
  NewDesk();
  const Desk* desk_2 = controller->desks()[1].get();

  auto window = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  auto* widget = views::Widget::GetWidgetForNativeWindow(window.get());

  // Assign |window| to all desks.
  widget->SetVisibleOnAllWorkspaces(true);
  ASSERT_TRUE(window->GetProperty(aura::client::kVisibleOnAllWorkspacesKey));

  // Move |window| to |desk_2| via keyboard shortcut. It should be on |desk_2|
  // and should no longer be visible on all desks.
  EXPECT_TRUE(controller->MoveWindowFromActiveDeskTo(
      window.get(), const_cast<Desk*>(desk_2), root,
      DesksMoveWindowFromActiveDeskSource::kShortcut));
  EXPECT_FALSE(desks_util::BelongsToActiveDesk(window.get()));
  EXPECT_EQ(0u, controller->visible_on_all_desks_windows().size());
  EXPECT_FALSE(window->GetProperty(aura::client::kVisibleOnAllWorkspacesKey));
  EXPECT_TRUE(base::Contains(desk_2->windows(), window.get()));
}

// Tests the behavior of a window that is visible on all desks when a user tries
// to move it using the context menu.
TEST_F(DesksTest, VisibleOnAllDesksMoveWindowToDeskViaContextMenu) {
  auto* controller = DesksController::Get();
  NewDesk();
  const Desk* desk_2 = controller->desks()[1].get();

  auto window = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  auto* widget = views::Widget::GetWidgetForNativeWindow(window.get());

  // Assign |window| to all desks.
  widget->SetVisibleOnAllWorkspaces(true);
  ASSERT_TRUE(window->GetProperty(aura::client::kVisibleOnAllWorkspacesKey));

  // Move |window| to |desk_2| via keyboard shortcut. It should be on |desk_2|
  // and should no longer be visible on all desks.
  controller->SendToDeskAtIndex(window.get(), controller->GetDeskIndex(desk_2));
  EXPECT_FALSE(desks_util::BelongsToActiveDesk(window.get()));
  EXPECT_EQ(0u, controller->visible_on_all_desks_windows().size());
  EXPECT_FALSE(window->GetProperty(aura::client::kVisibleOnAllWorkspacesKey));
  EXPECT_TRUE(base::Contains(desk_2->windows(), window.get()));
}

// Tests that when a window that is visible on all desks is destroyed it is
// removed from DesksController.visible_on_all_desks_windows_.
TEST_F(DesksTest, VisibleOnAllDesksWindowDestruction) {
  auto* controller = DesksController::Get();
  NewDesk();
  const Desk* desk_1 = controller->desks()[0].get();
  auto* root = Shell::GetPrimaryRootWindow();

  auto window = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  auto* widget = views::Widget::GetWidgetForNativeWindow(window.get());

  // Assign |window| to all desks.
  widget->SetVisibleOnAllWorkspaces(true);
  ASSERT_TRUE(window->GetProperty(aura::client::kVisibleOnAllWorkspacesKey));
  EXPECT_EQ(1u, controller->visible_on_all_desks_windows().size());
  EXPECT_EQ(1u, desk_1->GetDeskContainerForRoot(root)->children().size());

  // Destroy |window|. It should be removed from
  // DesksController.visible_on_all_desks_windows_.
  window.reset();
  EXPECT_EQ(0u, controller->visible_on_all_desks_windows().size());
  EXPECT_EQ(0u, desk_1->GetDeskContainerForRoot(root)->children().size());
}

TEST_F(DesksTest, EnterOverviewWithCorrectDesksBarState) {
  auto* controller = DesksController::Get();
  ASSERT_EQ(1u, controller->desks().size());
  EnterOverview();

  auto* root_window = Shell::GetPrimaryRootWindow();
  auto* desks_bar_view = GetOverviewGridForRoot(root_window)->desks_bar_view();

  // Desks bar should stay in zero state if there is only one desk and no mini
  // view has been created.
  EXPECT_TRUE(desks_bar_view->IsZeroState());
  EXPECT_TRUE(desks_bar_view->mini_views().empty());

  // Click new desk button in the zero state bar to create a new desk.
  ClickOnView(desks_bar_view->zero_state_new_desk_button(),
              GetEventGenerator());
  ExitOverview();

  // Desks bar should not stay in zero state if there are more than one desks.
  EXPECT_EQ(2u, controller->desks().size());
  EnterOverview();
  desks_bar_view = GetOverviewGridForRoot(root_window)->desks_bar_view();
  EXPECT_EQ(2u, desks_bar_view->mini_views().size());
  EXPECT_FALSE(desks_bar_view->IsZeroState());
}

// Tests the behavior of desks bar zero state.
TEST_F(DesksTest, DesksBarZeroState) {
  EnterOverview();

  auto* root_window = Shell::GetPrimaryRootWindow();
  auto* desks_bar_view = GetOverviewGridForRoot(root_window)->desks_bar_view();
  ASSERT_TRUE(desks_bar_view->IsZeroState());
  ASSERT_TRUE(desks_bar_view->mini_views().empty());

  auto* event_generator = GetEventGenerator();
  auto* zero_state_default_desk_button =
      desks_bar_view->zero_state_default_desk_button();
  auto* zero_state_new_desk_button =
      desks_bar_view->zero_state_new_desk_button();
  ClickOnView(zero_state_default_desk_button, event_generator);
  // Click the zero state default desk button should switch to expanded desks
  // bar and focus on the default desk's name view. The two buttons in zero
  // state bar should also be hidden.
  EXPECT_FALSE(desks_bar_view->IsZeroState());
  EXPECT_EQ(1u, desks_bar_view->mini_views().size());
  EXPECT_TRUE(desks_bar_view->mini_views()[0]->desk_name_view()->HasFocus());
  EXPECT_FALSE(zero_state_default_desk_button->GetVisible());
  EXPECT_FALSE(zero_state_new_desk_button->GetVisible());

  ExitOverview();
  EnterOverview();
  desks_bar_view = GetOverviewGridForRoot(root_window)->desks_bar_view();
  ASSERT_TRUE(desks_bar_view->IsZeroState());

  zero_state_default_desk_button =
      desks_bar_view->zero_state_default_desk_button();
  zero_state_new_desk_button = desks_bar_view->zero_state_new_desk_button();
  ClickOnView(zero_state_new_desk_button, event_generator);
  // Click the zero state new desk button should switch to expand desks bar,
  // create a new desk and focus on the new created desk's name view. The two
  // buttons in zero state bar should also be hidden.
  EXPECT_FALSE(desks_bar_view->IsZeroState());
  EXPECT_EQ(2u, desks_bar_view->mini_views().size());
  EXPECT_TRUE(desks_bar_view->mini_views()[1]->desk_name_view()->HasFocus());
  EXPECT_FALSE(zero_state_default_desk_button->GetVisible());
  EXPECT_FALSE(zero_state_new_desk_button->GetVisible());

  // Should switch to zero state if there is only one desk after removing.
  CloseDeskFromMiniView(desks_bar_view->mini_views()[0], event_generator);
  EXPECT_TRUE(zero_state_default_desk_button->GetVisible());
  EXPECT_TRUE(zero_state_new_desk_button->GetVisible());
}

TEST_F(DesksTest, NewDeskButton) {
  auto* controller = DesksController::Get();
  EnterOverview();

  auto* root_window = Shell::GetPrimaryRootWindow();
  auto* desks_bar_view = GetOverviewGridForRoot(root_window)->desks_bar_view();

  // Click the default desk button in the zero state bar to switch to expanded
  // desks bar.
  auto* event_generator = GetEventGenerator();
  ClickOnView(desks_bar_view->zero_state_default_desk_button(),
              event_generator);
  auto* new_desk_button =
      desks_bar_view->expanded_state_new_desk_button()->new_desk_button();
  EXPECT_TRUE(new_desk_button->GetVisible());
  EXPECT_TRUE(new_desk_button->GetEnabled());

  for (size_t i = 1; i < desks_util::kMaxNumberOfDesks; i++)
    ClickOnView(new_desk_button, event_generator);
  // The new desk button should become disabled after maximum number of desks
  // have been created.
  EXPECT_FALSE(controller->CanCreateDesks());
  EXPECT_FALSE(new_desk_button->GetEnabled());

  // The new desk button should become enabled again after removing a desk.
  CloseDeskFromMiniView(desks_bar_view->mini_views()[0], event_generator);
  EXPECT_TRUE(controller->CanCreateDesks());
  EXPECT_TRUE(new_desk_button->GetEnabled());
}

TEST_F(DesksTest, ZeroStateDeskButtonText) {
  UpdateDisplay("1600x1200");
  EnterOverview();

  auto* root_window = Shell::GetPrimaryRootWindow();
  auto* desks_bar_view = GetOverviewGridForRoot(root_window)->desks_bar_view();
  ASSERT_TRUE(desks_bar_view->IsZeroState());
  // Show the default name "Desk 1" while initializing the desks bar at the
  // first time.
  EXPECT_EQ(u"Desk 1",
            desks_bar_view->zero_state_default_desk_button()->GetText());

  auto* event_generator = GetEventGenerator();
  ClickOnView(desks_bar_view->zero_state_default_desk_button(),
              event_generator);
  EXPECT_TRUE(desks_bar_view->mini_views()[0]->desk_name_view()->HasFocus());

  // Change the desk name to "test".
  SendKey(ui::VKEY_T);
  SendKey(ui::VKEY_E);
  SendKey(ui::VKEY_S);
  SendKey(ui::VKEY_T);
  SendKey(ui::VKEY_RETURN);
  ExitOverview();
  EnterOverview();

  desks_bar_view = GetOverviewGridForRoot(root_window)->desks_bar_view();
  EXPECT_TRUE(desks_bar_view->IsZeroState());
  // Should show the desk's current name "test" instead of the default name.
  EXPECT_EQ(u"test",
            desks_bar_view->zero_state_default_desk_button()->GetText());

  // Create 'Desk 2'.
  ClickOnView(desks_bar_view->zero_state_new_desk_button(), event_generator);
  EXPECT_FALSE(desks_bar_view->IsZeroState());
  SendKey(ui::VKEY_RETURN);
  EXPECT_EQ(u"Desk 2", DesksController::Get()->desks()[1].get()->name());

  // Close desk 'test' should return to zero state and the zero state default
  // desk button should show current desk's name, which is 'Desk 1'.
  CloseDeskFromMiniView(desks_bar_view->mini_views()[0], event_generator);
  EXPECT_TRUE(desks_bar_view->IsZeroState());
  EXPECT_EQ(u"Desk 1",
            desks_bar_view->zero_state_default_desk_button()->GetText());

  // Set a super long desk name.
  ClickOnView(desks_bar_view->zero_state_default_desk_button(),
              event_generator);
  for (size_t i = 0; i < DeskNameView::kMaxLength + 5; i++)
    SendKey(ui::VKEY_A);
  SendKey(ui::VKEY_RETURN);
  ExitOverview();
  EnterOverview();

  desks_bar_view = GetOverviewGridForRoot(root_window)->desks_bar_view();
  auto* zero_state_default_desk_button =
      desks_bar_view->zero_state_default_desk_button();
  std::u16string desk_button_text = zero_state_default_desk_button->GetText();
  std::u16string expected_desk_name(DeskNameView::kMaxLength, L'a');
  // Zero state desk button should show the elided name as the DeskNameView.
  EXPECT_EQ(expected_desk_name,
            DesksController::Get()->desks()[0].get()->name());
  EXPECT_NE(expected_desk_name, desk_button_text);
  EXPECT_TRUE(base::StartsWith(base::UTF16ToUTF8(desk_button_text), "aaa",
                               base::CompareCase::SENSITIVE));
  EXPECT_FALSE(base::EndsWith(base::UTF16ToUTF8(desk_button_text), "aaa",
                              base::CompareCase::SENSITIVE));
}

TEST_F(DesksTest, ReorderDesksByMouse) {
  auto* desks_controller = DesksController::Get();

  EnterOverview();
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  auto* root_window = Shell::GetPrimaryRootWindow();
  const auto* desks_bar_view =
      GetOverviewGridForRoot(root_window)->desks_bar_view();

  auto* event_generator = GetEventGenerator();

  // Add two desks (Now we have three desks).
  NewDesk();
  NewDesk();

  // Cache the mini view and corresponding desks.
  std::vector<DeskMiniView*> mini_views = desks_bar_view->mini_views();
  DeskMiniView* mini_view_0 = mini_views[0];
  Desk* desk_0 = mini_view_0->desk();
  DeskMiniView* mini_view_1 = mini_views[1];
  Desk* desk_1 = mini_view_1->desk();
  DeskMiniView* mini_view_2 = mini_views[2];
  Desk* desk_2 = mini_view_2->desk();

  // Set desk names. Force update user prefs because `SetName()` does not
  // trigger it but `DeskMiniView::OnViewBlurred`.
  desk_0->SetName(u"0", /*set_by_user=*/true);
  desk_1->SetName(u"1", /*set_by_user=*/true);
  desk_2->SetName(u"2", /*set_by_user=*/true);
  desks_restore_util::UpdatePrimaryUserDeskNamesPrefs();

  auto* prefs = Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  VerifyDesksRestoreData(prefs, std::vector<std::string>{"0", "1", "2"});

  // Dragging the desk preview will trigger drag & drop.
  StartDragDeskPreview(mini_view_1, event_generator);
  EXPECT_TRUE(desks_bar_view->IsDraggingDesk());

  event_generator->ReleaseLeftButton();

  // Reorder the second desk
  StartDragDeskPreview(mini_view_1, event_generator);
  EXPECT_TRUE(desks_bar_view->IsDraggingDesk());

  // Swap the positions of the second desk and the third desk.
  gfx::Point desk_center_2 =
      mini_view_2->GetPreviewBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(desk_center_2);

  // Now, the desks order should be [0, 2, 1]:
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_0));
  EXPECT_EQ(1, desks_controller->GetDeskIndex(desk_2));
  EXPECT_EQ(2, desks_controller->GetDeskIndex(desk_1));
  VerifyDesksRestoreData(prefs, std::vector<std::string>{"0", "2", "1"});

  // Swap the positions of the second desk and the first desk.
  gfx::Point desk_center_0 =
      mini_view_0->GetPreviewBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(desk_center_0);

  // Now, the desks order should be [1, 0, 2]:
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_1));
  EXPECT_EQ(1, desks_controller->GetDeskIndex(desk_0));
  EXPECT_EQ(2, desks_controller->GetDeskIndex(desk_2));
  VerifyDesksRestoreData(prefs, std::vector<std::string>{"1", "0", "2"});

  event_generator->ReleaseLeftButton();
}

TEST_F(DesksTest, ReorderDesksByGesture) {
  auto* desks_controller = DesksController::Get();

  EnterOverview();
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  auto* root_window = Shell::GetPrimaryRootWindow();
  const auto* desks_bar_view =
      GetOverviewGridForRoot(root_window)->desks_bar_view();

  auto* event_generator = GetEventGenerator();

  // Add two desks (Now we have three desks).
  NewDesk();
  NewDesk();

  // Cache the mini view and corresponding desks.
  std::vector<DeskMiniView*> mini_views = desks_bar_view->mini_views();
  DeskMiniView* mini_view_0 = mini_views[0];
  Desk* desk_0 = mini_view_0->desk();
  DeskMiniView* mini_view_1 = mini_views[1];
  Desk* desk_1 = mini_view_1->desk();
  DeskMiniView* mini_view_2 = mini_views[2];
  Desk* desk_2 = mini_view_2->desk();

  // Set desk names and save to user prefs.
  desk_0->SetName(u"0", /*set_by_user=*/true);
  desk_1->SetName(u"1", /*set_by_user=*/true);
  desk_2->SetName(u"2", /*set_by_user=*/true);
  desks_restore_util::UpdatePrimaryUserDeskNamesPrefs();

  auto* prefs = Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  VerifyDesksRestoreData(prefs, std::vector<std::string>{"0", "1", "2"});

  // If long press on the second desk preview, drag & drop will be triggered.
  // Perform by gesture:
  LongTapOnDeskPreview(mini_view_1, event_generator);
  EXPECT_TRUE(desks_bar_view->IsDraggingDesk());

  event_generator->ReleaseTouch();

  // Reorder the second desk
  LongTapOnDeskPreview(mini_view_1, event_generator);
  EXPECT_TRUE(desks_bar_view->IsDraggingDesk());

  // Swap the positions of the second desk and the third desk.
  gfx::Point desk_center_2 =
      mini_view_2->GetPreviewBoundsInScreen().CenterPoint();
  event_generator->MoveTouch(desk_center_2);

  // Now, the desks order should be [0, 2, 1]:
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_0));
  EXPECT_EQ(1, desks_controller->GetDeskIndex(desk_2));
  EXPECT_EQ(2, desks_controller->GetDeskIndex(desk_1));
  VerifyDesksRestoreData(prefs, std::vector<std::string>{"0", "2", "1"});

  // Swap the positions of the second desk and the first desk.
  gfx::Point desk_center_0 =
      mini_view_0->GetPreviewBoundsInScreen().CenterPoint();
  event_generator->MoveTouch(desk_center_0);

  // Now, the desks order should be [1, 0, 2]:
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_1));
  EXPECT_EQ(1, desks_controller->GetDeskIndex(desk_0));
  EXPECT_EQ(2, desks_controller->GetDeskIndex(desk_2));
  VerifyDesksRestoreData(prefs, std::vector<std::string>{"1", "0", "2"});

  event_generator->ReleaseTouch();
}

TEST_F(DesksTest, ReorderDesksByKeyboard) {
  auto* desks_controller = DesksController::Get();

  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  auto* root_window = Shell::GetPrimaryRootWindow();
  auto* overview_grid = GetOverviewGridForRoot(root_window);
  const auto* desks_bar_view = overview_grid->desks_bar_view();

  auto* event_generator = GetEventGenerator();

  // Add two desks (Now we have three desks).
  NewDesk();
  NewDesk();

  overview_grid->CommitDeskNameChanges();

  // Cache the mini view and corresponding desks.
  std::vector<DeskMiniView*> mini_views = desks_bar_view->mini_views();
  DeskMiniView* mini_view_0 = mini_views[0];
  Desk* desk_0 = mini_view_0->desk();
  DeskMiniView* mini_view_1 = mini_views[1];
  Desk* desk_1 = mini_view_1->desk();
  DeskMiniView* mini_view_2 = mini_views[2];
  Desk* desk_2 = mini_view_2->desk();

  // Set desk names and save to user prefs.
  desk_0->SetName(u"0", /*set_by_user=*/true);
  desk_1->SetName(u"1", /*set_by_user=*/true);
  desk_2->SetName(u"2", /*set_by_user=*/true);
  desks_restore_util::UpdatePrimaryUserDeskNamesPrefs();

  auto* prefs = Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  VerifyDesksRestoreData(prefs, std::vector<std::string>{"0", "1", "2"});

  // Highlight the second desk.
  overview_controller->overview_session()
      ->highlight_controller()
      ->MoveHighlightToView(mini_view_1);

  // Swap the positions of the second desk and the third desk by pressing Ctrl +
  // ->.
  event_generator->PressKey(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);

  // Now, the desks order should be [0, 2, 1]:
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_0));
  EXPECT_EQ(1, desks_controller->GetDeskIndex(desk_2));
  EXPECT_EQ(2, desks_controller->GetDeskIndex(desk_1));
  VerifyDesksRestoreData(prefs, std::vector<std::string>{"0", "2", "1"});

  // Keep pressing -> won't swap desks.
  event_generator->PressKey(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);

  // Now, the desks order should be [0, 2, 1]:
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_0));
  EXPECT_EQ(1, desks_controller->GetDeskIndex(desk_2));
  EXPECT_EQ(2, desks_controller->GetDeskIndex(desk_1));
  VerifyDesksRestoreData(prefs, std::vector<std::string>{"0", "2", "1"});

  // Press Ctrl + <- twice will swap the positions of the second and first desk.
  event_generator->PressKey(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);
  event_generator->PressKey(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);

  // Now, the desks order should be [1, 0, 2]:
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_1));
  EXPECT_EQ(1, desks_controller->GetDeskIndex(desk_0));
  EXPECT_EQ(2, desks_controller->GetDeskIndex(desk_2));
  VerifyDesksRestoreData(prefs, std::vector<std::string>{"1", "0", "2"});

  // Keep pressing <- won't swap desks.
  event_generator->PressKey(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);

  // Now, the desks order should be [1, 0, 2]:
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_1));
  EXPECT_EQ(1, desks_controller->GetDeskIndex(desk_0));
  EXPECT_EQ(2, desks_controller->GetDeskIndex(desk_2));
  VerifyDesksRestoreData(prefs, std::vector<std::string>{"1", "0", "2"});
}

// Test reordering desks in RTL mode.
TEST_F(DesksTest, ReorderDesksInRTLMode) {
  // Turn on RTL mode.
  const bool default_rtl = base::i18n::IsRTL();
  base::i18n::SetRTLForTesting(true);
  EXPECT_TRUE(base::i18n::IsRTL());

  auto* desks_controller = DesksController::Get();

  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  auto* root_window = Shell::GetPrimaryRootWindow();
  const auto* desks_bar_view =
      GetOverviewGridForRoot(root_window)->desks_bar_view();

  auto* event_generator = GetEventGenerator();

  // Add two desks (Now we have three desks).
  NewDesk();
  NewDesk();

  // Cache the mini view and corresponding desks.
  std::vector<DeskMiniView*> mini_views = desks_bar_view->mini_views();
  DeskMiniView* mini_view_0 = mini_views[0];
  Desk* desk_0 = mini_view_0->desk();
  DeskMiniView* mini_view_1 = mini_views[1];
  Desk* desk_1 = mini_view_1->desk();
  DeskMiniView* mini_view_2 = mini_views[2];
  Desk* desk_2 = mini_view_2->desk();

  // Set desk names. Force update user prefs because `SetName()` does not
  // trigger it but `DeskMiniView::OnViewBlurred`.
  desk_0->SetName(u"0", /*set_by_user=*/true);
  desk_1->SetName(u"1", /*set_by_user=*/true);
  desk_2->SetName(u"2", /*set_by_user=*/true);
  desks_restore_util::UpdatePrimaryUserDeskNamesPrefs();

  auto* prefs = Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  VerifyDesksRestoreData(prefs, std::vector<std::string>{"0", "1", "2"});

  // Swap the positions of the |desk_1| and the |desk_2| by mouse.
  StartDragDeskPreview(mini_view_1, event_generator);
  EXPECT_TRUE(desks_bar_view->IsDraggingDesk());

  gfx::Point desk_center_2 =
      mini_view_2->GetPreviewBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(desk_center_2);

  // Now, the desks order should be [0, 2, 1]:
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_0));
  EXPECT_EQ(1, desks_controller->GetDeskIndex(desk_2));
  EXPECT_EQ(2, desks_controller->GetDeskIndex(desk_1));
  VerifyDesksRestoreData(prefs, std::vector<std::string>{"0", "2", "1"});
  event_generator->ReleaseLeftButton();

  // Swap the positions of the |desk_1| and the |desk_0| by gesture.
  LongTapOnDeskPreview(mini_view_1, event_generator);
  EXPECT_TRUE(desks_bar_view->IsDraggingDesk());

  gfx::Point desk_center_0 =
      mini_view_0->GetPreviewBoundsInScreen().CenterPoint();
  event_generator->MoveTouch(desk_center_0);

  // Now, the desks order should be [1, 0, 2]:
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_1));
  EXPECT_EQ(1, desks_controller->GetDeskIndex(desk_0));
  EXPECT_EQ(2, desks_controller->GetDeskIndex(desk_2));
  VerifyDesksRestoreData(prefs, std::vector<std::string>{"1", "0", "2"});
  event_generator->ReleaseTouch();

  // Swap the positions of the |desk_0| and the |desk_2| by keyboard.
  // Highlight the |desk_0|.
  overview_controller->overview_session()
      ->highlight_controller()
      ->MoveHighlightToView(mini_view_0);

  // Swap the positions of the |desk_0| and the |desk_2| by pressing Ctrl + <-.
  event_generator->PressKey(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);

  // Now, the desks order should be [1, 2, 0]:
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_1));
  EXPECT_EQ(1, desks_controller->GetDeskIndex(desk_2));
  EXPECT_EQ(2, desks_controller->GetDeskIndex(desk_0));
  VerifyDesksRestoreData(prefs, std::vector<std::string>{"1", "2", "0"});

  // Recover to default RTL mode.
  base::i18n::SetRTLForTesting(default_rtl);
}

// Tests the behavior when drag a desk on the scroll button.
TEST_F(DesksTest, ScrollBarByDraggedDesk) {
  // Make a flat long window to generate multiple pages on desks bar.
  UpdateDisplay("800x150");
  const size_t max_desks_size = desks_util::kMaxNumberOfDesks;
  for (size_t i = 1; i < max_desks_size; i++)
    NewDesk();

  auto* desks_controller = DesksController::Get();
  EXPECT_EQ(desks_controller->desks().size(), max_desks_size);
  EnterOverview();
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  auto* desks_bar =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();

  views::ScrollView* scroll_view = DesksTestApi::GetDesksBarScrollView();
  const int page_size = scroll_view->width();
  auto mini_views = desks_bar->mini_views();
  const int mini_view_width = mini_views[0]->bounds().width();
  int desks_in_one_page = page_size / mini_view_width;
  float fractional_page = static_cast<float>(page_size % mini_view_width) /
                          static_cast<float>(mini_view_width);
  if (fractional_page > 0.5)
    desks_in_one_page++;

  int current_index = 0;
  ScrollArrowButton* left_button = DesksTestApi::GetDesksBarLeftScrollButton();
  ScrollArrowButton* right_button =
      DesksTestApi::GetDesksBarRightScrollButton();

  // At first, left scroll button is hidden and right scroll button is visible.
  EXPECT_FALSE(left_button->GetVisible());
  EXPECT_TRUE(right_button->GetVisible());

  // Dragging a desk on the right scroll button should scroll to the next page.
  // And the final scroll position should be adjusted to make sure the desk
  // preview will not be cropped.
  auto* event_generator = GetEventGenerator();
  DeskMiniView* mini_view_0 = mini_views[0];
  Desk* desk_0 = mini_view_0->desk();

  StartDragDeskPreview(mini_view_0, event_generator);
  EXPECT_TRUE(desks_bar->IsDraggingDesk());
  event_generator->MoveMouseTo(right_button->GetBoundsInScreen().CenterPoint());
  current_index += desks_in_one_page;
  EXPECT_EQ(scroll_view->GetVisibleRect().x(),
            mini_views[current_index]->bounds().x());

  // Both scroll buttons should be visible.
  EXPECT_TRUE(left_button->GetVisible());
  EXPECT_TRUE(right_button->GetVisible());

  // Wait for 1s, there will be another scroll.
  WaitForMilliseconds(1000);
  current_index += desks_in_one_page;
  EXPECT_EQ(scroll_view->GetVisibleRect().x(),
            mini_views[current_index]->bounds().x());

  // While scrolling, the desk cannot be reordered.
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_0));

  // Wait for 1s, it will scroll to the maximum offset. Scroll ends.
  WaitForMilliseconds(1000);
  EXPECT_EQ(scroll_view->GetVisibleRect().x(),
            scroll_view->contents()->width() - page_size);

  // Left scroll button should be visible and right scroll button should be
  // hidden.
  EXPECT_TRUE(left_button->GetVisible());
  EXPECT_FALSE(right_button->GetVisible());

  // Move the dragged desk to the center of the last desk.
  event_generator->MoveMouseTo(
      mini_views[max_desks_size - 1]->GetBoundsInScreen().CenterPoint());
  // The dragged desk is reordered to the end.
  const int max_index = static_cast<int>(max_desks_size) - 1;
  // Now the desk is reordered to the last position.
  EXPECT_EQ(max_index, desks_controller->GetDeskIndex(desk_0));
  event_generator->ReleaseLeftButton();

  // Dragging the desk to left scroll button should scroll to the previous page.
  // And the final scroll position should also be adjusted while scrolling to
  // the previous page to make sure the desk preview will not be cropped.
  mini_views = desks_bar->mini_views();
  StartDragDeskPreview(mini_views[max_desks_size - 1], event_generator);
  event_generator->MoveMouseTo(left_button->GetBoundsInScreen().CenterPoint());
  current_index -= desks_in_one_page;
  EXPECT_EQ(scroll_view->GetVisibleRect().x(),
            mini_views[current_index]->bounds().x());

  // Wait for 1s, there is another scroll.
  WaitForMilliseconds(1000);
  current_index -= desks_in_one_page;
  EXPECT_EQ(scroll_view->GetVisibleRect().x(),
            mini_views[current_index]->bounds().x());

  // The desk is still not reordered while scrolling backward.
  EXPECT_EQ(max_index, desks_controller->GetDeskIndex(desk_0));

  // Drop the desk. Desks bar will scroll to show the desk's target position.
  event_generator->ReleaseLeftButton();
  gfx::Rect bounds_0 = mini_view_0->bounds();
  gfx::Rect bounds_visible = scroll_view->GetVisibleRect();
  EXPECT_LE(bounds_visible.x(), bounds_0.x());
  EXPECT_GE(bounds_visible.right(), bounds_0.right());
}

// Tests that while reordering desks by drag & drop, when a desk is snapping
// back, click its target location won't cause any crashes.
// Regression test of https://crbug.com/1171880.
TEST_F(DesksTest, ClickTargetLocationOfDroppedDesk) {
  EnterOverview();
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();

  // Add a desk.
  NewDesk();

  // Cache the second desk's mini view.
  DeskMiniView* mini_view = desks_bar_view->mini_views()[1];

  auto* event_generator = GetEventGenerator();

  // Drag the second desk away from the desk bar.
  StartDragDeskPreview(mini_view, event_generator);
  EXPECT_TRUE(desks_bar_view->IsDraggingDesk());

  event_generator->MoveMouseBy(0, desks_bar_view->height());

  ui::ScopedAnimationDurationScaleMode normal_anim(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  // Drop the desk and click on the mini view.
  event_generator->ReleaseLeftButton();
  EXPECT_TRUE(desks_bar_view->IsDraggingDesk());
  DeskSwitchAnimationWaiter waiter;
  ClickOnView(mini_view, event_generator);
  waiter.Wait();
}

// Tests that while reordering desks by drag & drop, when a desk is snapping
// back, dragging a desk preview on the shelf will start a new drag.
TEST_F(DesksTest, DragNewDeskWhileSnappingBack) {
  EnterOverview();
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();

  // Add a desk.
  NewDesk();

  // Cache the desks' mini views.
  DeskMiniView* mini_view_1 = desks_bar_view->mini_views()[0];
  DeskMiniView* mini_view_2 = desks_bar_view->mini_views()[1];

  auto* event_generator = GetEventGenerator();

  // Drag the second desk away from the desk bar.
  StartDragDeskPreview(mini_view_2, event_generator);
  EXPECT_EQ(DesksTestApi::GetDesksBarDragView(), mini_view_2);

  event_generator->MoveMouseBy(0, desks_bar_view->height());

  ui::ScopedAnimationDurationScaleMode normal_anim(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Drop the desk and drag first desk.
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(DesksTestApi::GetDesksBarDragView(), mini_view_2);
  StartDragDeskPreview(mini_view_1, event_generator);
  EXPECT_EQ(DesksTestApi::GetDesksBarDragView(), mini_view_1);
}

// Tests that dragging desk is ended in two cases: (1) removing a dragged desk.
// (2) removing a desk makes the dragged desk the only one left. Then, releasing
// mouse or exiting overview will not have UAF issues
// (https://crbug.com/1222120).
TEST_F(DesksTest, RemoveDeskWhileDragging) {
  EnterOverview();
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  const auto* desks_bar_view =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();

  auto* event_generator = GetEventGenerator();

  // Add two desks (Now we have three desks).
  NewDesk();
  NewDesk();

  // Cache the mini views.
  const std::vector<DeskMiniView*>& mini_views = desks_bar_view->mini_views();
  DeskMiniView* mini_view_0 = mini_views[0];
  DeskMiniView* mini_view_1 = mini_views[1];
  DeskMiniView* mini_view_2 = mini_views[2];

  // Dragging the first desk preview will trigger drag & drop.
  StartDragDeskPreview(mini_view_0, event_generator);
  EXPECT_TRUE(desks_bar_view->IsDraggingDesk());

  // Removing the first desk will end dragging.
  RemoveDesk(mini_view_0->desk());
  EXPECT_FALSE(desks_bar_view->IsDraggingDesk());

  // Releasing mouse will not have any issue.
  event_generator->ReleaseLeftButton();

  // There are only two desks left.
  EXPECT_EQ(2u, mini_views.size());

  // Dragging the second desk preview.
  StartDragDeskPreview(mini_view_1, event_generator);
  EXPECT_TRUE(desks_bar_view->IsDraggingDesk());

  // Removing the third desk will end dragging (the dragged desk is the only one
  // left).
  RemoveDesk(mini_view_2->desk());
  EXPECT_FALSE(desks_bar_view->IsDraggingDesk());

  // Exiting overview will not have any issue.
  ExitOverview();
}

// Tests that the right desk containers are visible when switching between desks
// really fast. Regression test for https://crbug.com/1194757.
TEST_F(DesksTest, FastDeskSwitches) {
  // Add 3 more desks and add a couple windows on each one.
  CreateTestWindow();
  CreateTestWindow();

  for (int i = 0; i < 3; ++i) {
    NewDesk();
    CreateTestWindow();
    CreateTestWindow();
  }

  // Start at the rightmost, 4th desk.
  auto* desks_controller = DesksController::Get();
  ASSERT_EQ(4u, desks_controller->desks().size());
  desks_controller->ActivateDesk(desks_controller->desks()[3].get(),
                                 DesksSwitchSource::kUserSwitch);

  ui::ScopedAnimationDurationScaleMode normal_anim(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Activate the first 2 desks very quickly, without waiting for screenshots to
  // be taken.
  desks_controller->ActivateAdjacentDesk(
      /*going_left=*/true, DesksSwitchSource::kDeskSwitchShortcut);
  desks_controller->ActivateAdjacentDesk(
      /*going_left=*/true, DesksSwitchSource::kDeskSwitchShortcut);

  // Let the last desk animation to complete.
  desks_controller->ActivateAdjacentDesk(
      /*going_left=*/true, DesksSwitchSource::kDeskSwitchShortcut);
  // Note that `DeskController::ActivateAdjacentDesk()` will trigger two
  // `OnDeskSwitchAnimationFinished()` calls. The first is from the previous
  // animation which we destroy since its screenshots have not been taken yet.
  // The second is the animation to the first desk that we want to wait to take
  // the screenshots and animate.
  DeskSwitchAnimationWaiter waiter;
  waiter.Wait();

  // Check the desk containers. Test that only the first desk container is
  // visible, but they should all be opaque.
  std::vector<aura::Window*> desk_containers =
      desks_util::GetDesksContainers(Shell::GetPrimaryRootWindow());
  ASSERT_EQ(8u, desk_containers.size());
  EXPECT_TRUE(desk_containers[0]->IsVisible());
  EXPECT_EQ(1.f, desk_containers[0]->layer()->opacity());

  for (size_t i = 1; i < desk_containers.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Desk #%lu", i + 1));
    EXPECT_FALSE(desk_containers[i]->IsVisible());
    EXPECT_EQ(1.f, desk_containers[i]->layer()->opacity());
  }
}

// Tests that when the user is in tablet mode, the virtual keyboard is opened
// during name nudges.
TEST_F(DesksTest, NameNudgesTabletMode) {
  TabletModeControllerTestApi().EnterTabletMode();

  // Tablet mode requires at least two desks so create one.
  NewDesk();
  ASSERT_EQ(2u, DesksController::Get()->desks().size());

  // Start overview.
  EnterOverview();
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Setup an internal keyboard and an external keyboard.
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      std::vector<ui::InputDevice>{
          {1, ui::INPUT_DEVICE_INTERNAL, "internal keyboard"},
          {2, ui::INPUT_DEVICE_USB, "external keyboard"}});
  auto* device_data_manager = ui::DeviceDataManager::GetInstance();
  auto keyboard_devices = device_data_manager->GetKeyboardDevices();
  ASSERT_EQ(2u, keyboard_devices.size());

  // Tap the new desk button. There should be a new DeskNameView that is created
  // and the virtual keyboard should not be shown because there is at least one
  // external keyboard attached.
  auto* event_generator = GetEventGenerator();
  const auto* desks_bar_view =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();
  GestureTapOnView(desks_bar_view->expanded_state_new_desk_button(),
                   event_generator);
  EXPECT_FALSE(keyboard::KeyboardUIController::Get()->IsKeyboardVisible());
  EXPECT_EQ(3u, desks_bar_view->mini_views().size());
  auto* desk_name_view = desks_bar_view->mini_views()[2]->desk_name_view();
  EXPECT_TRUE(desk_name_view->HasFocus());
  EXPECT_EQ(std::u16string(), desk_name_view->GetText());

  // Reset the devices and make it so there's only an internal keyboard.
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      std::vector<ui::InputDevice>{
          {1, ui::INPUT_DEVICE_INTERNAL, "internal keyboard"},
      });
  keyboard_devices = device_data_manager->GetKeyboardDevices();
  ASSERT_EQ(1u, keyboard_devices.size());

  // Tap the new desk button again. There should be a new DeskNameView that is
  // created and the virtual keyboard should be shown.
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({});
  GestureTapOnView(desks_bar_view->expanded_state_new_desk_button(),
                   event_generator);
  ASSERT_TRUE(keyboard::WaitUntilShown());
  EXPECT_TRUE(keyboard::KeyboardUIController::Get()->IsKeyboardVisible());
  EXPECT_EQ(4u, desks_bar_view->mini_views().size());
  desk_name_view = desks_bar_view->mini_views()[3]->desk_name_view();
  EXPECT_TRUE(desk_name_view->HasFocus());
  EXPECT_EQ(std::u16string(), desk_name_view->GetText());
}

// Tests the time period to set perf `kUserHasUsedDesksRecently`.
TEST_F(DesksTest, PrimaryUserHasUsedDesksRecently) {
  base::SimpleTestClock test_clock;
  base::Time time;
  auto* desks_controller = DesksController::Get();
  // `kUserHasUsedDesksRecently` should not be set before 07/27/2021.
  ASSERT_TRUE(base::Time::FromString("Mon, 26 Jul 2021 23:59:59", &time));
  test_clock.SetNow(time);
  desks_restore_util::OverrideClockForTesting(&test_clock);
  NewDesk();
  RemoveDesk(desks_controller->desks().back().get());
  EXPECT_FALSE(desks_restore_util::HasPrimaryUserUsedDesksRecently());

  // `kUserHasUsedDesksRecently` should not be set in 09/07/2021 and after.
  ASSERT_TRUE(base::Time::FromString("Tue, 7 Sep 2021 00:00:01", &time));
  test_clock.SetNow(time);

  NewDesk();
  RemoveDesk(desks_controller->desks().back().get());
  EXPECT_FALSE(desks_restore_util::HasPrimaryUserUsedDesksRecently());

  // `kUserHasUsedDesksRecently` should be set during [07/27/2021, 09/07/2021).
  ASSERT_TRUE(base::Time::FromString("Tue, 27 Jul 2021 00:00:01", &time));
  test_clock.SetNow(time);

  NewDesk();
  RemoveDesk(desks_controller->desks().back().get());
  EXPECT_TRUE(desks_restore_util::HasPrimaryUserUsedDesksRecently());

  // `kUserHasUsedDesksRecently` should be kept as true after setting.
  test_clock.Advance(base::TimeDelta::FromDays(50));
  EXPECT_TRUE(desks_restore_util::HasPrimaryUserUsedDesksRecently());
  desks_restore_util::OverrideClockForTesting(nullptr);
}

// A test class that uses a mock time test environment.
class DesksMockTimeTest : public DesksTest {
 public:
  DesksMockTimeTest()
      : DesksTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  DesksMockTimeTest(const DesksMockTimeTest&) = delete;
  DesksMockTimeTest& operator=(const DesksMockTimeTest&) = delete;
  ~DesksMockTimeTest() override = default;

  // DesksTest:
  void SetUp() override {
    // The `g_weekly_active_desks` global counter may be non-zero at the
    // beginning of tests so reset it. See https://crbug.com/1200684.
    Desk::SetWeeklyActiveDesks(0);
    DesksTest::SetUp();
  }
};

// Tests that the weekly active desks metric is properly recorded.
TEST_F(DesksMockTimeTest, WeeklyActiveDesks) {
  constexpr char kWeeklyActiveDesksHistogram[] = "Ash.Desks.WeeklyActiveDesks";
  base::HistogramTester histogram_tester;

  // Create three new desks.
  NewDesk();
  NewDesk();
  NewDesk();
  auto* controller = DesksController::Get();
  ASSERT_EQ(4u, controller->desks().size());
  const Desk* desk_1 = controller->desks()[0].get();
  const Desk* desk_2 = controller->desks()[1].get();
  Desk* desk_3 = controller->desks()[2].get();
  Desk* desk_4 = controller->desks()[3].get();

  // Let a week elapse. There should be a new entry for four since there were
  // three created desks and the initial active desk.
  task_environment()->AdvanceClock(base::TimeDelta::FromDays(7));
  task_environment()->RunUntilIdle();
  histogram_tester.ExpectBucketCount(kWeeklyActiveDesksHistogram, 4, 1);
  EXPECT_EQ(1u,
            histogram_tester.GetAllSamples(kWeeklyActiveDesksHistogram).size());

  // Activate the second desk and quickly activate the first desk. Let a week
  // elapse. There should be a new entry for one since we shouldn't count a desk
  // that was briefly visited.
  EXPECT_EQ(1, Desk::GetWeeklyActiveDesks());
  ActivateDesk(desk_2);
  ActivateDesk(desk_1);
  task_environment()->AdvanceClock(base::TimeDelta::FromDays(7));
  task_environment()->RunUntilIdle();
  histogram_tester.ExpectBucketCount(kWeeklyActiveDesksHistogram, 1, 1);
  EXPECT_EQ(2u,
            histogram_tester.GetAllSamples(kWeeklyActiveDesksHistogram).size());

  // Activate the second desk and wait on it. Activate the first desk and wait
  // on it. Activate the second desk again. Let a week elapse. There should be a
  // new entry for two since we shouldn't count activating the second desk
  // twice.
  EXPECT_EQ(1, Desk::GetWeeklyActiveDesks());
  ActivateDesk(desk_2);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(5));
  ActivateDesk(desk_1);
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(5));
  ActivateDesk(desk_2);
  task_environment()->AdvanceClock(base::TimeDelta::FromDays(7));
  task_environment()->RunUntilIdle();
  histogram_tester.ExpectBucketCount(kWeeklyActiveDesksHistogram, 2, 1);
  EXPECT_EQ(3u,
            histogram_tester.GetAllSamples(kWeeklyActiveDesksHistogram).size());

  // Rename the third desk twice and move two windows to the fourth desk. Let a
  // week elapse. There should be a new entry for three.
  EXPECT_EQ(1, Desk::GetWeeklyActiveDesks());
  desk_3->SetName(u"foo", /*set_by_user=*/true);
  desk_3->SetName(u"bar", /*set_by_user=*/true);

  auto win1 = CreateAppWindow();
  auto win2 = CreateAppWindow();
  controller->MoveWindowFromActiveDeskTo(
      win1.get(), desk_4, win1->GetRootWindow(),
      DesksMoveWindowFromActiveDeskSource::kSendToDesk);
  controller->MoveWindowFromActiveDeskTo(
      win2.get(), desk_4, win2->GetRootWindow(),
      DesksMoveWindowFromActiveDeskSource::kSendToDesk);

  task_environment()->AdvanceClock(base::TimeDelta::FromDays(7));
  task_environment()->RunUntilIdle();
  histogram_tester.ExpectBucketCount(kWeeklyActiveDesksHistogram, 3, 1);
  EXPECT_EQ(4u,
            histogram_tester.GetAllSamples(kWeeklyActiveDesksHistogram).size());

  // Wait six days on the current desk. Since a week hasn't elapsed relative to
  // the previous report, this should not report anything. Currently the weekly
  // active desks count should be one, so check the "one-count" bucket to make
  // sure it matches its previous number of entries.
  EXPECT_EQ(1, Desk::GetWeeklyActiveDesks());
  const int number_of_one_bucket_entries =
      histogram_tester.GetBucketCount(kWeeklyActiveDesksHistogram, 1);
  task_environment()->AdvanceClock(base::TimeDelta::FromDays(6));
  task_environment()->RunUntilIdle();
  histogram_tester.ExpectBucketCount(kWeeklyActiveDesksHistogram, 1,
                                     number_of_one_bucket_entries);

  // Wait one more day and it should now report an entry for one, accounting for
  // the current active desk.
  task_environment()->AdvanceClock(base::TimeDelta::FromDays(1));
  task_environment()->RunUntilIdle();
  histogram_tester.ExpectBucketCount(kWeeklyActiveDesksHistogram, 1,
                                     number_of_one_bucket_entries + 1);
}

class PersistentDesksBarTest : public DesksTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kBentoBar);
    DesksTest::SetUp();

    desks_restore_util::SetPrimaryUserHasUsedDesksRecentlyForTesting(true);
    ASSERT_TRUE(desks_restore_util::HasPrimaryUserUsedDesksRecently());
  }
  PersistentDesksBarTest() = default;
  PersistentDesksBarTest(const PersistentDesksBarTest&) = delete;
  PersistentDesksBarTest& operator=(const PersistentDesksBarTest&) = delete;
  ~PersistentDesksBarTest() override = default;

  const views::Widget* GetBarWidget() const {
    return Shell::Get()
        ->persistent_desks_bar_controller()
        ->persistent_desks_bar_widget();
  }

  bool IsWidgetVisible() const {
    auto* bar_widget = GetBarWidget();
    DCHECK(bar_widget);
    return bar_widget->GetLayer()->GetTargetVisibility();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the bar will only be created and shown when there are more than
// one desk.
TEST_F(PersistentDesksBarTest, MoreThanOneDesk) {
  auto* shell = Shell::Get();
  ASSERT_FALSE(shell->tablet_mode_controller()->InTabletMode());
  auto* desks_controller = DesksController::Get();
  ASSERT_EQ(1u, desks_controller->desks().size());
  auto* bar_controller = shell->persistent_desks_bar_controller();
  ASSERT_TRUE(bar_controller);

  // The bar should not be created if there is only one desk.
  EXPECT_FALSE(GetBarWidget());

  // Create a new desk should cause the bar to be created and shown.
  NewDesk();
  EXPECT_EQ(2u, desks_controller->desks().size());
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(IsWidgetVisible());

  // The bar should be destroyed after removed a desk and then there is only one
  // desk left.
  RemoveDesk(desks_controller->desks()[1].get());
  EXPECT_EQ(1u, desks_controller->desks().size());
  EXPECT_FALSE(GetBarWidget());
}

// Tests that the bar will only be created and shown in clamshell mode.
TEST_F(PersistentDesksBarTest, ClamshellOnly) {
  auto* shell = Shell::Get();
  TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_TRUE(shell->tablet_mode_controller()->InTabletMode());
  auto* desks_controller = DesksController::Get();
  ASSERT_EQ(1u, desks_controller->desks().size());
  auto* bar_controller = shell->persistent_desks_bar_controller();
  ASSERT_TRUE(bar_controller);

  // Create or remove a desk in tablet mode should not create the bar or cause
  // any crash.
  EXPECT_FALSE(GetBarWidget());
  NewDesk();
  EXPECT_EQ(2u, desks_controller->desks().size());
  EXPECT_FALSE(GetBarWidget());
  RemoveDesk(desks_controller->desks()[0].get());
  EXPECT_FALSE(GetBarWidget());
  NewDesk();

  // Leaving tablet mode to clamshell mode with more than one desk should create
  // and show the bar.
  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(IsWidgetVisible());

  // The bar should be destroyed if tablet mode is entered.
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(shell->tablet_mode_controller()->InTabletMode());
  EXPECT_FALSE(GetBarWidget());
}

// Tests the bar's visibility while entering or leaving overview mode.
TEST_F(PersistentDesksBarTest, OverviewMode) {
  auto* desks_controller = DesksController::Get();
  ASSERT_EQ(1u, desks_controller->desks().size());

  NewDesk();
  EXPECT_TRUE(GetBarWidget());
  // Create a window thus `UpdateBarOnWindowStateChanges()` will be called while
  // entering overview mode. Bento bar should not be created and the desks bar
  // should be at the top of the display in this case.
  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(0, 0, 300, 300));
  WindowState* window_state = WindowState::Get(window.get());
  window_state->Minimize();

  // Entering overview mode should destroy the bar. Exiting overview mode with
  // more than one desk should create the bar and show it.
  EnterOverview();
  EXPECT_EQ(GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())
                ->desks_bar_view()
                ->GetBoundsInScreen()
                .origin(),
            gfx::Point(0, 0));
  EXPECT_FALSE(GetBarWidget());
  EXPECT_EQ(2u, desks_controller->desks().size());
  ExitOverview();
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(IsWidgetVisible());

  // Exiting overview mode with only one desk should not create the bar.
  EnterOverview();
  EXPECT_FALSE(GetBarWidget());
  RemoveDesk(desks_controller->desks()[1].get());
  EXPECT_EQ(1u, desks_controller->desks().size());
  ExitOverview();
  EXPECT_FALSE(GetBarWidget());

  // Each desk button in the bar should show the corresponding desk's name.
  EnterOverview();
  NewDesk();
  EXPECT_EQ(2u, desks_controller->desks().size());
  desks_controller->desks()[1].get()->SetName(u"test", /*set_by_user=*/true);
  ExitOverview();
  auto desk_buttons = DesksTestApi::GetPersistentDesksBarDeskButtons();
  for (size_t i = 0; i < desk_buttons.size(); i++)
    EXPECT_EQ(desk_buttons[i]->GetText(), desks_controller->desks()[i]->name());

  // The desk buttons should have the same order as the desks after reordering.
  auto* event_generator = GetEventGenerator();
  EnterOverview();
  EXPECT_EQ(u"test", desks_controller->desks()[1]->name());
  const auto* desks_bar_view =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();
  StartDragDeskPreview(desks_bar_view->mini_views()[1], event_generator);
  event_generator->MoveMouseTo(desks_bar_view->mini_views()[0]
                                   ->GetPreviewBoundsInScreen()
                                   .CenterPoint());
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(u"test", desks_controller->desks()[0]->name());
  ExitOverview();
  desk_buttons = DesksTestApi::GetPersistentDesksBarDeskButtons();
  for (size_t i = 0; i < desk_buttons.size(); i++)
    EXPECT_EQ(desk_buttons[i]->GetText(), desks_controller->desks()[i]->name());
}

// Tests the desk activation changes after clicking the desk button in the bar.
TEST_F(PersistentDesksBarTest, DeskActivation) {
  NewDesk();
  auto* desks_controller = DesksController::Get();
  EXPECT_EQ(2u, desks_controller->desks().size());
  EXPECT_TRUE(GetBarWidget());
  EXPECT_EQ(desks_controller->desks()[0].get(),
            desks_controller->active_desk());

  // Should activate `Desk 2` after clicking the corresponding desk button.
  DeskSwitchAnimationWaiter waiter;
  ClickOnView(DesksTestApi::GetPersistentDesksBarDeskButtons()[1],
              GetEventGenerator());
  waiter.Wait();
  EXPECT_TRUE(GetBarWidget());
  EXPECT_EQ(desks_controller->desks()[1].get(),
            desks_controller->active_desk());
}

TEST_F(PersistentDesksBarTest, LeavingOrEnteringTabletModeWithOverviewModeOn) {
  NewDesk();
  EXPECT_EQ(2u, DesksController::Get()->desks().size());
  EXPECT_TRUE(GetBarWidget());

  // The bar should not be created after entering or leaving tablet mode with
  // overview mode on.
  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_FALSE(GetBarWidget());
  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_FALSE(GetBarWidget());
}

// Tests that the bar can be shown or hidden correctly through the context menu
// of the bar.
TEST_F(PersistentDesksBarTest, ShowOrHideBarThroughContextMenu) {
  NewDesk();
  EXPECT_EQ(2u, DesksController::Get()->desks().size());
  EXPECT_TRUE(GetBarWidget());

  // The bar should be destroyed after it is set to hide.
  PersistentDesksBarContextMenu* context_menu =
      DesksTestApi::GetPersistentDesksBarContextMenu();
  context_menu->ExecuteCommand(
      static_cast<int>(
          PersistentDesksBarContextMenu::CommandId::kShowOrHideBar),
      /*event_flags=*/0);
  EXPECT_FALSE(GetBarWidget());

  // With the bar being set to hide, it should not be created after exiting
  // overview mode with more than one desk.
  EnterOverview();
  ExitOverview();
  EXPECT_FALSE(GetBarWidget());

  // With the bar being set to show, it should be created after exiting overview
  // mode with more than one desk.
  EnterOverview();
  context_menu = DesksTestApi::GetDesksBarContextMenu();
  context_menu->ExecuteCommand(
      static_cast<int>(
          PersistentDesksBarContextMenu::CommandId::kShowOrHideBar),
      /*event_flags=*/0);
  ExitOverview();
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(IsWidgetVisible());
}

// Tests that the bar will only be created and shown when the shelf is
// bottom-aligned.
TEST_F(PersistentDesksBarTest, BentoBarWithShelfAlignment) {
  Shelf* shelf = Shelf::ForWindow(Shell::GetPrimaryRootWindow());

  // Create a new desk should cause the bar to be created and shown.
  NewDesk();
  EXPECT_EQ(2u, DesksController::Get()->desks().size());
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(IsWidgetVisible());

  // The bar should be created when the shelf is bottom aligned.
  EXPECT_TRUE(shelf->alignment() == ShelfAlignment::kBottom);
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(IsWidgetVisible());

  // The bar should be destroyed when the shelf is left aligned.
  shelf->SetAlignment(ShelfAlignment::kLeft);
  EXPECT_TRUE(shelf->alignment() == ShelfAlignment::kLeft);
  EXPECT_FALSE(GetBarWidget());

  // The bar should be destroyed when the shelf is right aligned.
  shelf->SetAlignment(ShelfAlignment::kRight);
  EXPECT_TRUE(shelf->alignment() == ShelfAlignment::kRight);
  EXPECT_FALSE(GetBarWidget());

  // The bar should be re-created when the shelf is bottom aligned.
  shelf->SetAlignment(ShelfAlignment::kBottom);
  EXPECT_TRUE(shelf->alignment() == ShelfAlignment::kBottom);
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(IsWidgetVisible());
}

// Tests that the bar will only be created when the app list is not in
// fullscreen mode.
TEST_F(PersistentDesksBarTest, AppListFullscreen) {
  AppListControllerImpl* app_list_controller =
      Shell::Get()->app_list_controller();
  AppListView* app_list_view = app_list_controller->presenter()->GetView();

  // The bar should be created when the app list view remains null.
  NewDesk();
  EXPECT_EQ(2u, DesksController::Get()->desks().size());
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(IsWidgetVisible());

  // The bar should be created when the app list view is created and
  // showing in kPeeking mode.
  app_list_controller->ShowAppList();
  app_list_view = app_list_controller->presenter()->GetView();
  ASSERT_TRUE(app_list_view);
  EXPECT_TRUE(app_list_view->app_list_state() == AppListViewState::kPeeking);
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(IsWidgetVisible());

  // The bar should be created when the app list is in kHalf mode.
  app_list_view->SetState(AppListViewState::kHalf);
  EXPECT_TRUE(app_list_view->app_list_state() == AppListViewState::kHalf);
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(IsWidgetVisible());

  // The bar should be destroyed when the app list is in kFullscreenSearch mode.
  app_list_view->SetState(AppListViewState::kFullscreenSearch);
  EXPECT_TRUE(app_list_view->app_list_state() ==
              AppListViewState::kFullscreenSearch);
  EXPECT_FALSE(GetBarWidget());

  // The bar should be destroyed when the app list is in kFullscreenAllApps
  // mode.
  app_list_view->SetState(AppListViewState::kFullscreenAllApps);
  EXPECT_TRUE(app_list_view->app_list_state() ==
              AppListViewState::kFullscreenAllApps);
  EXPECT_FALSE(GetBarWidget());

  // The bar should be created when the app list is in kClosed mode.
  app_list_view->SetState(AppListViewState::kClosed);
  EXPECT_TRUE(app_list_view->app_list_state() == AppListViewState::kClosed);
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(IsWidgetVisible());
}

// Tests that the bar will not be created if Docked Magnifier is on.
TEST_F(PersistentDesksBarTest, NoPersistentDesksBarWithDockedMagnifierOn) {
  AccessibilityControllerImpl* accessibility_controller =
      Shell::Get()->accessibility_controller();

  // Create a new desk should cause the bar to be created and shown.
  NewDesk();
  EXPECT_EQ(2u, DesksController::Get()->desks().size());
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(IsWidgetVisible());

  // Use the bounds at this point as reference for comparison later.
  gfx::Rect bounds = GetBarWidget()->GetWindowBoundsInScreen();

  // The bar should be destroyed when the Docked Magnifier is on.
  accessibility_controller->docked_magnifier().SetEnabled(true);
  EXPECT_TRUE(accessibility_controller->docked_magnifier().enabled());
  EXPECT_FALSE(GetBarWidget());

  // The bar should be created when the Docked Magnifier is off.
  accessibility_controller->docked_magnifier().SetEnabled(false);
  EXPECT_FALSE(accessibility_controller->docked_magnifier().enabled());
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(IsWidgetVisible());

  // The bounds should be the same with its original value.
  EXPECT_EQ(bounds, GetBarWidget()->GetWindowBoundsInScreen());
}

// Tests that the bar will not be created if ChromeVox is on.
TEST_F(PersistentDesksBarTest, NoPersistentDesksBarWithChromeVoxOn) {
  AccessibilityControllerImpl* accessibility_controller =
      Shell::Get()->accessibility_controller();

  // Create a new desk should cause the bar to be created and shown.
  NewDesk();
  EXPECT_EQ(2u, DesksController::Get()->desks().size());
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(IsWidgetVisible());

  // Use the bounds at this point as reference for comparison later.
  gfx::Rect bounds = GetBarWidget()->GetWindowBoundsInScreen();

  // The bar should be destroyed when Chromevox is on.
  accessibility_controller->spoken_feedback().SetEnabled(true);
  EXPECT_TRUE(accessibility_controller->spoken_feedback().enabled());
  EXPECT_FALSE(GetBarWidget());

  // The bar should be created when Chromevox is off.
  accessibility_controller->spoken_feedback().SetEnabled(false);
  EXPECT_FALSE(accessibility_controller->spoken_feedback().enabled());
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(IsWidgetVisible());

  // The bounds should be the same with its original value.
  EXPECT_EQ(bounds, GetBarWidget()->GetWindowBoundsInScreen());
}

// Tests that the bar will not be created if any window is fullscreened.
TEST_F(PersistentDesksBarTest, NoPersistentDesksBarWithFullscreenedWindow) {
  // Create a secondary display.
  UpdateDisplay("400x300,500x400");
  WMEvent event_toggle_fullscreen(WM_EVENT_TOGGLE_FULLSCREEN);
  WMEvent event_fullscreen(WM_EVENT_FULLSCREEN);
  std::unique_ptr<aura::Window> window1 =
      CreateTestWindow(gfx::Rect(0, 0, 300, 300));
  std::unique_ptr<aura::Window> window2 =
      CreateTestWindow(gfx::Rect(0, 0, 300, 300));
  WindowState* window_state1 = WindowState::Get(window1.get());
  WindowState* window_state2 = WindowState::Get(window2.get());
  NewDesk();
  gfx::Rect bounds = GetBarWidget()->GetWindowBoundsInScreen();

  // window1 and window2 should fall into the primary display.
  EXPECT_EQ(window1->GetRootWindow(), Shell::GetPrimaryRootWindow());
  EXPECT_EQ(window2->GetRootWindow(), Shell::GetPrimaryRootWindow());

  // The bar should be destroyed after `window1` entering fullscreen mode and be
  // recreated after 'window1' exiting fullscreen mode.
  window_state1->OnWMEvent(&event_toggle_fullscreen);
  EXPECT_TRUE(window_state1->IsFullscreen());
  EXPECT_FALSE(GetBarWidget());
  window_state1->OnWMEvent(&event_toggle_fullscreen);
  EXPECT_FALSE(window_state1->IsFullscreen());
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ(bounds, GetBarWidget()->GetWindowBoundsInScreen());

  // The bar should not be created until both `window1` and `window2` exiting
  // fullscreen mode.
  window_state1->OnWMEvent(&event_toggle_fullscreen);
  window_state2->OnWMEvent(&event_toggle_fullscreen);
  EXPECT_FALSE(GetBarWidget());
  window_state1->OnWMEvent(&event_toggle_fullscreen);
  EXPECT_FALSE(GetBarWidget());
  window_state2->OnWMEvent(&event_toggle_fullscreen);
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ(bounds, GetBarWidget()->GetWindowBoundsInScreen());

  // Fullscreen window within the secondary display should not impact
  // the persistent desks bar.
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();
  window3->SetBoundsInScreen(gfx::Rect(600, 0, 125, 100),
                             GetSecondaryDisplay());
  WindowState* window_state3 = WindowState::Get(window3.get());
  EXPECT_EQ(window3->GetRootWindow(), Shell::Get()->GetAllRootWindows()[1]);
  window_state3->OnWMEvent(&event_toggle_fullscreen);
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ(bounds, GetBarWidget()->GetWindowBoundsInScreen());

  // The bar should be created after `window1` being minimized.
  window_state1->Minimize();
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ(bounds, GetBarWidget()->GetWindowBoundsInScreen());
  window_state1->OnWMEvent(&event_fullscreen);
  EXPECT_FALSE(GetBarWidget());
  window_state1->OnWMEvent(&event_toggle_fullscreen);

  // The bar should not be created until both `window1` and `window2` being
  // closed.
  window_state1->OnWMEvent(&event_toggle_fullscreen);
  window_state2->OnWMEvent(&event_toggle_fullscreen);
  window1.reset();
  EXPECT_FALSE(GetBarWidget());
  window2.reset();
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ(bounds, GetBarWidget()->GetWindowBoundsInScreen());
}

// Tests that the bar should not be created in non-active user session.
TEST_F(PersistentDesksBarTest, NoPersistentDesksBarInNonActiveUserSession) {
  AccessibilityControllerImpl* accessibility_controller =
      Shell::Get()->accessibility_controller();
  TestSessionControllerClient* client = GetSessionControllerClient();

  // The bar should be created with two desks.
  NewDesk();
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(IsWidgetVisible());

  // The bar should not be created in LOCKED user session when docked magnifier
  // is enabled/disabled.
  client->SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_FALSE(GetBarWidget());
  accessibility_controller->docked_magnifier().SetEnabled(true);
  EXPECT_FALSE(GetBarWidget());
  accessibility_controller->docked_magnifier().SetEnabled(false);
  EXPECT_FALSE(GetBarWidget());

  // The bar should be created when the user session is active.
  client->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(IsWidgetVisible());
}

TEST_F(PersistentDesksBarTest, DisplayMetricsChanged) {
  UpdateDisplay("800x600,400x500");
  NewDesk();
  EXPECT_TRUE(GetBarWidget());
  EXPECT_EQ(GetBarWidget()->GetWindowBoundsInScreen().width(),
            GetPrimaryDisplay().bounds().width());

  // The bar should be recreated in the new primary display and with the same
  // width as it.
  const display::Display old_primary_display = GetPrimaryDisplay();
  SwapPrimaryDisplay();
  const display::Display new_primary_display = GetPrimaryDisplay();
  ASSERT_NE(old_primary_display.id(), new_primary_display.id());
  ASSERT_NE(old_primary_display.bounds().width(),
            new_primary_display.bounds().width());
  EXPECT_TRUE(GetBarWidget());
  EXPECT_EQ(GetBarWidget()->GetWindowBoundsInScreen().width(),
            new_primary_display.bounds().width());

  // The bar should be recreated on display rotation to adapt the new display
  // bounds.
  SwapPrimaryDisplay();
  const int display_width_before_rotate = GetPrimaryDisplay().bounds().width();
  EXPECT_EQ(display_width_before_rotate,
            GetBarWidget()->GetWindowBoundsInScreen().width());
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kPortraitSecondary);
  const int display_width_after_rotate = GetPrimaryDisplay().bounds().width();
  ASSERT_NE(display_width_before_rotate, display_width_after_rotate);
  EXPECT_EQ(display_width_after_rotate,
            GetBarWidget()->GetWindowBoundsInScreen().width());

  // Scale up the display, the bar should have the same width as the display
  // after scale up.
  const int display_width_before_scale_up = display_width_after_rotate;
  SendKey(ui::VKEY_OEM_MINUS, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  const int display_width_after_scale_up = GetPrimaryDisplay().bounds().width();
  EXPECT_LE(display_width_before_scale_up, display_width_after_scale_up);
  EXPECT_EQ(display_width_after_scale_up,
            GetBarWidget()->GetWindowBoundsInScreen().width());
}

// Tests bento bar's state on the pref `kBentoBarEnabled` changes. And its value
// should be independent among users.
TEST_F(PersistentDesksBarTest, UpdateBarStateOnPrefChanges) {
  const char kUser1[] = "user1@test.com";
  const char kUser2[] = "user2@test.com";
  const AccountId kUserAccount1 = AccountId::FromUserEmail(kUser1);
  const AccountId kUserAccount2 = AccountId::FromUserEmail(kUser2);

  TestSessionControllerClient* session_controller =
      GetSessionControllerClient();
  // Setup 2 users.
  session_controller->AddUserSession(kUser1, user_manager::USER_TYPE_REGULAR,
                                     /*provide_pref_service=*/false);
  session_controller->AddUserSession(kUser2, user_manager::USER_TYPE_REGULAR,
                                     /*provide_pref_service=*/false);

  auto user_1_prefs = std::make_unique<TestingPrefServiceSimple>();
  RegisterUserProfilePrefs(user_1_prefs->registry(), /*for_test=*/true);
  auto user_2_prefs = std::make_unique<TestingPrefServiceSimple>();
  RegisterUserProfilePrefs(user_2_prefs->registry(), /*for_test=*/true);
  session_controller->SetUserPrefService(kUserAccount1,
                                         std::move(user_1_prefs));
  session_controller->SetUserPrefService(kUserAccount2,
                                         std::move(user_2_prefs));

  session_controller->SwitchActiveUser(kUserAccount1);
  session_controller->SetSessionState(session_manager::SessionState::ACTIVE);
  auto* bar_controller = Shell::Get()->persistent_desks_bar_controller();
  // Toggling to hide the bar for user1.
  NewDesk();
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(bar_controller->IsEnabled());
  bar_controller->ToggleEnabledState();
  EXPECT_FALSE(GetBarWidget());
  EXPECT_FALSE(bar_controller->IsEnabled());

  // Toggling to hide the bar for user1 should not affect user2. The bar should
  // still visible for user2.
  session_controller->SwitchActiveUser(kUserAccount2);
  EXPECT_TRUE(GetBarWidget());
  EXPECT_TRUE(bar_controller->IsEnabled());

  // Switching back to user1. The bar should still be hidden.
  session_controller->SwitchActiveUser(kUserAccount1);
  EXPECT_FALSE(GetBarWidget());
  EXPECT_FALSE(bar_controller->IsEnabled());
}

// TODO(afakhry): Add more tests:
// - Always on top windows are not tracked by any desk.
// - Reusing containers when desks are removed and created.

// Instantiate the parametrized tests.
INSTANTIATE_TEST_SUITE_P(All, DesksTest, ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(All, PerDeskShelfTest, ::testing::Bool());

}  // namespace

}  // namespace ash
