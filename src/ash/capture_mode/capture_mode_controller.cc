// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_controller.h"

#include <utility>

#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_notification_view.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/capture_mode/video_recording_watcher.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/capture_mode_delegate.h"
#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/message_view_factory.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/bind_post_task.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/vector_icons/vector_icons.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/aura/env.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/display/types/display_constants.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/snapshot/snapshot.h"

namespace ash {

namespace {

CaptureModeController* g_instance = nullptr;

// The amount of time that can elapse from the prior screenshot to be considered
// consecutive.
constexpr base::TimeDelta kConsecutiveScreenshotThreshold =
    base::TimeDelta::FromSeconds(5);

constexpr char kScreenCaptureNotificationId[] = "capture_mode_notification";
constexpr char kScreenCaptureStoppedNotificationId[] =
    "capture_mode_stopped_notification";
constexpr char kScreenCaptureNotifierId[] = "ash.capture_mode_controller";
constexpr char kScreenShotNotificationType[] = "screen_shot_notification_type";
constexpr char kScreenRecordingNotificationType[] =
    "screen_recording_notification_type";

// The format strings of the file names of captured images.
// TODO(afakhry): Discuss with UX localizing "Screenshot" and "Screen
// recording".
constexpr char kScreenshotFileNameFmtStr[] = "Screenshot %s %s";
constexpr char kVideoFileNameFmtStr[] = "Screen recording %s %s";
constexpr char kDateFmtStr[] = "%d-%02d-%02d";
constexpr char k24HourTimeFmtStr[] = "%02d.%02d.%02d";
constexpr char kAmPmTimeFmtStr[] = "%d.%02d.%02d";

// Duration to clear the capture region selection from the previous session.
constexpr base::TimeDelta kResetCaptureRegionDuration =
    base::TimeDelta::FromMinutes(8);

// The screenshot notification button index.
enum ScreenshotNotificationButtonIndex {
  BUTTON_EDIT = 0,
  BUTTON_DELETE,
};

// The video notification button index.
enum VideoNotificationButtonIndex {
  BUTTON_DELETE_VIDEO = 0,
};

// Returns the date extracted from |timestamp| as a string to be part of
// captured file names. Note that naturally formatted dates includes slashes
// (e.g. 2020/09/02), which will cause problems when used in file names since
// slash is a path separator.
std::string GetDateStr(const base::Time::Exploded& timestamp) {
  return base::StringPrintf(kDateFmtStr, timestamp.year, timestamp.month,
                            timestamp.day_of_month);
}

// Returns the time extracted from |timestamp| as a string to be part of
// captured file names. Also note that naturally formatted times include colons
// (e.g. 11:20 AM), which is restricted in file names in most file systems.
// https://en.wikipedia.org/wiki/Filename#Comparison_of_filename_limitations.
std::string GetTimeStr(const base::Time::Exploded& timestamp,
                       bool use_24_hour) {
  if (use_24_hour) {
    return base::StringPrintf(k24HourTimeFmtStr, timestamp.hour,
                              timestamp.minute, timestamp.second);
  }

  int hour = timestamp.hour % 12;
  if (hour <= 0)
    hour += 12;

  std::string time = base::StringPrintf(kAmPmTimeFmtStr, hour, timestamp.minute,
                                        timestamp.second);
  return time.append(timestamp.hour >= 12 ? " PM" : " AM");
}

// Writes the given |data| in a file with |path|. Returns true if saving
// succeeded, or false otherwise.
bool SaveFile(scoped_refptr<base::RefCountedMemory> data,
              const base::FilePath& path) {
  DCHECK(data);
  const int size = static_cast<int>(data->size());
  DCHECK(size);
  DCHECK(!base::CurrentUIThread::IsSet());
  DCHECK(!path.empty());

  if (!base::PathExists(path.DirName())) {
    LOG(ERROR) << "File path doesn't exist: " << path.DirName();
    return false;
  }

  if (size != base::WriteFile(
                  path, reinterpret_cast<const char*>(data->front()), size)) {
    LOG(ERROR) << "Failed to save file: " << path;
    return false;
  }

  return true;
}

void DeleteFileAsync(scoped_refptr<base::SequencedTaskRunner> task_runner,
                     const base::FilePath& path) {
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::DeleteFile, path),
      base::BindOnce(
          [](const base::FilePath& path, bool success) {
            // TODO(afakhry): Show toast?
            if (!success)
              LOG(ERROR) << "Failed to delete the file: " << path;
          },
          path));
}

// Shows a Capture Mode related notification with the given parameters.
// |for_video_thumbnail| will be considered only if |optional_fields| contain
// an image to show in the notification as a thumbnail for what was captured.
void ShowNotification(
    const std::string& notification_id,
    int title_id,
    int message_id,
    const message_center::RichNotificationData& optional_fields,
    scoped_refptr<message_center::NotificationDelegate> delegate,
    message_center::SystemNotificationWarningLevel warning_level =
        message_center::SystemNotificationWarningLevel::NORMAL,
    const gfx::VectorIcon& notification_icon = kCaptureModeIcon,
    bool for_video_thumbnail = false) {
  const auto type = optional_fields.image.IsEmpty()
                        ? message_center::NOTIFICATION_TYPE_SIMPLE
                        : message_center::NOTIFICATION_TYPE_CUSTOM;
  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotification(
          type, notification_id, l10n_util::GetStringUTF16(title_id),
          l10n_util::GetStringUTF16(message_id),
          l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_DISPLAY_SOURCE),
          GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kScreenCaptureNotifierId),
          optional_fields, delegate, notification_icon, warning_level);
  if (type == message_center::NOTIFICATION_TYPE_CUSTOM) {
    notification->set_custom_view_type(for_video_thumbnail
                                           ? kScreenRecordingNotificationType
                                           : kScreenShotNotificationType);
  }

  // Remove the previous notification before showing the new one if there is
  // any.
  auto* message_center = message_center::MessageCenter::Get();
  message_center->RemoveNotification(notification_id,
                                     /*by_user=*/false);
  message_center->AddNotification(std::move(notification));
}

// Shows a notification informing the user that a Capture Mode operation has
// failed.
void ShowFailureNotification() {
  ShowNotification(kScreenCaptureStoppedNotificationId,
                   IDS_ASH_SCREEN_CAPTURE_FAILURE_TITLE,
                   IDS_ASH_SCREEN_CAPTURE_FAILURE_MESSAGE,
                   /*optional_fields=*/{}, /*delegate=*/nullptr);
}

// Returns the ID of the message or the title for the notification based on
// |allowance| and |for_title|.
int GetDisabledNotificationMessageId(CaptureAllowance allowance,
                                     bool for_title) {
  switch (allowance) {
    case CaptureAllowance::kDisallowedByPolicy:
      return for_title ? IDS_ASH_SCREEN_CAPTURE_POLICY_DISABLED_TITLE
                       : IDS_ASH_SCREEN_CAPTURE_POLICY_DISABLED_MESSAGE;
    case CaptureAllowance::kDisallowedByDlp:
      return for_title ? IDS_ASH_SCREEN_CAPTURE_DLP_DISABLED_TITLE
                       : IDS_ASH_SCREEN_CAPTURE_DLP_DISABLED_MESSAGE;
    case CaptureAllowance::kDisallowedByHdcp:
      return for_title ? IDS_ASH_SCREEN_CAPTURE_HDCP_STOPPED_TITLE
                       : IDS_ASH_SCREEN_CAPTURE_HDCP_BLOCKED_MESSAGE;
    case CaptureAllowance::kAllowed:
      NOTREACHED();
      return IDS_ASH_SCREEN_CAPTURE_POLICY_DISABLED_MESSAGE;
  }
}

// Shows a notification informing the user that Capture Mode operations are
// currently disabled. |allowance| identifies the reason why the operation is
// currently disabled.
void ShowDisabledNotification(CaptureAllowance allowance) {
  DCHECK(allowance != CaptureAllowance::kAllowed);
  ShowNotification(
      kScreenCaptureNotificationId,
      GetDisabledNotificationMessageId(allowance, /*for_title=*/true),
      GetDisabledNotificationMessageId(allowance, /*for_title=*/false),
      /*optional_fields=*/{}, /*delegate=*/nullptr,
      message_center::SystemNotificationWarningLevel::CRITICAL_WARNING,
      allowance == CaptureAllowance::kDisallowedByHdcp
          ? kCaptureModeIcon
          : vector_icons::kBusinessIcon);
}

// Shows a notification informing the user that video recording was stopped. If
// |for_hdcp| is true, then this was due to a content-enforced protection,
// otherwise it was due to DLP which is admin enforced.
void ShowVideoRecordingStoppedNotification(bool for_hdcp) {
  ShowNotification(
      kScreenCaptureStoppedNotificationId,
      for_hdcp ? IDS_ASH_SCREEN_CAPTURE_HDCP_STOPPED_TITLE
               : IDS_ASH_SCREEN_CAPTURE_DLP_STOPPED_TITLE,
      for_hdcp ? IDS_ASH_SCREEN_CAPTURE_HDCP_BLOCKED_MESSAGE
               : IDS_ASH_SCREEN_CAPTURE_DLP_STOPPED_MESSAGE,
      /*optional_fields=*/{}, /*delegate=*/nullptr,
      message_center::SystemNotificationWarningLevel::CRITICAL_WARNING,
      for_hdcp ? kCaptureModeIcon : vector_icons::kBusinessIcon);
}

// Copies the bitmap representation of the given |image| to the clipboard.
void CopyImageToClipboard(const gfx::Image& image) {
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteImage(image.AsBitmap());
}

// Emits UMA samples for the |status| of the recording as reported by the
// recording service.
void EmitServiceRecordingStatus(recording::mojom::RecordingStatus status) {
  using recording::mojom::RecordingStatus;
  switch (status) {
    case RecordingStatus::kSuccess:
      // We emit no samples for success status, as in this case the recording
      // was ended normally by the client, and the end reason for that is
      // emitted else where.
      break;
    case RecordingStatus::kServiceClosing:
      RecordEndRecordingReason(EndRecordingReason::kServiceClosing);
      break;
    case RecordingStatus::kVizVideoCapturerDisconnected:
      RecordEndRecordingReason(
          EndRecordingReason::kVizVideoCaptureDisconnected);
      break;
    case RecordingStatus::kAudioEncoderInitializationFailure:
      RecordEndRecordingReason(
          EndRecordingReason::kAudioEncoderInitializationFailure);
      break;
    case RecordingStatus::kVideoEncoderInitializationFailure:
      RecordEndRecordingReason(
          EndRecordingReason::kVideoEncoderInitializationFailure);
      break;
    case RecordingStatus::kAudioEncodingError:
      RecordEndRecordingReason(EndRecordingReason::kAudioEncodingError);
      break;
    case RecordingStatus::kVideoEncodingError:
      RecordEndRecordingReason(EndRecordingReason::kVideoEncodingError);
      break;
    case RecordingStatus::kIoError:
      RecordEndRecordingReason(EndRecordingReason::kFileIoError);
      break;
    case RecordingStatus::kLowDiskSpace:
      RecordEndRecordingReason(EndRecordingReason::kLowDiskSpace);
      break;
  }
}

}  // namespace

CaptureModeController::CaptureModeController(
    std::unique_ptr<CaptureModeDelegate> delegate)
    : delegate_(std::move(delegate)),
      blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          // A task priority of BEST_EFFORT is good enough for this runner,
          // since it's used for blocking file IO such as saving the screenshots
          // or the successive webm video chunks received from the recording
          // service.
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      recording_service_client_receiver_(this),
      num_consecutive_screenshots_scheduler_(
          FROM_HERE,
          kConsecutiveScreenshotThreshold,
          this,
          &CaptureModeController::RecordAndResetConsecutiveScreenshots) {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;

  // Schedule recording of the number of screenshots taken per day.
  num_screenshots_taken_in_last_day_scheduler_.Start(
      FROM_HERE, base::TimeDelta::FromDays(1),
      base::BindRepeating(
          &CaptureModeController::RecordAndResetScreenshotsTakenInLastDay,
          weak_ptr_factory_.GetWeakPtr()));

  // Schedule recording of the number of screenshots taken per week.
  num_screenshots_taken_in_last_week_scheduler_.Start(
      FROM_HERE, base::TimeDelta::FromDays(7),
      base::BindRepeating(
          &CaptureModeController::RecordAndResetScreenshotsTakenInLastWeek,
          weak_ptr_factory_.GetWeakPtr()));

  DCHECK(!MessageViewFactory::HasCustomNotificationViewFactory(
      kScreenShotNotificationType));
  DCHECK(!MessageViewFactory::HasCustomNotificationViewFactory(
      kScreenRecordingNotificationType));
  MessageViewFactory::SetCustomNotificationViewFactory(
      kScreenShotNotificationType,
      base::BindRepeating(&CaptureModeNotificationView::CreateForImage));
  MessageViewFactory::SetCustomNotificationViewFactory(
      kScreenRecordingNotificationType,
      base::BindRepeating(&CaptureModeNotificationView::CreateForVideo));

  Shell::Get()->session_controller()->AddObserver(this);
  chromeos::PowerManagerClient::Get()->AddObserver(this);
}

CaptureModeController::~CaptureModeController() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
  // Remove the custom notification view factories.
  MessageViewFactory::ClearCustomNotificationViewFactory(
      kScreenShotNotificationType);
  MessageViewFactory::ClearCustomNotificationViewFactory(
      kScreenRecordingNotificationType);

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
CaptureModeController* CaptureModeController::Get() {
  DCHECK(g_instance);
  return g_instance;
}

bool CaptureModeController::IsActive() const {
  return capture_mode_session_ && !capture_mode_session_->is_shutting_down();
}

void CaptureModeController::SetSource(CaptureModeSource source) {
  if (source == source_)
    return;

  source_ = source;
  if (capture_mode_session_)
    capture_mode_session_->OnCaptureSourceChanged(source_);
}

void CaptureModeController::SetType(CaptureModeType type) {
  if (is_recording_in_progress_ && type == CaptureModeType::kVideo) {
    // Overwrite video capture types to image, as we can't have more than one
    // recording at a time.
    type = CaptureModeType::kImage;
  }

  if (type == type_)
    return;

  type_ = type;
  if (capture_mode_session_)
    capture_mode_session_->OnCaptureTypeChanged(type_);
}

void CaptureModeController::EnableAudioRecording(bool enable_audio_recording) {
  if (enable_audio_recording_ == enable_audio_recording)
    return;

  enable_audio_recording_ = enable_audio_recording;
  DCHECK(capture_mode_session_);
  capture_mode_session_->OnMicrophoneChanged(enable_audio_recording_);
}

void CaptureModeController::Start(CaptureModeEntryType entry_type) {
  if (capture_mode_session_)
    return;

  if (!delegate_->IsCaptureAllowedByPolicy()) {
    ShowDisabledNotification(CaptureAllowance::kDisallowedByPolicy);
    return;
  }
  if (delegate_->IsCaptureModeInitRestrictedByDlp()) {
    ShowDisabledNotification(CaptureAllowance::kDisallowedByDlp);
    return;
  }

  // Before we start the session, if video recording is in progress, we need to
  // set the current type to image, as we can't have more than one recording at
  // a time. The video toggle button in the capture mode bar will be disabled.
  if (is_recording_in_progress_)
    SetType(CaptureModeType::kImage);

  RecordCaptureModeEntryType(entry_type);
  // Reset the user capture region if enough time has passed as it can be
  // annoying to still have the old capture region from the previous session
  // long time ago.
  if (!user_capture_region_.IsEmpty() &&
      base::TimeTicks::Now() - last_capture_region_update_time_ >
          kResetCaptureRegionDuration) {
    SetUserCaptureRegion(gfx::Rect(), /*by_user=*/false);
  }

  delegate_->OnSessionStateChanged(/*started=*/true);

  capture_mode_session_ = std::make_unique<CaptureModeSession>(this);
  capture_mode_session_->Initialize();
}

void CaptureModeController::Stop() {
  DCHECK(IsActive());
  capture_mode_session_->ReportSessionHistograms();
  capture_mode_session_->Shutdown();
  capture_mode_session_.reset();

  delegate_->OnSessionStateChanged(/*started=*/false);
}

void CaptureModeController::SetUserCaptureRegion(const gfx::Rect& region,
                                                 bool by_user) {
  user_capture_region_ = region;
  if (!user_capture_region_.IsEmpty() && by_user)
    last_capture_region_update_time_ = base::TimeTicks::Now();
}

void CaptureModeController::CaptureScreenshotsOfAllDisplays() {
  if (!delegate_->IsCaptureAllowedByPolicy()) {
    ShowDisabledNotification(CaptureAllowance::kDisallowedByPolicy);
    return;
  }
  if (delegate_->IsCaptureModeInitRestrictedByDlp()) {
    ShowDisabledNotification(CaptureAllowance::kDisallowedByDlp);
    return;
  }
  // Get a vector of RootWindowControllers with primary root window at first.
  const std::vector<RootWindowController*> controllers =
      RootWindowController::root_window_controllers();
  // Capture screenshot for each individual display.
  int display_index = 1;
  for (RootWindowController* controller : controllers) {
    // TODO(shidi): Check with UX what notification should show if
    // some (but not all) of the displays have restricted content and
    // whether we should localize the display name.
    const CaptureParams capture_params{controller->GetRootWindow(),
                                       controller->GetRootWindow()->bounds()};
    CaptureImage(capture_params, controllers.size() == 1
                                     ? BuildImagePath()
                                     : BuildImagePathForDisplay(display_index));
    ++display_index;
  }

  // Since this doesn't create a capture mode session, log metrics here.
  RecordCaptureModeEntryType(CaptureModeEntryType::kCaptureAllDisplays);
  RecordCaptureModeConfiguration(CaptureModeType::kImage,
                                 CaptureModeSource::kFullscreen);
}

void CaptureModeController::PerformCapture() {
  DCHECK(IsActive());
  const absl::optional<CaptureParams> capture_params = GetCaptureParams();
  if (!capture_params)
    return;

  const CaptureAllowance allowance =
      IsCaptureAllowedByEnterprisePolicies(*capture_params);
  if (allowance != CaptureAllowance::kAllowed) {
    ShowDisabledNotification(allowance);
    Stop();
    return;
  }

  if (type_ == CaptureModeType::kImage) {
    CaptureImage(*capture_params, BuildImagePath());
  } else {
    // HDCP affects only video recording.
    if (ShouldBlockRecordingForContentProtection(capture_params->window)) {
      ShowDisabledNotification(CaptureAllowance::kDisallowedByHdcp);
      Stop();
      return;
    }

    CaptureVideo(*capture_params);
  }
}

void CaptureModeController::EndVideoRecording(EndRecordingReason reason) {
  RecordEndRecordingReason(reason);
  recording_service_remote_->StopRecording();
  TerminateRecordingUiElements();
}

void CaptureModeController::SetWindowProtectionMask(aura::Window* window,
                                                    uint32_t protection_mask) {
  if (protection_mask == display::CONTENT_PROTECTION_METHOD_NONE)
    protected_windows_.erase(window);
  else
    protected_windows_[window] = protection_mask;

  RefreshContentProtection();
}

void CaptureModeController::RefreshContentProtection() {
  if (!is_recording_in_progress_)
    return;

  DCHECK(video_recording_watcher_);
  if (ShouldBlockRecordingForContentProtection(
          video_recording_watcher_->window_being_recorded())) {
    // HDCP violation is also considered a failure, and we're going to terminate
    // the service immediately so as not to record any further frames.
    RecordEndRecordingReason(EndRecordingReason::kHdcpInterruption);
    FinalizeRecording(/*success=*/false, gfx::ImageSkia());
    ShowVideoRecordingStoppedNotification(/*for_hdcp=*/true);
  }
}

void CaptureModeController::OnRecordingEnded(
    recording::mojom::RecordingStatus status,
    const gfx::ImageSkia& thumbnail) {
  low_disk_space_threshold_reached_ =
      status == recording::mojom::RecordingStatus::kLowDiskSpace;
  EmitServiceRecordingStatus(status);
  FinalizeRecording(status == recording::mojom::RecordingStatus::kSuccess,
                    thumbnail);
}

void CaptureModeController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  EndSessionOrRecording(EndRecordingReason::kActiveUserChange);

  // Remove the previous notification when switching to another user.
  auto* message_center = message_center::MessageCenter::Get();
  message_center->RemoveNotification(kScreenCaptureNotificationId,
                                     /*by_user=*/false);
}

void CaptureModeController::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (Shell::Get()->session_controller()->IsUserSessionBlocked())
    EndSessionOrRecording(EndRecordingReason::kSessionBlocked);
}

void CaptureModeController::OnChromeTerminating() {
  EndSessionOrRecording(EndRecordingReason::kShuttingDown);
}

void CaptureModeController::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  EndSessionOrRecording(EndRecordingReason::kImminentSuspend);
}

void CaptureModeController::StartVideoRecordingImmediatelyForTesting() {
  DCHECK(IsActive());
  DCHECK_EQ(type_, CaptureModeType::kVideo);
  OnVideoRecordCountDownFinished();
}

void CaptureModeController::PushNewRootSizeToRecordingService(
    const gfx::Size& root_size,
    float device_scale_factor) {
  DCHECK(is_recording_in_progress_);
  DCHECK(video_recording_watcher_);
  DCHECK(recording_service_remote_);

  recording_service_remote_->OnFrameSinkSizeChanged(root_size,
                                                    device_scale_factor);
}

void CaptureModeController::OnRecordedWindowChangingRoot(
    aura::Window* window,
    aura::Window* new_root) {
  DCHECK(is_recording_in_progress_);
  DCHECK(video_recording_watcher_);
  DCHECK_EQ(window, video_recording_watcher_->window_being_recorded());
  DCHECK(recording_service_remote_);
  DCHECK(new_root);

  // When a window being recorded changes displays either due to a display
  // getting disconnected, or moved by the user, the stop-recording button
  // should follow that window to that display.
  capture_mode_util::SetStopRecordingButtonVisibility(window->GetRootWindow(),
                                                      false);
  capture_mode_util::SetStopRecordingButtonVisibility(new_root, true);

  recording_service_remote_->OnRecordedWindowChangingRoot(
      new_root->GetFrameSinkId(), new_root->GetBoundsInRootWindow().size(),
      new_root->GetHost()->device_scale_factor());
}

void CaptureModeController::OnRecordedWindowSizeChanged(
    const gfx::Size& new_size) {
  DCHECK(is_recording_in_progress_);
  DCHECK(video_recording_watcher_);
  DCHECK(recording_service_remote_);

  recording_service_remote_->OnRecordedWindowSizeChanged(new_size);
}

bool CaptureModeController::ShouldBlockRecordingForContentProtection(
    aura::Window* window_being_recorded) const {
  DCHECK(window_being_recorded);

  // The protected window can be a descendant of the window being recorded, for
  // examples:
  //   - When recording a fullscreen or partial region of it, the
  //     |window_being_recorded| in this case is the root window, and a
  //     protected window on this root will be a descendant.
  //   - When recording a browser window showing a page with protected content,
  //     the |window_being_recorded| in this case is the BrowserFrame, while the
  //     protected window will be the RenderWidgetHostViewAura, which is also a
  //     descendant.
  for (const auto& iter : protected_windows_) {
    if (window_being_recorded->Contains(iter.first))
      return true;
  }

  return false;
}

void CaptureModeController::EndSessionOrRecording(EndRecordingReason reason) {
  if (IsActive()) {
    // Suspend or user session changes can happen while the capture mode session
    // is active or after the three-second countdown had started but not
    // finished yet.
    Stop();
    return;
  }

  if (!is_recording_in_progress_)
    return;

  if (reason == EndRecordingReason::kImminentSuspend) {
    // If suspend happens while recording is in progress, we consider this a
    // failure, and cut the recording immediately. The recording service will
    // flush any remaining buffered chunks in the muxer before it terminates.
    RecordEndRecordingReason(EndRecordingReason::kImminentSuspend);
    FinalizeRecording(/*success=*/false, gfx::ImageSkia());
    return;
  }

  EndVideoRecording(reason);
}

absl::optional<CaptureModeController::CaptureParams>
CaptureModeController::GetCaptureParams() const {
  DCHECK(IsActive());

  aura::Window* window = nullptr;
  gfx::Rect bounds;
  switch (source_) {
    case CaptureModeSource::kFullscreen:
      window = capture_mode_session_->current_root();
      DCHECK(window);
      DCHECK(window->IsRootWindow());
      bounds = window->bounds();
      break;

    case CaptureModeSource::kWindow:
      window = capture_mode_session_->GetSelectedWindow();
      if (!window) {
        // TODO(afakhry): Consider showing a toast or a notification that no
        // window was selected.
        return absl::nullopt;
      }
      // window->bounds() are in root coordinates, but we want to get the
      // capture area in |window|'s coordinates.
      bounds = gfx::Rect(window->bounds().size());
      break;

    case CaptureModeSource::kRegion:
      window = capture_mode_session_->current_root();
      DCHECK(window);
      DCHECK(window->IsRootWindow());
      if (user_capture_region_.IsEmpty()) {
        // TODO(afakhry): Consider showing a toast or a notification that no
        // region was selected.
        return absl::nullopt;
      }
      // TODO(afakhry): Consider any special handling of display scale changes
      // while video recording is in progress.
      bounds = user_capture_region_;
      break;
  }

  DCHECK(window);

  return CaptureParams{window, bounds};
}

void CaptureModeController::LaunchRecordingServiceAndStartRecording(
    const CaptureParams& capture_params,
    mojo::PendingReceiver<viz::mojom::FrameSinkVideoCaptureOverlay>
        cursor_overlay) {
  DCHECK(!recording_service_remote_.is_bound())
      << "Should not launch a new recording service while one is already "
         "running.";

  recording_service_remote_.reset();
  recording_service_client_receiver_.reset();

  recording_service_remote_ = delegate_->LaunchRecordingService();
  recording_service_remote_.set_disconnect_handler(
      base::BindOnce(&CaptureModeController::OnRecordingServiceDisconnected,
                     base::Unretained(this)));

  // Prepare the pending remotes of the client, the video capturer, and the
  // audio stream factory.
  mojo::PendingRemote<recording::mojom::RecordingServiceClient> client =
      recording_service_client_receiver_.BindNewPipeAndPassRemote();
  mojo::Remote<viz::mojom::FrameSinkVideoCapturer> video_capturer_remote;
  aura::Env::GetInstance()
      ->context_factory()
      ->GetHostFrameSinkManager()
      ->CreateVideoCapturer(video_capturer_remote.BindNewPipeAndPassReceiver());

  // The overlay is to be rendered on top of the video frames.
  constexpr int kStackingIndex = 1;
  video_capturer_remote->CreateOverlay(kStackingIndex,
                                       std::move(cursor_overlay));

  // We bind the audio stream factory only if audio recording is enabled. This
  // is ok since the |audio_stream_factory| parameter in the recording service
  // APIs is optional, and can be not bound.
  mojo::PendingRemote<media::mojom::AudioStreamFactory> audio_stream_factory;
  if (enable_audio_recording_) {
    delegate_->BindAudioStreamFactory(
        audio_stream_factory.InitWithNewPipeAndPassReceiver());
  }

  auto* root_window = capture_params.window->GetRootWindow();
  const auto frame_sink_id = root_window->GetFrameSinkId();
  DCHECK(frame_sink_id.is_valid());
  const float device_scale_factor =
      root_window->GetHost()->device_scale_factor();
  const gfx::Size frame_sink_size_dip = root_window->bounds().size();

  const auto bounds = capture_params.bounds;
  switch (source_) {
    case CaptureModeSource::kFullscreen:
      recording_service_remote_->RecordFullscreen(
          std::move(client), video_capturer_remote.Unbind(),
          std::move(audio_stream_factory), current_video_file_path_,
          frame_sink_id, frame_sink_size_dip, device_scale_factor);
      break;

    case CaptureModeSource::kWindow:
      // Non-root window are not capturable by the |FrameSinkVideoCapturer|
      // unless its layer tree is identified by a |viz::SubtreeCaptureId|.
      // The |VideoRecordingWatcher| that we create while recording is in
      // progress creates a request to mark that window as capturable.
      // See https://crbug.com/1143930 for more details.
      DCHECK(!capture_params.window->IsRootWindow());
      DCHECK(capture_params.window->subtree_capture_id().is_valid());

      recording_service_remote_->RecordWindow(
          std::move(client), video_capturer_remote.Unbind(),
          std::move(audio_stream_factory), current_video_file_path_,
          frame_sink_id, frame_sink_size_dip, device_scale_factor,
          capture_params.window->subtree_capture_id(), bounds.size());
      break;

    case CaptureModeSource::kRegion:
      recording_service_remote_->RecordRegion(
          std::move(client), video_capturer_remote.Unbind(),
          std::move(audio_stream_factory), current_video_file_path_,
          frame_sink_id, frame_sink_size_dip, device_scale_factor, bounds);
      break;
  }
}

void CaptureModeController::OnRecordingServiceDisconnected() {
  // TODO(afakhry): Consider what to do if the service crashes during an ongoing
  // video recording. Do we try to resume recording, or notify with failure?
  // For now, just end the recording.
  // Note that the service could disconnect between the time we ask it to
  // StopRecording(), and it calling us back with OnRecordingEnded(), so we call
  // FinalizeRecording() in all cases.
  RecordEndRecordingReason(EndRecordingReason::kRecordingServiceDisconnected);
  FinalizeRecording(/*success=*/false, gfx::ImageSkia());
}

CaptureAllowance CaptureModeController::IsCaptureAllowedByEnterprisePolicies(
    const CaptureParams& capture_params) const {
  if (!delegate_->IsCaptureAllowedByPolicy()) {
    return CaptureAllowance::kDisallowedByPolicy;
  }
  if (!delegate_->IsCaptureAllowedByDlp(
          capture_params.window, capture_params.bounds,
          /*for_video=*/type_ == CaptureModeType::kVideo)) {
    return CaptureAllowance::kDisallowedByDlp;
  }

  return CaptureAllowance::kAllowed;
}

void CaptureModeController::FinalizeRecording(bool success,
                                              const gfx::ImageSkia& thumbnail) {
  delegate_->StopObservingRestrictedContent();

  // If |success| is false, then recording has been force-terminated due to a
  // failure on the service side, or a disconnection to it. We need to terminate
  // the recording-related UI elements.
  if (!success) {
    // TODO(afakhry): Show user a failure message.
    TerminateRecordingUiElements();
  }

  // Resetting the service remote would terminate its process.
  recording_service_remote_.reset();
  delegate_->OnServiceRemoteReset();
  recording_service_client_receiver_.reset();

  OnVideoFileSaved(thumbnail, success);
}

void CaptureModeController::TerminateRecordingUiElements() {
  if (!is_recording_in_progress_)
    return;

  is_recording_in_progress_ = false;
  capture_mode_util::SetStopRecordingButtonVisibility(
      video_recording_watcher_->window_being_recorded()->GetRootWindow(),
      false);
  video_recording_watcher_.reset();

  capture_mode_util::TriggerAccessibilityAlert(
      IDS_ASH_SCREEN_CAPTURE_ALERT_RECORDING_STOPPED);
}

void CaptureModeController::CaptureImage(const CaptureParams& capture_params,
                                         const base::FilePath& path) {
  // Note that |type_| may not necessarily be |kImage| here, since this may be
  // called to take an instant fullscreen screenshot for the keyboard shortcut,
  // which doesn't go through the capture mode UI, and doesn't change |type_|.
  DCHECK_EQ(CaptureAllowance::kAllowed,
            IsCaptureAllowedByEnterprisePolicies(capture_params));

  // Stop the capture session now, so as not to take a screenshot of the capture
  // bar.
  if (IsActive())
    Stop();

  DCHECK(!capture_params.bounds.IsEmpty());

  auto* cursor_manager = Shell::Get()->cursor_manager();
  bool was_cursor_originally_blocked = cursor_manager->IsCursorLocked();
  if (!was_cursor_originally_blocked) {
    cursor_manager->HideCursor();
    cursor_manager->LockCursor();
  }

  ui::GrabWindowSnapshotAsyncPNG(
      capture_params.window, capture_params.bounds,
      base::BindOnce(&CaptureModeController::OnImageCaptured,
                     weak_ptr_factory_.GetWeakPtr(), path,
                     was_cursor_originally_blocked));

  ++num_screenshots_taken_in_last_day_;
  ++num_screenshots_taken_in_last_week_;

  ++num_consecutive_screenshots_;
  num_consecutive_screenshots_scheduler_.Reset();

  capture_mode_util::TriggerAccessibilityAlert(
      IDS_ASH_SCREEN_CAPTURE_ALERT_SCREENSHOT_CAPTURED);
}

void CaptureModeController::CaptureVideo(const CaptureParams& capture_params) {
  DCHECK_EQ(CaptureModeType::kVideo, type_);
  DCHECK_EQ(CaptureAllowance::kAllowed,
            IsCaptureAllowedByEnterprisePolicies(capture_params));

  if (skip_count_down_ui_) {
    OnVideoRecordCountDownFinished();
    return;
  }

  capture_mode_session_->StartCountDown(
      base::BindOnce(&CaptureModeController::OnVideoRecordCountDownFinished,
                     weak_ptr_factory_.GetWeakPtr()));

  capture_mode_util::TriggerAccessibilityAlert(
      IDS_ASH_SCREEN_CAPTURE_ALERT_RECORDING_STARTING);
}

void CaptureModeController::OnImageCaptured(
    const base::FilePath& path,
    bool was_cursor_originally_blocked,
    scoped_refptr<base::RefCountedMemory> png_bytes) {
  if (!was_cursor_originally_blocked) {
    auto* shell = Shell::Get();
    auto* cursor_manager = shell->cursor_manager();
    if (!shell->tablet_mode_controller()->InTabletMode())
      cursor_manager->ShowCursor();
    cursor_manager->UnlockCursor();
  }

  if (!png_bytes || !png_bytes->size()) {
    LOG(ERROR) << "Failed to capture image.";
    ShowFailureNotification();
    return;
  }

  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&SaveFile, png_bytes, path),
      base::BindOnce(&CaptureModeController::OnImageFileSaved,
                     weak_ptr_factory_.GetWeakPtr(), png_bytes, path));
}

void CaptureModeController::OnImageFileSaved(
    scoped_refptr<base::RefCountedMemory> png_bytes,
    const base::FilePath& path,
    bool success) {
  if (!success) {
    ShowFailureNotification();
    return;
  }

  if (!on_file_saved_callback_.is_null())
    std::move(on_file_saved_callback_).Run(path);

  DCHECK(png_bytes && png_bytes->size());
  const auto image = gfx::Image::CreateFrom1xPNGBytes(png_bytes);
  CopyImageToClipboard(image);
  ShowPreviewNotification(path, image, CaptureModeType::kImage);

  HoldingSpaceClient* client = HoldingSpaceController::Get()->client();
  if (client)  // May be `nullptr` in tests.
    client->AddScreenshot(path);
}

void CaptureModeController::OnVideoFileSaved(
    const gfx::ImageSkia& video_thumbnail,
    bool success) {
  DCHECK(base::CurrentUIThread::IsSet());

  if (!success) {
    ShowFailureNotification();
  } else {
    ShowPreviewNotification(current_video_file_path_,
                            gfx::Image(video_thumbnail),
                            CaptureModeType::kVideo);
    DCHECK(!recording_start_time_.is_null());
    RecordCaptureModeRecordTime(
        (base::TimeTicks::Now() - recording_start_time_).InSeconds());

    HoldingSpaceClient* client = HoldingSpaceController::Get()->client();
    if (client)  // May be `nullptr` in tests.
      client->AddScreenRecording(current_video_file_path_);
  }

  if (!on_file_saved_callback_.is_null())
    std::move(on_file_saved_callback_).Run(current_video_file_path_);

  low_disk_space_threshold_reached_ = false;
  recording_start_time_ = base::TimeTicks();
  current_video_file_path_.clear();
}

void CaptureModeController::ShowPreviewNotification(
    const base::FilePath& screen_capture_path,
    const gfx::Image& preview_image,
    const CaptureModeType type) {
  const bool for_video = type == CaptureModeType::kVideo;
  const int title_id = for_video ? IDS_ASH_SCREEN_CAPTURE_RECORDING_TITLE
                                 : IDS_ASH_SCREEN_CAPTURE_SCREENSHOT_TITLE;
  const int message_id = for_video && low_disk_space_threshold_reached_
                             ? IDS_ASH_SCREEN_CAPTURE_LOW_DISK_SPACE_MESSAGE
                             : IDS_ASH_SCREEN_CAPTURE_MESSAGE;

  message_center::RichNotificationData optional_fields;
  message_center::ButtonInfo edit_button(
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_BUTTON_EDIT));
  if (!for_video && !Shell::Get()->session_controller()->IsUserSessionBlocked())
    optional_fields.buttons.push_back(edit_button);
  message_center::ButtonInfo delete_button(
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_BUTTON_DELETE));
  optional_fields.buttons.push_back(delete_button);

  optional_fields.image = preview_image;

  ShowNotification(
      kScreenCaptureNotificationId, title_id, message_id, optional_fields,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&CaptureModeController::HandleNotificationClicked,
                              weak_ptr_factory_.GetWeakPtr(),
                              screen_capture_path, type)),
      message_center::SystemNotificationWarningLevel::NORMAL, kCaptureModeIcon,
      for_video);
}

void CaptureModeController::HandleNotificationClicked(
    const base::FilePath& screen_capture_path,
    const CaptureModeType type,
    absl::optional<int> button_index) {
  if (!button_index.has_value()) {
    // Show the item in the folder.
    delegate_->ShowScreenCaptureItemInFolder(screen_capture_path);
    RecordScreenshotNotificationQuickAction(CaptureQuickAction::kFiles);
  } else {
    const int button_index_value = button_index.value();
    if (type == CaptureModeType::kVideo) {
      DCHECK_EQ(button_index_value,
                VideoNotificationButtonIndex::BUTTON_DELETE_VIDEO);
      DeleteFileAsync(blocking_task_runner_, screen_capture_path);
    } else {
      DCHECK_EQ(type, CaptureModeType::kImage);
      switch (button_index_value) {
        case ScreenshotNotificationButtonIndex::BUTTON_EDIT:
          delegate_->OpenScreenshotInImageEditor(screen_capture_path);
          RecordScreenshotNotificationQuickAction(
              CaptureQuickAction::kBacklight);
          break;
        case ScreenshotNotificationButtonIndex::BUTTON_DELETE:
          DeleteFileAsync(blocking_task_runner_, screen_capture_path);
          RecordScreenshotNotificationQuickAction(CaptureQuickAction::kDelete);
          break;
        default:
          NOTREACHED();
          break;
      }
    }
  }

  // This has to be done at the end to avoid a use-after-free crash, since
  // removing the notification will delete its delegate, which owns the callback
  // to this function. The callback's state owns any passed-by-ref arguments,
  // such as |screen_capture_path| which we use in this function.
  message_center::MessageCenter::Get()->RemoveNotification(
      kScreenCaptureNotificationId, /*by_user=*/false);
}

base::FilePath CaptureModeController::BuildImagePath() const {
  return BuildPathNoExtension(kScreenshotFileNameFmtStr, base::Time::Now())
      .AddExtension("png");
}

base::FilePath CaptureModeController::BuildVideoPath() const {
  return BuildPathNoExtension(kVideoFileNameFmtStr, base::Time::Now())
      .AddExtension("webm");
}

base::FilePath CaptureModeController::BuildImagePathForDisplay(
    int display_index) const {
  auto path_str =
      BuildPathNoExtension(kScreenshotFileNameFmtStr, base::Time::Now())
          .value();
  auto full_path = base::StringPrintf("%s - Display %d.png", path_str.c_str(),
                                      display_index);
  return base::FilePath(full_path);
}

base::FilePath CaptureModeController::BuildPathNoExtension(
    const char* const format_string,
    base::Time timestamp) const {
  const base::FilePath path = delegate_->GetScreenCaptureDir();
  base::Time::Exploded exploded_time;
  timestamp.LocalExplode(&exploded_time);

  return path.AppendASCII(base::StringPrintf(
      format_string, GetDateStr(exploded_time).c_str(),
      GetTimeStr(exploded_time, delegate_->Uses24HourFormat()).c_str()));
}

void CaptureModeController::RecordAndResetScreenshotsTakenInLastDay() {
  RecordNumberOfScreenshotsTakenInLastDay(num_screenshots_taken_in_last_day_);
  num_screenshots_taken_in_last_day_ = 0;
}

void CaptureModeController::RecordAndResetScreenshotsTakenInLastWeek() {
  RecordNumberOfScreenshotsTakenInLastWeek(num_screenshots_taken_in_last_week_);
  num_screenshots_taken_in_last_week_ = 0;
}

void CaptureModeController::RecordAndResetConsecutiveScreenshots() {
  RecordNumberOfConsecutiveScreenshots(num_consecutive_screenshots_);
  num_consecutive_screenshots_ = 0;
}

void CaptureModeController::OnVideoRecordCountDownFinished() {
  // If this event is dispatched after the capture session was cancelled or
  // destroyed, this should be a no-op.
  if (!IsActive())
    return;

  // Do not trigger an alert when exiting the session, since we end the session
  // to start recording.
  capture_mode_session_->set_a11y_alert_on_session_exit(false);

  const absl::optional<CaptureParams> capture_params = GetCaptureParams();

  // Acquire the session's layer in order to potentially reuse it for painting
  // a highlight around the region being recorded.
  std::unique_ptr<ui::Layer> session_layer =
      capture_mode_session_->ReleaseLayer();
  session_layer->set_delegate(nullptr);

  // Stop the capture session now, so the bar doesn't show up in the captured
  // video.
  Stop();

  if (!capture_params)
    return;

  // During the 3-second count down, screen content might have changed such that
  // admin-restricted or HDCP content became present. We must check again.
  const CaptureAllowance allowance =
      IsCaptureAllowedByEnterprisePolicies(*capture_params);
  if (allowance != CaptureAllowance::kAllowed) {
    ShowDisabledNotification(allowance);
    return;
  }

  if (ShouldBlockRecordingForContentProtection(capture_params->window)) {
    ShowDisabledNotification(CaptureAllowance::kDisallowedByHdcp);
    return;
  }

  is_recording_in_progress_ = true;
  mojo::PendingRemote<viz::mojom::FrameSinkVideoCaptureOverlay>
      cursor_capture_overlay;
  auto cursor_overlay_receiver =
      cursor_capture_overlay.InitWithNewPipeAndPassReceiver();
  video_recording_watcher_ = std::make_unique<VideoRecordingWatcher>(
      this, capture_params->window, std::move(cursor_capture_overlay));

  // We only paint the recorded area highlight for window and region captures.
  if (source_ != CaptureModeSource::kFullscreen)
    video_recording_watcher_->Reset(std::move(session_layer));

  DCHECK(current_video_file_path_.empty());
  recording_start_time_ = base::TimeTicks::Now();
  current_video_file_path_ = BuildVideoPath();

  LaunchRecordingServiceAndStartRecording(*capture_params,
                                          std::move(cursor_overlay_receiver));

  delegate_->StartObservingRestrictedContent(
      capture_params->window, capture_params->bounds,
      base::BindOnce(&CaptureModeController::InterruptVideoRecording,
                     weak_ptr_factory_.GetWeakPtr()));

  capture_mode_util::SetStopRecordingButtonVisibility(
      capture_params->window->GetRootWindow(), true);
}

void CaptureModeController::InterruptVideoRecording() {
  ShowVideoRecordingStoppedNotification(/*for_hdcp=*/false);
  EndVideoRecording(EndRecordingReason::kDlpInterruption);
}

}  // namespace ash
