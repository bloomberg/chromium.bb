// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_TEST_UTIL_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_TEST_UTIL_H_

#include <string>

#include "ash/capture_mode/capture_mode_types.h"
#include "ash/projector/test/mock_projector_client.h"
#include "base/test/scoped_feature_list.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace base {
class FilePath;
}  // namespace base

namespace gfx {
class Point;
}  // namespace gfx

namespace ui::test {
class EventGenerator;
}  // namespace ui::test

namespace views {
class View;
}  // namespace views

// Functions that are used by capture mode related unit tests and only meant to
// be used in ash_unittests.

namespace ash {

class CaptureModeController;

// Starts the capture mode session with given `source` and `type`.
CaptureModeController* StartCaptureSession(CaptureModeSource source,
                                           CaptureModeType type);

void ClickOnView(const views::View* view,
                 ui::test::EventGenerator* event_generator);

// Waits until the recording is in progress.
void WaitForRecordingToStart();

// Moves the mouse and updates the cursor's display manually to imitate what a
// real mouse move event does in shell.
// TODO(crbug.com/990589): Unit tests should be able to simulate mouse input
// without having to call |CursorManager::SetDisplay|.
void MoveMouseToAndUpdateCursorDisplay(
    const gfx::Point& point,
    ui::test::EventGenerator* event_generator);

// Starts recording immediately without the 3-seconds count down.
void StartVideoRecordingImmediately();

// Returns the whole file path where the screen capture file is saved to. The
// returned file path could be either under the default downloads folder or the
// custom folder.
base::FilePath WaitForCaptureFileToBeSaved();

// Creates and returns the custom folder path. The custom folder is created in
// the default downloads folder with given `custom_folder_name`.
base::FilePath CreateCustomFolderInUserDownloadsPath(
    const std::string& custom_folder_name);

// Sends a press release key combo `count` times.
void SendKey(ui::KeyboardCode key_code,
             ui::test::EventGenerator* event_generator,
             int flags = ui::EF_NONE,
             int count = 1);

// Wait for a specific `seconds`.
void WaitForSeconds(int seconds);

// To avoid flaky failures due to mouse devices blocking entering tablet mode,
// we detach all mouse devices. This shouldn't affect testing the cursor
// status.
void SwitchToTabletMode();

// Open the `view` by touch.
void TouchOnView(const views::View* view,
                 ui::test::EventGenerator* event_generator);

// Clicks or taps on the `view` based on whether the user is in clamshell or
// tablet mode.
void ClickOrTapView(const views::View* view,
                    bool in_table_mode,
                    ui::test::EventGenerator* event_generator);

// Defines a helper class to allow setting up and testing the Projector feature
// in multiple test fixtures. Note that this helper initializes the Projector-
// related features in its constructor, so test fixtures that use this should
// also initialize their `ScopedFeatureList` in their constructors to avoid
// DCHECKing when nested ScopedFeatureLists being destroyed in a different order
// than they are initialized.
class ProjectorCaptureModeIntegrationHelper {
 public:
  ProjectorCaptureModeIntegrationHelper();
  ProjectorCaptureModeIntegrationHelper(
      const ProjectorCaptureModeIntegrationHelper&) = delete;
  ProjectorCaptureModeIntegrationHelper& operator=(
      const ProjectorCaptureModeIntegrationHelper&) = delete;
  ~ProjectorCaptureModeIntegrationHelper() = default;

  MockProjectorClient* projector_client() { return &projector_client_; }

  // Sets up the projector feature. Must be called after `AshTestBase::SetUp()`
  // has been called.
  void SetUp();

  // Starts a new projector capture session.
  void StartProjectorModeSession();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  MockProjectorClient projector_client_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_TEST_UTIL_H_
