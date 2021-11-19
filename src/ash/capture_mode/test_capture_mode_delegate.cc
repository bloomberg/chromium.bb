// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/test_capture_mode_delegate.h"

#include "ash/capture_mode/capture_mode_types.h"
#include "ash/public/cpp/capture_mode/recording_overlay_view.h"
#include "ash/services/recording/public/mojom/recording_service.mojom.h"
#include "ash/services/recording/recording_service_test_api.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"

namespace ash {

namespace {

class TestRecordingOverlayView : public RecordingOverlayView {
 public:
  TestRecordingOverlayView() = default;
  TestRecordingOverlayView(const TestRecordingOverlayView&) = delete;
  TestRecordingOverlayView& operator=(const TestRecordingOverlayView&) = delete;
  ~TestRecordingOverlayView() override = default;
};

}  // namespace

TestCaptureModeDelegate::TestCaptureModeDelegate() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  const bool created_downloads_dir =
      base::CreateNewTempDirectory(/*prefix=*/"", &fake_downloads_dir_);
  DCHECK(created_downloads_dir);
  const bool created_root_drive_dir =
      fake_drive_fs_mount_path_.CreateUniqueTempDir();
  DCHECK(created_root_drive_dir);
}

TestCaptureModeDelegate::~TestCaptureModeDelegate() = default;

void TestCaptureModeDelegate::ResetAllowancesToDefault() {
  is_allowed_by_dlp_ = true;
  is_allowed_by_policy_ = true;
}

viz::FrameSinkId TestCaptureModeDelegate::GetCurrentFrameSinkId() const {
  return recording_service_ ? recording_service_->GetCurrentFrameSinkId()
                            : viz::FrameSinkId();
}

gfx::Size TestCaptureModeDelegate::GetCurrentFrameSinkSizeInPixels() const {
  return recording_service_
             ? recording_service_->GetCurrentFrameSinkSizeInPixels()
             : gfx::Size();
}

gfx::Size TestCaptureModeDelegate::GetCurrentVideoSize() const {
  return recording_service_ ? recording_service_->GetCurrentVideoSize()
                            : gfx::Size();
}

gfx::ImageSkia TestCaptureModeDelegate::GetVideoThumbnail() const {
  return recording_service_ ? recording_service_->GetVideoThumbnail()
                            : gfx::ImageSkia();
}

void TestCaptureModeDelegate::RequestAndWaitForVideoFrame() {
  DCHECK(recording_service_);

  recording_service_->RequestAndWaitForVideoFrame();
}

base::FilePath TestCaptureModeDelegate::GetUserDefaultDownloadsFolder() const {
  DCHECK(Shell::Get()->session_controller()->IsActiveUserSessionStarted());

  return fake_downloads_dir_;
}

void TestCaptureModeDelegate::ShowScreenCaptureItemInFolder(
    const base::FilePath& file_path) {}

void TestCaptureModeDelegate::OpenScreenshotInImageEditor(
    const base::FilePath& file_path) {}

bool TestCaptureModeDelegate::Uses24HourFormat() const {
  return false;
}

bool TestCaptureModeDelegate::IsCaptureModeInitRestrictedByDlp() const {
  return false;
}

bool TestCaptureModeDelegate::IsCaptureAllowedByDlp(const aura::Window* window,
                                                    const gfx::Rect& bounds,
                                                    bool for_video) const {
  return is_allowed_by_dlp_;
}

bool TestCaptureModeDelegate::IsCaptureAllowedByPolicy() const {
  return is_allowed_by_policy_;
}

void TestCaptureModeDelegate::StartObservingRestrictedContent(
    const aura::Window* window,
    const gfx::Rect& bounds,
    base::OnceClosure stop_callback) {
  // This is called at the last stage of recording initialization to signal that
  // recording has actually started.
  if (on_recording_started_callback_)
    std::move(on_recording_started_callback_).Run();
}

void TestCaptureModeDelegate::StopObservingRestrictedContent(
    OnCaptureModeDlpRestrictionChecked callback) {
  DCHECK(callback);
  std::move(callback).Run(should_save_after_dlp_check_);
}

mojo::Remote<recording::mojom::RecordingService>
TestCaptureModeDelegate::LaunchRecordingService() {
  mojo::Remote<recording::mojom::RecordingService> service_remote;
  recording_service_ = std::make_unique<recording::RecordingServiceTestApi>(
      service_remote.BindNewPipeAndPassReceiver());
  return service_remote;
}

void TestCaptureModeDelegate::BindAudioStreamFactory(
    mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {}

void TestCaptureModeDelegate::OnSessionStateChanged(bool started) {
  if (on_session_state_changed_callback_)
    std::move(on_session_state_changed_callback_).Run();
}

void TestCaptureModeDelegate::OnServiceRemoteReset() {
  // We simulate what the ServiceProcessHost does when the service remote is
  // reset (on which it shuts down the service process). Here since the service
  // is running in-process with ash_unittests, we just delete the instance.
  recording_service_.reset();
}

bool TestCaptureModeDelegate::GetDriveFsMountPointPath(
    base::FilePath* result) const {
  *result = fake_drive_fs_mount_path_.GetPath();
  return true;
}

std::unique_ptr<RecordingOverlayView>
TestCaptureModeDelegate::CreateRecordingOverlayView() const {
  return std::make_unique<TestRecordingOverlayView>();
}

}  // namespace ash
