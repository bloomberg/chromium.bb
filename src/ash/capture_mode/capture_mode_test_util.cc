// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_test_util.h"

#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session_test_api.h"
#include "ash/capture_mode/capture_mode_source_view.h"
#include "ash/capture_mode/test_capture_mode_delegate.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/projector/projector_session.h"
#include "ash/shell.h"
#include "ash/wm/cursor_manager_chromeos.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view.h"

namespace ash {

CaptureModeController* StartCaptureSession(CaptureModeSource source,
                                           CaptureModeType type) {
  auto* controller = CaptureModeController::Get();
  controller->SetSource(source);
  controller->SetType(type);
  controller->Start(CaptureModeEntryType::kQuickSettings);
  DCHECK(controller->IsActive());
  return controller;
}

void ClickOnView(const views::View* view,
                 ui::test::EventGenerator* event_generator) {
  DCHECK(view);
  DCHECK(event_generator);

  const gfx::Point view_center = view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(view_center);
  event_generator->ClickLeftButton();
}

void WaitForRecordingToStart() {
  auto* controller = CaptureModeController::Get();
  if (controller->is_recording_in_progress())
    return;
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  ASSERT_TRUE(test_delegate);
  base::RunLoop run_loop;
  test_delegate->set_on_recording_started_callback(run_loop.QuitClosure());
  run_loop.Run();
  ASSERT_TRUE(controller->is_recording_in_progress());
}

void MoveMouseToAndUpdateCursorDisplay(
    const gfx::Point& point,
    ui::test::EventGenerator* event_generator) {
  Shell::Get()->cursor_manager()->SetDisplay(
      display::Screen::GetScreen()->GetDisplayNearestPoint(point));
  event_generator->MoveMouseTo(point);
}

void StartVideoRecordingImmediately() {
  CaptureModeController::Get()->StartVideoRecordingImmediatelyForTesting();
  WaitForRecordingToStart();
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

base::FilePath CreateCustomFolderInUserDownloadsPath(
    const std::string& custom_folder_name) {
  base::FilePath custom_folder = CaptureModeController::Get()
                                     ->delegate_for_testing()
                                     ->GetUserDefaultDownloadsFolder()
                                     .Append(custom_folder_name);
  base::ScopedAllowBlockingForTesting allow_blocking;
  const bool result = base::CreateDirectory(custom_folder);
  DCHECK(result);
  return custom_folder;
}

void SendKey(ui::KeyboardCode key_code,
             ui::test::EventGenerator* event_generator,
             int flags,
             int count) {
  for (int i = 0; i < count; ++i)
    event_generator->PressAndReleaseKey(key_code, flags);
}

void WaitForSeconds(int seconds) {
  base::RunLoop loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), base::Seconds(seconds));
  loop.Run();
}

void SwitchToTabletMode() {
  TabletModeControllerTestApi test_api;
  test_api.DetachAllMice();
  test_api.EnterTabletMode();
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

void ClickOrTapView(const views::View* view,
                    const bool in_tablet_mode,
                    ui::test::EventGenerator* event_generator) {
  if (in_tablet_mode)
    TouchOnView(view, event_generator);
  else
    ClickOnView(view, event_generator);
}

CaptureModeBarView* GetCaptureModeBarView() {
  auto* session = CaptureModeController::Get()->capture_mode_session();
  DCHECK(session);
  return CaptureModeSessionTestApi(session).GetCaptureModeBarView();
}

CaptureModeToggleButton* GetFullscreenToggleButton() {
  auto* controller = CaptureModeController::Get();
  DCHECK(controller->IsActive());
  return GetCaptureModeBarView()
      ->capture_source_view()
      ->fullscreen_toggle_button();
}

CaptureModeToggleButton* GetRegionToggleButton() {
  auto* controller = CaptureModeController::Get();
  DCHECK(controller->IsActive());
  return GetCaptureModeBarView()->capture_source_view()->region_toggle_button();
}

UserNudgeController* GetUserNudgeController() {
  auto* session = CaptureModeController::Get()->capture_mode_session();
  DCHECK(session);
  return CaptureModeSessionTestApi(session).GetUserNudgeController();
}

// -----------------------------------------------------------------------------
// ProjectorCaptureModeIntegrationHelper:

ProjectorCaptureModeIntegrationHelper::ProjectorCaptureModeIntegrationHelper() {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kProjector,
                            features::kProjectorAnnotator},
      /*disabled_features=*/{});
}

void ProjectorCaptureModeIntegrationHelper::SetUp() {
  auto* projector_controller = ProjectorController::Get();
  projector_controller->SetClient(&projector_client_);
  ON_CALL(projector_client_, StopSpeechRecognition)
      .WillByDefault(testing::Invoke(
          []() { ProjectorController::Get()->OnSpeechRecognitionStopped(); }));

  // Simulate the availability of speech recognition.
  projector_controller->OnSpeechRecognitionAvailabilityChanged(
      SpeechRecognitionAvailability::kAvailable);
  EXPECT_CALL(projector_client_, IsDriveFsMounted())
      .WillRepeatedly(testing::Return(true));
}

void ProjectorCaptureModeIntegrationHelper::StartProjectorModeSession() {
  auto* projector_session = ProjectorSession::Get();
  EXPECT_FALSE(projector_session->is_active());
  auto* projector_controller = ProjectorController::Get();
  EXPECT_CALL(projector_client_, MinimizeProjectorApp());
  projector_controller->StartProjectorSession("projector_data");
  EXPECT_TRUE(projector_session->is_active());
  auto* controller = CaptureModeController::Get();
  EXPECT_EQ(controller->source(), CaptureModeSource::kFullscreen);
}

bool IsLayerStackedRightBelow(ui::Layer* layer, ui::Layer* sibling) {
  DCHECK_EQ(layer->parent(), sibling->parent());
  const auto& children = layer->parent()->children();
  const int sibling_index =
      std::find(children.begin(), children.end(), sibling) - children.begin();
  return sibling_index > 0 && children[sibling_index - 1] == layer;
}

}  // namespace ash
