// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/accessibility/magnifier/magnifier_glass.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/capture_mode/capture_label_view.h"
#include "ash/capture_mode/capture_mode_advanced_settings_test_api.h"
#include "ash/capture_mode/capture_mode_advanced_settings_view.h"
#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_button.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_menu_group.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_settings_entry_view.h"
#include "ash/capture_mode/capture_mode_settings_view.h"
#include "ash/capture_mode/capture_mode_source_view.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/capture_mode/capture_mode_type_view.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/capture_mode/fake_folder_selection_dialog_factory.h"
#include "ash/capture_mode/recording_overlay_controller.h"
#include "ash/capture_mode/stop_recording_button_tray.h"
#include "ash/capture_mode/test_capture_mode_delegate.h"
#include "ash/capture_mode/user_nudge_controller.h"
#include "ash/capture_mode/video_recording_watcher.h"
#include "ash/constants/ash_features.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/display/output_protection_delegate.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/projector/test/mock_projector_client.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/public/cpp/holding_space/holding_space_test_api.h"
#include "ash/public/cpp/projector/projector_session.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/services/recording/recording_service_test_api.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_widget_builder.h"
#include "ash/wm/cursor_manager_chromeos.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/capture_client_observer.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/core/window_modality_controller.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

constexpr char kEndRecordingReasonInClamshellHistogramName[] =
    "Ash.CaptureModeController.EndRecordingReason.ClamshellMode";
constexpr char kScreenCaptureNotificationId[] = "capture_mode_notification";

// Returns true if the software-composited cursor is enabled.
bool IsCursorCompositingEnabled() {
  return Shell::Get()
      ->window_tree_host_manager()
      ->cursor_window_controller()
      ->is_cursor_compositing_enabled();
}

void ClickOnView(const views::View* view,
                 ui::test::EventGenerator* event_generator) {
  DCHECK(view);
  DCHECK(event_generator);

  const gfx::Point view_center = view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(view_center);
  event_generator->ClickLeftButton();
}

void TouchOnView(const views::View* view,
                 ui::test::EventGenerator* event_generator) {
  DCHECK(view);
  DCHECK(event_generator);

  const gfx::Point view_center = view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveTouch(view_center);
  event_generator->PressTouch();
  event_generator->ReleaseTouch();
}

// Sends a press release key combo |count| times.
void SendKey(ui::KeyboardCode key_code,
             ui::test::EventGenerator* event_generator,
             bool shift_down = false,
             int count = 1) {
  const int flags = shift_down ? ui::EF_SHIFT_DOWN : 0;
  for (int i = 0; i < count; ++i) {
    event_generator->PressAndReleaseKey(key_code, flags);
  }
}

const message_center::Notification* GetPreviewNotification() {
  const message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  for (const auto* notification : notifications) {
    if (notification->id() == kScreenCaptureNotificationId)
      return notification;
  }
  return nullptr;
}

void ClickNotification(absl::optional<int> button_index) {
  const message_center::Notification* notification = GetPreviewNotification();
  DCHECK(notification);
  notification->delegate()->Click(button_index, absl::nullopt);
}

// Sets up a callback that will be triggered when a capture file (image or
// video) is deleted as a result of a user action. The callback will verify the
// successful deletion of the file, and will quit the given `loop`.
void SetUpFileDeletionVerifier(base::RunLoop* loop) {
  DCHECK(loop);
  CaptureModeTestApi().SetOnCaptureFileDeletedCallback(
      base::BindLambdaForTesting(
          [loop](const base::FilePath& path, bool delete_successful) {
            EXPECT_TRUE(delete_successful);
            base::ScopedAllowBlockingForTesting allow_blocking;
            EXPECT_FALSE(base::PathExists(path));
            loop->Quit();
          }));
}

// Moves the mouse and updates the cursor's display manually to imitate what a
// real mouse move event does in shell.
// TODO(crbug.com/990589): Unit tests should be able to simulate mouse input
// without having to call |CursorManager::SetDisplay|.
void MoveMouseToAndUpdateCursorDisplay(
    const gfx::Point& point,
    ui::test::EventGenerator* event_generator) {
  Shell::Get()->cursor_manager()->SetDisplay(
      display::Screen::GetScreen()->GetDisplayNearestPoint(point));
  event_generator->MoveMouseTo(point);
}

bool IsLayerStackedRightBelow(ui::Layer* layer, ui::Layer* sibling) {
  DCHECK_EQ(layer->parent(), sibling->parent());
  const auto& children = layer->parent()->children();
  const int sibling_index =
      std::find(children.begin(), children.end(), sibling) - children.begin();
  return sibling_index > 0 && children[sibling_index - 1] == layer;
}

std::unique_ptr<aura::Window> CreateTransientModalChildWindow(
    aura::Window* transient_parent,
    const gfx::Rect& bounds) {
  auto child =
      std::make_unique<aura::Window>(nullptr, aura::client::WINDOW_TYPE_POPUP);
  child->Init(ui::LAYER_NOT_DRAWN);
  child->SetBounds(bounds);
  wm::AddTransientChild(transient_parent, child.get());
  aura::client::ParentWindowWithContext(
      child.get(), transient_parent->GetRootWindow(), bounds);
  child->Show();

  child->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
  wm::SetModalParent(child.get(), transient_parent);
  return child;
}

// To avoid flaky failures due to mouse devices blocking entering tablet mode,
// we detach all mouse devices. This shouldn't affect testing the cursor
// status.
void SwitchToTabletMode() {
  TabletModeControllerTestApi test_api;
  test_api.DetachAllMice();
  test_api.EnterTabletMode();
}

void LeaveTabletMode() {
  TabletModeControllerTestApi test_api;
  test_api.LeaveTabletMode();
}

// Defines a capture client observer, that sets the input capture to the window
// given to the constructor, and destroys it once capture is lost.
class TestCaptureClientObserver : public aura::client::CaptureClientObserver {
 public:
  explicit TestCaptureClientObserver(std::unique_ptr<aura::Window> window)
      : window_(std::move(window)) {
    DCHECK(window_);
    auto* capture_client =
        aura::client::GetCaptureClient(window_->GetRootWindow());
    capture_client->SetCapture(window_.get());
    capture_client->AddObserver(this);
  }

  ~TestCaptureClientObserver() override { StopObserving(); }

  // aura::client::CaptureClientObserver:
  void OnCaptureChanged(aura::Window* lost_capture,
                        aura::Window* gained_capture) override {
    if (lost_capture != window_.get())
      return;

    StopObserving();
    window_.reset();
  }

 private:
  void StopObserving() {
    if (!window_)
      return;

    auto* capture_client =
        aura::client::GetCaptureClient(window_->GetRootWindow());
    capture_client->RemoveObserver(this);
  }

  std::unique_ptr<aura::Window> window_;
};

}  // namespace

// Wrapper for CaptureModeSession that exposes internal state to test functions.
class CaptureModeSessionTestApi {
 public:
  explicit CaptureModeSessionTestApi(CaptureModeSession* session)
      : session_(session) {}
  CaptureModeSessionTestApi(const CaptureModeSessionTestApi&) = delete;
  CaptureModeSessionTestApi& operator=(const CaptureModeSessionTestApi&) =
      delete;
  ~CaptureModeSessionTestApi() = default;

  CaptureModeBarView* capture_mode_bar_view() const {
    return session_->capture_mode_bar_view_;
  }

  CaptureModeSettingsView* capture_mode_settings_view() const {
    return session_->capture_mode_settings_view_;
  }

  CaptureModeAdvancedSettingsView* capture_mode_advanced_settings_view() const {
    EXPECT_TRUE(features::AreImprovedScreenCaptureSettingsEnabled());
    return session_->capture_mode_advanced_settings_view_;
  }

  views::Widget* capture_mode_settings_widget() const {
    return session_->capture_mode_settings_widget_.get();
  }

  views::Widget* capture_label_widget() const {
    return session_->capture_label_widget_.get();
  }

  views::Widget* dimensions_label_widget() const {
    return session_->dimensions_label_widget_.get();
  }

  UserNudgeController* user_nudge_controller() const {
    return session_->user_nudge_controller_.get();
  }

  const MagnifierGlass& magnifier_glass() const {
    return session_->magnifier_glass_;
  }

  bool IsUsingCustomCursor(CaptureModeType type) const {
    return session_->IsUsingCustomCursor(type);
  }

  CaptureModeSessionFocusCycler::FocusGroup GetCurrentFocusGroup() const {
    return session_->focus_cycler_->current_focus_group_;
  }

  size_t GetCurrentFocusIndex() const {
    return session_->focus_cycler_->focus_index_;
  }

  CaptureModeSessionFocusCycler::HighlightableView* GetCurrentFocusedView() {
    return session_->focus_cycler_->GetGroupItems(
        GetCurrentFocusGroup())[GetCurrentFocusIndex()];
  }

  bool HasFocus() const { return session_->focus_cycler_->HasFocus(); }

  bool IsFolderSelectionDialogShown() const {
    return session_->folder_selection_dialog_controller_ &&
           session_->folder_selection_dialog_controller_->dialog_window();
  }

  bool IsAllUisVisible() const { return session_->is_all_uis_visible_; }

 private:
  const CaptureModeSession* const session_;
};

class CaptureModeTest : public AshTestBase {
 public:
  CaptureModeTest() = default;
  explicit CaptureModeTest(base::test::TaskEnvironment::TimeSource time)
      : AshTestBase(time) {}
  CaptureModeTest(const CaptureModeTest&) = delete;
  CaptureModeTest& operator=(const CaptureModeTest&) = delete;
  ~CaptureModeTest() override = default;

  CaptureModeBarView* GetCaptureModeBarView() const {
    auto* session = CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    return CaptureModeSessionTestApi(session).capture_mode_bar_view();
  }

  views::Widget* GetCaptureModeBarWidget() const {
    auto* session = CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    return session->capture_mode_bar_widget();
  }

  views::Widget* GetCaptureModeLabelWidget() const {
    auto* session = CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    return CaptureModeSessionTestApi(session).capture_label_widget();
  }

  CaptureModeSettingsView* GetCaptureModeSettingsView() const {
    auto* session = CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    return CaptureModeSessionTestApi(session).capture_mode_settings_view();
  }

  views::Widget* GetCaptureModeSettingsWidget() const {
    auto* session = CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    return CaptureModeSessionTestApi(session).capture_mode_settings_widget();
  }

  bool IsFolderSelectionDialogShown() const {
    auto* session = CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    return CaptureModeSessionTestApi(session).IsFolderSelectionDialogShown();
  }

  bool IsAllCaptureSessionUisVisible() const {
    auto* session = CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    return CaptureModeSessionTestApi(session).IsAllUisVisible();
  }

  CaptureModeToggleButton* GetImageToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return GetCaptureModeBarView()->capture_type_view()->image_toggle_button();
  }

  CaptureModeToggleButton* GetVideoToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return GetCaptureModeBarView()->capture_type_view()->video_toggle_button();
  }

  CaptureModeToggleButton* GetFullscreenToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return GetCaptureModeBarView()
        ->capture_source_view()
        ->fullscreen_toggle_button();
  }

  CaptureModeToggleButton* GetRegionToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return GetCaptureModeBarView()
        ->capture_source_view()
        ->region_toggle_button();
  }

  CaptureModeToggleButton* GetWindowToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return GetCaptureModeBarView()
        ->capture_source_view()
        ->window_toggle_button();
  }

  CaptureModeToggleButton* GetSettingsButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return GetCaptureModeBarView()->settings_button();
  }

  CaptureModeButton* GetCloseButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return GetCaptureModeBarView()->close_button();
  }

  views::ToggleButton* GetMicrophoneToggle() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    DCHECK(GetCaptureModeSettingsView());
    return GetCaptureModeSettingsView()
        ->microphone_view()
        ->toggle_button_view();
  }

  aura::Window* GetDimensionsLabelWindow() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    auto* widget = CaptureModeSessionTestApi(controller->capture_mode_session())
                       .dimensions_label_widget();
    return widget ? widget->GetNativeWindow() : nullptr;
  }

  absl::optional<gfx::Point> GetMagnifierGlassCenterPoint() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    auto& magnifier =
        CaptureModeSessionTestApi(controller->capture_mode_session())
            .magnifier_glass();
    if (magnifier.host_widget_for_testing()) {
      return magnifier.host_widget_for_testing()
          ->GetWindowBoundsInScreen()
          .CenterPoint();
    }
    return absl::nullopt;
  }

  CaptureModeController* StartCaptureSession(CaptureModeSource source,
                                             CaptureModeType type) {
    auto* controller = CaptureModeController::Get();
    controller->SetSource(source);
    controller->SetType(type);
    controller->Start(CaptureModeEntryType::kQuickSettings);
    DCHECK(controller->IsActive());
    return controller;
  }

  void StartVideoRecordingImmediately() {
    CaptureModeController::Get()->StartVideoRecordingImmediatelyForTesting();
    WaitForRecordingToStart();
  }

  // Start Capture Mode with source region and type image.
  CaptureModeController* StartImageRegionCapture() {
    return StartCaptureSession(CaptureModeSource::kRegion,
                               CaptureModeType::kImage);
  }

  CaptureModeController* StartSessionAndRecordWindow(aura::Window* window) {
    auto* controller = StartCaptureSession(CaptureModeSource::kWindow,
                                           CaptureModeType::kVideo);
    GetEventGenerator()->MoveMouseToCenterOf(window);
    StartVideoRecordingImmediately();
    EXPECT_TRUE(controller->is_recording_in_progress());
    return controller;
  }

  // Select a region by pressing and dragging the mouse.
  void SelectRegion(const gfx::Rect& region, bool release_mouse = true) {
    auto* controller = CaptureModeController::Get();
    ASSERT_TRUE(controller->IsActive());
    ASSERT_EQ(CaptureModeSource::kRegion, controller->source());
    auto* event_generator = GetEventGenerator();
    event_generator->set_current_screen_location(region.origin());
    event_generator->PressLeftButton();
    event_generator->MoveMouseTo(region.bottom_right());
    if (release_mouse)
      event_generator->ReleaseLeftButton();
    EXPECT_EQ(region, controller->user_capture_region());
  }

  void WaitForSessionToEnd() {
    auto* controller = CaptureModeController::Get();
    if (!controller->IsActive())
      return;

    auto* test_delegate = static_cast<TestCaptureModeDelegate*>(
        controller->delegate_for_testing());
    ASSERT_TRUE(test_delegate);
    base::RunLoop run_loop;
    test_delegate->set_on_session_state_changed_callback(
        run_loop.QuitClosure());
    run_loop.Run();
    ASSERT_FALSE(controller->IsActive());
  }

  void RemoveSecondaryDisplay() {
    const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
    display::ManagedDisplayInfo primary_info =
        display_manager()->GetDisplayInfo(primary_id);
    std::vector<display::ManagedDisplayInfo> display_info_list;
    display_info_list.push_back(primary_info);
    display_manager()->OnNativeDisplaysChanged(display_info_list);

    // Spin the run loop so that we get a signal that the associated root window
    // of the removed display is destroyed.
    base::RunLoop().RunUntilIdle();
  }

  void SwitchToUser2() {
    auto* session_controller = GetSessionControllerClient();
    constexpr char kUserEmail[] = "user2@capture_mode";
    session_controller->AddUserSession(kUserEmail);
    session_controller->SwitchActiveUser(AccountId::FromUserEmail(kUserEmail));
  }

  void WaitForSeconds(int seconds) {
    base::RunLoop loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, loop.QuitClosure(), base::Seconds(seconds));
    loop.Run();
  }

  base::FilePath WaitForCaptureFileToBeSaved() {
    base::FilePath result;
    base::RunLoop run_loop;
    ash::CaptureModeTestApi().SetOnCaptureFileSavedCallback(
        base::BindLambdaForTesting([&](const base::FilePath& path) {
          result = path;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  void WaitForRecordingToStart() {
    auto* controller = CaptureModeController::Get();
    if (controller->is_recording_in_progress())
      return;
    auto* test_delegate = static_cast<TestCaptureModeDelegate*>(
        controller->delegate_for_testing());
    ASSERT_TRUE(test_delegate);
    base::RunLoop run_loop;
    test_delegate->set_on_recording_started_callback(run_loop.QuitClosure());
    run_loop.Run();
    ASSERT_TRUE(controller->is_recording_in_progress());
  }

  base::FilePath CreateCustomFolder(const std::string& custom_folder_name) {
    base::FilePath custom_folder = CaptureModeController::Get()
                                       ->delegate_for_testing()
                                       ->GetUserDefaultDownloadsFolder()
                                       .Append(custom_folder_name);
    base::ScopedAllowBlockingForTesting allow_blocking;
    const bool result = base::CreateDirectory(custom_folder);
    DCHECK(result);
    return custom_folder;
  }

  base::FilePath CreateFolderOnDriveFS(const std::string& custom_folder_name) {
    auto* test_delegate = CaptureModeController::Get()->delegate_for_testing();
    base::FilePath mount_point_path;
    EXPECT_TRUE(test_delegate->GetDriveFsMountPointPath(&mount_point_path));
    base::FilePath folder_on_drive_fs =
        mount_point_path.Append("root").Append(custom_folder_name);
    base::ScopedAllowBlockingForTesting allow_blocking;
    const bool result = base::CreateDirectory(folder_on_drive_fs);
    EXPECT_TRUE(result);
    return folder_on_drive_fs;
  }

  std::string GetCaptureModeHistogramName(std::string prefix) {
    prefix.append(Shell::Get()->IsInTabletMode() ? ".TabletMode"
                                                 : ".ClamshellMode");
    return prefix;
  }
};

class CaptureSessionWidgetObserver : public views::WidgetObserver {
 public:
  explicit CaptureSessionWidgetObserver(views::Widget* widget) {
    DCHECK(widget);
    observer_.Observe(widget);
  }
  CaptureSessionWidgetObserver(const CaptureSessionWidgetObserver&) = delete;
  CaptureSessionWidgetObserver& operator=(const CaptureSessionWidgetObserver&) =
      delete;
  ~CaptureSessionWidgetObserver() override = default;

  bool GetWidgetDestroyed() const { return !observer_.IsObserving(); }

  // views::WidgetObserver
  void OnWidgetClosing(views::Widget* widget) override {
    DCHECK(observer_.IsObservingSource(widget));
    observer_.Reset();
  }

 private:
  base::ScopedObservation<views::Widget, views::WidgetObserver> observer_{this};
};

class CaptureNotificationWaiter : public message_center::MessageCenterObserver {
 public:
  CaptureNotificationWaiter() {
    message_center::MessageCenter::Get()->AddObserver(this);
  }
  ~CaptureNotificationWaiter() override {
    message_center::MessageCenter::Get()->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override {
    if (notification_id == kScreenCaptureNotificationId)
      run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
};

TEST_F(CaptureModeTest, StartStop) {
  auto* controller = CaptureModeController::Get();
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_TRUE(controller->IsActive());
  // Calling start again is a no-op.
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_TRUE(controller->IsActive());

  // Closing the session should close the native window of capture mode bar
  // immediately.
  auto* bar_window = GetCaptureModeBarWidget()->GetNativeWindow();
  aura::WindowTracker tracker({bar_window});
  controller->Stop();
  EXPECT_TRUE(tracker.windows().empty());
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(CaptureModeTest, CheckCursorVisibility) {
  // Hide cursor before entering capture mode.
  auto* cursor_manager = Shell::Get()->cursor_manager();
  cursor_manager->SetCursor(ui::mojom::CursorType::kPointer);
  cursor_manager->HideCursor();
  cursor_manager->DisableMouseEvents();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());

  auto* controller = CaptureModeController::Get();
  controller->Start(CaptureModeEntryType::kQuickSettings);
  // After capture mode initialization, cursor should be visible.
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsMouseEventsEnabled());

  // Enter tablet mode.
  SwitchToTabletMode();
  // After entering tablet mode, cursor should be invisible and locked.
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Leave tablet mode, cursor should be visible again.
  LeaveTabletMode();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
}

TEST_F(CaptureModeTest, CheckCursorVisibilityOnTabletMode) {
  auto* cursor_manager = Shell::Get()->cursor_manager();

  // Enter tablet mode.
  SwitchToTabletMode();
  // After entering tablet mode, cursor should be invisible.
  EXPECT_FALSE(cursor_manager->IsCursorVisible());

  // Open capture mode.
  auto* controller = CaptureModeController::Get();
  controller->Start(CaptureModeEntryType::kQuickSettings);
  // Cursor should be invisible since it's still in tablet mode.
  EXPECT_FALSE(cursor_manager->IsCursorVisible());

  // Leave tablet mode, cursor should be visible now.
  LeaveTabletMode();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
}

// Regression test for https://crbug.com/1172425.
TEST_F(CaptureModeTest, NoCrashOnClearingCapture) {
  TestCaptureClientObserver observer(CreateTestWindow(gfx::Rect(200, 200)));
  auto* controller = StartImageRegionCapture();
  EXPECT_TRUE(controller->IsActive());
}

TEST_F(CaptureModeTest, CheckWidgetClosed) {
  auto* controller = CaptureModeController::Get();
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_TRUE(controller->IsActive());
  EXPECT_TRUE(GetCaptureModeBarWidget());
  CaptureSessionWidgetObserver observer(GetCaptureModeBarWidget());
  EXPECT_FALSE(observer.GetWidgetDestroyed());
  controller->Stop();
  EXPECT_FALSE(controller->IsActive());
  EXPECT_FALSE(controller->capture_mode_session());
  // The Widget should have been destroyed by now.
  EXPECT_TRUE(observer.GetWidgetDestroyed());
}

TEST_F(CaptureModeTest, StartWithMostRecentTypeAndSource) {
  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kFullscreen);
  controller->SetType(CaptureModeType::kVideo);
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_TRUE(controller->IsActive());

  EXPECT_FALSE(GetImageToggleButton()->GetToggled());
  EXPECT_TRUE(GetVideoToggleButton()->GetToggled());
  EXPECT_TRUE(GetFullscreenToggleButton()->GetToggled());
  EXPECT_FALSE(GetRegionToggleButton()->GetToggled());
  EXPECT_FALSE(GetWindowToggleButton()->GetToggled());

  ClickOnView(GetCloseButton(), GetEventGenerator());
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(CaptureModeTest, ChangeTypeAndSourceFromUI) {
  auto* controller = CaptureModeController::Get();
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_TRUE(controller->IsActive());

  EXPECT_TRUE(GetImageToggleButton()->GetToggled());
  EXPECT_FALSE(GetVideoToggleButton()->GetToggled());
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetVideoToggleButton(), event_generator);
  EXPECT_FALSE(GetImageToggleButton()->GetToggled());
  EXPECT_TRUE(GetVideoToggleButton()->GetToggled());
  EXPECT_EQ(controller->type(), CaptureModeType::kVideo);

  ClickOnView(GetWindowToggleButton(), event_generator);
  EXPECT_FALSE(GetFullscreenToggleButton()->GetToggled());
  EXPECT_FALSE(GetRegionToggleButton()->GetToggled());
  EXPECT_TRUE(GetWindowToggleButton()->GetToggled());
  EXPECT_EQ(controller->source(), CaptureModeSource::kWindow);

  ClickOnView(GetFullscreenToggleButton(), event_generator);
  EXPECT_TRUE(GetFullscreenToggleButton()->GetToggled());
  EXPECT_FALSE(GetRegionToggleButton()->GetToggled());
  EXPECT_FALSE(GetWindowToggleButton()->GetToggled());
  EXPECT_EQ(controller->source(), CaptureModeSource::kFullscreen);
}

TEST_F(CaptureModeTest, VideoRecordingUiBehavior) {
  // Start Capture Mode in a fullscreen video recording mode.
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  EXPECT_TRUE(controller->IsActive());
  EXPECT_FALSE(controller->is_recording_in_progress());
  EXPECT_FALSE(IsCursorCompositingEnabled());

  // Hit Enter to begin recording.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_RETURN, event_generator);
  EXPECT_EQ(ui::mojom::CursorType::kPointer,
            Shell::Get()->cursor_manager()->GetCursor().type());
  WaitForRecordingToStart();
  EXPECT_FALSE(controller->IsActive());
  EXPECT_TRUE(controller->is_recording_in_progress());

  // The composited cursor should remain disabled now that we're using the
  // cursor overlay on the capturer. The stop-recording button should show up in
  // the status area widget.
  EXPECT_FALSE(IsCursorCompositingEnabled());
  auto* stop_recording_button = Shell::GetPrimaryRootWindowController()
                                    ->GetStatusAreaWidget()
                                    ->stop_recording_button_tray();
  EXPECT_TRUE(stop_recording_button->visible_preferred());

  // End recording via the stop-recording button. Expect that it's now hidden.
  base::HistogramTester histogram_tester;
  ClickOnView(stop_recording_button, event_generator);
  EXPECT_FALSE(stop_recording_button->visible_preferred());
  EXPECT_FALSE(controller->is_recording_in_progress());
  EXPECT_FALSE(IsCursorCompositingEnabled());
  histogram_tester.ExpectBucketCount(
      kEndRecordingReasonInClamshellHistogramName,
      EndRecordingReason::kStopRecordingButton, 1);
}

// Tests the behavior of repositioning a region with capture mode.
TEST_F(CaptureModeTest, CaptureRegionRepositionBehavior) {
  // Use a set display size as we will be choosing points in this test.
  UpdateDisplay("800x700");

  auto* controller = StartImageRegionCapture();

  // The first time selecting a region, the region is a default rect.
  EXPECT_EQ(gfx::Rect(), controller->user_capture_region());

  // Press down and drag to select a region.
  SelectRegion(gfx::Rect(100, 100, 600, 600));

  // Click somewhere in the center on the region and drag. The whole region
  // should move. Note that the point cannot be in the capture button bounds,
  // which is located in the center of the region.
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(gfx::Point(200, 200));
  event_generator->DragMouseBy(-50, -50);
  EXPECT_EQ(gfx::Rect(50, 50, 600, 600), controller->user_capture_region());

  // Try to drag the region offscreen. The region should be bound by the display
  // size.
  event_generator->set_current_screen_location(gfx::Point(100, 100));
  event_generator->DragMouseBy(-150, -150);
  EXPECT_EQ(gfx::Rect(600, 600), controller->user_capture_region());
}

// Tests the behavior of resizing a region with capture mode using the corner
// drag affordances.
TEST_F(CaptureModeTest, CaptureRegionCornerResizeBehavior) {
  // Use a set display size as we will be choosing points in this test.
  UpdateDisplay("800x700");

  auto* controller = StartImageRegionCapture();
  // Create the initial region.
  const gfx::Rect target_region(gfx::Rect(200, 200, 400, 400));
  SelectRegion(target_region);

  // For each corner point try dragging to several points and verify that the
  // capture region is as expected.
  struct {
    std::string trace;
    gfx::Point drag_point;
    // The point that stays the same while dragging. It is the opposite vertex
    // to |drag_point| on |target_region|.
    gfx::Point anchor_point;
  } kDragCornerCases[] = {
      {"origin", target_region.origin(), target_region.bottom_right()},
      {"top_right", target_region.top_right(), target_region.bottom_left()},
      {"bottom_right", target_region.bottom_right(), target_region.origin()},
      {"bottom_left", target_region.bottom_left(), target_region.top_right()},
  };

  // The test corner points are one in each corner outside |target_region| and
  // one point inside |target_region|.
  auto drag_test_points = {gfx::Point(100, 100), gfx::Point(700, 100),
                           gfx::Point(700, 700), gfx::Point(100, 700),
                           gfx::Point(400, 400)};
  auto* event_generator = GetEventGenerator();
  for (auto test_case : kDragCornerCases) {
    SCOPED_TRACE(test_case.trace);
    event_generator->set_current_screen_location(test_case.drag_point);
    event_generator->PressLeftButton();

    // At each drag test point, the region rect should be the rect created by
    // the given |corner_point| and the drag test point. That is, the width
    // should match the x distance between the two points, the height should
    // match the y distance between the two points and that both points are
    // contained in the region.
    for (auto drag_test_point : drag_test_points) {
      event_generator->MoveMouseTo(drag_test_point);
      gfx::Rect region = controller->user_capture_region();
      const gfx::Vector2d distance = test_case.anchor_point - drag_test_point;
      EXPECT_EQ(std::abs(distance.x()), region.width());
      EXPECT_EQ(std::abs(distance.y()), region.height());

      // gfx::Rect::Contains returns the point (x+width, y+height) as false, so
      // make the region one unit bigger to account for this.
      region.Inset(gfx::Insets(-1));
      EXPECT_TRUE(region.Contains(drag_test_point));
      EXPECT_TRUE(region.Contains(test_case.anchor_point));
    }

    // Make sure the region is reset for the next iteration.
    event_generator->MoveMouseTo(test_case.drag_point);
    event_generator->ReleaseLeftButton();
    ASSERT_EQ(target_region, controller->user_capture_region());
  }
}

// Tests the behavior of resizing a region with capture mode using the edge drag
// affordances.
TEST_F(CaptureModeTest, CaptureRegionEdgeResizeBehavior) {
  // Use a set display size as we will be choosing points in this test.
  UpdateDisplay("800x700");

  auto* controller = StartImageRegionCapture();
  // Create the initial region.
  const gfx::Rect target_region(gfx::Rect(200, 200, 200, 200));
  SelectRegion(target_region);

  // For each edge point try dragging to several points and verify that the
  // capture region is as expected.
  struct {
    std::string trace;
    gfx::Point drag_point;
    // True if horizontal direction (left, right). Height stays the same while
    // dragging if true, width stays the same while dragging if false.
    bool horizontal;
    // The edge that stays the same while dragging. It is the opposite edge to
    // |drag_point|. For example, if |drag_point| is the left center of
    // |target_region|, then |anchor_edge| is the right edge.
    int anchor_edge;
  } kDragEdgeCases[] = {
      {"left", target_region.left_center(), true, target_region.right()},
      {"top", target_region.top_center(), false, target_region.bottom()},
      {"right", target_region.right_center(), true, target_region.x()},
      {"bottom", target_region.bottom_center(), false, target_region.y()},
  };

  // Drag to a couple of points that change both x and y. In all these cases,
  // only the width or height should change.
  auto drag_test_points = {gfx::Point(150, 150), gfx::Point(350, 350),
                           gfx::Point(450, 450)};
  auto* event_generator = GetEventGenerator();
  for (auto test_case : kDragEdgeCases) {
    SCOPED_TRACE(test_case.trace);
    event_generator->set_current_screen_location(test_case.drag_point);
    event_generator->PressLeftButton();

    for (auto drag_test_point : drag_test_points) {
      event_generator->MoveMouseTo(drag_test_point);
      const gfx::Rect region = controller->user_capture_region();

      // One of width/height will always be the same as |target_region|'s
      // initial width/height, depending on the edge affordance. The other
      // dimension will be the distance from |drag_test_point| to the anchor
      // edge.
      const int variable_length = std::abs(
          (test_case.horizontal ? drag_test_point.x() : drag_test_point.y()) -
          test_case.anchor_edge);
      const int expected_width =
          test_case.horizontal ? variable_length : target_region.width();
      const int expected_height =
          test_case.horizontal ? target_region.height() : variable_length;

      EXPECT_EQ(expected_width, region.width());
      EXPECT_EQ(expected_height, region.height());
    }

    // Make sure the region is reset for the next iteration.
    event_generator->MoveMouseTo(test_case.drag_point);
    event_generator->ReleaseLeftButton();
    ASSERT_EQ(target_region, controller->user_capture_region());
  }
}

// Tests that the capture region persists after exiting and reentering capture
// mode.
TEST_F(CaptureModeTest, CaptureRegionPersistsAfterExit) {
  auto* controller = StartImageRegionCapture();
  const gfx::Rect region(100, 100, 200, 200);
  SelectRegion(region);

  controller->Stop();
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_EQ(region, controller->user_capture_region());
}

// Tests that the capture region resets when clicking outside the current
// capture regions bounds.
TEST_F(CaptureModeTest, CaptureRegionResetsOnClickOutside) {
  auto* controller = StartImageRegionCapture();
  SelectRegion(gfx::Rect(100, 100, 200, 200));

  // Click on an area outside of the current capture region. The capture region
  // should reset to default rect.
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(gfx::Point(400, 400));
  event_generator->ClickLeftButton();
  EXPECT_EQ(gfx::Rect(), controller->user_capture_region());
}

// Tests that buttons on the capture mode bar still work when a region is
// "covering" them.
TEST_F(CaptureModeTest, CaptureRegionCoversCaptureModeBar) {
  UpdateDisplay("800x700");

  auto* controller = StartImageRegionCapture();

  // Select a region such that the capture mode bar is covered.
  SelectRegion(gfx::Rect(5, 5, 795, 795));
  EXPECT_TRUE(controller->user_capture_region().Contains(
      GetCaptureModeBarView()->GetBoundsInScreen()));

  // Click on the fullscreen toggle button to verify that we enter fullscreen
  // capture mode. Then click on the region toggle button to verify that we
  // reenter region capture mode and that the region is still covering the
  // capture mode bar.
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetFullscreenToggleButton(), event_generator);
  EXPECT_EQ(CaptureModeSource::kFullscreen, controller->source());
  ClickOnView(GetRegionToggleButton(), GetEventGenerator());
  ASSERT_EQ(CaptureModeSource::kRegion, controller->source());
  ASSERT_TRUE(controller->user_capture_region().Contains(
      GetCaptureModeBarView()->GetBoundsInScreen()));

  ClickOnView(GetCloseButton(), event_generator);
  EXPECT_FALSE(controller->IsActive());
}

// Tests that the magnifying glass appears while fine tuning the capture region,
// and that the cursor is hidden if the magnifying glass is present.
TEST_F(CaptureModeTest, CaptureRegionMagnifierWhenFineTuning) {
  const gfx::Vector2d kDragDelta(50, 50);
  UpdateDisplay("800x700");

  // Start Capture Mode in a region in image mode.
  StartImageRegionCapture();

  // Press down and drag to select a region. The magnifier should not be
  // visible yet.
  gfx::Rect capture_region{200, 200, 400, 400};
  SelectRegion(capture_region);
  EXPECT_EQ(absl::nullopt, GetMagnifierGlassCenterPoint());

  auto check_magnifier_shows_properly = [this](const gfx::Point& origin,
                                               const gfx::Point& destination,
                                               bool should_show_magnifier) {
    // If |should_show_magnifier|, check that the magnifying glass is centered
    // on the mouse after press and during drag, and that the cursor is hidden.
    // If not |should_show_magnifier|, check that the magnifying glass never
    // shows. Should always be not visible when mouse button is released.
    auto* event_generator = GetEventGenerator();
    absl::optional<gfx::Point> expected_origin =
        should_show_magnifier ? absl::make_optional(origin) : absl::nullopt;
    absl::optional<gfx::Point> expected_destination =
        should_show_magnifier ? absl::make_optional(destination)
                              : absl::nullopt;

    auto* cursor_manager = Shell::Get()->cursor_manager();
    EXPECT_TRUE(cursor_manager->IsCursorVisible());

    // Move cursor to |origin| and click.
    event_generator->set_current_screen_location(origin);
    event_generator->PressLeftButton();
    EXPECT_EQ(expected_origin, GetMagnifierGlassCenterPoint());
    EXPECT_NE(should_show_magnifier, cursor_manager->IsCursorVisible());

    // Drag to |destination| while holding left button.
    event_generator->MoveMouseTo(destination);
    EXPECT_EQ(expected_destination, GetMagnifierGlassCenterPoint());
    EXPECT_NE(should_show_magnifier, cursor_manager->IsCursorVisible());

    // Drag back to |origin| while still holding left button.
    event_generator->MoveMouseTo(origin);
    EXPECT_EQ(expected_origin, GetMagnifierGlassCenterPoint());
    EXPECT_NE(should_show_magnifier, cursor_manager->IsCursorVisible());

    // Release left button.
    event_generator->ReleaseLeftButton();
    EXPECT_EQ(absl::nullopt, GetMagnifierGlassCenterPoint());
    EXPECT_TRUE(cursor_manager->IsCursorVisible());
  };

  // Drag the capture region from within the existing selected region. The
  // magnifier should not be visible at any point.
  check_magnifier_shows_properly(gfx::Point(400, 250), gfx::Point(500, 350),
                                 /*should_show_magnifier=*/false);

  // Check that each corner fine tune position shows the magnifier when
  // dragging.
  struct {
    std::string trace;
    FineTunePosition position;
  } kFineTunePositions[] = {{"top_left", FineTunePosition::kTopLeft},
                            {"top_right", FineTunePosition::kTopRight},
                            {"bottom_right", FineTunePosition::kBottomRight},
                            {"bottom_left", FineTunePosition::kBottomLeft}};
  for (const auto& fine_tune_position : kFineTunePositions) {
    SCOPED_TRACE(fine_tune_position.trace);
    const gfx::Point drag_affordance_location =
        capture_mode_util::GetLocationForFineTunePosition(
            capture_region, fine_tune_position.position);
    check_magnifier_shows_properly(drag_affordance_location,
                                   drag_affordance_location + kDragDelta,
                                   /*should_show_magnifier=*/true);
  }
}

// Tests that the dimensions label properly renders for capture regions.
TEST_F(CaptureModeTest, CaptureRegionDimensionsLabelLocation) {
  UpdateDisplay("900x800");

  // Start Capture Mode in a region in image mode.
  StartImageRegionCapture();

  // Press down and don't move the mouse. Label shouldn't display for empty
  // capture regions.
  auto* generator = GetEventGenerator();
  generator->set_current_screen_location(gfx::Point(0, 0));
  generator->PressLeftButton();
  auto* controller = CaptureModeController::Get();
  EXPECT_TRUE(controller->IsActive());
  EXPECT_TRUE(controller->user_capture_region().IsEmpty());
  EXPECT_EQ(nullptr, GetDimensionsLabelWindow());
  generator->ReleaseLeftButton();

  // Press down and drag to select a large region. Verify that the dimensions
  // label is centered and that the label is below the capture region.
  gfx::Rect capture_region{100, 100, 600, 200};
  SelectRegion(capture_region, /*release_mouse=*/false);
  EXPECT_EQ(capture_region.CenterPoint().x(),
            GetDimensionsLabelWindow()->bounds().CenterPoint().x());
  EXPECT_EQ(capture_region.bottom() +
                CaptureModeSession::kSizeLabelYDistanceFromRegionDp,
            GetDimensionsLabelWindow()->bounds().y());
  generator->ReleaseLeftButton();
  EXPECT_EQ(nullptr, GetDimensionsLabelWindow());

  // Create a new capture region close to the left side of the screen such that
  // if the label was centered it would extend out of the screen.
  // The x value of the label should be the left edge of the screen (0).
  capture_region.SetRect(2, 100, 2, 100);
  SelectRegion(capture_region, /*release_mouse=*/false);
  EXPECT_EQ(0, GetDimensionsLabelWindow()->bounds().x());
  generator->ReleaseLeftButton();
  EXPECT_EQ(nullptr, GetDimensionsLabelWindow());

  // Create a new capture region close to the right side of the screen such that
  // if the label was centered it would extend out of the screen.
  // The right (x + width) of the label should be the right edge of the screen
  // (900).
  capture_region.SetRect(896, 100, 2, 100);
  SelectRegion(capture_region, /*release_mouse=*/false);
  EXPECT_EQ(900, GetDimensionsLabelWindow()->bounds().right());
  generator->ReleaseLeftButton();
  EXPECT_EQ(nullptr, GetDimensionsLabelWindow());

  // Create a new capture region close to the bottom side of the screen.
  // The label should now appear inside the capture region, just above the
  // bottom edge. It should be above the bottom of the screen as well.
  capture_region.SetRect(100, 700, 600, 790);
  SelectRegion(capture_region, /*release_mouse=*/false);
  EXPECT_EQ(800 - CaptureModeSession::kSizeLabelYDistanceFromRegionDp,
            GetDimensionsLabelWindow()->bounds().bottom());
  generator->ReleaseLeftButton();
  EXPECT_EQ(nullptr, GetDimensionsLabelWindow());
}

TEST_F(CaptureModeTest, CaptureRegionCaptureButtonLocation) {
  UpdateDisplay("900x800");

  auto* controller = StartImageRegionCapture();

  // Select a large region. Verify that the capture button widget is centered.
  SelectRegion(gfx::Rect(100, 100, 600, 600));

  views::Widget* capture_button_widget = GetCaptureModeLabelWidget();
  ASSERT_TRUE(capture_button_widget);
  aura::Window* capture_button_window =
      capture_button_widget->GetNativeWindow();
  EXPECT_EQ(gfx::Point(400, 400),
            capture_button_window->bounds().CenterPoint());

  // Drag the bottom corner so that the region is too small to fit the capture
  // button. Verify that the button is aligned horizontally and placed below the
  // region.
  auto* event_generator = GetEventGenerator();
  event_generator->DragMouseTo(gfx::Point(120, 120));
  EXPECT_EQ(gfx::Rect(100, 100, 20, 20), controller->user_capture_region());
  EXPECT_EQ(110, capture_button_window->bounds().CenterPoint().x());
  const int distance_from_region =
      CaptureModeSession::kCaptureButtonDistanceFromRegionDp;
  EXPECT_EQ(120 + distance_from_region, capture_button_window->bounds().y());

  // Click inside the region to drag the entire region to the bottom of the
  // screen. Verify that the button is aligned horizontally and placed above the
  // region.
  event_generator->set_current_screen_location(gfx::Point(110, 110));
  event_generator->DragMouseTo(gfx::Point(110, 790));
  EXPECT_EQ(gfx::Rect(100, 780, 20, 20), controller->user_capture_region());
  EXPECT_EQ(110, capture_button_window->bounds().CenterPoint().x());
  EXPECT_EQ(780 - distance_from_region,
            capture_button_window->bounds().bottom());
}

// Tests some edge cases to ensure the capture button does not intersect the
// capture bar and end up unclickable since it is stacked below the capture bar.
// Regression test for https://crbug.com/1186462.
TEST_F(CaptureModeTest, CaptureRegionCaptureButtonDoesNotIntersectCaptureBar) {
  UpdateDisplay("800x700");

  StartImageRegionCapture();

  // Create a region that would cover the capture mode bar. Add some insets to
  // ensure that the capture button could fit inside. Verify that the two
  // widgets do not overlap.
  const gfx::Rect capture_bar_bounds =
      GetCaptureModeBarWidget()->GetWindowBoundsInScreen();
  gfx::Rect region_bounds = capture_bar_bounds;
  region_bounds.Inset(-20, -20);
  SelectRegion(region_bounds);
  EXPECT_FALSE(capture_bar_bounds.Intersects(
      GetCaptureModeLabelWidget()->GetWindowBoundsInScreen()));

  // Create a thin region above the capture mode bar. The algorithm would
  // normally place the capture label under the region, but should adjust to
  // avoid intersecting.
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(gfx::Point());
  event_generator->ClickLeftButton();
  const int capture_bar_midpoint_x = capture_bar_bounds.CenterPoint().x();
  SelectRegion(
      gfx::Rect(capture_bar_midpoint_x, capture_bar_bounds.y() - 10, 20, 10));
  EXPECT_FALSE(capture_bar_bounds.Intersects(
      GetCaptureModeLabelWidget()->GetWindowBoundsInScreen()));

  // Create a thin region below the capture mode bar which reaches the bottom of
  // the display. The algorithm would  normally place the capture label above
  // the region, but should adjust to avoid intersecting.
  event_generator->set_current_screen_location(gfx::Point());
  event_generator->ClickLeftButton();
  SelectRegion(gfx::Rect(capture_bar_midpoint_x, capture_bar_bounds.bottom(),
                         20, 800 - capture_bar_bounds.bottom()));
  EXPECT_FALSE(capture_bar_bounds.Intersects(
      GetCaptureModeLabelWidget()->GetWindowBoundsInScreen()));

  // Create a thin region that is vertical as tall as the display, and at the
  // left edge of the display. The capture label button should be right of the
  // region.
  event_generator->set_current_screen_location(gfx::Point());
  event_generator->ClickLeftButton();
  SelectRegion(gfx::Rect(20, 800));
  EXPECT_GT(GetCaptureModeLabelWidget()->GetWindowBoundsInScreen().x(), 20);
}

TEST_F(CaptureModeTest, WindowCapture) {
  // Create 2 windows that overlap with each other.
  const gfx::Rect bounds1(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateTestWindow(bounds1));
  const gfx::Rect bounds2(150, 150, 200, 200);
  std::unique_ptr<aura::Window> window2(CreateTestWindow(bounds2));

  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kWindow);
  controller->SetType(CaptureModeType::kImage);
  controller->Start(CaptureModeEntryType::kAccelTakeWindowScreenshot);
  EXPECT_TRUE(controller->IsActive());

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window1.get());
  auto* capture_mode_session = controller->capture_mode_session();
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window1.get());
  event_generator->MoveMouseToCenterOf(window2.get());
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window2.get());

  // Now move the mouse to the overlapped area.
  event_generator->MoveMouseTo(gfx::Point(175, 175));
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window2.get());
  // Close the current selected window should automatically focus to next one.
  window2.reset();
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window1.get());
  // Open another one on top also change the selected window.
  std::unique_ptr<aura::Window> window3(CreateTestWindow(bounds2));
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window3.get());
  // Minimize the window should also automatically change the selected window.
  WindowState::Get(window3.get())->Minimize();
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window1.get());

  // Stop the capture session to avoid CaptureModeSession from receiving more
  // events during test tearing down.
  controller->Stop();
}

// Tests that the capture bar is located on the root with the cursor when
// starting capture mode.
TEST_F(CaptureModeTest, MultiDisplayCaptureBarInitialLocation) {
  UpdateDisplay("800x700,801+0-800x700");

  auto* event_generator = GetEventGenerator();
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(1000, 500), event_generator);

  auto* controller = StartImageRegionCapture();
  EXPECT_TRUE(gfx::Rect(801, 0, 800, 800)
                  .Contains(GetCaptureModeBarView()->GetBoundsInScreen()));
  controller->Stop();

  MoveMouseToAndUpdateCursorDisplay(gfx::Point(100, 500), event_generator);
  StartImageRegionCapture();
  EXPECT_TRUE(gfx::Rect(800, 800).Contains(
      GetCaptureModeBarView()->GetBoundsInScreen()));
}

// Tests behavior of a capture mode session if the active display is removed.
TEST_F(CaptureModeTest, DisplayRemoval) {
  UpdateDisplay("800x700,801+0-800x700");

  // Start capture mode on the secondary display.
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(1000, 500), GetEventGenerator());
  auto* controller = StartImageRegionCapture();
  auto* session = controller->capture_mode_session();
  EXPECT_TRUE(gfx::Rect(801, 0, 800, 800)
                  .Contains(GetCaptureModeBarView()->GetBoundsInScreen()));
  ASSERT_EQ(Shell::GetAllRootWindows()[1], session->current_root());

  RemoveSecondaryDisplay();

  // Tests that the capture mode bar is now on the primary display.
  EXPECT_TRUE(gfx::Rect(800, 800).Contains(
      GetCaptureModeBarView()->GetBoundsInScreen()));
  ASSERT_EQ(Shell::GetAllRootWindows()[0], session->current_root());
}

// Tests that using fullscreen or window source, moving the mouse across
// displays will change the root window of the capture session.
TEST_F(CaptureModeTest, MultiDisplayFullscreenOrWindowSourceRootWindow) {
  UpdateDisplay("800x700,801+0-800x700");
  ASSERT_EQ(2u, Shell::GetAllRootWindows().size());

  auto* event_generator = GetEventGenerator();
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(100, 500), event_generator);

  for (auto source :
       {CaptureModeSource::kFullscreen, CaptureModeSource::kWindow}) {
    SCOPED_TRACE(source == CaptureModeSource::kFullscreen ? "Fullscreen source"
                                                          : "Window source");

    auto* controller = StartCaptureSession(source, CaptureModeType::kImage);
    auto* session = controller->capture_mode_session();
    EXPECT_EQ(Shell::GetAllRootWindows()[0], session->current_root());

    MoveMouseToAndUpdateCursorDisplay(gfx::Point(1000, 500), event_generator);
    EXPECT_EQ(Shell::GetAllRootWindows()[1], session->current_root());

    MoveMouseToAndUpdateCursorDisplay(gfx::Point(100, 500), event_generator);
    EXPECT_EQ(Shell::GetAllRootWindows()[0], session->current_root());

    controller->Stop();
  }
}

// Tests that in region mode, moving the mouse across displays will not change
// the root window of the capture session, but clicking on a new display will.
TEST_F(CaptureModeTest, MultiDisplayRegionSourceRootWindow) {
  UpdateDisplay("800x700,801+0-800x700");
  ASSERT_EQ(2u, Shell::GetAllRootWindows().size());

  auto* event_generator = GetEventGenerator();
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(100, 500), event_generator);

  auto* controller = StartImageRegionCapture();
  auto* session = controller->capture_mode_session();
  EXPECT_EQ(Shell::GetAllRootWindows()[0], session->current_root());

  // Tests that moving the mouse to the secondary display does not change the
  // root.
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(1000, 500), event_generator);
  EXPECT_EQ(Shell::GetAllRootWindows()[0], session->current_root());

  // Tests that pressing the mouse changes the root. The capture bar stays on
  // the primary display until the mouse is released.
  event_generator->PressLeftButton();
  EXPECT_EQ(Shell::GetAllRootWindows()[1], session->current_root());
  EXPECT_TRUE(gfx::Rect(800, 800).Contains(
      GetCaptureModeBarView()->GetBoundsInScreen()));

  event_generator->ReleaseLeftButton();
  EXPECT_EQ(Shell::GetAllRootWindows()[1], session->current_root());
  EXPECT_TRUE(gfx::Rect(801, 0, 800, 800)
                  .Contains(GetCaptureModeBarView()->GetBoundsInScreen()));
}

// Tests that using touch on multi display setups works as intended. Regression
// test for https://crbug.com/1159512.
TEST_F(CaptureModeTest, MultiDisplayTouch) {
  UpdateDisplay("800x700,801+0-800x700");
  ASSERT_EQ(2u, Shell::GetAllRootWindows().size());

  auto* controller = StartImageRegionCapture();
  auto* session = controller->capture_mode_session();
  ASSERT_EQ(Shell::GetAllRootWindows()[0], session->current_root());

  // Touch and move your finger on the secondary display. We should switch roots
  // and the region size should be as expected.
  auto* event_generator = GetEventGenerator();
  event_generator->PressTouch(gfx::Point(1000, 200));
  event_generator->MoveTouch(gfx::Point(1200, 400));
  event_generator->ReleaseTouch();
  EXPECT_EQ(Shell::GetAllRootWindows()[1], session->current_root());
  EXPECT_EQ(gfx::Size(200, 200), controller->user_capture_region().size());
}

TEST_F(CaptureModeTest, RegionCursorStates) {
  using ui::mojom::CursorType;

  auto* cursor_manager = Shell::Get()->cursor_manager();
  CursorType original_cursor_type = cursor_manager->GetCursor().type();
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(CursorType::kPointer, original_cursor_type);

  auto* event_generator = GetEventGenerator();
  auto* controller = StartImageRegionCapture();
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  event_generator->MoveMouseTo(gfx::Point(175, 175));
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kCell, cursor_manager->GetCursor().type());

  const gfx::Rect target_region(gfx::Rect(200, 200, 200, 200));
  SelectRegion(target_region);

  // Makes sure that the cursor is updated when the user releases the region
  // select and is still hovering in the same location.
  EXPECT_EQ(CursorType::kSouthEastResize, cursor_manager->GetCursor().type());

  // Verify that all of the |FineTunePosition| locations have the correct cursor
  // when hovered over.
  event_generator->MoveMouseTo(target_region.origin());
  EXPECT_EQ(CursorType::kNorthWestResize, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.top_center());
  EXPECT_EQ(CursorType::kNorthSouthResize, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.top_right());
  EXPECT_EQ(CursorType::kNorthEastResize, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.right_center());
  EXPECT_EQ(CursorType::kEastWestResize, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.bottom_right());
  EXPECT_EQ(CursorType::kSouthEastResize, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.bottom_center());
  EXPECT_EQ(CursorType::kNorthSouthResize, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.bottom_left());
  EXPECT_EQ(CursorType::kSouthWestResize, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.left_center());
  EXPECT_EQ(CursorType::kEastWestResize, cursor_manager->GetCursor().type());

  // Tests that within the bounds of the selected region, the cursor is a hand
  // when hovering over the capture button, otherwise it is a multi-directional
  // move cursor.
  event_generator->MoveMouseTo(gfx::Point(250, 250));
  EXPECT_EQ(CursorType::kMove, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.CenterPoint());
  EXPECT_EQ(CursorType::kHand, cursor_manager->GetCursor().type());

  // Tests that the cursor changes to a cell type when hovering over the
  // unselected region.
  event_generator->MoveMouseTo(gfx::Point(50, 50));
  EXPECT_EQ(CursorType::kCell, cursor_manager->GetCursor().type());

  // Check that cursor is unlocked when changing sources, and that the cursor
  // changes to a pointer when hovering over the capture mode bar.
  event_generator->MoveMouseTo(
      GetRegionToggleButton()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(
      GetWindowToggleButton()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());
  event_generator->ClickLeftButton();
  ASSERT_EQ(CaptureModeSource::kWindow, controller->source());
  // The event on the capture bar to change capture source will still keep the
  // cursor locked.
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(original_cursor_type, cursor_manager->GetCursor().type());

  // Tests that on changing back to region capture mode, the cursor becomes
  // locked, and is still a pointer type over the bar, whilst a cell cursor
  // otherwise (not over the selected region).
  event_generator->MoveMouseTo(
      GetRegionToggleButton()->GetBoundsInScreen().CenterPoint());
  original_cursor_type = cursor_manager->GetCursor().type();
  event_generator->ClickLeftButton();
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());
  // Tests that clicking on the button again doesn't change the cursor.
  event_generator->ClickLeftButton();
  EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(gfx::Point(50, 50));
  EXPECT_EQ(CursorType::kCell, cursor_manager->GetCursor().type());

  // Enter tablet mode, the cursor should be hidden.
  SwitchToTabletMode();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Move mouse but it should still be invisible.
  event_generator->MoveMouseTo(gfx::Point(100, 100));
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Return to clamshell mode, mouse should appear again.
  LeaveTabletMode();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kCell, cursor_manager->GetCursor().type());

  // Tests that when exiting capture mode that the cursor is restored to its
  // original state.
  controller->Stop();
  EXPECT_FALSE(controller->IsActive());
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(original_cursor_type, cursor_manager->GetCursor().type());
}

TEST_F(CaptureModeTest, FullscreenCursorStates) {
  using ui::mojom::CursorType;

  auto* cursor_manager = Shell::Get()->cursor_manager();
  CursorType original_cursor_type = cursor_manager->GetCursor().type();
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(CursorType::kPointer, original_cursor_type);

  auto* event_generator = GetEventGenerator();
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kImage);
  EXPECT_EQ(controller->type(), CaptureModeType::kImage);
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  event_generator->MoveMouseTo(gfx::Point(175, 175));
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  // Use image capture icon as the mouse cursor icon in image capture mode.
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kImage));

  // Move the mouse over to capture label widget won't change the cursor since
  // it's a label not a label button.
  event_generator->MoveMouseTo(
      test_api.capture_label_widget()->GetWindowBoundsInScreen().CenterPoint());
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kImage));

  // Use pointer mouse if the event is on the capture bar.
  ClickOnView(GetVideoToggleButton(), event_generator);
  EXPECT_EQ(controller->type(), CaptureModeType::kVideo);
  EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  // Use video record icon as the mouse cursor icon in video recording mode.
  event_generator->MoveMouseTo(gfx::Point(175, 175));
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kVideo));
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  // Enter tablet mode, the cursor should be hidden.
  // To avoid flaky failures due to mouse devices blocking entering tablet mode,
  // we detach all mouse devices. This shouldn't affect testing the cursor
  // status.
  SwitchToTabletMode();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Exit tablet mode, the cursor should appear again.
  LeaveTabletMode();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kVideo));
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Stop capture mode, the cursor should be restored to its original state.
  controller->Stop();
  EXPECT_FALSE(controller->IsActive());
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(original_cursor_type, cursor_manager->GetCursor().type());

  // Test that if we're in tablet mode for dev purpose, the cursor should still
  // be visible.
  Shell::Get()->tablet_mode_controller()->SetEnabledForDev(true);
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kImage);
  EXPECT_EQ(controller->type(), CaptureModeType::kImage);
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  event_generator->MoveMouseTo(gfx::Point(175, 175));
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
}

TEST_F(CaptureModeTest, WindowCursorStates) {
  using ui::mojom::CursorType;

  std::unique_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(200, 200)));

  auto* cursor_manager = Shell::Get()->cursor_manager();
  CursorType original_cursor_type = cursor_manager->GetCursor().type();
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(CursorType::kPointer, original_cursor_type);

  auto* event_generator = GetEventGenerator();
  CaptureModeController* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);
  EXPECT_EQ(controller->type(), CaptureModeType::kImage);

  // If the mouse is above the window, use the image capture icon.
  event_generator->MoveMouseTo(gfx::Point(150, 150));
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kImage));

  // If the mouse is not above the window, use the original mouse cursor.
  event_generator->MoveMouseTo(gfx::Point(300, 300));
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(original_cursor_type, cursor_manager->GetCursor().type());

  // Use pointer mouse if the event is on the capture bar.
  ClickOnView(GetVideoToggleButton(), event_generator);
  EXPECT_EQ(controller->type(), CaptureModeType::kVideo);
  EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  // Use video record icon as the mouse cursor icon in video recording mode.
  event_generator->MoveMouseTo(gfx::Point(150, 150));
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kVideo));

  // If the mouse is not above the window, use the original mouse cursor.
  event_generator->MoveMouseTo(gfx::Point(300, 300));
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(original_cursor_type, cursor_manager->GetCursor().type());

  // Move above the window again, the cursor should change back to the video
  // record icon.
  event_generator->MoveMouseTo(gfx::Point(150, 150));
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kVideo));

  // Enter tablet mode, the cursor should be hidden.
  // To avoid flaky failures due to mouse devices blocking entering tablet mode,
  // we detach all mouse devices. This shouldn't affect testing the cursor
  // status.
  SwitchToTabletMode();
  EXPECT_FALSE(cursor_manager->IsCursorVisible());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Exit tablet mode, the cursor should appear again.
  LeaveTabletMode();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kVideo));
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Stop capture mode, the cursor should be restored to its original state.
  controller->Stop();
  EXPECT_FALSE(controller->IsActive());
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(original_cursor_type, cursor_manager->GetCursor().type());
}

// Tests that nothing crashes when windows are destroyed while being observed.
TEST_F(CaptureModeTest, WindowDestruction) {
  using ui::mojom::CursorType;

  // Create 2 windows that overlap with each other.
  const gfx::Rect bounds1(0, 0, 200, 200);
  const gfx::Rect bounds2(150, 150, 200, 200);
  const gfx::Rect bounds3(50, 50, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateTestWindow(bounds1));
  std::unique_ptr<aura::Window> window2(CreateTestWindow(bounds2));

  auto* cursor_manager = Shell::Get()->cursor_manager();
  CursorType original_cursor_type = cursor_manager->GetCursor().type();
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(CursorType::kPointer, original_cursor_type);

  // Start capture session with Image type, so we have a custom cursor.
  auto* event_generator = GetEventGenerator();
  CaptureModeController* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);
  EXPECT_EQ(controller->type(), CaptureModeType::kImage);

  // If the mouse is above the window, use the image capture icon.
  event_generator->MoveMouseToCenterOf(window2.get());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  auto* capture_mode_session = controller->capture_mode_session();
  CaptureModeSessionTestApi test_api(capture_mode_session);
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kImage));

  // Destroy the window while hovering. There is no window underneath, so it
  // should revert back to the original cursor.
  window2.reset();
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(original_cursor_type, cursor_manager->GetCursor().type());

  // Destroy the window while mouse is in a pressed state. Cursor should revert
  // back to the original cursor.
  std::unique_ptr<aura::Window> window3(CreateTestWindow(bounds2));
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kImage));
  event_generator->PressLeftButton();
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kImage));
  window3.reset();
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(original_cursor_type, cursor_manager->GetCursor().type());

  // When hovering over a window, if it is destroyed and there is another window
  // under the cursor location in screen, then the selected window is
  // automatically updated.
  std::unique_ptr<aura::Window> window4(CreateTestWindow(bounds3));
  event_generator->MoveMouseToCenterOf(window4.get());
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kImage));
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window4.get());
  window4.reset();
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kImage));
  // Check to see it's observing window1.
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window1.get());

  // Cursor is over a window in the mouse pressed state. If the window is
  // destroyed and there is another window under the cursor, the selected window
  // is updated and the new selected window is captured.
  std::unique_ptr<aura::Window> window5(CreateTestWindow(bounds3));
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window5.get());
  event_generator->PressLeftButton();
  window5.reset();
  EXPECT_EQ(CursorType::kCustom, cursor_manager->GetCursor().type());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kImage));
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window1.get());
  event_generator->ReleaseLeftButton();
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(CaptureModeTest, CursorUpdatedOnDisplayRotation) {
  using ui::mojom::CursorType;

  UpdateDisplay("600x400");
  const int64_t display_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::Display::SetInternalDisplayId(display_id);
  ScreenOrientationControllerTestApi orientation_test_api(
      Shell::Get()->screen_orientation_controller());

  auto* event_generator = GetEventGenerator();
  auto* cursor_manager = Shell::Get()->cursor_manager();
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kImage);
  event_generator->MoveMouseTo(gfx::Point(175, 175));
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  // Use image capture icon as the mouse cursor icon in image capture mode.
  const ui::Cursor landscape_cursor = cursor_manager->GetCursor();
  EXPECT_EQ(CursorType::kCustom, landscape_cursor.type());
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kImage));

  // Rotate the screen.
  orientation_test_api.SetDisplayRotation(
      display::Display::ROTATE_270, display::Display::RotationSource::ACTIVE);
  const ui::Cursor portrait_cursor = cursor_manager->GetCursor();
  EXPECT_TRUE(test_api.IsUsingCustomCursor(CaptureModeType::kImage));
  EXPECT_NE(landscape_cursor, portrait_cursor);
}

// Tests that in Region mode, cursor compositing is used instead of the system
// cursor when the cursor is being dragged.
TEST_F(CaptureModeTest, RegionDragCursorCompositing) {
  auto* event_generator = GetEventGenerator();
  auto* session = StartImageRegionCapture()->capture_mode_session();
  auto* cursor_manager = Shell::Get()->cursor_manager();

  // Initially cursor should be visible and cursor compositing is not enabled.
  EXPECT_FALSE(session->is_drag_in_progress());
  EXPECT_FALSE(IsCursorCompositingEnabled());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  const gfx::Rect target_region(gfx::Rect(200, 200, 200, 200));

  // For each start and end point try dragging and verify that cursor
  // compositing is functioning as expected.
  struct {
    std::string trace;
    gfx::Point start_point;
    gfx::Point end_point;
  } kDragCases[] = {
      {"initial_region", target_region.origin(), target_region.bottom_right()},
      {"edge_resize", target_region.right_center(),
       gfx::Point(target_region.right_center() + gfx::Vector2d(50, 0))},
      {"corner_resize", target_region.origin(), gfx::Point(175, 175)},
      {"move", gfx::Point(250, 250), gfx::Point(300, 300)},
  };

  for (auto test_case : kDragCases) {
    SCOPED_TRACE(test_case.trace);

    event_generator->MoveMouseTo(test_case.start_point);
    event_generator->PressLeftButton();
    EXPECT_TRUE(session->is_drag_in_progress());
    EXPECT_TRUE(IsCursorCompositingEnabled());

    event_generator->MoveMouseTo(test_case.end_point);
    EXPECT_TRUE(session->is_drag_in_progress());
    EXPECT_TRUE(IsCursorCompositingEnabled());

    event_generator->ReleaseLeftButton();
    EXPECT_FALSE(session->is_drag_in_progress());
    EXPECT_FALSE(IsCursorCompositingEnabled());
  }
}

// Test that during countdown, capture mode session should not handle any
// incoming input events.
TEST_F(CaptureModeTest, DoNotHandleEventDuringCountDown) {
  // We need a non-zero duration to avoid infinite loop on countdown.
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Create 2 windows that overlap with each other.
  std::unique_ptr<aura::Window> window1(CreateTestWindow(gfx::Rect(200, 200)));
  std::unique_ptr<aura::Window> window2(
      CreateTestWindow(gfx::Rect(150, 150, 200, 200)));

  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kWindow);
  controller->SetType(CaptureModeType::kVideo);
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_TRUE(controller->IsActive());

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window1.get());
  auto* capture_mode_session = controller->capture_mode_session();
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window1.get());

  // Start video recording. Countdown should start at this moment.
  event_generator->ClickLeftButton();

  // Now move the mouse onto the other window, we should not change the captured
  // window during countdown.
  event_generator->MoveMouseToCenterOf(window2.get());
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window1.get());
  EXPECT_NE(capture_mode_session->GetSelectedWindow(), window2.get());

  WaitForRecordingToStart();
}

// Test that during countdown, window changes or crashes are handled.
TEST_F(CaptureModeTest, WindowChangesDuringCountdown) {
  // We need a non-zero duration to avoid infinite loop on countdown.
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<aura::Window> window;

  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kWindow);
  controller->SetType(CaptureModeType::kVideo);

  auto start_countdown = [this, &window, controller]() {
    window = CreateTestWindow(gfx::Rect(200, 200));
    controller->Start(CaptureModeEntryType::kQuickSettings);

    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseToCenterOf(window.get());
    event_generator->ClickLeftButton();

    EXPECT_TRUE(controller->IsActive());
    EXPECT_FALSE(controller->is_recording_in_progress());
  };

  // Destroying or minimizing the observed window terminates the countdown and
  // exits capture mode.
  start_countdown();
  window.reset();
  EXPECT_FALSE(controller->IsActive());

  start_countdown();
  WindowState::Get(window.get())->Minimize();
  EXPECT_FALSE(controller->IsActive());

  // Activation changes (such as opening overview) should not terminate the
  // countdown.
  start_countdown();
  EnterOverview();
  EXPECT_TRUE(controller->IsActive());
  EXPECT_FALSE(controller->is_recording_in_progress());

  // Wait for countdown to finish and check that recording starts.
  WaitForRecordingToStart();
  EXPECT_FALSE(controller->IsActive());
  EXPECT_TRUE(controller->is_recording_in_progress());
}

// Tests that metrics are recorded properly for capture mode entry points.
TEST_F(CaptureModeTest, CaptureModeEntryPointHistograms) {
  constexpr char kClamshellHistogram[] =
      "Ash.CaptureModeController.EntryPoint.ClamshellMode";
  constexpr char kTabletHistogram[] =
      "Ash.CaptureModeController.EntryPoint.TabletMode";
  base::HistogramTester histogram_tester;

  auto* controller = CaptureModeController::Get();

  // Test the various entry points in clamshell mode.
  controller->Start(CaptureModeEntryType::kAccelTakeWindowScreenshot);
  histogram_tester.ExpectBucketCount(
      kClamshellHistogram, CaptureModeEntryType::kAccelTakeWindowScreenshot, 1);
  controller->Stop();

  controller->Start(CaptureModeEntryType::kAccelTakePartialScreenshot);
  histogram_tester.ExpectBucketCount(
      kClamshellHistogram, CaptureModeEntryType::kAccelTakePartialScreenshot,
      1);
  controller->Stop();

  controller->Start(CaptureModeEntryType::kQuickSettings);
  histogram_tester.ExpectBucketCount(kClamshellHistogram,
                                     CaptureModeEntryType::kQuickSettings, 1);
  controller->Stop();

  controller->Start(CaptureModeEntryType::kStylusPalette);
  histogram_tester.ExpectBucketCount(kClamshellHistogram,
                                     CaptureModeEntryType::kStylusPalette, 1);
  controller->Stop();

  // Enter tablet mode and test the various entry points in tablet mode.
  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();
  tablet_mode_controller->SetEnabledForTest(true);
  ASSERT_TRUE(tablet_mode_controller->InTabletMode());

  controller->Start(CaptureModeEntryType::kAccelTakeWindowScreenshot);
  histogram_tester.ExpectBucketCount(
      kTabletHistogram, CaptureModeEntryType::kAccelTakeWindowScreenshot, 1);
  controller->Stop();

  controller->Start(CaptureModeEntryType::kAccelTakePartialScreenshot);
  histogram_tester.ExpectBucketCount(
      kTabletHistogram, CaptureModeEntryType::kAccelTakePartialScreenshot, 1);
  controller->Stop();

  controller->Start(CaptureModeEntryType::kQuickSettings);
  histogram_tester.ExpectBucketCount(kTabletHistogram,
                                     CaptureModeEntryType::kQuickSettings, 1);
  controller->Stop();

  controller->Start(CaptureModeEntryType::kStylusPalette);
  histogram_tester.ExpectBucketCount(kTabletHistogram,
                                     CaptureModeEntryType::kStylusPalette, 1);
  controller->Stop();

  // Check total counts for each histogram to ensure calls aren't counted in
  // multiple buckets.
  histogram_tester.ExpectTotalCount(kClamshellHistogram, 4);
  histogram_tester.ExpectTotalCount(kTabletHistogram, 4);

  // Check that histogram isn't counted if we don't actually enter capture mode.
  controller->Start(CaptureModeEntryType::kAccelTakePartialScreenshot);
  histogram_tester.ExpectBucketCount(
      kTabletHistogram, CaptureModeEntryType::kAccelTakePartialScreenshot, 2);
  controller->Start(CaptureModeEntryType::kAccelTakePartialScreenshot);
  histogram_tester.ExpectBucketCount(
      kTabletHistogram, CaptureModeEntryType::kAccelTakePartialScreenshot, 2);
}

// Verifies that the video notification will show the same thumbnail image as
// sent by recording service.
TEST_F(CaptureModeTest, VideoNotificationThumbnail) {
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  CaptureModeTestApi().FlushRecordingServiceForTesting();

  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());

  // Request and wait for a video frame so that the recording service can use it
  // to create a video thumbnail.
  test_delegate->RequestAndWaitForVideoFrame();
  SkBitmap service_thumbnail =
      gfx::Image(test_delegate->GetVideoThumbnail()).AsBitmap();
  EXPECT_FALSE(service_thumbnail.drawsNothing());

  CaptureNotificationWaiter waiter;
  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  EXPECT_FALSE(controller->is_recording_in_progress());
  waiter.Wait();

  // Verify that the service's thumbnail is the same image shown in the
  // notification shown when recording ends.
  const message_center::Notification* notification = GetPreviewNotification();
  EXPECT_TRUE(notification);
  EXPECT_FALSE(notification->image().IsEmpty());
  const SkBitmap notification_thumbnail = notification->image().AsBitmap();
  EXPECT_TRUE(
      gfx::test::AreBitmapsEqual(notification_thumbnail, service_thumbnail));
}

TEST_F(CaptureModeTest, WindowRecordingCaptureId) {
  auto window = CreateTestWindow(gfx::Rect(200, 200));
  StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window.get());
  auto* controller = CaptureModeController::Get();
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());

  // The window should have a valid capture ID.
  EXPECT_TRUE(window->subtree_capture_id().is_valid());

  // Once recording ends, the window should no longer be marked as capturable.
  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  EXPECT_FALSE(controller->is_recording_in_progress());
  EXPECT_FALSE(window->subtree_capture_id().is_valid());
}

TEST_F(CaptureModeTest, ClosingDimmedWidgetAboveRecordedWindow) {
  views::Widget* widget = TestWidgetBuilder().BuildOwnedByNativeWidget();
  auto* window = widget->GetNativeWindow();
  auto recorded_window = CreateTestWindow(gfx::Rect(200, 200));

  auto* controller = StartSessionAndRecordWindow(recorded_window.get());
  EXPECT_TRUE(controller->is_recording_in_progress());
  auto* recording_watcher = controller->video_recording_watcher_for_testing();

  // Activate the window so that it becomes on top of the recorded window, and
  // expect it gets dimmed.
  wm::ActivateWindow(window);
  EXPECT_TRUE(recording_watcher->IsWindowDimmedForTesting(window));

  // Close the widget, this should not lead to any use-after-free. See
  // https://crbug.com/1273197.
  widget->Close();
}

TEST_F(CaptureModeTest, DimmingOfUnRecordedWindows) {
  auto win1 = CreateTestWindow(gfx::Rect(200, 200));
  auto win2 = CreateTestWindow(gfx::Rect(200, 200));
  auto recorded_window = CreateTestWindow(gfx::Rect(200, 200));

  auto* controller = StartSessionAndRecordWindow(recorded_window.get());
  auto* recording_watcher = controller->video_recording_watcher_for_testing();
  auto* shield_layer = recording_watcher->layer();
  // Since the recorded window is the top most, no windows should be
  // individually dimmed.
  EXPECT_TRUE(recording_watcher->should_paint_layer());
  EXPECT_TRUE(IsLayerStackedRightBelow(shield_layer, recorded_window->layer()));
  EXPECT_FALSE(
      recording_watcher->IsWindowDimmedForTesting(recorded_window.get()));
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(win1.get()));
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(win2.get()));

  // Activating |win1| brings it to the front of the shield, so it should be
  // dimmed separately.
  wm::ActivateWindow(win1.get());
  EXPECT_TRUE(recording_watcher->should_paint_layer());
  EXPECT_TRUE(IsLayerStackedRightBelow(shield_layer, recorded_window->layer()));
  EXPECT_FALSE(
      recording_watcher->IsWindowDimmedForTesting(recorded_window.get()));
  EXPECT_TRUE(recording_watcher->IsWindowDimmedForTesting(win1.get()));
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(win2.get()));
  // Similarly for |win2|.
  wm::ActivateWindow(win2.get());
  EXPECT_TRUE(recording_watcher->should_paint_layer());
  EXPECT_TRUE(IsLayerStackedRightBelow(shield_layer, recorded_window->layer()));
  EXPECT_FALSE(
      recording_watcher->IsWindowDimmedForTesting(recorded_window.get()));
  EXPECT_TRUE(recording_watcher->IsWindowDimmedForTesting(win1.get()));
  EXPECT_TRUE(recording_watcher->IsWindowDimmedForTesting(win2.get()));

  // Minimizing the recorded window should stop painting the shield, and the
  // dimmers should be removed.
  WindowState::Get(recorded_window.get())->Minimize();
  EXPECT_FALSE(recording_watcher->should_paint_layer());
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(win1.get()));
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(win2.get()));

  // Activating the recorded window again unminimizes the window, which will
  // reenable painting the shield.
  wm::ActivateWindow(recorded_window.get());
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(win1.get()));
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(win2.get()));
  EXPECT_FALSE(WindowState::Get(recorded_window.get())->IsMinimized());
  EXPECT_TRUE(recording_watcher->should_paint_layer());

  // Destroying a dimmed window is correctly tracked.
  wm::ActivateWindow(win2.get());
  EXPECT_TRUE(recording_watcher->IsWindowDimmedForTesting(win2.get()));
  win2.reset();
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(win2.get()));
}

TEST_F(CaptureModeTest, DimmingWithDesks) {
  auto recorded_window = CreateAppWindow(gfx::Rect(250, 100));
  auto* controller = StartSessionAndRecordWindow(recorded_window.get());
  auto* recording_watcher = controller->video_recording_watcher_for_testing();
  EXPECT_TRUE(recording_watcher->should_paint_layer());

  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  Desk* desk_2 = desks_controller->desks()[1].get();
  ActivateDesk(desk_2);

  // A window on a different desk than that of the recorded window should not be
  // dimmed.
  auto win1 = CreateAppWindow(gfx::Rect(200, 200));
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(win1.get()));

  // However, moving it to the desk of the recorded window should give it a
  // dimmer, since it's a more recently-used window (i.e. above the recorded
  // window).
  Desk* desk_1 = desks_controller->desks()[0].get();
  desks_controller->MoveWindowFromActiveDeskTo(
      win1.get(), desk_1, win1->GetRootWindow(),
      DesksMoveWindowFromActiveDeskSource::kShortcut);
  EXPECT_TRUE(recording_watcher->IsWindowDimmedForTesting(win1.get()));

  // Moving the recorded window out of the active desk should destroy the
  // dimmer.
  ActivateDesk(desk_1);
  desks_controller->MoveWindowFromActiveDeskTo(
      recorded_window.get(), desk_2, recorded_window->GetRootWindow(),
      DesksMoveWindowFromActiveDeskSource::kShortcut);
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(win1.get()));
}

TEST_F(CaptureModeTest, DimmingWithDisplays) {
  UpdateDisplay("500x400,401+0-800x700");
  auto recorded_window = CreateAppWindow(gfx::Rect(250, 100));
  auto* controller = StartSessionAndRecordWindow(recorded_window.get());
  auto* recording_watcher = controller->video_recording_watcher_for_testing();
  EXPECT_TRUE(recording_watcher->should_paint_layer());

  // Create a new window on the second display. It should not be dimmed.
  auto window = CreateTestWindow(gfx::Rect(420, 10, 200, 200));
  auto roots = Shell::GetAllRootWindows();
  EXPECT_EQ(roots[1], window->GetRootWindow());
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(window.get()));

  // However when moved to the first display, it gets dimmed.
  window_util::MoveWindowToDisplay(window.get(),
                                   roots[0]->GetHost()->GetDisplayId());
  EXPECT_TRUE(recording_watcher->IsWindowDimmedForTesting(window.get()));

  // Moving the recorded window to the second display will remove the dimming of
  // |window|.
  window_util::MoveWindowToDisplay(recorded_window.get(),
                                   roots[1]->GetHost()->GetDisplayId());
  EXPECT_FALSE(recording_watcher->IsWindowDimmedForTesting(window.get()));
}

TEST_F(CaptureModeTest, MultiDisplayWindowRecording) {
  UpdateDisplay("500x400,401+0-800x700");
  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());

  auto window = CreateTestWindow(gfx::Rect(200, 200));
  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window.get());
  auto* session_layer = controller->capture_mode_session()->layer();
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  // The session layer is reused to paint the recording shield.
  auto* shield_layer =
      controller->video_recording_watcher_for_testing()->layer();
  EXPECT_EQ(session_layer, shield_layer);
  EXPECT_EQ(shield_layer->parent(), window->layer()->parent());
  EXPECT_TRUE(IsLayerStackedRightBelow(shield_layer, window->layer()));
  EXPECT_EQ(shield_layer->bounds(), roots[0]->bounds());

  // The capturer should capture from the frame sink of the first display.
  // The video size should match the window's size.
  CaptureModeTestApi test_api;
  test_api.FlushRecordingServiceForTesting();
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  EXPECT_EQ(roots[0]->GetFrameSinkId(), test_delegate->GetCurrentFrameSinkId());
  EXPECT_EQ(roots[0]->bounds().size(),
            test_delegate->GetCurrentFrameSinkSizeInPixels());
  EXPECT_EQ(window->bounds().size(), test_delegate->GetCurrentVideoSize());

  // Moving a window to a different display should be propagated to the service,
  // with the new root's frame sink ID, and the new root's size.
  window_util::MoveWindowToDisplay(window.get(),
                                   roots[1]->GetHost()->GetDisplayId());
  test_api.FlushRecordingServiceForTesting();
  ASSERT_EQ(window->GetRootWindow(), roots[1]);
  EXPECT_EQ(roots[1]->GetFrameSinkId(), test_delegate->GetCurrentFrameSinkId());
  EXPECT_EQ(roots[1]->bounds().size(),
            test_delegate->GetCurrentFrameSinkSizeInPixels());
  EXPECT_EQ(window->bounds().size(), test_delegate->GetCurrentVideoSize());

  // The shield layer should move with the window, and maintain the stacking
  // below the window's layer.
  EXPECT_EQ(shield_layer->parent(), window->layer()->parent());
  EXPECT_TRUE(IsLayerStackedRightBelow(shield_layer, window->layer()));
  EXPECT_EQ(shield_layer->bounds(), roots[1]->bounds());
}

TEST_F(CaptureModeTest, WindowResizing) {
  UpdateDisplay("700x600");
  auto window = CreateTestWindow(gfx::Rect(200, 200));
  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window.get());
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());

  CaptureModeTestApi test_api;
  test_api.FlushRecordingServiceForTesting();
  EXPECT_EQ(gfx::Size(200, 200), test_delegate->GetCurrentVideoSize());
  EXPECT_EQ(gfx::Size(700, 600),
            test_delegate->GetCurrentFrameSinkSizeInPixels());

  // Multiple resize events should be throttled.
  window->SetBounds(gfx::Rect(250, 250));
  test_api.FlushRecordingServiceForTesting();
  EXPECT_EQ(gfx::Size(200, 200), test_delegate->GetCurrentVideoSize());

  window->SetBounds(gfx::Rect(250, 300));
  test_api.FlushRecordingServiceForTesting();
  EXPECT_EQ(gfx::Size(200, 200), test_delegate->GetCurrentVideoSize());

  window->SetBounds(gfx::Rect(300, 300));
  test_api.FlushRecordingServiceForTesting();
  EXPECT_EQ(gfx::Size(200, 200), test_delegate->GetCurrentVideoSize());

  // Once throttling ends, the current size is pushed.
  auto* recording_watcher = controller->video_recording_watcher_for_testing();
  recording_watcher->SendThrottledWindowSizeChangedNowForTesting();
  test_api.FlushRecordingServiceForTesting();
  EXPECT_EQ(gfx::Size(300, 300), test_delegate->GetCurrentVideoSize());
  EXPECT_EQ(gfx::Size(700, 600),
            test_delegate->GetCurrentFrameSinkSizeInPixels());

  // Maximizing a window changes its size, and is pushed to the service with
  // throttling.
  WindowState::Get(window.get())->Maximize();
  test_api.FlushRecordingServiceForTesting();
  EXPECT_EQ(gfx::Size(300, 300), test_delegate->GetCurrentVideoSize());

  recording_watcher->SendThrottledWindowSizeChangedNowForTesting();
  test_api.FlushRecordingServiceForTesting();
  EXPECT_NE(gfx::Size(300, 300), test_delegate->GetCurrentVideoSize());
  EXPECT_EQ(window->bounds().size(), test_delegate->GetCurrentVideoSize());
}

TEST_F(CaptureModeTest, RotateDisplayWhileRecording) {
  UpdateDisplay("600x800");

  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kVideo);
  SelectRegion(gfx::Rect(20, 40, 100, 200));
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());

  // Initially the frame sink size matches the un-rotated display size in DIPs,
  // but the video size matches the size of the crop region.
  CaptureModeTestApi test_api;
  test_api.FlushRecordingServiceForTesting();
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  EXPECT_EQ(gfx::Size(600, 800),
            test_delegate->GetCurrentFrameSinkSizeInPixels());
  EXPECT_EQ(gfx::Size(100, 200), test_delegate->GetCurrentVideoSize());

  // Rotate by 90 degree, the frame sink size should be updated to match that.
  // The video size should remain unaffected.
  Shell::Get()->display_manager()->SetDisplayRotation(
      WindowTreeHostManager::GetPrimaryDisplayId(), display::Display::ROTATE_90,
      display::Display::RotationSource::USER);
  test_api.FlushRecordingServiceForTesting();
  EXPECT_EQ(gfx::Size(800, 600),
            test_delegate->GetCurrentFrameSinkSizeInPixels());
  EXPECT_EQ(gfx::Size(100, 200), test_delegate->GetCurrentVideoSize());
}

// Tests that the video frames delivered to the service for recorded windows are
// valid (i.e. they have the correct size, and suffer from no letterboxing, even
// when the window gets resized).
// This is a regression test for https://crbug.com/1214023.
TEST_F(CaptureModeTest, VerifyWindowRecordingVideoFrames) {
  auto window = CreateTestWindow(gfx::Rect(100, 50, 200, 200));
  StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window.get());
  auto* controller = CaptureModeController::Get();
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  CaptureModeTestApi test_api;
  test_api.FlushRecordingServiceForTesting();

  bool is_video_frame_valid = false;
  std::string failures;
  auto verify_video_frame = [&](const media::VideoFrame& frame,
                                const gfx::Rect& content_rect) {
    is_video_frame_valid = true;
    failures.clear();

    // Having the content positioned at (0,0) with a size that matches the
    // current window's size means that there is no letterboxing.
    if (gfx::Point() != content_rect.origin()) {
      is_video_frame_valid = false;
      failures =
          base::StringPrintf("content_rect is not at (0,0), instead at: %s\n",
                             content_rect.origin().ToString().c_str());
    }

    const gfx::Size window_size = window->bounds().size();
    if (window_size != content_rect.size()) {
      is_video_frame_valid = false;
      failures += base::StringPrintf(
          "content_rect doesn't match the window size:\n"
          "  content_rect.size(): %s\n"
          "  window_size: %s\n",
          content_rect.size().ToString().c_str(),
          window_size.ToString().c_str());
    }

    // The video frame contents should match the bounds of the video frame.
    if (frame.visible_rect() != content_rect) {
      is_video_frame_valid = false;
      failures += base::StringPrintf(
          "content_rect doesn't match the frame's visible_rect:\n"
          "  content_rect: %s\n"
          "  visible_rect: %s\n",
          content_rect.ToString().c_str(),
          frame.visible_rect().ToString().c_str());
    }

    if (frame.coded_size() != window_size) {
      is_video_frame_valid = false;
      failures += base::StringPrintf(
          "the frame's coded size doesn't match the window size:\n"
          "  frame.coded_size(): %s\n"
          "  window_size: %s\n",
          frame.coded_size().ToString().c_str(),
          window_size.ToString().c_str());
    }
  };

  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  ASSERT_TRUE(test_delegate->recording_service());
  {
    SCOPED_TRACE("Initial window size");
    test_delegate->recording_service()->RequestAndWaitForVideoFrame(
        base::BindLambdaForTesting(verify_video_frame));
    EXPECT_TRUE(is_video_frame_valid) << failures;
  }

  // Even when the window is resized and the throttled size reaches the service,
  // new video frames should still be valid.
  window->SetBounds(gfx::Rect(120, 60, 600, 500));
  auto* recording_watcher = controller->video_recording_watcher_for_testing();
  recording_watcher->SendThrottledWindowSizeChangedNowForTesting();
  test_api.FlushRecordingServiceForTesting();
  {
    SCOPED_TRACE("After window resizing");
    // A video frame is produced on the Viz side when a CopyOutputRequest is
    // fulfilled. Those CopyOutputRequests could have been placed before the
    // window's layer resize results in a new resized render pass in Viz. But
    // eventually this must happen, and a valid frame must be delivered.
    int remaining_attempts = 2;
    do {
      --remaining_attempts;
      test_delegate->recording_service()->RequestAndWaitForVideoFrame(
          base::BindLambdaForTesting(verify_video_frame));
    } while (!is_video_frame_valid && remaining_attempts);
    EXPECT_TRUE(is_video_frame_valid) << failures;
  }

  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  EXPECT_FALSE(controller->is_recording_in_progress());
}

class CaptureModeSaveFileTest
    : public CaptureModeTest,
      public testing::WithParamInterface<CaptureModeType> {
 public:
  CaptureModeSaveFileTest() = default;
  CaptureModeSaveFileTest(
      const CaptureModeSaveFileTest& capture_mode_save_file_test) = delete;
  CaptureModeSaveFileTest& operator=(const CaptureModeSaveFileTest&) = delete;
  ~CaptureModeSaveFileTest() override = default;

  void StartCaptureSessionWithParam() {
    StartCaptureSession(CaptureModeSource::kFullscreen, GetParam());
  }

  // Based on the `CaptureModeType`, it performs the capture and then returns
  // the path of the saved image or video files.
  base::FilePath PerformCapture() {
    auto* controller = CaptureModeController::Get();
    switch (GetParam()) {
      case CaptureModeType::kImage:
        controller->PerformCapture();
        return WaitForCaptureFileToBeSaved();

      case CaptureModeType::kVideo:
        StartVideoRecordingImmediately();
        controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
        return WaitForCaptureFileToBeSaved();
    }
  }
};

// Tests that if the custom folder becomes unavailable, the captured file should
// be saved into the default folder. Otherwise, it's saved into custom folder.
TEST_P(CaptureModeSaveFileTest, SaveCapturedFileWithCustomFolder) {
  auto* controller = CaptureModeController::Get();
  const base::FilePath default_folder =
      controller->delegate_for_testing()->GetUserDefaultDownloadsFolder();
  const base::FilePath custom_folder((FILE_PATH_LITERAL("/home/tests")));
  controller->SetCustomCaptureFolder(custom_folder);

  // Make sure the current folder is the custom folder here and then perform
  // capture.
  auto capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_FALSE(capture_folder.is_default_downloads_folder);
  StartCaptureSessionWithParam();
  base::FilePath file_saved_path = PerformCapture();

  // Since `custom_folder` is not available, the captured files will be saved
  // into default folder;
  EXPECT_EQ(file_saved_path.DirName(), default_folder);

  // Now create an available custom folder and set it for custom capture folder.
  const base::FilePath available_custom_folder = CreateCustomFolder("test");
  controller->SetCustomCaptureFolder(available_custom_folder);

  capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_FALSE(capture_folder.is_default_downloads_folder);
  StartCaptureSessionWithParam();
  file_saved_path = PerformCapture();

  // Since `available_custom_folder` is now available, the captured files will
  // be saved into the custom folder;
  EXPECT_EQ(file_saved_path.DirName(), available_custom_folder);
}

TEST_P(CaptureModeSaveFileTest, CaptureModeSaveToLocationMetric) {
  constexpr char kHistogramBase[] = "Ash.CaptureModeController.SaveLocation";
  base::HistogramTester histogram_tester;
  auto* controller = CaptureModeController::Get();
  auto* test_delegate = controller->delegate_for_testing();

  // Initialize four different save-to locations for screen capture that
  // includes default downloads folder, local customized folder, root drive and
  // a specific folder on drive.
  const auto downloads_folder = test_delegate->GetUserDefaultDownloadsFolder();
  const base::FilePath custom_folder = CreateCustomFolder("test");
  base::FilePath mount_point_path;
  test_delegate->GetDriveFsMountPointPath(&mount_point_path);
  const auto root_drive_folder = mount_point_path.Append("root");
  const base::FilePath non_root_drive_folder = CreateFolderOnDriveFS("test");
  struct {
    base::FilePath set_save_file_folder;
    CaptureModeSaveToLocation save_location;
  } kTestCases[] = {
      {downloads_folder, CaptureModeSaveToLocation::kDefault},
      {custom_folder, CaptureModeSaveToLocation::kCustomizedFolder},
      {root_drive_folder, CaptureModeSaveToLocation::kDrive},
      {non_root_drive_folder, CaptureModeSaveToLocation::kDriveFolder},
  };
  for (auto test_case : kTestCases) {
    histogram_tester.ExpectBucketCount(
        GetCaptureModeHistogramName(kHistogramBase), test_case.save_location,
        0);
  }
  // Set four different save-to locations in clamshell mode and check the
  // histogram results.
  EXPECT_FALSE(Shell::Get()->IsInTabletMode());
  for (auto test_case : kTestCases) {
    StartCaptureSessionWithParam();
    controller->SetCustomCaptureFolder(test_case.set_save_file_folder);
    auto file_saved_path = PerformCapture();
    histogram_tester.ExpectBucketCount(
        GetCaptureModeHistogramName(kHistogramBase), test_case.save_location,
        1);
  }

  // Set four different save-to locations in tablet mode and check the histogram
  // results.
  SwitchToTabletMode();
  EXPECT_TRUE(Shell::Get()->IsInTabletMode());
  for (auto test_case : kTestCases) {
    StartCaptureSessionWithParam();
    controller->SetCustomCaptureFolder(test_case.set_save_file_folder);
    auto file_saved_path = PerformCapture();
    histogram_tester.ExpectBucketCount(
        GetCaptureModeHistogramName(kHistogramBase), test_case.save_location,
        1);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         CaptureModeSaveFileTest,
                         testing::Values(CaptureModeType::kImage,
                                         CaptureModeType::kVideo));

// Test fixture for verifying that the videos are recorded at the pixel size of
// the targets being captured in all recording modes. This avoids having the
// scaling in CopyOutputRequests when performing the capture at a different size
// than that of the render pass (which is in pixels). This scaling causes a loss
// of quality, and a blurry video frames. https://crbug.com/1215185.
class CaptureModeRecordingSizeTest : public CaptureModeTest {
 public:
  CaptureModeRecordingSizeTest() = default;
  ~CaptureModeRecordingSizeTest() override = default;

  // CaptureModeTest:
  void SetUp() override {
    CaptureModeTest::SetUp();
    window_ = CreateTestWindow(gfx::Rect(100, 50, 200, 200));
    CaptureModeController::Get()->SetUserCaptureRegion(user_region_,
                                                       /*by_user=*/true);
    UpdateDisplay("800x600");
  }

  void TearDown() override {
    window_.reset();
    CaptureModeTest::TearDown();
  }

  // Converts the given |size| from DIPs to pixels based on the given value of
  // |dsf|.
  gfx::Size ToPixels(const gfx::Size& size, float dsf) const {
    return gfx::ToFlooredSize(gfx::ConvertSizeToPixels(size, dsf));
  }

 protected:
  // Verifies the size of the received video frame.
  static void VerifyVideoFrame(const gfx::Size& expected_video_size,
                               const media::VideoFrame& frame,
                               const gfx::Rect& content_rect) {
    // The I420 pixel format does not like odd dimensions, so the size of the
    // visible rect in the video frame will be adjusted to be an even value.
    const gfx::Size adjusted_size(expected_video_size.width() & ~1,
                                  expected_video_size.height() & ~1);
    EXPECT_EQ(adjusted_size, frame.visible_rect().size());
  }

  CaptureModeController* StartVideoRecording(CaptureModeSource source) {
    auto* controller = StartCaptureSession(source, CaptureModeType::kVideo);
    if (source == CaptureModeSource::kWindow)
      GetEventGenerator()->MoveMouseToCenterOf(window_.get());
    StartVideoRecordingImmediately();
    EXPECT_TRUE(controller->is_recording_in_progress());
    CaptureModeTestApi().FlushRecordingServiceForTesting();
    return controller;
  }

  void SetDeviceScaleFactor(float dsf) {
    const auto display_id = display_manager()->GetDisplayAt(0).id();
    display_manager()->UpdateZoomFactor(display_id, dsf);
    EXPECT_EQ(dsf, window_->GetHost()->device_scale_factor());
    auto* controller = CaptureModeController::Get();
    if (controller->is_recording_in_progress()) {
      CaptureModeTestApi().FlushRecordingServiceForTesting();
      auto* test_delegate = static_cast<TestCaptureModeDelegate*>(
          controller->delegate_for_testing());
      // Consume any pending video frame from before changing the DSF prior to
      // proceeding.
      test_delegate->RequestAndWaitForVideoFrame();
    }
  }

 protected:
  const gfx::Rect user_region_{20, 50};
  std::unique_ptr<aura::Window> window_;
};

TEST_F(CaptureModeRecordingSizeTest, CaptureAtPixelsFullscreen) {
  float dsf = 1.6f;
  SetDeviceScaleFactor(dsf);
  auto* controller = StartVideoRecording(CaptureModeSource::kFullscreen);
  auto* root = window_->GetRootWindow();
  gfx::Size initial_root_window_size_pixels =
      ToPixels(root->bounds().size(), dsf);
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  ASSERT_TRUE(test_delegate->recording_service());
  {
    SCOPED_TRACE("Testing @ 1.6 device scale factor");
    EXPECT_EQ(initial_root_window_size_pixels,
              test_delegate->GetCurrentVideoSize());

    EXPECT_EQ(
        dsf, test_delegate->recording_service()->GetCurrentDeviceScaleFactor());

    EXPECT_EQ(initial_root_window_size_pixels,
              test_delegate->GetCurrentFrameSinkSizeInPixels());

    test_delegate->recording_service()->RequestAndWaitForVideoFrame(
        base::BindOnce(&CaptureModeRecordingSizeTest::VerifyVideoFrame,
                       initial_root_window_size_pixels));
  }

  // Change the DSF and expect the video size will remain at the initial pixel
  // size of the fullscreen.
  dsf = 2.f;
  SetDeviceScaleFactor(dsf);
  {
    SCOPED_TRACE("Testing @ 2.0 device scale factor");
    EXPECT_EQ(initial_root_window_size_pixels,
              test_delegate->GetCurrentVideoSize());

    // The recording service still tracks the up-to-date DSF and frame sink
    // pixel size even though it doesn't change the video size from its initial
    // value.
    EXPECT_EQ(
        dsf, test_delegate->recording_service()->GetCurrentDeviceScaleFactor());

    EXPECT_EQ(ToPixels(root->bounds().size(), dsf),
              test_delegate->GetCurrentFrameSinkSizeInPixels());

    test_delegate->recording_service()->RequestAndWaitForVideoFrame(
        base::BindOnce(&CaptureModeRecordingSizeTest::VerifyVideoFrame,
                       initial_root_window_size_pixels));
  }

  // When recording the fullscreen, the video size never changes, and remains at
  // the initial pixel size of the recording. Hence, there should be no
  // reconfigures.
  EXPECT_EQ(0, test_delegate->recording_service()
                   ->GetNumberOfVideoEncoderReconfigures());
}

TEST_F(CaptureModeRecordingSizeTest, CaptureAtPixelsRegion) {
  float dsf = 1.6f;
  SetDeviceScaleFactor(dsf);
  auto* controller = StartVideoRecording(CaptureModeSource::kRegion);
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  ASSERT_TRUE(test_delegate->recording_service());

  {
    SCOPED_TRACE("Testing @ 1.6 device scale factor");
    const gfx::Size expected_video_size = ToPixels(user_region_.size(), dsf);
    EXPECT_EQ(expected_video_size, test_delegate->GetCurrentVideoSize());

    EXPECT_EQ(
        dsf, test_delegate->recording_service()->GetCurrentDeviceScaleFactor());

    test_delegate->recording_service()->RequestAndWaitForVideoFrame(
        base::BindOnce(&CaptureModeRecordingSizeTest::VerifyVideoFrame,
                       expected_video_size));
  }

  // Change the DSF and expect the video size to change to match the new pixel
  // size of the recorded target.
  dsf = 2.f;
  SetDeviceScaleFactor(dsf);
  {
    SCOPED_TRACE("Testing @ 2.0 device scale factor");
    const gfx::Size expected_video_size = ToPixels(user_region_.size(), dsf);
    EXPECT_EQ(expected_video_size, test_delegate->GetCurrentVideoSize());

    EXPECT_EQ(
        dsf, test_delegate->recording_service()->GetCurrentDeviceScaleFactor());

    test_delegate->recording_service()->RequestAndWaitForVideoFrame(
        base::BindOnce(&CaptureModeRecordingSizeTest::VerifyVideoFrame,
                       expected_video_size));
  }

  // Since the user chooses the capture region in DIPs, its corresponding pixel
  // size will change when changing the device scale factor. Therefore, the
  // encoder is expected to reconfigure once.
  EXPECT_EQ(1, test_delegate->recording_service()
                   ->GetNumberOfVideoEncoderReconfigures());
}

TEST_F(CaptureModeRecordingSizeTest, CaptureAtPixelsWindow) {
  float dsf = 1.6f;
  SetDeviceScaleFactor(dsf);
  auto* controller = StartVideoRecording(CaptureModeSource::kWindow);
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  ASSERT_TRUE(test_delegate->recording_service());

  {
    SCOPED_TRACE("Testing @ 1.6 device scale factor");
    const gfx::Size expected_video_size =
        ToPixels(window_->GetBoundsInRootWindow().size(), dsf);
    EXPECT_EQ(expected_video_size, test_delegate->GetCurrentVideoSize());

    EXPECT_EQ(
        dsf, test_delegate->recording_service()->GetCurrentDeviceScaleFactor());

    test_delegate->recording_service()->RequestAndWaitForVideoFrame(
        base::BindOnce(&CaptureModeRecordingSizeTest::VerifyVideoFrame,
                       expected_video_size));
  }

  // Change the DSF and expect the video size to change to match the new pixel
  // size of the recorded target.
  dsf = 2.f;
  SetDeviceScaleFactor(dsf);
  {
    SCOPED_TRACE("Testing @ 2.0 device scale factor");
    const gfx::Size expected_video_size =
        ToPixels(window_->GetBoundsInRootWindow().size(), dsf);
    EXPECT_EQ(expected_video_size, test_delegate->GetCurrentVideoSize());

    EXPECT_EQ(
        dsf, test_delegate->recording_service()->GetCurrentDeviceScaleFactor());

    test_delegate->recording_service()->RequestAndWaitForVideoFrame(
        base::BindOnce(&CaptureModeRecordingSizeTest::VerifyVideoFrame,
                       expected_video_size));
  }

  // When changing the device scale factor, the DIPs size of the window doesn't
  // change, but (like |kRegion|) its pixel size will. Hence, the
  // reconfiguration.
  EXPECT_EQ(1, test_delegate->recording_service()
                   ->GetNumberOfVideoEncoderReconfigures());
}

// Tests the behavior of screen recording with the presence of HDCP secure
// content on the screen in all capture mode sources (fullscreen, region, and
// window) depending on the test param.
class CaptureModeHdcpTest
    : public CaptureModeTest,
      public ::testing::WithParamInterface<CaptureModeSource> {
 public:
  CaptureModeHdcpTest() = default;
  ~CaptureModeHdcpTest() override = default;

  // CaptureModeTest:
  void SetUp() override {
    CaptureModeTest::SetUp();
    window_ = CreateTestWindow(gfx::Rect(200, 200));
    // Create a child window with protected content. This simulates the real
    // behavior of a browser window hosting a page with protected content, where
    // the window that has a protection mask is the RenderWidgetHostViewAura,
    // which is a descendant of the BrowserFrame window which can get recorded.
    protected_content_window_ = CreateTestWindow(gfx::Rect(150, 150));
    window_->AddChild(protected_content_window_.get());
    protection_delegate_ = std::make_unique<OutputProtectionDelegate>(
        protected_content_window_.get());
    CaptureModeController::Get()->SetUserCaptureRegion(gfx::Rect(20, 50),
                                                       /*by_user=*/true);
  }

  void TearDown() override {
    protection_delegate_.reset();
    protected_content_window_.reset();
    window_.reset();
    CaptureModeTest::TearDown();
  }

  // Enters the capture mode session.
  void StartSessionForVideo() {
    StartCaptureSession(GetParam(), CaptureModeType::kVideo);
  }

  // Attempts video recording from the capture mode source set by the test
  // param.
  void AttemptRecording() {
    auto* controller = CaptureModeController::Get();
    ASSERT_TRUE(controller->IsActive());

    switch (GetParam()) {
      case CaptureModeSource::kFullscreen:
      case CaptureModeSource::kRegion:
        controller->StartVideoRecordingImmediatelyForTesting();
        break;

      case CaptureModeSource::kWindow:
        // Window capture mode selects the window under the cursor as the
        // capture source.
        auto* event_generator = GetEventGenerator();
        event_generator->MoveMouseToCenterOf(window_.get());
        controller->StartVideoRecordingImmediatelyForTesting();
        break;
    }
  }

 protected:
  std::unique_ptr<aura::Window> window_;
  std::unique_ptr<aura::Window> protected_content_window_;
  std::unique_ptr<OutputProtectionDelegate> protection_delegate_;
};

TEST_P(CaptureModeHdcpTest, WindowBecomesProtectedWhileRecording) {
  StartSessionForVideo();
  AttemptRecording();
  WaitForRecordingToStart();

  auto* controller = CaptureModeController::Get();
  EXPECT_TRUE(controller->is_recording_in_progress());

  // The window becomes HDCP protected, which should end video recording.
  base::HistogramTester histogram_tester;
  protection_delegate_->SetProtection(display::CONTENT_PROTECTION_METHOD_HDCP,
                                      base::DoNothing());

  EXPECT_FALSE(controller->is_recording_in_progress());
  histogram_tester.ExpectBucketCount(
      kEndRecordingReasonInClamshellHistogramName,
      EndRecordingReason::kHdcpInterruption, 1);
}

TEST_P(CaptureModeHdcpTest, ProtectedWindowDestruction) {
  auto window_2 = CreateTestWindow(gfx::Rect(100, 50));
  OutputProtectionDelegate protection_delegate_2(window_2.get());
  protection_delegate_2.SetProtection(display::CONTENT_PROTECTION_METHOD_HDCP,
                                      base::DoNothing());

  StartSessionForVideo();
  AttemptRecording();

  // Recording cannot start because of another protected window on the screen,
  // except when we're capturing a different |window_|.
  auto* controller = CaptureModeController::Get();
  EXPECT_FALSE(controller->IsActive());
  if (GetParam() == CaptureModeSource::kWindow) {
    WaitForRecordingToStart();
    EXPECT_TRUE(controller->is_recording_in_progress());
    controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
    EXPECT_FALSE(controller->is_recording_in_progress());
    // Wait for the video file to be saved so that we can start a new recording.
    WaitForCaptureFileToBeSaved();
  } else {
    EXPECT_FALSE(controller->is_recording_in_progress());
  }

  // When the protected window is destroyed, it's possbile now to record from
  // all capture sources.
  window_2.reset();
  StartSessionForVideo();
  AttemptRecording();
  WaitForRecordingToStart();

  EXPECT_FALSE(controller->IsActive());
  EXPECT_TRUE(controller->is_recording_in_progress());
}

TEST_P(CaptureModeHdcpTest, WindowBecomesProtectedBeforeRecording) {
  protection_delegate_->SetProtection(display::CONTENT_PROTECTION_METHOD_HDCP,
                                      base::DoNothing());
  StartSessionForVideo();
  AttemptRecording();

  // Recording cannot even start.
  auto* controller = CaptureModeController::Get();
  EXPECT_FALSE(controller->is_recording_in_progress());
  EXPECT_FALSE(controller->IsActive());
}

TEST_P(CaptureModeHdcpTest, ProtectedWindowInMultiDisplay) {
  UpdateDisplay("500x400,401+0-500x400");
  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());
  protection_delegate_->SetProtection(display::CONTENT_PROTECTION_METHOD_HDCP,
                                      base::DoNothing());

  // Move the cursor to the secondary display before starting the session to
  // make sure the session starts on that display.
  auto* event_generator = GetEventGenerator();
  MoveMouseToAndUpdateCursorDisplay(roots[1]->GetBoundsInScreen().CenterPoint(),
                                    event_generator);
  StartSessionForVideo();
  // Also, make sure the selected region is in the secondary display.
  auto* controller = CaptureModeController::Get();
  EXPECT_EQ(controller->capture_mode_session()->current_root(), roots[1]);
  AttemptRecording();

  // Recording should be able to start (since the protected window is on the
  // first display) unless the protected window itself is the one being
  // recorded.
  if (GetParam() == CaptureModeSource::kWindow) {
    EXPECT_FALSE(controller->is_recording_in_progress());
  } else {
    WaitForRecordingToStart();
    EXPECT_TRUE(controller->is_recording_in_progress());

    // Moving the protected window to the display being recorded should
    // terminate the recording.
    base::HistogramTester histogram_tester;
    window_util::MoveWindowToDisplay(window_.get(),
                                     roots[1]->GetHost()->GetDisplayId());
    ASSERT_EQ(window_->GetRootWindow(), roots[1]);
    ASSERT_EQ(protected_content_window_->GetRootWindow(), roots[1]);
    EXPECT_FALSE(controller->is_recording_in_progress());
    histogram_tester.ExpectBucketCount(
        kEndRecordingReasonInClamshellHistogramName,
        EndRecordingReason::kHdcpInterruption, 1);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         CaptureModeHdcpTest,
                         testing::Values(CaptureModeSource::kFullscreen,
                                         CaptureModeSource::kRegion,
                                         CaptureModeSource::kWindow));

TEST_F(CaptureModeTest, ClosingWindowBeingRecorded) {
  auto window = CreateTestWindow(gfx::Rect(200, 200));
  StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window.get());
  auto* controller = CaptureModeController::Get();
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());

  // The window should have a valid capture ID.
  EXPECT_TRUE(window->subtree_capture_id().is_valid());

  // Generate a couple of mouse moves, so that the second one gets throttled
  // using the `VideoRecordingWatcher::cursor_events_throttle_timer_`. This is
  // needed for a regression testing of https://crbug.com/1273609.
  event_generator->MoveMouseBy(20, 30);
  event_generator->MoveMouseBy(-10, -20);

  // Closing the window being recorded should end video recording.
  base::HistogramTester histogram_tester;
  window.reset();

  auto* stop_recording_button = Shell::GetPrimaryRootWindowController()
                                    ->GetStatusAreaWidget()
                                    ->stop_recording_button_tray();
  EXPECT_FALSE(stop_recording_button->visible_preferred());
  EXPECT_FALSE(controller->is_recording_in_progress());
  WaitForCaptureFileToBeSaved();
  EXPECT_FALSE(controller->video_recording_watcher_for_testing());
  histogram_tester.ExpectBucketCount(
      kEndRecordingReasonInClamshellHistogramName,
      EndRecordingReason::kDisplayOrWindowClosing, 1);
}

TEST_F(CaptureModeTest, DetachDisplayWhileWindowRecording) {
  UpdateDisplay("500x400,401+0-500x400");
  // Create a window on the second display.
  auto window = CreateTestWindow(gfx::Rect(450, 20, 200, 200));
  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());
  EXPECT_EQ(window->GetRootWindow(), roots[1]);
  StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);

  auto* event_generator = GetEventGenerator();
  MoveMouseToAndUpdateCursorDisplay(window->GetBoundsInScreen().CenterPoint(),
                                    event_generator);
  auto* controller = CaptureModeController::Get();
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());

  auto* stop_recording_button = RootWindowController::ForWindow(roots[1])
                                    ->GetStatusAreaWidget()
                                    ->stop_recording_button_tray();
  EXPECT_TRUE(stop_recording_button->visible_preferred());

  // Disconnecting the display, on which the window being recorded is located,
  // should not end the recording. The window should be reparented to another
  // display, and the stop-recording button should move with to that display.
  RemoveSecondaryDisplay();
  roots = Shell::GetAllRootWindows();
  ASSERT_EQ(1u, roots.size());

  EXPECT_TRUE(controller->is_recording_in_progress());
  stop_recording_button = RootWindowController::ForWindow(roots[0])
                              ->GetStatusAreaWidget()
                              ->stop_recording_button_tray();
  EXPECT_TRUE(stop_recording_button->visible_preferred());
}

TEST_F(CaptureModeTest, SuspendWhileSessionIsActive) {
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  EXPECT_TRUE(controller->IsActive());
  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent::IDLE);
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(CaptureModeTest, SuspendAfterCountdownStarts) {
  // User NORMAL_DURATION for the countdown animation so we can have predictable
  // timings.
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  // Hit Enter to begin recording, wait for 1 second, then suspend the device.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_RETURN, event_generator);
  WaitForSeconds(1);
  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent::IDLE);
  EXPECT_FALSE(controller->IsActive());
  EXPECT_FALSE(controller->is_recording_in_progress());
}

TEST_F(CaptureModeTest, SuspendAfterRecordingStarts) {
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  base::HistogramTester histogram_tester;
  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent::IDLE);
  EXPECT_FALSE(controller->is_recording_in_progress());
  histogram_tester.ExpectBucketCount(
      kEndRecordingReasonInClamshellHistogramName,
      EndRecordingReason::kImminentSuspend, 1);
}

TEST_F(CaptureModeTest, SwitchUsersWhileRecording) {
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  StartVideoRecordingImmediately();
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(controller->is_recording_in_progress());
  SwitchToUser2();
  EXPECT_FALSE(controller->is_recording_in_progress());
  histogram_tester.ExpectBucketCount(
      kEndRecordingReasonInClamshellHistogramName,
      EndRecordingReason::kActiveUserChange, 1);
}

TEST_F(CaptureModeTest, SwitchUsersAfterCountdownStarts) {
  // User NORMAL_DURATION for the countdown animation so we can have predictable
  // timings.
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  // Hit Enter to begin recording, wait for 1 second, then switch users.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_RETURN, event_generator);
  WaitForSeconds(1);
  SwitchToUser2();
  EXPECT_FALSE(controller->IsActive());
  EXPECT_FALSE(controller->is_recording_in_progress());
}

TEST_F(CaptureModeTest, ClosingDisplayBeingFullscreenRecorded) {
  UpdateDisplay("500x400,401+0-500x400");
  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);

  auto* event_generator = GetEventGenerator();
  MoveMouseToAndUpdateCursorDisplay(roots[1]->GetBoundsInScreen().CenterPoint(),
                                    event_generator);
  auto* controller = CaptureModeController::Get();
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());

  auto* stop_recording_button = RootWindowController::ForWindow(roots[1])
                                    ->GetStatusAreaWidget()
                                    ->stop_recording_button_tray();
  EXPECT_TRUE(stop_recording_button->visible_preferred());

  // Disconnecting the display being fullscreen recorded should end the
  // recording and remove the stop recording button.
  base::HistogramTester histogram_tester;
  RemoveSecondaryDisplay();
  roots = Shell::GetAllRootWindows();
  ASSERT_EQ(1u, roots.size());

  EXPECT_FALSE(controller->is_recording_in_progress());
  stop_recording_button = RootWindowController::ForWindow(roots[0])
                              ->GetStatusAreaWidget()
                              ->stop_recording_button_tray();
  EXPECT_FALSE(stop_recording_button->visible_preferred());
  histogram_tester.ExpectBucketCount(
      kEndRecordingReasonInClamshellHistogramName,
      EndRecordingReason::kDisplayOrWindowClosing, 1);
}

TEST_F(CaptureModeTest, ShuttingDownWhileRecording) {
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);

  auto* controller = CaptureModeController::Get();
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());

  // Exiting the test now will shut down ash while recording is in progress,
  // there should be no crashes when
  // VideoRecordingWatcher::OnChromeTerminating() terminates the recording.
}

// Tests that metrics are recorded properly for capture mode bar buttons.
TEST_F(CaptureModeTest, CaptureModeBarButtonTypeHistograms) {
  constexpr char kClamshellHistogram[] =
      "Ash.CaptureModeController.BarButtons.ClamshellMode";
  constexpr char kTabletHistogram[] =
      "Ash.CaptureModeController.BarButtons.TabletMode";
  base::HistogramTester histogram_tester;

  CaptureModeController::Get()->Start(CaptureModeEntryType::kQuickSettings);
  auto* event_generator = GetEventGenerator();

  // Tests each bar button in clamshell mode.
  ClickOnView(GetImageToggleButton(), event_generator);
  histogram_tester.ExpectBucketCount(
      kClamshellHistogram, CaptureModeBarButtonType::kScreenCapture, 1);

  ClickOnView(GetVideoToggleButton(), event_generator);
  histogram_tester.ExpectBucketCount(
      kClamshellHistogram, CaptureModeBarButtonType::kScreenRecord, 1);

  ClickOnView(GetFullscreenToggleButton(), event_generator);
  histogram_tester.ExpectBucketCount(kClamshellHistogram,
                                     CaptureModeBarButtonType::kFull, 1);

  ClickOnView(GetRegionToggleButton(), event_generator);
  histogram_tester.ExpectBucketCount(kClamshellHistogram,
                                     CaptureModeBarButtonType::kRegion, 1);

  ClickOnView(GetWindowToggleButton(), event_generator);
  histogram_tester.ExpectBucketCount(kClamshellHistogram,
                                     CaptureModeBarButtonType::kWindow, 1);

  // Enter tablet mode and test the bar buttons.
  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();
  tablet_mode_controller->SetEnabledForTest(true);
  ASSERT_TRUE(tablet_mode_controller->InTabletMode());

  ClickOnView(GetImageToggleButton(), event_generator);
  histogram_tester.ExpectBucketCount(
      kTabletHistogram, CaptureModeBarButtonType::kScreenCapture, 1);

  ClickOnView(GetVideoToggleButton(), event_generator);
  histogram_tester.ExpectBucketCount(
      kTabletHistogram, CaptureModeBarButtonType::kScreenRecord, 1);

  ClickOnView(GetFullscreenToggleButton(), event_generator);
  histogram_tester.ExpectBucketCount(kTabletHistogram,
                                     CaptureModeBarButtonType::kFull, 1);

  ClickOnView(GetRegionToggleButton(), event_generator);
  histogram_tester.ExpectBucketCount(kTabletHistogram,
                                     CaptureModeBarButtonType::kRegion, 1);

  ClickOnView(GetWindowToggleButton(), event_generator);
  histogram_tester.ExpectBucketCount(kTabletHistogram,
                                     CaptureModeBarButtonType::kWindow, 1);
}

TEST_F(CaptureModeTest, CaptureSessionSwitchedModeMetric) {
  constexpr char kHistogramName[] =
      "Ash.CaptureModeController.SwitchesFromInitialCaptureMode";
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHistogramName, false, 0);
  histogram_tester.ExpectBucketCount(kHistogramName, true, 0);

  // Perform a capture without switching modes. A false should be recorded.
  auto* controller = StartImageRegionCapture();
  SelectRegion(gfx::Rect(100, 100));
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_RETURN, event_generator);
  histogram_tester.ExpectBucketCount(kHistogramName, false, 1);
  histogram_tester.ExpectBucketCount(kHistogramName, true, 0);

  // Perform a capture after switching to fullscreen mode. A true should be
  // recorded.
  controller->Start(CaptureModeEntryType::kQuickSettings);
  ClickOnView(GetFullscreenToggleButton(), event_generator);
  SendKey(ui::VKEY_RETURN, event_generator);
  histogram_tester.ExpectBucketCount(kHistogramName, false, 1);
  histogram_tester.ExpectBucketCount(kHistogramName, true, 1);

  // Perform a capture after switching to another mode and back to the original
  // mode. A true should still be recorded as there was some switching done.
  controller->Start(CaptureModeEntryType::kQuickSettings);
  ClickOnView(GetRegionToggleButton(), event_generator);
  ClickOnView(GetFullscreenToggleButton(), event_generator);
  SendKey(ui::VKEY_RETURN, event_generator);
  histogram_tester.ExpectBucketCount(kHistogramName, false, 1);
  histogram_tester.ExpectBucketCount(kHistogramName, true, 2);
}

// Test that cancel recording during countdown won't cause crash.
TEST_F(CaptureModeTest, CancelCaptureDuringCountDown) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  // Hit Enter to begin recording, Wait for 1 second, then press ESC while count
  // down is in progress.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_RETURN, event_generator);
  WaitForSeconds(1);
  SendKey(ui::VKEY_ESCAPE, event_generator);
}

// Tests that metrics are recorded properly for capture region adjustments.
TEST_F(CaptureModeTest, NumberOfCaptureRegionAdjustmentsHistogram) {
  constexpr char kClamshellHistogram[] =
      "Ash.CaptureModeController.CaptureRegionAdjusted.ClamshellMode";
  constexpr char kTabletHistogram[] =
      "Ash.CaptureModeController.CaptureRegionAdjusted.TabletMode";
  base::HistogramTester histogram_tester;
  UpdateDisplay("800x700");

  auto* controller = StartImageRegionCapture();
  // Create the initial region.
  const gfx::Rect target_region(gfx::Rect(200, 200, 400, 400));
  SelectRegion(target_region);

  auto resize_and_reset_region = [](ui::test::EventGenerator* event_generator,
                                    const gfx::Point& top_right) {
    // Enlarges the region and then resize it back to its original size.
    event_generator->set_current_screen_location(top_right);
    event_generator->DragMouseTo(top_right + gfx::Vector2d(50, 50));
    event_generator->DragMouseTo(top_right);
  };

  auto move_and_reset_region = [](ui::test::EventGenerator* event_generator,
                                  const gfx::Point& drag_point) {
    // Moves the region and then moves it back to its original position.
    event_generator->set_current_screen_location(drag_point);
    event_generator->DragMouseTo(drag_point + gfx::Vector2d(-50, -50));
    event_generator->DragMouseTo(drag_point);
  };

  // Resize the region twice by dragging the top right of the region out and
  // then back again.
  auto* event_generator = GetEventGenerator();
  auto top_right = target_region.top_right();
  resize_and_reset_region(event_generator, top_right);

  // Move the region twice by dragging within the region.
  const gfx::Point drag_point(300, 300);
  move_and_reset_region(event_generator, drag_point);

  // Perform a capture to record the count.
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(kClamshellHistogram, 4, 1);

  // Create a new image region capture. Move the region twice then change
  // sources to fullscreen and back to region. This toggle should reset the
  // count. Perform a capture to record the count.
  StartImageRegionCapture();
  move_and_reset_region(event_generator, drag_point);
  controller->SetSource(CaptureModeSource::kFullscreen);
  controller->SetSource(CaptureModeSource::kRegion);
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(kClamshellHistogram, 0, 1);

  // Enter tablet mode and restart the capture session. The capture region
  // should be remembered.
  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();
  tablet_mode_controller->SetEnabledForTest(true);
  ASSERT_TRUE(tablet_mode_controller->InTabletMode());
  StartImageRegionCapture();
  ASSERT_EQ(target_region, controller->user_capture_region());

  // Resize the region twice by dragging the top right of the region out and
  // then back again.
  resize_and_reset_region(event_generator, top_right);

  // Move the region twice by dragging within the region.
  move_and_reset_region(event_generator, drag_point);

  // Perform a capture to record the count.
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(kTabletHistogram, 4, 1);

  // Restart the region capture and resize it. Then create a new region by
  // dragging outside of the existing capture region. This should reset the
  // counter. Change source to record a sample.
  StartImageRegionCapture();
  resize_and_reset_region(event_generator, top_right);
  SelectRegion(gfx::Rect(0, 0, 100, 100));
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(kTabletHistogram, 0, 1);
}

TEST_F(CaptureModeTest, FullscreenCapture) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kImage);
  EXPECT_TRUE(controller->IsActive());
  // Press anywhere to capture image.
  auto* event_generator = GetEventGenerator();
  event_generator->ClickLeftButton();
  EXPECT_FALSE(controller->IsActive());

  controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                   CaptureModeType::kVideo);
  EXPECT_TRUE(controller->IsActive());
  // Press anywhere to capture video.
  event_generator->ClickLeftButton();
  WaitForRecordingToStart();
  EXPECT_FALSE(controller->IsActive());
}

// Tests that metrics are recorded properly for capture mode configurations when
// taking a screenshot.
TEST_F(CaptureModeTest, ScreenshotConfigurationHistogram) {
  constexpr char kClamshellHistogram[] =
      "Ash.CaptureModeController.CaptureConfiguration.ClamshellMode";
  constexpr char kTabletHistogram[] =
      "Ash.CaptureModeController.CaptureConfiguration.TabletMode";
  base::HistogramTester histogram_tester;
  // Use a set display size as we will be choosing points in this test.
  UpdateDisplay("800x700");

  // Create a window for window captures later.
  std::unique_ptr<aura::Window> window1(
      CreateTestWindow(gfx::Rect(600, 600, 100, 100)));

  // Perform a fullscreen screenshot.
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kImage);
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(
      kClamshellHistogram, CaptureModeConfiguration::kFullscreenScreenshot, 1);

  // Perform a region screenshot.
  controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  const gfx::Rect capture_region(200, 200, 400, 400);
  SelectRegion(capture_region);
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(
      kClamshellHistogram, CaptureModeConfiguration::kRegionScreenshot, 1);

  // Perform a window screenshot.
  controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window1.get());
  EXPECT_EQ(window1.get(),
            controller->capture_mode_session()->GetSelectedWindow());
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(
      kClamshellHistogram, CaptureModeConfiguration::kWindowScreenshot, 1);

  // Switch to tablet mode.
  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();
  tablet_mode_controller->SetEnabledForTest(true);
  ASSERT_TRUE(tablet_mode_controller->InTabletMode());

  // Perform a fullscreen screenshot.
  controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                   CaptureModeType::kImage);
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(
      kTabletHistogram, CaptureModeConfiguration::kFullscreenScreenshot, 1);

  // Perform a region screenshot.
  controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(
      kTabletHistogram, CaptureModeConfiguration::kRegionScreenshot, 1);

  // Perform a window screenshot.
  controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);
  event_generator->MoveMouseToCenterOf(window1.get());
  EXPECT_EQ(window1.get(),
            controller->capture_mode_session()->GetSelectedWindow());
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(
      kTabletHistogram, CaptureModeConfiguration::kWindowScreenshot, 1);
}

// Tests that there is no crash when touching the capture label widget in tablet
// mode when capturing a window. Regression test for https://crbug.com/1152938.
TEST_F(CaptureModeTest, TabletTouchCaptureLabelWidgetWindowMode) {
  SwitchToTabletMode();

  // Enter capture window mode.
  CaptureModeController* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);
  ASSERT_TRUE(controller->IsActive());

  // Press and release on where the capture label widget would be.
  auto* event_generator = GetEventGenerator();
  DCHECK(GetCaptureModeLabelWidget());
  event_generator->set_current_screen_location(
      GetCaptureModeLabelWidget()->GetWindowBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  event_generator->ReleaseTouch();

  // There are no windows and home screen window is excluded from window capture
  // mode, so capture mode will still remain active.
  EXPECT_TRUE(Shell::Get()->app_list_controller()->IsHomeScreenVisible());
  EXPECT_TRUE(controller->IsActive());
}

// Tests that after rotating a display, the capture session widgets are updated
// and the capture region is reset.
TEST_F(CaptureModeTest, DisplayRotation) {
  UpdateDisplay("1200x600");

  auto* controller = StartImageRegionCapture();
  SelectRegion(gfx::Rect(1200, 400));

  // Rotate the primary display by 90 degrees. Test that the region and capture
  // bar fit within the rotated bounds, and the capture label widget is still
  // centered in the region.
  Shell::Get()->display_manager()->SetDisplayRotation(
      WindowTreeHostManager::GetPrimaryDisplayId(), display::Display::ROTATE_90,
      display::Display::RotationSource::USER);
  const gfx::Rect rotated_root_bounds(600, 1200);
  EXPECT_TRUE(rotated_root_bounds.Contains(controller->user_capture_region()));
  EXPECT_TRUE(rotated_root_bounds.Contains(
      GetCaptureModeBarView()->GetBoundsInScreen()));
  views::Widget* capture_label_widget = GetCaptureModeLabelWidget();
  ASSERT_TRUE(capture_label_widget);
  EXPECT_EQ(controller->user_capture_region().CenterPoint(),
            capture_label_widget->GetWindowBoundsInScreen().CenterPoint());
}

TEST_F(CaptureModeTest, DisplayBoundsChange) {
  UpdateDisplay("1200x600");

  auto* controller = StartImageRegionCapture();
  SelectRegion(gfx::Rect(1200, 400));

  // Shrink the display. The capture region should shrink, and the capture bar
  // should be adjusted to be centered.
  UpdateDisplay("700x600");
  EXPECT_EQ(gfx::Rect(700, 400), controller->user_capture_region());
  EXPECT_EQ(350,
            GetCaptureModeBarView()->GetBoundsInScreen().CenterPoint().x());
}

TEST_F(CaptureModeTest, ReenterOnSmallerDisplay) {
  UpdateDisplay("1200x600,1201+0-700x600");

  // Start off with the primary display as the targeted display. Create a region
  // that fits the primary display but would be too big for the secondary
  // display.
  auto* event_generator = GetEventGenerator();
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(700, 300), event_generator);
  auto* controller = StartImageRegionCapture();
  SelectRegion(gfx::Rect(1200, 400));
  EXPECT_EQ(gfx::Rect(1200, 400), controller->user_capture_region());
  controller->Stop();

  // Make the secondary display the targeted display. Test that the region has
  // shrunk to fit the display.
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(1500, 300), event_generator);
  StartImageRegionCapture();
  EXPECT_EQ(gfx::Rect(700, 400), controller->user_capture_region());
}

// Tests tabbing when in capture window mode.
TEST_F(CaptureModeTest, KeyboardNavigationBasic) {
  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);

  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());

  // Initially nothing is focused.
  EXPECT_EQ(FocusGroup::kNone, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Tab once, we are now focusing the type and source buttons group on the
  // capture bar.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Tab four times to focus the last source button (window mode button).
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, /*count=*/4);
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(4u, test_api.GetCurrentFocusIndex());

  // Tab once to focus the settings and close buttons group on the capture bar.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Shift tab to focus the last source button again.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/true);
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(4u, test_api.GetCurrentFocusIndex());

  // Press esc to clear focus, but remain in capture mode.
  SendKey(ui::VKEY_ESCAPE, event_generator);
  EXPECT_EQ(FocusGroup::kNone, test_api.GetCurrentFocusGroup());
  EXPECT_TRUE(controller->IsActive());

  // Tests that pressing esc when there is no focus will exit capture mode.
  SendKey(ui::VKEY_ESCAPE, event_generator);
  EXPECT_FALSE(controller->IsActive());
}

// Tests tabbing through windows on multiple displays when in capture window
// mode.
TEST_F(CaptureModeTest, KeyboardNavigationTabThroughWindowsOnMultipleDisplays) {
  UpdateDisplay("800x700,801+0-800x700");
  std::vector<aura::Window*> root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  // Create three windows, one of them is a modal transient child.
  std::unique_ptr<aura::Window> window1(
      CreateTestWindow(gfx::Rect(0, 0, 200, 200)));
  auto window1_transient = CreateTransientModalChildWindow(
      window1.get(), gfx::Rect(20, 30, 200, 150));
  std::unique_ptr<aura::Window> window2(
      CreateTestWindow(gfx::Rect(900, 0, 200, 200)));

  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);
  CaptureModeSession* capture_mode_session = controller->capture_mode_session();

  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  CaptureModeSessionTestApi test_api(capture_mode_session);

  // Initially nothing is focused.
  EXPECT_EQ(FocusGroup::kNone, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Tab five times, we are now focusing the window mode button on the
  // capture bar.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_TAB, event_generator, false, 5);
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(4u, test_api.GetCurrentFocusIndex());

  // Enter space to select window mode.
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(FocusGroup::kTypeSource, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(CaptureModeSource::kWindow, controller->source());

  // Tab once, we are now focusing |window2| and capture mode bar is on
  // display2.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(window2.get(), capture_mode_session->GetSelectedWindow());
  EXPECT_EQ(root_windows[1], capture_mode_session->current_root());

  // Tab once, we are now focusing |window1_transient|. Since
  // |window1_transient| is on display1, capture mode bar will be moved to
  // display1 as well.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(window1_transient.get(), capture_mode_session->GetSelectedWindow());
  EXPECT_EQ(root_windows[0], capture_mode_session->current_root());

  // Tab once, we are now focusing |window1|. Capture mode bar still stays on
  // display1.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(2u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(window1.get(), capture_mode_session->GetSelectedWindow());
  EXPECT_EQ(root_windows[0], capture_mode_session->current_root());

  // Press space, make sure nothing is changed and no crash.
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_TRUE(controller->IsActive());
  EXPECT_EQ(window1.get(), capture_mode_session->GetSelectedWindow());
  EXPECT_EQ(root_windows[0], capture_mode_session->current_root());

  // Tab once to focus the settings and close buttons group on the capture bar.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Shift tab to focus |window1| again.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/true);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(2u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(window1.get(), capture_mode_session->GetSelectedWindow());

  // Shift tab to focus |window1_transient|.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/true);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(1u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(window1_transient.get(), capture_mode_session->GetSelectedWindow());
  EXPECT_EQ(root_windows[0], capture_mode_session->current_root());

  // Shift tab to focus |window2|. Capture mode bar will be moved to display2 as
  // well.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/true);
  EXPECT_EQ(FocusGroup::kCaptureWindow, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  EXPECT_EQ(window2.get(), capture_mode_session->GetSelectedWindow());
  EXPECT_EQ(root_windows[1], capture_mode_session->current_root());

  // Press esc to clear focus, but remain in capture mode with |window2|
  // selected.
  SendKey(ui::VKEY_ESCAPE, event_generator);
  EXPECT_EQ(FocusGroup::kNone, test_api.GetCurrentFocusGroup());
  EXPECT_TRUE(controller->IsActive());
  EXPECT_EQ(window2.get(), capture_mode_session->GetSelectedWindow());
  EXPECT_EQ(root_windows[1], capture_mode_session->current_root());

  // Press return. Since there's a selected window, capture mode will
  // be ended after capturing the selected window.
  SendKey(ui::VKEY_RETURN, event_generator);
  EXPECT_FALSE(controller->IsActive());
}

// Tests that a click will remove focus.
TEST_F(CaptureModeTest, KeyboardNavigationClicksRemoveFocus) {
  auto* controller = StartImageRegionCapture();
  auto* event_generator = GetEventGenerator();

  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_TRUE(test_api.HasFocus());

  event_generator->ClickLeftButton();
  EXPECT_FALSE(test_api.HasFocus());
}

// Tests that pressing space on a focused button will click it.
TEST_F(CaptureModeTest, KeyboardNavigationSpaceToClickButtons) {
  auto* controller = StartImageRegionCapture();
  SelectRegion(gfx::Rect(200, 200));

  auto* event_generator = GetEventGenerator();

  // Tab to the button which changes the capture type to video and hit space.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, 2);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(CaptureModeType::kVideo, controller->type());

  // Shift tab and space to change the capture type back to image.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/true);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(CaptureModeType::kImage, controller->type());

  // Tab to the fullscreen button and hit space.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, 2);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(CaptureModeSource::kFullscreen, controller->source());

  // Tab to the region button and hit space to return to region capture mode.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(CaptureModeSource::kRegion, controller->source());

  // Tab to the capture button and hit space to perform a capture, which exits
  // capture mode.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, 11);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(CaptureModeTest, KeyboardNavigationSettingsMenuBehavior) {
  // This test is specific to the old settings view.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kImprovedScreenCaptureSettings);

  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;

  // Use window capture mode to avoid having to tab through the selection
  // region.
  auto* controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);
  auto* event_generator = GetEventGenerator();

  // Tests that pressing tab closes the settings menu if it was opened with a
  // button click.
  ClickOnView(GetSettingsButton(), event_generator);
  ASSERT_TRUE(GetCaptureModeSettingsWidget());
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_FALSE(GetCaptureModeSettingsWidget());

  // Tab until we reach the settings button and press space to open the settings
  // menu.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, 6);
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  ASSERT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  ASSERT_EQ(0u, test_api.GetCurrentFocusIndex());
  SendKey(ui::VKEY_SPACE, event_generator);
  ASSERT_TRUE(GetCaptureModeSettingsWidget());

  // Verify that at this point, the focus cycler is in a pending state.
  EXPECT_EQ(FocusGroup::kPendingSettings, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // The next tab should focus the microphone settings entry. Pressing space
  // should toggle the setting.
  SendKey(ui::VKEY_TAB, event_generator);
  ASSERT_FALSE(controller->enable_audio_recording());
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_TRUE(controller->enable_audio_recording());

  // Tests that the next tab will keep the menu open and move focus to the
  // settings button.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_TRUE(GetCaptureModeSettingsWidget());
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Tests that pressing escape will close the menu and clear focus.
  SendKey(ui::VKEY_ESCAPE, event_generator);
  EXPECT_FALSE(GetCaptureModeSettingsWidget());
}

// Tests that functionality to create and adjust a region with keyboard
// shortcuts works as intended.
TEST_F(CaptureModeTest, KeyboardNavigationSelectRegion) {
  auto* controller = StartImageRegionCapture();
  auto* event_generator = GetEventGenerator();
  ASSERT_TRUE(controller->user_capture_region().IsEmpty());

  // Test that hitting space will create a default region.
  SendKey(ui::VKEY_SPACE, event_generator);
  gfx::Rect capture_region = controller->user_capture_region();
  EXPECT_FALSE(capture_region.IsEmpty());

  // Test that hitting an arrow key will do nothing as the selection region is
  // not focused initially.
  SendKey(ui::VKEY_RIGHT, event_generator);
  EXPECT_EQ(capture_region, controller->user_capture_region());

  const int arrow_shift = capture_mode::kArrowKeyboardRegionChangeDp;

  // Hit tab until the whole region is focused.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, /*count=*/6);
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  EXPECT_EQ(CaptureModeSessionFocusCycler::FocusGroup::kSelection,
            test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Arrow keys should shift the whole region.
  SendKey(ui::VKEY_RIGHT, event_generator);
  EXPECT_EQ(capture_region.origin() + gfx::Vector2d(arrow_shift, 0),
            controller->user_capture_region().origin());
  EXPECT_EQ(capture_region.size(), controller->user_capture_region().size());
  SendKey(ui::VKEY_RIGHT, event_generator, /*shift_down=*/true);
  EXPECT_EQ(
      capture_region.origin() +
          gfx::Vector2d(
              arrow_shift + capture_mode::kShiftArrowKeyboardRegionChangeDp, 0),
      controller->user_capture_region().origin());
  EXPECT_EQ(capture_region.size(), controller->user_capture_region().size());

  // Hit tab so that the top left affordance circle is focused. Left and up keys
  // should enlarge the region, right and bottom keys should shrink the region.
  capture_region = controller->user_capture_region();
  SendKey(ui::VKEY_TAB, event_generator);
  SendKey(ui::VKEY_LEFT, event_generator);
  SendKey(ui::VKEY_UP, event_generator);
  EXPECT_EQ(capture_region.size() + gfx::Size(arrow_shift, arrow_shift),
            controller->user_capture_region().size());
  SendKey(ui::VKEY_RIGHT, event_generator);
  SendKey(ui::VKEY_DOWN, event_generator);
  EXPECT_EQ(capture_region.size(), controller->user_capture_region().size());

  // Tab until we focus the bottom right affordance circle. Left and up keys
  // should shrink the region, right and bottom keys should enlarge the region.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, 4);

  SendKey(ui::VKEY_LEFT, event_generator);
  SendKey(ui::VKEY_UP, event_generator);
  EXPECT_EQ(capture_region.size() - gfx::Size(arrow_shift, arrow_shift),
            controller->user_capture_region().size());
  SendKey(ui::VKEY_RIGHT, event_generator);
  SendKey(ui::VKEY_DOWN, event_generator);
  EXPECT_EQ(capture_region.size(), controller->user_capture_region().size());
}

// Tests behavior regarding the default region when using keyboard navigation.
TEST_F(CaptureModeTest, KeyboardNavigationDefaultRegion) {
  auto* controller = StartImageRegionCapture();
  auto* event_generator = GetEventGenerator();
  ASSERT_TRUE(controller->user_capture_region().IsEmpty());

  // Hit space when nothing is focused to get the expected default capture
  // region.
  SendKey(ui::VKEY_SPACE, event_generator);
  const gfx::Rect expected_default_region = controller->user_capture_region();
  SelectRegion(gfx::Rect(20, 20, 200, 200));

  // Hit space when there is an existing region. Tests that the region remains
  // unchanged.
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(gfx::Rect(20, 20, 200, 200), controller->user_capture_region());

  // Tab to the image toggle button. Tests that hitting space does not change
  // the region size.
  SelectRegion(gfx::Rect());
  SendKey(ui::VKEY_TAB, event_generator);
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());
  ASSERT_EQ(CaptureModeSessionFocusCycler::FocusGroup::kTypeSource,
            test_api.GetCurrentFocusGroup());
  ASSERT_EQ(0u, test_api.GetCurrentFocusIndex());

  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(gfx::Rect(), controller->user_capture_region());

  // Tests that hitting space while focusing the region toggle button when in
  // region capture mode will make the capture region the default size.
  // SelectRegion removes focus since it uses mouse clicks.
  SelectRegion(gfx::Rect());
  SendKey(ui::VKEY_TAB, event_generator,
          /*is_shift_down=*/false,
          /*count=*/4);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(expected_default_region, controller->user_capture_region());

  // Tests that hitting space while focusing the region toggle button when not
  // in region capture mode does nothing to the capture region.
  SelectRegion(gfx::Rect());
  ClickOnView(GetWindowToggleButton(), event_generator);
  SendKey(ui::VKEY_TAB, event_generator,
          /*is_shift_down=*/false,
          /*count=*/4);
  ASSERT_EQ(CaptureModeSessionFocusCycler::FocusGroup::kTypeSource,
            test_api.GetCurrentFocusGroup());
  ASSERT_EQ(3u, test_api.GetCurrentFocusIndex());
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(gfx::Rect(), controller->user_capture_region());
}

// Tests that accessibility overrides are set as expected on capture mode
// widgets.
TEST_F(CaptureModeTest, AccessibilityFocusAnnotator) {
  StartImageRegionCapture();

  // Helper that takes in a current widget and checks if the accessibility next
  // and previous focus widgets match the given.
  auto check_a11y_overrides = [](const std::string& id, views::Widget* widget,
                                 views::Widget* expected_previous,
                                 views::Widget* expected_next) -> void {
    SCOPED_TRACE(id);
    views::View* contents_view = widget->GetContentsView();
    views::ViewAccessibility& view_accessibility =
        contents_view->GetViewAccessibility();
    EXPECT_EQ(expected_previous, view_accessibility.GetPreviousFocus());
    EXPECT_EQ(expected_next, view_accessibility.GetNextFocus());
  };

  // With no region, there is no capture label button and no settings menu
  // opened, so the bar is the only focusable capture session widget.
  views::Widget* bar_widget = GetCaptureModeBarWidget();
  check_a11y_overrides("bar", bar_widget, nullptr, nullptr);

  // With a region, the focus should go from the bar widget to the label widget
  // and back.
  SendKey(ui::VKEY_SPACE, GetEventGenerator());
  views::Widget* label_widget = GetCaptureModeLabelWidget();
  check_a11y_overrides("bar", bar_widget, label_widget, label_widget);
  check_a11y_overrides("label", label_widget, bar_widget, bar_widget);

  // With a settings menu open, the focus should go from the bar widget to the
  // label widget to the settings widget and back to the bar widget.
  ClickOnView(GetSettingsButton(), GetEventGenerator());
  views::Widget* settings_widget = GetCaptureModeSettingsWidget();
  ASSERT_TRUE(settings_widget);
  check_a11y_overrides("bar", bar_widget, settings_widget, label_widget);
  check_a11y_overrides("label", label_widget, bar_widget, settings_widget);
  check_a11y_overrides("settings", settings_widget, label_widget, bar_widget);
}

// Tests that a captured image is written to the clipboard.
TEST_F(CaptureModeTest, ClipboardWrite) {
  auto* clipboard = ui::Clipboard::GetForCurrentThread();
  ASSERT_NE(clipboard, nullptr);

  const ui::ClipboardSequenceNumberToken before_sequence_number =
      clipboard->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste);

  CaptureNotificationWaiter waiter;
  CaptureModeController::Get()->CaptureScreenshotsOfAllDisplays();
  waiter.Wait();

  const ui::ClipboardSequenceNumberToken after_sequence_number =
      clipboard->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste);

  EXPECT_NE(before_sequence_number, after_sequence_number);
}

// A test class that uses a mock time task environment.
class CaptureModeMockTimeTest : public CaptureModeTest {
 public:
  CaptureModeMockTimeTest()
      : CaptureModeTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  CaptureModeMockTimeTest(const CaptureModeMockTimeTest&) = delete;
  CaptureModeMockTimeTest& operator=(const CaptureModeMockTimeTest&) = delete;
  ~CaptureModeMockTimeTest() override = default;
};

// Tests that the consecutive screenshots histogram is recorded properly.
TEST_F(CaptureModeMockTimeTest, ConsecutiveScreenshotsHistograms) {
  constexpr char kConsecutiveScreenshotsHistogram[] =
      "Ash.CaptureModeController.ConsecutiveScreenshots";
  base::HistogramTester histogram_tester;

  auto take_n_screenshots = [this](int n) {
    for (int i = 0; i < n; ++i) {
      auto* controller = StartImageRegionCapture();
      controller->PerformCapture();
    }
  };

  // Take three consecutive screenshots. Should only record after 5 seconds.
  StartImageRegionCapture();
  const gfx::Rect capture_region(200, 200, 400, 400);
  SelectRegion(capture_region);
  take_n_screenshots(3);
  histogram_tester.ExpectBucketCount(kConsecutiveScreenshotsHistogram, 3, 0);
  task_environment()->FastForwardBy(base::Seconds(5));
  histogram_tester.ExpectBucketCount(kConsecutiveScreenshotsHistogram, 3, 1);

  // Take only one screenshot. This should not be recorded.
  take_n_screenshots(1);
  histogram_tester.ExpectBucketCount(kConsecutiveScreenshotsHistogram, 1, 0);
  task_environment()->FastForwardBy(base::Seconds(5));
  histogram_tester.ExpectBucketCount(kConsecutiveScreenshotsHistogram, 1, 0);

  // Take a screenshot, change source and take another screenshot. This should
  // count as 2 consecutive screenshots.
  take_n_screenshots(1);
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kImage);
  controller->PerformCapture();
  histogram_tester.ExpectBucketCount(kConsecutiveScreenshotsHistogram, 2, 0);
  task_environment()->FastForwardBy(base::Seconds(5));
  histogram_tester.ExpectBucketCount(kConsecutiveScreenshotsHistogram, 2, 1);
}

// Tests that the user capture region will be cleared up after a period of time.
TEST_F(CaptureModeMockTimeTest, ClearUserCaptureRegionBetweenSessions) {
  UpdateDisplay("900x800");
  auto* controller = StartImageRegionCapture();
  EXPECT_EQ(gfx::Rect(), controller->user_capture_region());

  const gfx::Rect capture_region(100, 100, 600, 700);
  SelectRegion(capture_region);
  EXPECT_EQ(capture_region, controller->user_capture_region());
  controller->PerformCapture();
  EXPECT_EQ(capture_region, controller->user_capture_region());

  // Start region image capture again shortly after the previous capture
  // session, we should still be able to reuse the previous capture region.
  task_environment()->FastForwardBy(base::Minutes(1));
  StartImageRegionCapture();
  EXPECT_EQ(capture_region, controller->user_capture_region());
  auto* event_generator = GetEventGenerator();
  // Even if the capture is cancelled, we still remember the capture region.
  SendKey(ui::VKEY_ESCAPE, event_generator);
  EXPECT_EQ(capture_region, controller->user_capture_region());

  // Wait for 8 second and then start region image capture again. We should have
  // forgot the previous capture region.
  task_environment()->FastForwardBy(base::Minutes(8));
  StartImageRegionCapture();
  EXPECT_EQ(gfx::Rect(), controller->user_capture_region());
}

// Tests that in Region mode, the capture bar hides and shows itself correctly.
TEST_F(CaptureModeTest, CaptureBarOpacity) {
  UpdateDisplay("800x700");

  auto* event_generator = GetEventGenerator();
  auto* controller = StartImageRegionCapture();
  EXPECT_TRUE(controller->IsActive());

  ui::Layer* capture_bar_layer = GetCaptureModeBarWidget()->GetLayer();

  // Check to see it starts off opaque.
  EXPECT_EQ(1.f, capture_bar_layer->GetTargetOpacity());

  // Make sure that the bar is transparent when selecting a region.
  const gfx::Rect target_region(gfx::BoundingRect(
      gfx::Point(0, 0),
      GetCaptureModeBarView()->GetBoundsInScreen().top_right() +
          gfx::Vector2d(0, -50)));
  event_generator->MoveMouseTo(target_region.origin());
  event_generator->PressLeftButton();
  EXPECT_EQ(0.f, capture_bar_layer->GetTargetOpacity());
  event_generator->MoveMouseTo(target_region.bottom_right());
  EXPECT_EQ(0.f, capture_bar_layer->GetTargetOpacity());
  event_generator->ReleaseLeftButton();

  // When there is no overlap of the selected region and the bar, the bar should
  // be opaque.
  EXPECT_EQ(1.f, capture_bar_layer->GetTargetOpacity());

  // Bar becomes transparent when the region is being moved.
  event_generator->MoveMouseTo(target_region.origin() + gfx::Vector2d(50, 50));
  event_generator->PressLeftButton();
  EXPECT_EQ(0.f, capture_bar_layer->GetTargetOpacity());
  event_generator->MoveMouseTo(target_region.bottom_center());
  EXPECT_EQ(0.f, capture_bar_layer->GetTargetOpacity());
  event_generator->ReleaseLeftButton();

  // The region overlaps the capture bar, so we set the opacity of the bar to
  // 0.1f.
  EXPECT_EQ(0.1f, capture_bar_layer->GetTargetOpacity());

  // When there is overlap, the toolbar turns opaque on mouseover.
  event_generator->MoveMouseTo(
      GetCaptureModeBarView()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(1.f, capture_bar_layer->GetTargetOpacity());

  // Capture bar drops back to 0.1 opacity when the mouse is no longer hovering.
  event_generator->MoveMouseTo(
      GetCaptureModeBarView()->GetBoundsInScreen().top_center() +
      gfx::Vector2d(0, -50));
  EXPECT_EQ(0.1f, capture_bar_layer->GetTargetOpacity());

  // Check that the opacity is reset when we select another region.
  SelectRegion(target_region);
  EXPECT_EQ(1.f, capture_bar_layer->GetTargetOpacity());
}

// Tests that the quick action histogram is recorded properly.
TEST_F(CaptureModeTest, QuickActionHistograms) {
  constexpr char kQuickActionHistogramName[] =
      "Ash.CaptureModeController.QuickAction";
  base::HistogramTester histogram_tester;

  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kImage);
  EXPECT_TRUE(controller->IsActive());
  {
    CaptureNotificationWaiter waiter;
    controller->PerformCapture();
    waiter.Wait();
  }
  // Verify clicking delete on screenshot notification.
  base::RunLoop loop;
  SetUpFileDeletionVerifier(&loop);
  const int delete_button = 1;
  ClickNotification(delete_button);
  loop.Run();
  EXPECT_FALSE(GetPreviewNotification());
  histogram_tester.ExpectBucketCount(kQuickActionHistogramName,
                                     CaptureQuickAction::kDelete, 1);

  controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                   CaptureModeType::kImage);
  {
    CaptureNotificationWaiter waiter;
    controller->PerformCapture();
    waiter.Wait();
  }
  // Click on the notification body. This should take us to the files app.
  ClickNotification(absl::nullopt);
  EXPECT_FALSE(GetPreviewNotification());
  histogram_tester.ExpectBucketCount(kQuickActionHistogramName,
                                     CaptureQuickAction::kFiles, 1);

  controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                   CaptureModeType::kImage);

  {
    CaptureNotificationWaiter waiter;
    controller->PerformCapture();
    waiter.Wait();
  }
  const int edit_button = 0;
  // Verify clicking edit on screenshot notification.
  ClickNotification(edit_button);
  EXPECT_FALSE(GetPreviewNotification());
  histogram_tester.ExpectBucketCount(kQuickActionHistogramName,
                                     CaptureQuickAction::kBacklight, 1);
}

TEST_F(CaptureModeTest, CannotDoMultipleRecordings) {
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);

  auto* controller = CaptureModeController::Get();
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  EXPECT_EQ(CaptureModeType::kVideo, controller->type());

  // Start a new session with the current type which set to kVideo, the type
  // should be switched automatically to kImage, and video toggle button should
  // be disabled.
  controller->Start(CaptureModeEntryType::kQuickSettings);
  EXPECT_TRUE(controller->IsActive());
  EXPECT_EQ(CaptureModeType::kImage, controller->type());
  EXPECT_TRUE(GetImageToggleButton()->GetToggled());
  EXPECT_FALSE(GetVideoToggleButton()->GetToggled());
  EXPECT_FALSE(GetVideoToggleButton()->GetEnabled());

  // Clicking on the video button should do nothing.
  ClickOnView(GetVideoToggleButton(), GetEventGenerator());
  EXPECT_TRUE(GetImageToggleButton()->GetToggled());
  EXPECT_FALSE(GetVideoToggleButton()->GetToggled());
  EXPECT_EQ(CaptureModeType::kImage, controller->type());

  // Things should go back to normal when there's no recording going on.
  controller->Stop();
  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  EXPECT_EQ(CaptureModeType::kVideo, controller->type());
  EXPECT_FALSE(GetImageToggleButton()->GetToggled());
  EXPECT_TRUE(GetVideoToggleButton()->GetToggled());
  EXPECT_TRUE(GetVideoToggleButton()->GetEnabled());
}

// Tests the basic settings menu functionality.
TEST_F(CaptureModeTest, SettingsMenuVisibilityBasic) {
  auto* event_generator = GetEventGenerator();
  auto* controller = StartImageRegionCapture();
  EXPECT_TRUE(controller->IsActive());

  // Session starts with settings menu not initialized.
  EXPECT_FALSE(GetCaptureModeSettingsWidget());

  // Test clicking the settings button toggles the button as well as
  // opens/closes the settings menu.
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_TRUE(GetCaptureModeSettingsWidget());
  EXPECT_TRUE(GetSettingsButton()->GetToggled());
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_FALSE(GetCaptureModeSettingsWidget());
  EXPECT_FALSE(GetSettingsButton()->GetToggled());
}

// Tests how interacting with the rest of the screen (i.e. clicking outside of
// the bar/menu, on other buttons) affects whether the settings menu should
// close or not.
TEST_F(CaptureModeTest, SettingsMenuVisibilityClicking) {
  // This test is specific to the old settings view.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kImprovedScreenCaptureSettings);

  UpdateDisplay("800x700");

  auto* event_generator = GetEventGenerator();
  auto* controller = StartImageRegionCapture();
  EXPECT_TRUE(controller->IsActive());

  // Test clicking on the settings menu and toggling settings doesn't close the
  // settings menu.
  ClickOnView(GetSettingsButton(), event_generator);
  ClickOnView(GetCaptureModeSettingsView(), event_generator);
  EXPECT_TRUE(GetCaptureModeSettingsWidget());
  EXPECT_TRUE(GetSettingsButton()->GetToggled());
  ClickOnView(GetMicrophoneToggle(), event_generator);
  EXPECT_TRUE(GetCaptureModeSettingsWidget());
  EXPECT_TRUE(GetSettingsButton()->GetToggled());

  // Test clicking on the capture bar closes the settings menu.
  event_generator->MoveMouseTo(
      GetCaptureModeBarView()->GetBoundsInScreen().top_center() +
      gfx::Vector2d(0, 2));
  event_generator->ClickLeftButton();
  EXPECT_FALSE(GetCaptureModeSettingsWidget());
  EXPECT_FALSE(GetSettingsButton()->GetToggled());

  // Test clicking on a different source closes the settings menu.
  ClickOnView(GetSettingsButton(), event_generator);
  ClickOnView(GetFullscreenToggleButton(), event_generator);
  EXPECT_FALSE(GetCaptureModeSettingsWidget());

  // Test clicking on a different type closes the settings menu.
  ClickOnView(GetSettingsButton(), event_generator);
  ClickOnView(GetVideoToggleButton(), event_generator);
  EXPECT_FALSE(GetCaptureModeSettingsWidget());

  // Exit the capture session with the settings menu open, and test to make sure
  // the new session starts with the settings menu hidden.
  ClickOnView(GetSettingsButton(), event_generator);
  SendKey(ui::VKEY_ESCAPE, event_generator);
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kImage);
  EXPECT_FALSE(GetCaptureModeSettingsWidget());

  // Take a screenshot with the settings menu open, and test to make sure the
  // new session starts with the settings menu hidden.
  ClickOnView(GetSettingsButton(), event_generator);
  // Take screenshot.
  SendKey(ui::VKEY_RETURN, event_generator);
  StartImageRegionCapture();
  EXPECT_FALSE(GetCaptureModeSettingsWidget());
}

// Tests the settings menu functionality when in region mode.
TEST_F(CaptureModeTest, SettingsMenuVisibilityDrawingRegion) {
  // This test is specific to the old settings view.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kImprovedScreenCaptureSettings);

  UpdateDisplay("800x700");

  auto* event_generator = GetEventGenerator();
  auto* controller = StartImageRegionCapture();
  EXPECT_TRUE(controller->IsActive());

  // Test the settings menu is hidden when the user clicks to start selecting a
  // region.
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_TRUE(GetCaptureModeSettingsWidget());
  const gfx::Rect target_region(gfx::BoundingRect(
      gfx::Point(0, 0),
      GetCaptureModeBarView()->GetBoundsInScreen().top_right() +
          gfx::Vector2d(0, -50)));
  event_generator->MoveMouseTo(target_region.origin());
  event_generator->PressLeftButton();
  EXPECT_FALSE(GetCaptureModeSettingsWidget());
  event_generator->MoveMouseTo(target_region.bottom_right());
  event_generator->ReleaseLeftButton();
  EXPECT_FALSE(GetCaptureModeSettingsWidget());

  // Test that the settings menu is hidden when we drag a region. This drags a
  // region that overlapps the capture bar for later steps of testing.
  ClickOnView(GetSettingsButton(), event_generator);
  event_generator->MoveMouseTo(target_region.origin() + gfx::Vector2d(50, 50));
  event_generator->PressLeftButton();
  EXPECT_FALSE(GetCaptureModeSettingsWidget());
  event_generator->MoveMouseTo(target_region.bottom_center());
  event_generator->ReleaseLeftButton();

  // With an overlapping region (as dragged to above), the capture bar opacity
  // is changed based on hover. If the settings menu is open/visible, we close
  // it when we hide the capture bar. Capture bar starts off opaque.
  ui::Layer* capture_bar_layer = GetCaptureModeBarWidget()->GetLayer();
  event_generator->MoveMouseTo(target_region.origin());
  EXPECT_EQ(0.1f, capture_bar_layer->GetTargetOpacity());
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_EQ(1.f, capture_bar_layer->GetTargetOpacity());
  // Move mouse onto the settings menu, confirm the capture bar is still
  // visible.
  event_generator->MoveMouseTo(
      GetCaptureModeSettingsView()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(1.f, capture_bar_layer->GetTargetOpacity());
  // Move the mouse off both the capture bar and the settings menu, and confirm
  // that both bars are no longer visible.
  event_generator->MoveMouseTo(
      GetCaptureModeSettingsView()->GetBoundsInScreen().top_center() +
      gfx::Vector2d(0, -50));
  EXPECT_EQ(0.1f, capture_bar_layer->GetTargetOpacity());
  EXPECT_FALSE(GetCaptureModeSettingsWidget());
}

// Tests that toggling the microphone setting updates the state in the
// controller, and persists between sessions.
TEST_F(CaptureModeTest, AudioRecordingSetting) {
  // This test is specific to the old settings view.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kImprovedScreenCaptureSettings);

  auto* controller = StartImageRegionCapture();
  auto* event_generator = GetEventGenerator();

  // Test that the audio recording preference is defaulted to false, so the
  // toggle should start in the off position.
  EXPECT_FALSE(controller->enable_audio_recording());

  // Test that toggling on the micophone updates the preference in the
  // controller, as well as displaying the toggle as on.
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_FALSE(GetMicrophoneToggle()->GetIsOn());
  ClickOnView(GetMicrophoneToggle(), event_generator);
  EXPECT_TRUE(controller->enable_audio_recording());
  EXPECT_TRUE(GetMicrophoneToggle()->GetIsOn());

  // Test that the user selected audio preference for audio recording is
  // remembered between sessions.
  SendKey(ui::VKEY_ESCAPE, event_generator);
  EXPECT_TRUE(controller->enable_audio_recording());
  StartImageRegionCapture();
  EXPECT_TRUE(controller->enable_audio_recording());
}

TEST_F(CaptureModeTest, CaptureFolderSetting) {
  auto* controller = CaptureModeController::Get();
  auto* test_delegate = controller->delegate_for_testing();
  const auto default_downloads_folder =
      test_delegate->GetUserDefaultDownloadsFolder();

  auto capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_EQ(capture_folder.path, default_downloads_folder);
  EXPECT_TRUE(capture_folder.is_default_downloads_folder);

  const base::FilePath custom_folder(FILE_PATH_LITERAL("/home/tests"));
  controller->SetCustomCaptureFolder(custom_folder);

  capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_EQ(capture_folder.path, custom_folder);
  EXPECT_FALSE(capture_folder.is_default_downloads_folder);
}

TEST_F(CaptureModeTest, CaptureFolderSetToDefaultDownloads) {
  auto* controller = CaptureModeController::Get();
  auto* test_delegate = controller->delegate_for_testing();

  const base::FilePath custom_folder(FILE_PATH_LITERAL("/home/tests"));
  controller->SetCustomCaptureFolder(custom_folder);
  auto capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_FALSE(capture_folder.is_default_downloads_folder);

  // If the user selects the default downloads folder manually, we should be
  // able to detect that.
  const auto default_downloads_folder =
      test_delegate->GetUserDefaultDownloadsFolder();
  controller->SetCustomCaptureFolder(default_downloads_folder);

  capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_EQ(capture_folder.path, default_downloads_folder);
  EXPECT_TRUE(capture_folder.is_default_downloads_folder);
}

TEST_F(CaptureModeTest, UsesDefaultFolderWithCustomFolderSet) {
  auto* controller = CaptureModeController::Get();
  auto* test_delegate = controller->delegate_for_testing();

  const base::FilePath custom_folder(FILE_PATH_LITERAL("/home/tests"));
  controller->SetCustomCaptureFolder(custom_folder);
  auto capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_FALSE(capture_folder.is_default_downloads_folder);

  // If the user selects to force use the default downloads folder even while
  // a custom folder is set, we should respect that, but we shouldn't clear the
  // custom folder.
  controller->SetUsesDefaultCaptureFolder(true);
  const auto default_downloads_folder =
      test_delegate->GetUserDefaultDownloadsFolder();
  capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_EQ(capture_folder.path, default_downloads_folder);
  EXPECT_TRUE(capture_folder.is_default_downloads_folder);

  // Setting another custom folder value, would reset the
  // "UsesDefaultCaptureFolder" value, and the new custom folder will be used.
  const base::FilePath custom_folder2(FILE_PATH_LITERAL("/home/tests2"));
  controller->SetCustomCaptureFolder(custom_folder2);
  capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_EQ(capture_folder.path, custom_folder2);
  EXPECT_FALSE(capture_folder.is_default_downloads_folder);
}

TEST_F(CaptureModeTest, CaptureFolderSetToEmptyPath) {
  auto* controller = CaptureModeController::Get();
  auto* test_delegate = controller->delegate_for_testing();

  const base::FilePath custom_folder(FILE_PATH_LITERAL("/home/tests"));
  controller->SetCustomCaptureFolder(custom_folder);
  auto capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_FALSE(capture_folder.is_default_downloads_folder);

  // If we set the custom path to an empty folder to clear, we should revert
  // back to the default downloads folder.
  controller->SetCustomCaptureFolder(base::FilePath());

  const auto default_downloads_folder =
      test_delegate->GetUserDefaultDownloadsFolder();
  capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_EQ(capture_folder.path, default_downloads_folder);
  EXPECT_TRUE(capture_folder.is_default_downloads_folder);
}

TEST_F(CaptureModeTest, SimulateUserCancelingDlpWarningDialog) {
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());

  // Simulate the user canceling the DLP warning dialog at the end of the
  // recording which is shown to the user to alert about restricted content
  // showing up on the screen during the recording. In this case, the user
  // requests the deletion of the file.
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  test_delegate->set_should_save_after_dlp_check(false);
  base::RunLoop loop;
  SetUpFileDeletionVerifier(&loop);
  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  loop.Run();
  // No notification should show in this case, nor any thing on Tote.
  EXPECT_FALSE(GetPreviewNotification());
  ash::HoldingSpaceTestApi holding_space_api;
  EXPECT_TRUE(holding_space_api.GetScreenCaptureViews().empty());
}

namespace {

// -----------------------------------------------------------------------------
// TestVideoCaptureOverlay:

// Defines a fake video capture overlay to be used in testing the behavior of
// the cursor overlay. The VideoRecordingWatcher will control this overlay via
// mojo.
using Overlay = viz::mojom::FrameSinkVideoCaptureOverlay;
class TestVideoCaptureOverlay : public Overlay {
 public:
  explicit TestVideoCaptureOverlay(mojo::PendingReceiver<Overlay> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~TestVideoCaptureOverlay() override = default;

  const gfx::RectF& last_bounds() const { return last_bounds_; }

  bool IsHidden() const { return last_bounds_ == gfx::RectF(); }

  // viz::mojom::FrameSinkVideoCaptureOverlay:
  void SetImageAndBounds(const SkBitmap& image,
                         const gfx::RectF& bounds) override {
    last_bounds_ = bounds;
  }
  void SetBounds(const gfx::RectF& bounds) override { last_bounds_ = bounds; }

 private:
  mojo::Receiver<viz::mojom::FrameSinkVideoCaptureOverlay> receiver_;
  gfx::RectF last_bounds_;
};

// -----------------------------------------------------------------------------
//  CaptureModeCursorOverlayTest:

// Defines a test fixure to test the behavior of the cursor overlay.
class CaptureModeCursorOverlayTest : public CaptureModeTest {
 public:
  CaptureModeCursorOverlayTest() = default;
  ~CaptureModeCursorOverlayTest() override = default;

  aura::Window* window() const { return window_.get(); }
  TestVideoCaptureOverlay* fake_overlay() const { return fake_overlay_.get(); }

  // CaptureModeTest:
  void SetUp() override {
    CaptureModeTest::SetUp();
    window_ = CreateTestWindow(gfx::Rect(200, 200));
  }

  void TearDown() override {
    window_.reset();
    CaptureModeTest::TearDown();
  }

  CaptureModeController* StartRecordingAndSetupFakeOverlay(
      CaptureModeSource source) {
    auto* controller = StartCaptureSession(source, CaptureModeType::kVideo);
    auto* event_generator = GetEventGenerator();
    if (source == CaptureModeSource::kWindow)
      event_generator->MoveMouseToCenterOf(window_.get());
    StartVideoRecordingImmediately();
    EXPECT_TRUE(controller->is_recording_in_progress());
    auto* recording_watcher = controller->video_recording_watcher_for_testing();
    mojo::PendingRemote<Overlay> overlay_pending_remote;
    fake_overlay_ = std::make_unique<TestVideoCaptureOverlay>(
        overlay_pending_remote.InitWithNewPipeAndPassReceiver());
    recording_watcher->BindCursorOverlayForTesting(
        std::move(overlay_pending_remote));

    // The overlay should be initially hidden until a mourse event is received.
    FlushOverlay();
    EXPECT_TRUE(fake_overlay()->IsHidden());

    // Generating some mouse events may or may not show the overlay, depending
    // on the conditions of the test. Each test will verify its expectation
    // after this returns.
    event_generator->MoveMouseBy(10, 10);
    FlushOverlay();

    return controller;
  }

  void FlushOverlay() {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->is_recording_in_progress());
    controller->video_recording_watcher_for_testing()
        ->FlushCursorOverlayForTesting();
  }

  // The docked magnifier is one of the features that force the software-
  // composited cursor to be used when enabled. We use it to test the behavior
  // of the cursor overlay in that case.
  void SetDockedMagnifierEnabled(bool enabled) {
    Shell::Get()->docked_magnifier_controller()->SetEnabled(enabled);
  }

  // Checks that capturing a screenshot hides the cursor. After the capture is
  // complete, checks that the cursor returns to the previous state, i.e.
  // hidden for tablet mode but visible for clamshell mode.
  void CaptureScreenshotAndCheckCursorVisibility(
      CaptureModeController* controller) {
    EXPECT_EQ(controller->type(), CaptureModeType::kImage);

    auto* shell = Shell::Get();
    auto* cursor_manager = shell->cursor_manager();
    bool in_tablet_mode = shell->tablet_mode_controller()->InTabletMode();

    // The capture mode session locks the cursor for the whole active session
    // except in the tablet mode unless the cursor is visible.
    EXPECT_EQ(!in_tablet_mode, cursor_manager->IsCursorLocked());
    EXPECT_EQ(!in_tablet_mode, cursor_manager->IsCursorVisible());
    EXPECT_TRUE(controller->IsActive());

    // Make sure the cursor is hidden while capturing the screenshot.
    CaptureNotificationWaiter waiter;
    controller->PerformCapture();
    EXPECT_FALSE(cursor_manager->IsCursorVisible());
    EXPECT_FALSE(controller->IsActive());

    // The cursor visibility should be restored after the capture is done.
    waiter.Wait();
    EXPECT_EQ(!in_tablet_mode, cursor_manager->IsCursorVisible());
    EXPECT_FALSE(cursor_manager->IsCursorLocked());
  }

 private:
  std::unique_ptr<aura::Window> window_;
  std::unique_ptr<TestVideoCaptureOverlay> fake_overlay_;
};

}  // namespace

TEST_F(CaptureModeCursorOverlayTest, TabletModeHidesCursorOverlay) {
  StartRecordingAndSetupFakeOverlay(CaptureModeSource::kFullscreen);
  EXPECT_FALSE(fake_overlay()->IsHidden());

  // Entering tablet mode should hide the cursor overlay.
  SwitchToTabletMode();
  FlushOverlay();
  EXPECT_TRUE(fake_overlay()->IsHidden());

  // Exiting tablet mode should reshow the overlay.
  LeaveTabletMode();
  FlushOverlay();
  EXPECT_FALSE(fake_overlay()->IsHidden());
}

// Tests that the cursor is hidden while taking a screenshot in tablet mode and
// remains hidden afterward.
TEST_F(CaptureModeCursorOverlayTest, TabletModeHidesCursor) {
  // Enter tablet mode.
  SwitchToTabletMode();

  auto* cursor_manager = Shell::Get()->cursor_manager();
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kImage);

  // Test the hardware cursor.
  CaptureScreenshotAndCheckCursorVisibility(controller);

  // Test the software cursor enabled by docked magnifier.
  SetDockedMagnifierEnabled(true);
  EXPECT_TRUE(IsCursorCompositingEnabled());
  controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                   CaptureModeType::kImage);
  CaptureScreenshotAndCheckCursorVisibility(controller);

  // Exiting tablet mode.
  LeaveTabletMode();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
}

// Tests that a cursor is hidden while taking a fullscreen screenshot
// (crbug.com/1186652).
TEST_F(CaptureModeCursorOverlayTest, CursorInFullscreenScreenshot) {
  auto* cursor_manager = Shell::Get()->cursor_manager();
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kImage);
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::Point(175, 175));

  // Test the hardware cursor.
  CaptureScreenshotAndCheckCursorVisibility(controller);

  // Test the software cursor enabled by docked magnifier.
  SetDockedMagnifierEnabled(true);
  EXPECT_TRUE(IsCursorCompositingEnabled());
  controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                   CaptureModeType::kImage);
  CaptureScreenshotAndCheckCursorVisibility(controller);
}

// Tests that a cursor is hidden while taking a region screenshot
// (crbug.com/1186652).
TEST_F(CaptureModeCursorOverlayTest, CursorInPartialRegionScreenshot) {
  // Use a set display size as we will be choosing points in this test.
  UpdateDisplay("800x700");

  auto* cursor_manager = Shell::Get()->cursor_manager();
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  auto* event_generator = GetEventGenerator();
  auto* controller = StartImageRegionCapture();

  // Create the initial capture region.
  const gfx::Rect target_region(gfx::Rect(50, 50, 200, 200));
  SelectRegion(target_region);
  event_generator->MoveMouseTo(gfx::Point(175, 175));

  // Test the hardware cursor.
  CaptureScreenshotAndCheckCursorVisibility(controller);

  // Test the software cursor enabled by docked magnifier.
  SetDockedMagnifierEnabled(true);
  EXPECT_TRUE(IsCursorCompositingEnabled());
  controller = StartImageRegionCapture();
  CaptureScreenshotAndCheckCursorVisibility(controller);
}

TEST_F(CaptureModeCursorOverlayTest, SoftwareCursorInitiallyEnabled) {
  // The software cursor is enabled before recording starts.
  SetDockedMagnifierEnabled(true);
  EXPECT_TRUE(IsCursorCompositingEnabled());

  // Hence the overlay will be hidden initially.
  StartRecordingAndSetupFakeOverlay(CaptureModeSource::kFullscreen);
  EXPECT_TRUE(fake_overlay()->IsHidden());
}

TEST_F(CaptureModeCursorOverlayTest, SoftwareCursorInFullscreenRecording) {
  StartRecordingAndSetupFakeOverlay(CaptureModeSource::kFullscreen);
  EXPECT_FALSE(fake_overlay()->IsHidden());

  // When the software-composited cursor is enabled, the overlay is hidden to
  // avoid having two overlapping cursors in the video.
  SetDockedMagnifierEnabled(true);
  EXPECT_TRUE(IsCursorCompositingEnabled());
  FlushOverlay();
  EXPECT_TRUE(fake_overlay()->IsHidden());

  SetDockedMagnifierEnabled(false);
  EXPECT_FALSE(IsCursorCompositingEnabled());
  FlushOverlay();
  EXPECT_FALSE(fake_overlay()->IsHidden());
}

TEST_F(CaptureModeCursorOverlayTest, SoftwareCursorInPartialRegionRecording) {
  CaptureModeController::Get()->SetUserCaptureRegion(gfx::Rect(20, 20),
                                                     /*by_user=*/true);
  StartRecordingAndSetupFakeOverlay(CaptureModeSource::kRegion);
  EXPECT_FALSE(fake_overlay()->IsHidden());

  // The behavior in this case is exactly the same as in fullscreen recording.
  SetDockedMagnifierEnabled(true);
  EXPECT_TRUE(IsCursorCompositingEnabled());
  FlushOverlay();
  EXPECT_TRUE(fake_overlay()->IsHidden());
}

TEST_F(CaptureModeCursorOverlayTest, SoftwareCursorInWindowRecording) {
  StartRecordingAndSetupFakeOverlay(CaptureModeSource::kWindow);
  EXPECT_FALSE(fake_overlay()->IsHidden());

  // When recording a window, the software cursor has no effect of the cursor
  // overlay, since the cursor widget is not in the recorded window subtree, so
  // it cannot be captured by the frame sink capturer. We have to provide cursor
  // capturing through the overlay.
  SetDockedMagnifierEnabled(true);
  EXPECT_TRUE(IsCursorCompositingEnabled());
  FlushOverlay();
  EXPECT_FALSE(fake_overlay()->IsHidden());
}

TEST_F(CaptureModeCursorOverlayTest, OverlayHidesWhenOutOfBounds) {
  StartRecordingAndSetupFakeOverlay(CaptureModeSource::kWindow);
  EXPECT_FALSE(fake_overlay()->IsHidden());

  const gfx::Point bottom_right =
      window()->GetBoundsInRootWindow().bottom_right();
  auto* generator = GetEventGenerator();
  // Generate a click event to overcome throttling.
  generator->MoveMouseTo(bottom_right);
  generator->ClickLeftButton();
  FlushOverlay();
  EXPECT_TRUE(fake_overlay()->IsHidden());
}

// Verifies that the cursor overlay bounds calculation takes into account the
// cursor image scale factor. https://crbug.com/1222494.
TEST_F(CaptureModeCursorOverlayTest, OverlayBoundsAccountForCursorScaleFactor) {
  UpdateDisplay("500x400");
  StartRecordingAndSetupFakeOverlay(CaptureModeSource::kFullscreen);
  EXPECT_FALSE(fake_overlay()->IsHidden());

  auto* cursor_manager = Shell::Get()->cursor_manager();
  auto set_cursor = [cursor_manager](const gfx::Size& cursor_image_size,
                                     float cursor_image_scale_factor) {
    const auto cursor_type = ui::mojom::CursorType::kCustom;
    gfx::NativeCursor cursor{cursor_type};
    SkBitmap cursor_image;
    cursor_image.allocN32Pixels(cursor_image_size.width(),
                                cursor_image_size.height());
    cursor.set_image_scale_factor(cursor_image_scale_factor);
    cursor.set_custom_bitmap(cursor_image);
    auto* platform_cursor_factory =
        ui::OzonePlatform::GetInstance()->GetCursorFactory();
    cursor.SetPlatformCursor(platform_cursor_factory->CreateImageCursor(
        cursor_type, cursor_image, cursor.custom_hotspot()));
    cursor_manager->SetCursor(cursor);
  };

  struct {
    gfx::Size cursor_size;
    float cursor_image_scale_factor;
  } kTestCases[] = {
      {
          gfx::Size(50, 50),
          /*cursor_image_scale_factor=*/2.f,
      },
      {
          gfx::Size(25, 25),
          /*cursor_image_scale_factor=*/1.f,
      },
  };

  // Both of the above test cases should yield the same cursor overlay relative
  // bounds when the cursor is at the center of the screen.
  // Origin is 0.5f (center)
  // Size is 25 (cursor image dip size) / {500,400} = {0.05f, 0.0625f}
  const gfx::RectF expected_overlay_bounds{0.5f, 0.5f, 0.05f, 0.0625f};

  const gfx::Point screen_center =
      window()->GetRootWindow()->bounds().CenterPoint();
  auto* generator = GetEventGenerator();

  for (const auto& test_case : kTestCases) {
    set_cursor(test_case.cursor_size, test_case.cursor_image_scale_factor);
    // Lock the cursor to prevent mouse events from changing it back to a
    // default kPointer cursor type.
    cursor_manager->LockCursor();

    // Generate a click event to overcome throttling.
    generator->MoveMouseTo(screen_center);
    generator->ClickLeftButton();
    FlushOverlay();
    EXPECT_FALSE(fake_overlay()->IsHidden());
    EXPECT_EQ(expected_overlay_bounds, fake_overlay()->last_bounds());

    // Unlock the cursor back.
    cursor_manager->UnlockCursor();
  }
}

// -----------------------------------------------------------------------------
// TODO(afakhry): Add more cursor overlay tests.

// Test fixture to verify capture mode + projector integration.
class ProjectorCaptureModeIntegrationTests
    : public CaptureModeTest,
      public ::testing::WithParamInterface<CaptureModeSource> {
 public:
  ProjectorCaptureModeIntegrationTests() = default;
  ~ProjectorCaptureModeIntegrationTests() override = default;

  static constexpr gfx::Rect kUserRegion{20, 50, 60, 70};

  // CaptureModeTest:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kProjector,
                              features::kProjectorAnnotator},
        /*disabled_features=*/{});
    CaptureModeTest::SetUp();
    auto* projector_controller = ProjectorController::Get();
    projector_controller->SetClient(&projector_client_);
    // Simulate the availability of speech recognition.
    projector_controller->OnSpeechRecognitionAvailabilityChanged(
        SpeechRecognitionAvailability::kAvailable);
    window_ = CreateTestWindow(gfx::Rect(20, 30, 200, 200));
    CaptureModeController::Get()->SetUserCaptureRegion(kUserRegion,
                                                       /*by_user=*/true);
    EXPECT_CALL(projector_client_, IsDriveFsMounted())
        .WillRepeatedly(testing::Return(true));
  }

  void TearDown() override {
    window_.reset();
    CaptureModeTest::TearDown();
  }

  void StartProjectorModeSession() {
    auto* projector_session = ProjectorSession::Get();
    EXPECT_FALSE(projector_session->is_active());
    auto* projector_controller = ProjectorController::Get();
    EXPECT_CALL(projector_client_, MinimizeProjectorApp());
    projector_controller->StartProjectorSession("projector_data");
    EXPECT_TRUE(projector_session->is_active());
  }

  void StartRecordingForProjectorFromSource(CaptureModeSource source) {
    auto* controller = CaptureModeController::Get();
    controller->SetSource(source);
    StartProjectorModeSession();

    switch (source) {
      case CaptureModeSource::kFullscreen:
      case CaptureModeSource::kRegion:
        break;
      case CaptureModeSource::kWindow:
        auto* generator = GetEventGenerator();
        generator->MoveMouseTo(window_->GetBoundsInScreen().CenterPoint());
        break;
    }
    CaptureModeTestApi().PerformCapture();
    WaitForRecordingToStart();
    EXPECT_TRUE(controller->is_recording_in_progress());
  }

  void VerifyOverlayEnabledState(aura::Window* overlay_window,
                                 bool overlay_enabled_state) {
    if (overlay_enabled_state) {
      EXPECT_TRUE(overlay_window->IsVisible());
      EXPECT_EQ(overlay_window->event_targeting_policy(),
                aura::EventTargetingPolicy::kTargetAndDescendants);
    } else {
      EXPECT_FALSE(overlay_window->IsVisible());
      EXPECT_EQ(overlay_window->event_targeting_policy(),
                aura::EventTargetingPolicy::kNone);
    }
  }

  void VerifyOverlayStacking(aura::Window* overlay_window,
                             aura::Window* window_being_recorded,
                             CaptureModeSource source) {
    auto* parent = overlay_window->parent();

    if (source == CaptureModeSource::kWindow) {
      // The overlay window should always be the top-most child of the window
      // being recorded when in window mode.
      ASSERT_EQ(parent, window_being_recorded);
      EXPECT_EQ(window_being_recorded->children().back(), overlay_window);
    } else {
      auto* menu_container = overlay_window->GetRootWindow()->GetChildById(
          kShellWindowId_MenuContainer);
      ASSERT_EQ(parent, menu_container);
      EXPECT_EQ(menu_container->children().front(), overlay_window);
    }
  }

  void VerifyOverlayWindow(aura::Window* overlay_window,
                           CaptureModeSource source) {
    auto* controller = CaptureModeController::Get();
    auto* recording_watcher = controller->video_recording_watcher_for_testing();
    auto* window_being_recorded = recording_watcher->window_being_recorded();

    VerifyOverlayStacking(overlay_window, window_being_recorded, source);

    switch (source) {
      case CaptureModeSource::kFullscreen:
      case CaptureModeSource::kWindow:
        EXPECT_EQ(overlay_window->bounds(),
                  gfx::Rect(window_being_recorded->bounds().size()));
        break;

      case CaptureModeSource::kRegion:
        EXPECT_EQ(overlay_window->bounds(), kUserRegion);
        break;
    }
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  MockProjectorClient projector_client_;
  std::unique_ptr<aura::Window> window_;
};

// static
constexpr gfx::Rect ProjectorCaptureModeIntegrationTests::kUserRegion;

TEST_F(ProjectorCaptureModeIntegrationTests, EntryPoint) {
  // With the most recent source type set to kImage, starting capture mode for
  // the projector workflow will still force it to kVideo.
  auto* controller = CaptureModeController::Get();
  controller->SetType(CaptureModeType::kImage);
  // Also, audio recording is initially disabled. However, the projector flow
  // forces it enabled.
  EXPECT_FALSE(controller->enable_audio_recording());

  base::HistogramTester histogram_tester;
  StartProjectorModeSession();
  EXPECT_TRUE(controller->IsActive());
  auto* session = controller->capture_mode_session();
  EXPECT_TRUE(session->is_in_projector_mode());
  EXPECT_TRUE(controller->enable_audio_recording());

  constexpr char kEntryPointHistogram[] =
      "Ash.CaptureModeController.EntryPoint.ClamshellMode";
  histogram_tester.ExpectBucketCount(kEntryPointHistogram,
                                     CaptureModeEntryType::kProjector, 1);
}

// Tests that when the advanced capture mode settings are enabled, a simplified
// view of the settings are shown.
TEST_F(ProjectorCaptureModeIntegrationTests, WithAdvancedSettings) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kImprovedScreenCaptureSettings};

  auto* controller = CaptureModeController::Get();
  StartProjectorModeSession();
  auto* event_generator = GetEventGenerator();

  ClickOnView(GetSettingsButton(), event_generator);

  CaptureModeAdvancedSettingsTestApi test_api;

  // The "Save-to" menu group should never be added.
  CaptureModeMenuGroup* save_to_menu_group = test_api.GetSaveToMenuGroup();
  EXPECT_FALSE(save_to_menu_group);

  // The audio-off option should be disabled, unchecked and not interactable
  // (even when clicked).
  views::View* audio_off_option = test_api.GetAudioOffOption();
  EXPECT_FALSE(audio_off_option->GetEnabled());
  CaptureModeMenuGroup* audio_input_menu_group =
      test_api.GetAudioInputMenuGroup();
  EXPECT_TRUE(audio_input_menu_group->IsOptionChecked(kAudioMicrophone));
  EXPECT_FALSE(audio_input_menu_group->IsOptionChecked(kAudioOff));
  ClickOnView(audio_off_option, event_generator);
  EXPECT_TRUE(audio_input_menu_group->IsOptionChecked(kAudioMicrophone));
  EXPECT_FALSE(audio_input_menu_group->IsOptionChecked(kAudioOff));
  EXPECT_TRUE(controller->enable_audio_recording());
}

// Tests the keyboard navigation for projector mode with advanced capture mode
// settings enabled. The `image_toggle_button_` in `CaptureModeTypeView` and the
// `Off` audio input option in `CaptureModeAdvancedSettingsView` are disabled in
// projector mode, thus they should be skipped while tabbing though.
TEST_F(ProjectorCaptureModeIntegrationTests,
       KeyboardNavigationWithAdvancedSettings) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kImprovedScreenCaptureSettings};
  ASSERT_TRUE(features::AreImprovedScreenCaptureSettingsEnabled());
  auto* controller = CaptureModeController::Get();
  // Use `kFullscreen` here to minimize the number of tabs to reach the setting
  // button.
  controller->SetSource(CaptureModeSource::kFullscreen);
  StartProjectorModeSession();

  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());

  EXPECT_FALSE(GetImageToggleButton()->GetEnabled());
  // Tab once, check the image toggle button is skipped and the current focused
  // view is the video toggle button.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(test_api.GetCurrentFocusedView()->GetView(),
            GetVideoToggleButton());

  // Now tab four times to focus the settings button and enter space to open the
  // setting menu.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, /*count=*/4);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(FocusGroup::kPendingSettings, test_api.GetCurrentFocusGroup());
  CaptureModeAdvancedSettingsView* settings_menu =
      test_api.capture_mode_advanced_settings_view();
  ASSERT_TRUE(settings_menu);

  CaptureModeAdvancedSettingsTestApi advanced_settings_test_api;
  // Tab twice, check the `Off` option is skipped and remains disabled. The
  // current focused view is the `Microphone` option.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, /*count=*/2);
  EXPECT_FALSE(advanced_settings_test_api.GetAudioOffOption()->GetEnabled());
  EXPECT_EQ(test_api.GetCurrentFocusedView()->GetView(),
            advanced_settings_test_api.GetMicrophoneOption());
}

TEST_F(ProjectorCaptureModeIntegrationTests, BarButtonsState) {
  auto* controller = CaptureModeController::Get();
  StartProjectorModeSession();
  EXPECT_TRUE(controller->IsActive());

  // The image toggle button should be disabled, whereas the video toggle button
  // should be enabled and active.
  EXPECT_FALSE(GetImageToggleButton()->GetEnabled());
  EXPECT_FALSE(GetImageToggleButton()->GetToggled());
  EXPECT_TRUE(GetVideoToggleButton()->GetEnabled());
  EXPECT_TRUE(GetVideoToggleButton()->GetToggled());
}

TEST_F(ProjectorCaptureModeIntegrationTests, StartEndRecording) {
  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kFullscreen);
  StartProjectorModeSession();
  EXPECT_TRUE(controller->IsActive());

  // Hit Enter to begin recording. The recording session should be marked for
  // projector.
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_CALL(projector_client_, StartSpeechRecognition());
  WaitForRecordingToStart();

  EXPECT_FALSE(controller->IsActive());
  EXPECT_TRUE(controller->is_recording_in_progress());
  EXPECT_TRUE(controller->video_recording_watcher_for_testing()
                  ->is_in_projector_mode());

  EXPECT_CALL(projector_client_, StopSpeechRecognition());
  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
}

TEST_F(ProjectorCaptureModeIntegrationTests,
       ProjectorSessionNeverStartsWhenCaptureModeIsBlocked) {
  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kFullscreen);

  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  test_delegate->set_is_allowed_by_policy(false);
  ProjectorController::Get()->StartProjectorSession("projector_data");

  // Both sessions will never start.
  EXPECT_FALSE(controller->IsActive());
  EXPECT_FALSE(ProjectorSession::Get()->is_active());
  EXPECT_FALSE(controller->is_recording_in_progress());
}

namespace {

enum AbortReason {
  kBlockedByDlp,
  kBlockedByPolicy,
  kUserPressedEsc,
};

struct {
  const std::string scope_trace;
  const AbortReason reason;
} kTestCases[] = {
    {"Blocked by DLP", kBlockedByDlp},
    {"Blocked by policy", kBlockedByPolicy},
    {"User Pressed Esc", kUserPressedEsc},
};

}  // namespace

TEST_F(ProjectorCaptureModeIntegrationTests,
       ProjectorSessionAbortedBeforeCountDownStarts) {
  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kFullscreen);

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.scope_trace);
    StartProjectorModeSession();
    auto* test_delegate = static_cast<TestCaptureModeDelegate*>(
        controller->delegate_for_testing());

    switch (test_case.reason) {
      case kBlockedByDlp:
        test_delegate->set_is_allowed_by_dlp(false);
        PressAndReleaseKey(ui::VKEY_RETURN);
        break;
      case kBlockedByPolicy:
        test_delegate->set_is_allowed_by_policy(false);
        PressAndReleaseKey(ui::VKEY_RETURN);
        break;
      case kUserPressedEsc:
        PressAndReleaseKey(ui::VKEY_ESCAPE);
        break;
    }

    // The session will end immediately without a count down.
    EXPECT_FALSE(controller->IsActive());
    EXPECT_FALSE(ProjectorSession::Get()->is_active());
    EXPECT_FALSE(controller->is_recording_in_progress());

    // Prepare for next iteration by resetting things back to default.
    test_delegate->ResetAllowancesToDefault();
  }
}

TEST_F(ProjectorCaptureModeIntegrationTests,
       ProjectorSessionAbortedAfterCountDownStarts) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::FAST_DURATION);
  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kFullscreen);

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.scope_trace);
    StartProjectorModeSession();
    PressAndReleaseKey(ui::VKEY_RETURN);
    auto* test_delegate = static_cast<TestCaptureModeDelegate*>(
        controller->delegate_for_testing());

    switch (test_case.reason) {
      case kBlockedByDlp:
        test_delegate->set_is_allowed_by_dlp(false);
        break;
      case kBlockedByPolicy:
        test_delegate->set_is_allowed_by_policy(false);
        break;
      case kUserPressedEsc:
        PressAndReleaseKey(ui::VKEY_ESCAPE);
        break;
    }

    WaitForSessionToEnd();
    EXPECT_FALSE(ProjectorSession::Get()->is_active());
    EXPECT_FALSE(controller->is_recording_in_progress());

    // Prepare for next iteration by resetting things back to default.
    test_delegate->ResetAllowancesToDefault();
  }
}

TEST_F(ProjectorCaptureModeIntegrationTests, RecordingOverlayWidget) {
  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kFullscreen);
  StartProjectorModeSession();
  EXPECT_TRUE(controller->IsActive());

  PressAndReleaseKey(ui::VKEY_RETURN);
  WaitForRecordingToStart();
  CaptureModeTestApi test_api;
  RecordingOverlayController* overlay_controller =
      test_api.GetRecordingOverlayController();
  EXPECT_FALSE(overlay_controller->is_enabled());
  auto* overlay_window = overlay_controller->GetOverlayNativeWindow();
  VerifyOverlayEnabledState(overlay_window, /*overlay_enabled_state=*/false);

  auto* projector_controller = ProjectorControllerImpl::Get();
  projector_controller->OnMarkerPressed();
  EXPECT_TRUE(overlay_controller->is_enabled());
  VerifyOverlayEnabledState(overlay_window, /*overlay_enabled_state=*/true);

  projector_controller->OnMarkerPressed();
  EXPECT_FALSE(overlay_controller->is_enabled());
  VerifyOverlayEnabledState(overlay_window, /*overlay_enabled_state=*/false);
}

TEST_F(ProjectorCaptureModeIntegrationTests, RecordingOverlayDockedMagnifier) {
  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kFullscreen);
  StartProjectorModeSession();
  EXPECT_TRUE(controller->IsActive());

  PressAndReleaseKey(ui::VKEY_RETURN);
  WaitForRecordingToStart();
  CaptureModeTestApi test_api;
  RecordingOverlayController* overlay_controller =
      test_api.GetRecordingOverlayController();

  auto* projector_controller = ProjectorControllerImpl::Get();
  projector_controller->OnMarkerPressed();
  EXPECT_TRUE(overlay_controller->is_enabled());
  auto* overlay_window = overlay_controller->GetOverlayNativeWindow();

  // Before the docked magnifier gets enabled, the overlay's bounds should match
  // the root window's bounds.
  auto* root_window = overlay_window->GetRootWindow();
  const gfx::Rect root_window_bounds = root_window->bounds();
  EXPECT_EQ(root_window_bounds, overlay_window->GetBoundsInRootWindow());

  // Once the magnifier is enabled, the overlay should be pushed down so that
  // it doesn't cover the magnifier viewport.
  auto* docked_magnifier = Shell::Get()->docked_magnifier_controller();
  docked_magnifier->SetEnabled(true);
  const gfx::Rect expected_bounds = gfx::SubtractRects(
      root_window_bounds,
      docked_magnifier->GetTotalMagnifierBoundsForRoot(root_window));
  EXPECT_EQ(expected_bounds, overlay_window->GetBoundsInRootWindow());

  // It should go back to original bounds once the magnifier is disabled.
  docked_magnifier->SetEnabled(false);
  EXPECT_EQ(root_window_bounds, overlay_window->GetBoundsInRootWindow());
}

TEST_P(ProjectorCaptureModeIntegrationTests, RecordingOverlayWidgetBounds) {
  const auto capture_source = GetParam();
  StartRecordingForProjectorFromSource(capture_source);
  CaptureModeTestApi test_api;
  RecordingOverlayController* overlay_controller =
      test_api.GetRecordingOverlayController();
  EXPECT_FALSE(overlay_controller->is_enabled());
  auto* overlay_window = overlay_controller->GetOverlayNativeWindow();
  VerifyOverlayWindow(overlay_window, capture_source);
}

// Tests that neither preview notification nor recording in tote is shown if in
// projector mode.
TEST_P(ProjectorCaptureModeIntegrationTests,
       NotShowRecordingInToteOrNotificationForProjectorMode) {
  const auto capture_source = GetParam();
  StartRecordingForProjectorFromSource(capture_source);
  CaptureModeTestApi().StopVideoRecording();
  WaitForCaptureFileToBeSaved();
  EXPECT_FALSE(GetPreviewNotification());
  ash::HoldingSpaceTestApi holding_space_api;
  EXPECT_TRUE(holding_space_api.GetScreenCaptureViews().empty());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ProjectorCaptureModeIntegrationTests,
                         testing::Values(CaptureModeSource::kFullscreen,
                                         CaptureModeSource::kRegion,
                                         CaptureModeSource::kWindow));

// -----------------------------------------------------------------------------
// CaptureModeAdvancedSettingsTest:

// Test fixture for CaptureMode advanced settings view.
class CaptureModeAdvancedSettingsTest : public CaptureModeTest {
 public:
  CaptureModeAdvancedSettingsTest() = default;
  ~CaptureModeAdvancedSettingsTest() override = default;

  // CaptureModeTest:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kImprovedScreenCaptureSettings);
    CaptureModeTest::SetUp();
    FakeFolderSelectionDialogFactory::Start();
  }

  void TearDown() override {
    FakeFolderSelectionDialogFactory::Stop();
    CaptureModeTest::TearDown();
  }

  CaptureModeAdvancedSettingsView* GetCaptureModeAdvancedSettingsView() const {
    auto* session = CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    return CaptureModeSessionTestApi(session)
        .capture_mode_advanced_settings_view();
  }

  UserNudgeController* GetUserNudgeController() const {
    auto* session = CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    return CaptureModeSessionTestApi(session).user_nudge_controller();
  }

  void WaitForSettingsMenuToBeRefreshed() {
    base::RunLoop run_loop;
    CaptureModeAdvancedSettingsTestApi().SetOnSettingsMenuRefreshedCallback(
        run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

enum class NudgeDismissalCause {
  kPressSettingsButton,
  kCaptureViaEnterKey,
  kCaptureViaClickOnScreen,
  kCaptureViaLabelButton,
};

// Test fixture to test that various causes that lead to the dismissal of the
// user nudge, they dismiss it forever.
class CaptureModeNudgeDismissalTest
    : public CaptureModeAdvancedSettingsTest,
      public ::testing::WithParamInterface<NudgeDismissalCause> {
 public:
  // Starts a session appropriate for the test param.
  CaptureModeController* StartSession() {
    switch (GetParam()) {
      case NudgeDismissalCause::kPressSettingsButton:
      case NudgeDismissalCause::kCaptureViaEnterKey:
      case NudgeDismissalCause::kCaptureViaClickOnScreen:
        return StartCaptureSession(CaptureModeSource::kFullscreen,
                                   CaptureModeType::kImage);
      case NudgeDismissalCause::kCaptureViaLabelButton:
        auto* controller = CaptureModeController::Get();
        controller->SetUserCaptureRegion(gfx::Rect(200, 300), /*by_user=*/true);
        StartCaptureSession(CaptureModeSource::kRegion,
                            CaptureModeType::kImage);
        return controller;
    }
  }

  void DoDismissalAction() {
    auto* controller = CaptureModeController::Get();
    auto* event_generator = GetEventGenerator();
    switch (GetParam()) {
      case NudgeDismissalCause::kPressSettingsButton:
        ClickOnView(GetSettingsButton(), event_generator);
        break;
      case NudgeDismissalCause::kCaptureViaEnterKey:
        PressAndReleaseKey(ui::VKEY_RETURN);
        EXPECT_FALSE(controller->IsActive());
        break;
      case NudgeDismissalCause::kCaptureViaClickOnScreen:
        event_generator->MoveMouseToCenterOf(Shell::GetPrimaryRootWindow());
        event_generator->ClickLeftButton();
        EXPECT_FALSE(controller->IsActive());
        break;
      case NudgeDismissalCause::kCaptureViaLabelButton:
        auto* label_button_widget =
            CaptureModeSessionTestApi(controller->capture_mode_session())
                .capture_label_widget();
        EXPECT_TRUE(label_button_widget);
        ClickOnView(label_button_widget->GetContentsView(), event_generator);
        break;
    }
  }
};

TEST_P(CaptureModeNudgeDismissalTest, NudgeDismissedForever) {
  auto* controller = StartSession();
  auto* nudge_controller = GetUserNudgeController();
  ASSERT_TRUE(nudge_controller);
  EXPECT_TRUE(nudge_controller->is_visible());
  EXPECT_TRUE(nudge_controller->toast_widget());

  // Trigger the action that dismisses the nudge forever, it should be removed
  // in this session (if the action doesn't stop the session) and any future
  // sessions.
  DoDismissalAction();
  if (controller->IsActive()) {
    EXPECT_FALSE(GetUserNudgeController());
    // Close the session in preparation for opening a new one.
    controller->Stop();
  }

  // Reopen a new session, the nudge should not show anymore.
  StartImageRegionCapture();
  EXPECT_FALSE(GetUserNudgeController());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CaptureModeNudgeDismissalTest,
    testing::Values(NudgeDismissalCause::kPressSettingsButton,
                    NudgeDismissalCause::kCaptureViaEnterKey,
                    NudgeDismissalCause::kCaptureViaClickOnScreen,
                    NudgeDismissalCause::kCaptureViaLabelButton));

TEST_F(CaptureModeAdvancedSettingsTest, NudgeChangesRootWithBar) {
  UpdateDisplay("800x700,801+0-800x700");

  auto* event_generator = GetEventGenerator();
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(100, 500), event_generator);

  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kImage);
  auto* session = controller->capture_mode_session();
  EXPECT_EQ(Shell::GetAllRootWindows()[0], session->current_root());
  auto* nudge_controller = GetUserNudgeController();
  EXPECT_EQ(
      nudge_controller->toast_widget()->GetNativeWindow()->GetRootWindow(),
      session->current_root());

  MoveMouseToAndUpdateCursorDisplay(gfx::Point(1000, 500), event_generator);
  EXPECT_EQ(Shell::GetAllRootWindows()[1], session->current_root());
  EXPECT_EQ(
      nudge_controller->toast_widget()->GetNativeWindow()->GetRootWindow(),
      session->current_root());
}

TEST_F(CaptureModeAdvancedSettingsTest, NudgeBehaviorWhenSelectingRegion) {
  UpdateDisplay("800x700,801+0-800x700");

  auto* event_generator = GetEventGenerator();
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(100, 500), event_generator);

  auto* controller = StartImageRegionCapture();
  auto* session = controller->capture_mode_session();
  EXPECT_EQ(Shell::GetAllRootWindows()[0], session->current_root());

  // Nudge hides while selecting a region, but doesn't change roots until the
  // region change is committed.
  auto* nudge_controller = GetUserNudgeController();
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(1000, 500), event_generator);
  event_generator->PressLeftButton();
  EXPECT_FALSE(nudge_controller->is_visible());
  event_generator->MoveMouseBy(50, 60);
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(Shell::GetAllRootWindows()[1], session->current_root());

  // The nudge shows again, and is on the second display.
  EXPECT_TRUE(nudge_controller->is_visible());
  EXPECT_EQ(
      nudge_controller->toast_widget()->GetNativeWindow()->GetRootWindow(),
      session->current_root());
}

TEST_F(CaptureModeAdvancedSettingsTest, NudgeDoesNotShowForAllUserTypes) {
  struct {
    std::string trace;
    user_manager::UserType user_type;
    bool can_see_nudge;
  } kTestCases[] = {
      {"regular user", user_manager::USER_TYPE_REGULAR, true},
      {"child", user_manager::USER_TYPE_CHILD, true},
      {"guest", user_manager::USER_TYPE_GUEST, false},
      {"public account", user_manager::USER_TYPE_PUBLIC_ACCOUNT, false},
      {"kiosk app", user_manager::USER_TYPE_KIOSK_APP, false},
      {"arc kiosk app", user_manager::USER_TYPE_ARC_KIOSK_APP, false},
      {"web kiosk app", user_manager::USER_TYPE_WEB_KIOSK_APP, false},
      {"active dir", user_manager::USER_TYPE_ACTIVE_DIRECTORY, false},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.trace);
    ClearLogin();
    SimulateUserLogin("example@gmail.com", test_case.user_type);

    auto* controller = StartImageRegionCapture();
    EXPECT_EQ(test_case.can_see_nudge,
              controller->CanShowFolderSelectionNudge());

    auto* nudge_controller = GetUserNudgeController();
    EXPECT_EQ(test_case.can_see_nudge, !!nudge_controller);

    controller->Stop();
  }
}

// Tests that it's possbile to take a screenshot using the keyboard shortcut at
// the login screen without any crashes. https://crbug.com/1266728.
TEST_F(CaptureModeAdvancedSettingsTest, TakeScreenshotAtLoginScreen) {
  ClearLogin();
  PressAndReleaseKey(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN);
  WaitForCaptureFileToBeSaved();
}

// Tests that clicking on audio input buttons updates the state in the
// controller, and persists between sessions.
TEST_F(CaptureModeAdvancedSettingsTest, AudioInputSettingsMenu) {
  auto* controller = StartImageRegionCapture();
  auto* event_generator = GetEventGenerator();

  // Test that the audio recording preference is defaulted to off.
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_FALSE(controller->enable_audio_recording());

  CaptureModeAdvancedSettingsTestApi test_api;
  CaptureModeMenuGroup* audio_input_menu_group =
      test_api.GetAudioInputMenuGroup();
  EXPECT_TRUE(audio_input_menu_group->IsOptionChecked(kAudioOff));
  EXPECT_FALSE(audio_input_menu_group->IsOptionChecked(kAudioMicrophone));

  // Click on the |microphone| option. It should be checked after click along
  // with |off| is unchecked. Recording preference is set to microphone.
  views::View* microphone_option = test_api.GetMicrophoneOption();
  ClickOnView(microphone_option, event_generator);
  EXPECT_TRUE(audio_input_menu_group->IsOptionChecked(kAudioMicrophone));
  EXPECT_FALSE(audio_input_menu_group->IsOptionChecked(kAudioOff));
  EXPECT_TRUE(controller->enable_audio_recording());

  // Test that the user selected audio preference for audio recording is
  // remembered between sessions.
  SendKey(ui::VKEY_ESCAPE, event_generator);
  StartImageRegionCapture();
  EXPECT_TRUE(controller->enable_audio_recording());
}

TEST_F(CaptureModeAdvancedSettingsTest, SelectFolderFromDialog) {
  auto* controller = StartImageRegionCapture();
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetSettingsButton(), event_generator);

  // Initially there should only be an option for the default downloads folder.
  CaptureModeAdvancedSettingsTestApi test_api;
  EXPECT_TRUE(test_api.GetDefaultDownloadsOption());
  EXPECT_FALSE(test_api.GetCustomFolderOptionIfAny());
  CaptureModeMenuGroup* save_to_menu_group = test_api.GetSaveToMenuGroup();
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));

  ClickOnView(test_api.GetSelectFolderMenuItem(), event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());
  EXPECT_FALSE(IsAllCaptureSessionUisVisible());

  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  auto* dialog_window = dialog_factory->GetDialogWindow();
  auto* window_state = WindowState::Get(dialog_window);
  ASSERT_TRUE(window_state);
  EXPECT_FALSE(window_state->CanMaximize());
  EXPECT_FALSE(window_state->CanMinimize());
  EXPECT_FALSE(window_state->CanResize());

  // Accepting the dialog with a folder selection should dismiss it and add a
  // new option for the custom selected folder in the settings menu.
  const base::FilePath custom_folder(CreateCustomFolder("test"));
  dialog_factory->AcceptPath(custom_folder);
  WaitForSettingsMenuToBeRefreshed();
  EXPECT_FALSE(IsFolderSelectionDialogShown());
  EXPECT_TRUE(IsAllCaptureSessionUisVisible());
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kCustomFolder));
  EXPECT_FALSE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
  EXPECT_EQ(u"test",
            save_to_menu_group->GetOptionLabelForTesting(kCustomFolder));

  // This should update the folder that will be used by the controller.
  auto capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_EQ(capture_folder.path, custom_folder);
  EXPECT_FALSE(capture_folder.is_default_downloads_folder);
}

// Tests that folder selection dialog can be opened without crash while in
// window capture mode.
TEST_F(CaptureModeAdvancedSettingsTest, SelectFolderInWindowCaptureMode) {
  std::unique_ptr<aura::Window> window1(
      CreateTestWindow(gfx::Rect(0, 0, 200, 300)));
  StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kImage);
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetSettingsButton(), event_generator);

  CaptureModeAdvancedSettingsTestApi test_api;
  ClickOnView(test_api.GetSelectFolderMenuItem(), event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());
}

TEST_F(CaptureModeAdvancedSettingsTest, DismissDialogWithoutSelection) {
  auto* controller = StartImageRegionCapture();
  const auto old_capture_folder = controller->GetCurrentCaptureFolder();

  // Open the settings menu, and click the "Select folder" menu item.
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetSettingsButton(), event_generator);
  CaptureModeAdvancedSettingsTestApi test_api;
  ClickOnView(test_api.GetSelectFolderMenuItem(), event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());

  // Cancel and dismiss the dialog. There should be no change in the folder
  // selection.
  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  dialog_factory->CancelDialog();
  EXPECT_FALSE(IsFolderSelectionDialogShown());
  EXPECT_FALSE(test_api.GetCustomFolderOptionIfAny());
  CaptureModeMenuGroup* save_to_menu_group = test_api.GetSaveToMenuGroup();
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));

  const auto new_capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_EQ(old_capture_folder.path, new_capture_folder.path);
  EXPECT_EQ(old_capture_folder.is_default_downloads_folder,
            new_capture_folder.is_default_downloads_folder);
}

TEST_F(CaptureModeAdvancedSettingsTest, AcceptUpdatedCustomFolderFromDialog) {
  // Begin a new session with a pre-configured custom folder.
  auto* controller = CaptureModeController::Get();
  const base::FilePath custom_folder(CreateCustomFolder("test"));
  controller->SetCustomCaptureFolder(custom_folder);
  StartImageRegionCapture();

  // Open the settings menu and check there already exists an item for that
  // pre-configured custom folder.
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetSettingsButton(), event_generator);
  WaitForSettingsMenuToBeRefreshed();
  CaptureModeAdvancedSettingsTestApi test_api;
  EXPECT_TRUE(test_api.GetDefaultDownloadsOption());
  auto* custom_folder_view = test_api.GetCustomFolderOptionIfAny();
  EXPECT_TRUE(custom_folder_view);
  CaptureModeMenuGroup* save_to_menu_group = test_api.GetSaveToMenuGroup();
  EXPECT_FALSE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kCustomFolder));

  // Now open the folder selection dialog and select a different folder. The
  // existing *same* item in the menu should be updated.
  ClickOnView(test_api.GetSelectFolderMenuItem(), event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());

  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  const base::FilePath new_folder(CreateCustomFolder("test1"));
  dialog_factory->AcceptPath(new_folder);
  WaitForSettingsMenuToBeRefreshed();
  EXPECT_FALSE(IsFolderSelectionDialogShown());
  EXPECT_EQ(custom_folder_view, test_api.GetCustomFolderOptionIfAny());
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kCustomFolder));
  EXPECT_FALSE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
  EXPECT_EQ(u"test1",
            save_to_menu_group->GetOptionLabelForTesting(kCustomFolder));

  // This should update the folder that will be used by the controller.
  const auto capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_EQ(capture_folder.path, new_folder);
  EXPECT_FALSE(capture_folder.is_default_downloads_folder);
}

TEST_F(CaptureModeAdvancedSettingsTest,
       InitializeSettingsViewWithUnavailableCustomFolder) {
  // Begin a new session with a pre-configured unavailable custom folder.
  auto* controller = CaptureModeController::Get();
  const base::FilePath default_folder =
      controller->delegate_for_testing()->GetUserDefaultDownloadsFolder();
  const base::FilePath custom_folder(FILE_PATH_LITERAL("/home/random"));
  controller->SetCustomCaptureFolder(custom_folder);
  StartImageRegionCapture();

  // Open the settings menu and check there already exists an item for that
  // pre-configured custom folder. Since the custom folder is unavailable, the
  // item should be disabled and dimmed. The item of the default folder should
  // be checked.
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetSettingsButton(), event_generator);
  WaitForSettingsMenuToBeRefreshed();

  CaptureModeAdvancedSettingsTestApi test_api;
  EXPECT_TRUE(test_api.GetDefaultDownloadsOption());
  auto* custom_folder_view = test_api.GetCustomFolderOptionIfAny();
  EXPECT_TRUE(custom_folder_view);
  CaptureModeMenuGroup* save_to_menu_group = test_api.GetSaveToMenuGroup();
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
  EXPECT_FALSE(save_to_menu_group->IsOptionChecked(kCustomFolder));
  EXPECT_FALSE(custom_folder_view->GetEnabled());
  EXPECT_EQ(u"random",
            save_to_menu_group->GetOptionLabelForTesting(kCustomFolder));

  // Now open the folder selection dialog and select an available folder. The
  // item of the custom folder should be checked and enabled.
  ClickOnView(test_api.GetSelectFolderMenuItem(), event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());

  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  const base::FilePath new_folder(CreateCustomFolder("test"));
  dialog_factory->AcceptPath(new_folder);
  WaitForSettingsMenuToBeRefreshed();
  EXPECT_EQ(custom_folder_view, test_api.GetCustomFolderOptionIfAny());
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kCustomFolder));
  EXPECT_FALSE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
  EXPECT_TRUE(custom_folder_view->GetEnabled());
  EXPECT_EQ(u"test",
            save_to_menu_group->GetOptionLabelForTesting(kCustomFolder));
}

TEST_F(CaptureModeAdvancedSettingsTest, DeleteCustomFolderFromDialog) {
  // Begin a new session with a pre-configured custom folder.
  auto* controller = CaptureModeController::Get();
  const base::FilePath custom_folder(CreateCustomFolder("test"));
  controller->SetCustomCaptureFolder(custom_folder);
  StartImageRegionCapture();

  // Open the settings menu and check there exists an item for that custom
  // folder. And the item is checked to indicate the current folder in use to
  // save the captured files is the custom folder.
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetSettingsButton(), event_generator);
  WaitForSettingsMenuToBeRefreshed();

  CaptureModeAdvancedSettingsTestApi test_api;
  EXPECT_TRUE(test_api.GetDefaultDownloadsOption());
  auto* custom_folder_view = test_api.GetCustomFolderOptionIfAny();
  EXPECT_TRUE(custom_folder_view);
  CaptureModeMenuGroup* save_to_menu_group = test_api.GetSaveToMenuGroup();
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kCustomFolder));

  // Now open the folder selection dialog and delete the custom folder. Check
  // the item on the settings menu for custom folder is still there but disabled
  // and dimmed. The item of the default folder is checked now.
  ClickOnView(test_api.GetSelectFolderMenuItem(), event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());
  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    const bool result = base::DeleteFile(custom_folder);
    DCHECK(result);
  }
  dialog_factory->CancelDialog();
  WaitForSettingsMenuToBeRefreshed();
  EXPECT_TRUE(custom_folder_view);
  EXPECT_FALSE(save_to_menu_group->IsOptionChecked(kCustomFolder));
  EXPECT_FALSE(custom_folder_view->GetEnabled());
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
}

TEST_F(CaptureModeAdvancedSettingsTest,
       AcceptDefaultDownloadsFolderFromDialog) {
  // Begin a new session with a pre-configured custom folder.
  auto* controller = CaptureModeController::Get();
  controller->SetCustomCaptureFolder(
      base::FilePath(FILE_PATH_LITERAL("/home/tests/foo")));
  StartImageRegionCapture();

  auto* event_generator = GetEventGenerator();
  ClickOnView(GetSettingsButton(), event_generator);
  WaitForSettingsMenuToBeRefreshed();
  CaptureModeAdvancedSettingsTestApi test_api;
  ClickOnView(test_api.GetSelectFolderMenuItem(), event_generator);

  // Selecting the same folder as the default downloads folder should result in
  // removing the custom folder option from the menu.
  auto* test_delegate = controller->delegate_for_testing();
  const auto default_downloads_folder =
      test_delegate->GetUserDefaultDownloadsFolder();
  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  dialog_factory->AcceptPath(default_downloads_folder);
  EXPECT_FALSE(IsFolderSelectionDialogShown());
  EXPECT_TRUE(test_api.GetDefaultDownloadsOption());
  EXPECT_FALSE(test_api.GetCustomFolderOptionIfAny());
  CaptureModeMenuGroup* save_to_menu_group = test_api.GetSaveToMenuGroup();
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
}

TEST_F(CaptureModeAdvancedSettingsTest, SwitchWhichFolderToUserFromOptions) {
  // Begin a new session with a pre-configured custom folder.
  auto* controller = CaptureModeController::Get();
  const base::FilePath custom_path((CreateCustomFolder("test")));
  controller->SetCustomCaptureFolder(custom_path);
  StartImageRegionCapture();
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetSettingsButton(), event_generator);
  WaitForSettingsMenuToBeRefreshed();

  // Clicking the "Downloads" option will set it as the folder of choice, but
  // won't clear the custom folder.
  CaptureModeAdvancedSettingsTestApi test_api;
  ClickOnView(test_api.GetDefaultDownloadsOption(), event_generator);
  CaptureModeMenuGroup* save_to_menu_group = test_api.GetSaveToMenuGroup();
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
  EXPECT_FALSE(save_to_menu_group->IsOptionChecked(kCustomFolder));
  const auto default_downloads_folder =
      controller->delegate_for_testing()->GetUserDefaultDownloadsFolder();
  auto capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_EQ(capture_folder.path, default_downloads_folder);
  EXPECT_TRUE(capture_folder.is_default_downloads_folder);
  EXPECT_EQ(custom_path, controller->GetCustomCaptureFolder());

  // Clicking on the custom folder option will switch back to using it.
  ClickOnView(test_api.GetCustomFolderOptionIfAny(), event_generator);
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kCustomFolder));
  EXPECT_FALSE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
  capture_folder = controller->GetCurrentCaptureFolder();
  EXPECT_EQ(capture_folder.path, custom_path);
  EXPECT_FALSE(capture_folder.is_default_downloads_folder);
}

// Tests that when there's no overlap betwwen capture label widget and settings
// widget, capture label widget is shown/hidden correctly after open/close the
// folder selection window.
TEST_F(CaptureModeAdvancedSettingsTest,
       CaptureLabelViewNotOverlapsWithSettingsView) {
  // Update the display size to make sure capture label widget will not
  // overlap with settings widget
  UpdateDisplay("1200x1000");

  auto* controller = StartImageRegionCapture();
  auto* event_generator = GetEventGenerator();

  // Tests that the capture label widget doesn't overlap with settings widget.
  // Both capture label widget and settings widget are visible.
  views::Widget* capture_label_widget = GetCaptureModeLabelWidget();
  ClickOnView(GetSettingsButton(), event_generator);
  views::Widget* settings_widget = GetCaptureModeSettingsWidget();
  EXPECT_FALSE(capture_label_widget->GetWindowBoundsInScreen().Intersects(
      settings_widget->GetWindowBoundsInScreen()));
  EXPECT_TRUE(capture_label_widget->IsVisible());
  EXPECT_TRUE(settings_widget->IsVisible());

  // Open folder selection window, check that both capture label widget and
  // settings widget are invisible.
  CaptureModeAdvancedSettingsTestApi test_api;
  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  ClickOnView(test_api.GetSelectFolderMenuItem(), event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());
  EXPECT_FALSE(capture_label_widget->IsVisible());
  EXPECT_FALSE(settings_widget->IsVisible());

  // Now close folder selection window, check that capture label widget and
  // settings widget become visible.
  dialog_factory->CancelDialog();
  EXPECT_FALSE(IsFolderSelectionDialogShown());
  EXPECT_TRUE(capture_label_widget->IsVisible());
  EXPECT_EQ(capture_label_widget->GetLayer()->GetTargetOpacity(), 1.f);
  EXPECT_TRUE(settings_widget->IsVisible());

  // Close settings widget. Capture label widget is visible.
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_TRUE(capture_label_widget->IsVisible());
  controller->Stop();
}

// Tests that when capture label widget overlaps with settings widget, capture
// label widget is shown/hidden correctly after open/close the folder selection
// window, open/close settings menu. Regression test for
// https://crbug.com/1279606.
TEST_F(CaptureModeAdvancedSettingsTest,
       CaptureLabelViewOverlapsWithSettingsView) {
  // Update display size to make capture label widget overlap with settings
  // widget.
  UpdateDisplay("1100x700");
  auto* controller = StartImageRegionCapture();
  auto* event_generator = GetEventGenerator();

  // Tests that capture label widget overlaps with settings widget and is
  // hidden after setting widget is shown.
  auto* capture_label_widget = GetCaptureModeLabelWidget();
  ClickOnView(GetSettingsButton(), event_generator);
  auto* settings_widget = GetCaptureModeSettingsWidget();
  EXPECT_TRUE(capture_label_widget->GetWindowBoundsInScreen().Intersects(
      settings_widget->GetWindowBoundsInScreen()));
  EXPECT_FALSE(GetCaptureModeLabelWidget()->IsVisible());
  EXPECT_TRUE(settings_widget->IsVisible());

  // Open folder selection window, capture label widget is invisible.
  CaptureModeAdvancedSettingsTestApi test_api;
  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  ClickOnView(test_api.GetSelectFolderMenuItem(), event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());
  EXPECT_FALSE(capture_label_widget->IsVisible());

  // Close folder selection window, capture label widget is invisible.
  dialog_factory->CancelDialog();
  EXPECT_FALSE(IsFolderSelectionDialogShown());
  EXPECT_FALSE(capture_label_widget->IsVisible());

  // Tests that capture label widget is visible after settings widget is
  // closed.
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_TRUE(capture_label_widget->IsVisible());
  EXPECT_EQ(capture_label_widget->GetLayer()->GetTargetOpacity(), 1.f);
  controller->Stop();
}

// Tests the basic keyboard navigation functions for the settings menu.
TEST_F(CaptureModeAdvancedSettingsTest, KeyboardNavigationForSettingsMenu) {
  auto* controller =
      StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kImage);

  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());

  // Tab six times to focus the settings button.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, /*count=*/6);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Enter space to open the settings menu. The current focus group should be
  // `kPendingSettings`.
  SendKey(ui::VKEY_SPACE, event_generator);
  ASSERT_TRUE(GetCaptureModeAdvancedSettingsView());
  EXPECT_EQ(FocusGroup::kPendingSettings, test_api.GetCurrentFocusGroup());

  CaptureModeAdvancedSettingsTestApi advanced_settings_test_api;
  CaptureModeMenuGroup* audio_input_menu_group =
      advanced_settings_test_api.GetAudioInputMenuGroup();
  // Tab once to focus the first item on the settings menu (`Audio input`
  // header).
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsMenu, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());

  // Tab once to focus the `Off` option on the settings menu. Check `Off` option
  // is the checked option not the `Microphone`.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_TRUE(audio_input_menu_group->IsOptionChecked(kAudioOff));
  EXPECT_FALSE(audio_input_menu_group->IsOptionChecked(kAudioMicrophone));

  // Now tab once to focus the `Microphone` option and enter space to select it.
  // Check now `Microphone` option is the checked option not the `Off`.
  SendKey(ui::VKEY_TAB, event_generator);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_FALSE(audio_input_menu_group->IsOptionChecked(kAudioOff));
  EXPECT_TRUE(audio_input_menu_group->IsOptionChecked(kAudioMicrophone));

  // Tab three times to focus the `Select folder...` menu item and enter space
  // to open the selection window.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, /*count=*/3);
  SendKey(ui::VKEY_SPACE, event_generator);
  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  EXPECT_TRUE(IsFolderSelectionDialogShown());

  // Close selection window.
  dialog_factory->CancelDialog();
  EXPECT_FALSE(IsFolderSelectionDialogShown());

  // Now tab once to focus on the settings button and enter space on it to close
  // the settings menu.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_FALSE(GetCaptureModeAdvancedSettingsView());
}

// Tests that the disabled option in the settings menu will be skipped while
// tabbing through.
TEST_F(CaptureModeAdvancedSettingsTest,
       KeyboardNavigationForSettingsMenuWithDisabledOption) {
  // Begin a new session with a pre-configured unavailable custom folder.
  auto* controller = CaptureModeController::Get();
  const base::FilePath custom_folder(FILE_PATH_LITERAL("/home/random"));
  controller->SetCustomCaptureFolder(custom_folder);
  StartImageRegionCapture();

  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());

  // Tab six times to focus the settings button and enter space to open the
  // setting menu.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, /*count=*/6);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(FocusGroup::kPendingSettings, test_api.GetCurrentFocusGroup());
  CaptureModeAdvancedSettingsView* settings_menu =
      GetCaptureModeAdvancedSettingsView();
  ASSERT_TRUE(settings_menu);

  // Since the custom folder is unavailable, the `kCustomFolder` should be
  // disabled and won't be returned via
  // `CaptureModeAdvancedSettingsViews::GetHighlightableItems`.
  CaptureModeAdvancedSettingsTestApi advanced_settings_test_api;
  auto* custom_folder_view =
      advanced_settings_test_api.GetCustomFolderOptionIfAny();
  ASSERT_TRUE(custom_folder_view);
  EXPECT_FALSE(custom_folder_view->GetEnabled());

  std::vector<CaptureModeSessionFocusCycler::HighlightableView*>
      highlightable_items = settings_menu->GetHighlightableItems();
  EXPECT_TRUE(
      std::find_if(highlightable_items.begin(), highlightable_items.end(),
                   [custom_folder_view](
                       CaptureModeSessionFocusCycler::HighlightableView* item) {
                     return item->GetView() == custom_folder_view;
                   }) == highlightable_items.end());

  // Tab five times to focus the default `Downloads` option.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, /*count=*/5);
  EXPECT_EQ(test_api.GetCurrentFocusedView()->GetView(),
            advanced_settings_test_api.GetDefaultDownloadsOption());

  // Tab once to check the disabled `kCustomFolder` option is skipped and now
  // the `Select folder...` menu item gets focused.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(test_api.GetCurrentFocusedView()->GetView(),
            advanced_settings_test_api.GetSelectFolderMenuItem());
}

// Tests that selecting the default `Downloads` folder as the custom folder via
// keyboard navigation doesn't lead to a crash. Regression test for
// https://crbug.com/1269373.
TEST_F(CaptureModeAdvancedSettingsTest,
       KeyboardNavigationForRemovingCustomFolderOption) {
  // Begin a new session with a pre-configured custom folder.
  auto* controller = CaptureModeController::Get();
  const base::FilePath custom_folder(CreateCustomFolder("test"));
  controller->SetCustomCaptureFolder(custom_folder);
  StartImageRegionCapture();

  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());

  // Tab six times to focus the settings button, then enter space to open the
  // setting menu. Wait for the setting menu to be refreshed.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, /*count=*/6);
  SendKey(ui::VKEY_SPACE, event_generator);
  WaitForSettingsMenuToBeRefreshed();
  EXPECT_EQ(FocusGroup::kPendingSettings, test_api.GetCurrentFocusGroup());
  CaptureModeAdvancedSettingsView* settings_menu =
      GetCaptureModeAdvancedSettingsView();
  ASSERT_TRUE(settings_menu);

  // Tab seven times to focus the `Select folder...` menu item and enter space
  // to open the selection window.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, /*count=*/7);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());
  // The current focus group is `FocusGroup::kSettingsMenu` and focus index is
  // 6u.
  EXPECT_EQ(FocusGroup::kSettingsMenu, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(6u, test_api.GetCurrentFocusIndex());

  // Select the default `Downloads` folder as the custom folder which will
  // have custom folder option get removed.
  auto* test_delegate = controller->delegate_for_testing();
  const auto default_downloads_folder =
      test_delegate->GetUserDefaultDownloadsFolder();
  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  dialog_factory->AcceptPath(default_downloads_folder);

  // Press space to ensure the selection window can be opened after the custom
  // folder is removed from the settings menu.
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());
  dialog_factory->CancelDialog();

  // Tab once to make sure there's no crash and the focus gets moved to
  // settings button.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
}

// Tests that first time selecting a custom folder via keyboard navigation.
// After the custom folder is selected, tabbing one more time will move focus
// from the settings menu to the settings button.
TEST_F(CaptureModeAdvancedSettingsTest,
       KeyboardNavigationForAddingCustomFolderOption) {
  auto* controller = CaptureModeController::Get();
  StartImageRegionCapture();

  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());

  // Tab six times to focus the settings button, then enter space to open the
  // setting menu.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, /*count=*/6);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(FocusGroup::kPendingSettings, test_api.GetCurrentFocusGroup());
  CaptureModeAdvancedSettingsView* settings_menu =
      GetCaptureModeAdvancedSettingsView();
  ASSERT_TRUE(settings_menu);

  // Tab six times to focus the `Select folder...` menu item and enter space
  // to open the selection window.
  SendKey(ui::VKEY_TAB, event_generator, /*shift_down=*/false, /*count=*/6);
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());
  // The current focus group is `FocusGroup::kSettingsMenu` and focus index is
  // 5u.
  EXPECT_EQ(FocusGroup::kSettingsMenu, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(5u, test_api.GetCurrentFocusIndex());

  // Select the custom folder. Wait for the settings menu to be refreshed. The
  // custom folder option should be added to the settings menu and checked.
  const base::FilePath custom_folder(CreateCustomFolder("test"));
  controller->SetCustomCaptureFolder(custom_folder);
  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  dialog_factory->AcceptPath(custom_folder);
  WaitForSettingsMenuToBeRefreshed();
  CaptureModeAdvancedSettingsTestApi advanced_test_api;
  EXPECT_TRUE(advanced_test_api.GetCustomFolderOptionIfAny());

  // Press space to ensure the selection window can be opened after the custom
  // folder is added to the settings menu.
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());
  dialog_factory->CancelDialog();

  // Tab once to make sure the focus gets moved to settings button.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(FocusGroup::kSettingsClose, test_api.GetCurrentFocusGroup());
  EXPECT_EQ(0u, test_api.GetCurrentFocusIndex());
}

// -----------------------------------------------------------------------------
// CaptureModeHistogramTest:

// Test fixture to verify screen capture histograms depending on the test
// param (true for tablet mode, false for clamshell mode).
class CaptureModeHistogramTest : public CaptureModeAdvancedSettingsTest,
                                 public ::testing::WithParamInterface<bool> {
 public:
  CaptureModeHistogramTest() = default;
  ~CaptureModeHistogramTest() override = default;

  // CaptureModeAdvancedSettingsTest:
  void SetUp() override {
    CaptureModeAdvancedSettingsTest::SetUp();
    if (GetParam())
      SwitchToTabletMode();
  }

  void StartSessionForVideo() {
    StartCaptureSession(CaptureModeSource::kFullscreen,
                        CaptureModeType::kVideo);
  }

  void StartRecording() { CaptureModeTestApi().PerformCapture(); }

  void StopRecording() { CaptureModeTestApi().StopVideoRecording(); }

  void OpenView(const views::View* view,
                ui::test::EventGenerator* event_generator) {
    if (GetParam())
      TouchOnView(view, event_generator);
    else
      ClickOnView(view, event_generator);
  }
};

TEST_P(CaptureModeHistogramTest, VideoRecordingAudioMetric) {
  constexpr char kHistogramNameBase[] =
      "Ash.CaptureModeController.CaptureAudioOnMetric";
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      GetCaptureModeHistogramName(kHistogramNameBase), false, 0);
  histogram_tester.ExpectBucketCount(
      GetCaptureModeHistogramName(kHistogramNameBase), true, 0);
  // Perform a video recording with audio off. A false should be recorded.
  StartSessionForVideo();
  CaptureModeTestApi().SetAudioRecordingEnabled(false);
  StartRecording();
  histogram_tester.ExpectBucketCount(
      GetCaptureModeHistogramName(kHistogramNameBase), false, 1);
  histogram_tester.ExpectBucketCount(
      GetCaptureModeHistogramName(kHistogramNameBase), true, 0);
  StopRecording();
  WaitForCaptureFileToBeSaved();
  // Perform a video recording with audio on. A true should be recorded.
  StartSessionForVideo();
  CaptureModeTestApi().SetAudioRecordingEnabled(true);
  StartRecording();
  histogram_tester.ExpectBucketCount(
      GetCaptureModeHistogramName(kHistogramNameBase), false, 1);
  histogram_tester.ExpectBucketCount(
      GetCaptureModeHistogramName(kHistogramNameBase), true, 1);
  StopRecording();
}

TEST_P(CaptureModeHistogramTest, CaptureModeSwitchToDefaultReasonMetric) {
  constexpr char kHistogramNameBase[] =
      "Ash.CaptureModeController.SwitchToDefaultReason";
  base::HistogramTester histogram_tester;
  auto* controller = CaptureModeController::Get();
  const auto downloads_folder =
      controller->delegate_for_testing()->GetUserDefaultDownloadsFolder();
  const base::FilePath non_available_custom_folder(
      FILE_PATH_LITERAL("/home/test"));
  const base::FilePath available_custom_folder = CreateCustomFolder("test");

  histogram_tester.ExpectBucketCount(
      GetCaptureModeHistogramName(kHistogramNameBase),
      CaptureModeSwitchToDefaultReason::kFolderUnavailable, 0);
  histogram_tester.ExpectBucketCount(
      GetCaptureModeHistogramName(kHistogramNameBase),
      CaptureModeSwitchToDefaultReason::kUserSelectedFromFolderSelectionDialog,
      0);
  histogram_tester.ExpectBucketCount(
      GetCaptureModeHistogramName(kHistogramNameBase),
      CaptureModeSwitchToDefaultReason::kUserSelectedFromSettingsMenu, 0);

  StartImageRegionCapture();

  // Set the custom folder to an unavailable folder the switch to default
  // reason should be recorded as kFolderUnavailable.
  controller->SetCustomCaptureFolder(non_available_custom_folder);
  EXPECT_EQ(controller->GetCurrentCaptureFolder().path,
            non_available_custom_folder);
  auto* event_generator = GetEventGenerator();
  OpenView(GetSettingsButton(), event_generator);
  WaitForSettingsMenuToBeRefreshed();
  CaptureModeAdvancedSettingsTestApi test_api;
  CaptureModeMenuGroup* save_to_menu_group = test_api.GetSaveToMenuGroup();
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
  histogram_tester.ExpectBucketCount(
      GetCaptureModeHistogramName(kHistogramNameBase),
      CaptureModeSwitchToDefaultReason::kFolderUnavailable, 1);

  // Select the save-to location to default downloads folder from folder
  // selection dialog and the switch to default reason should be recorded as
  // kUserSelectedFromSettingsMenu.
  controller->SetCustomCaptureFolder(available_custom_folder);
  EXPECT_EQ(controller->GetCurrentCaptureFolder().path,
            available_custom_folder);
  OpenView(test_api.GetSelectFolderMenuItem(), event_generator);
  EXPECT_TRUE(IsFolderSelectionDialogShown());
  auto* dialog_factory = FakeFolderSelectionDialogFactory::Get();
  dialog_factory->AcceptPath(downloads_folder);
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
  histogram_tester.ExpectBucketCount(
      GetCaptureModeHistogramName(kHistogramNameBase),
      CaptureModeSwitchToDefaultReason::kUserSelectedFromFolderSelectionDialog,
      1);

  // Select the save-to location to default downloads folder from settings
  // menu and the switch to default reason should be recorded as
  // kUserSelectedFromFolderSelectionDialog.
  controller->SetCustomCaptureFolder(available_custom_folder);
  EXPECT_EQ(controller->GetCurrentCaptureFolder().path,
            available_custom_folder);
  OpenView(test_api.GetDefaultDownloadsOption(), event_generator);
  EXPECT_TRUE(save_to_menu_group->IsOptionChecked(kDownloadsFolder));
  histogram_tester.ExpectBucketCount(
      GetCaptureModeHistogramName(kHistogramNameBase),
      CaptureModeSwitchToDefaultReason::kUserSelectedFromSettingsMenu, 1);
}

INSTANTIATE_TEST_SUITE_P(All, CaptureModeHistogramTest, ::testing::Bool());

}  // namespace ash
