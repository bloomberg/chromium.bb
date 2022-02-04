// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_ui_controller.h"

#include "ash/accessibility/caption_bubble_context_ash.h"
#include "ash/accessibility/magnifier/partial_magnifier_controller.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/projector/projector_annotation_tray.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/projector/projector_metrics.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/projector/projector_annotator_controller.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "base/callback_helpers.h"
#include "components/live_caption/views/caption_bubble.h"
#include "components/live_caption/views/caption_bubble_model.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {
namespace {

// A unique id to identify system notifications coming from this file.
constexpr char kProjectorNotifierId[] = "ash.projector_ui_controller";

// A unique id for system notifications reporting a failure.
constexpr char kProjectorErrorNotificationId[] = "projector_error_notification";

void EnableLaserPointer(bool enabled) {
  auto* laser_pointer_controller = Shell::Get()->laser_pointer_controller();
  DCHECK(laser_pointer_controller);
  Shell::Get()->laser_pointer_controller()->SetEnabled(enabled);
}

void ToggleAnnotator() {
  auto* capture_mode_controller = CaptureModeController::Get();
  // TODO(b/200292852): This check should not be necessary, but because
  // several Projector unit tests that rely on mocking and don't test the real
  // code path, we can end up calling |ToggleRecordingOverlayEnabled()|
  // without ever starting a Projector recording session.
  // |CaptureModeController| asserts all invariants via DCHECKs, and those
  // tests would crash. Remove any unnecessary mocks and test the real thing
  // if possible.
  if (capture_mode_controller->is_recording_in_progress())
    capture_mode_controller->ToggleRecordingOverlayEnabled();
  return;
}

// Shows a Projector-related notification to the user with the given parameters.
void ShowNotification(
    const std::string& notification_id,
    int title_id,
    int message_id,
    message_center::SystemNotificationWarningLevel warning_level =
        message_center::SystemNotificationWarningLevel::NORMAL,
    const message_center::RichNotificationData& optional_fields = {},
    scoped_refptr<message_center::NotificationDelegate> delegate = nullptr,
    const gfx::VectorIcon& notification_icon = kPaletteTrayIconProjectorIcon) {
  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
          l10n_util::GetStringUTF16(title_id),
          l10n_util::GetStringUTF16(message_id),
          l10n_util::GetStringUTF16(IDS_ASH_PROJECTOR_DISPLAY_SOURCE), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kProjectorNotifierId),
          optional_fields, delegate, notification_icon, warning_level);

  // Remove the previous notification before showing the new one if there are
  // any.
  auto* message_center = message_center::MessageCenter::Get();
  message_center->RemoveNotification(notification_id,
                                     /*by_user=*/false);
  message_center->AddNotification(std::move(notification));
}

}  // namespace

// static
void ProjectorUiController::ShowFailureNotification(int message_id) {
  ShowNotification(
      kProjectorErrorNotificationId, IDS_ASH_PROJECTOR_FAILURE_TITLE,
      message_id,
      message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);
}

ProjectorUiController::ProjectorUiController(
    ProjectorControllerImpl* projector_controller) {
  auto* laser_pointer_controller = Shell::Get()->laser_pointer_controller();
  DCHECK(laser_pointer_controller);
  laser_pointer_controller_observation_.Observe(laser_pointer_controller);

  projector_session_observation_.Observe(
      projector_controller->projector_session());
}

ProjectorUiController::~ProjectorUiController() = default;

void ProjectorUiController::ShowToolbar() {
  // Show the tray icon
  auto* projector_annotation_tray = Shell::GetPrimaryRootWindowController()
                                        ->GetStatusAreaWidget()
                                        ->projector_annotation_tray();
  DCHECK(projector_annotation_tray);
  projector_annotation_tray->SetVisiblePreferred(true);
}

void ProjectorUiController::CloseToolbar() {
  // Hide the tray icon
  auto* projector_annotation_tray = Shell::GetPrimaryRootWindowController()
                                        ->GetStatusAreaWidget()
                                        ->projector_annotation_tray();
  DCHECK(projector_annotation_tray);
  projector_annotation_tray->SetVisiblePreferred(false);
}

void ProjectorUiController::OnLaserPointerPressed() {
  EnableLaserPointer(!IsLaserPointerEnabled());
  RecordToolbarMetrics(ProjectorToolbar::kLaserPointer);
}

void ProjectorUiController::OnMarkerPressed() {
  EnableLaserPointer(false);
  ToggleAnnotator();
  annotator_enabled_ = !annotator_enabled_;
  RecordToolbarMetrics(ProjectorToolbar::kMarkerTool);
}

void ProjectorUiController::ResetTools() {
  if (annotator_enabled_) {
    ToggleAnnotator();
    annotator_enabled_ = false;
  }

  if (IsLaserPointerEnabled())
    EnableLaserPointer(false);
}

bool ProjectorUiController::IsLaserPointerEnabled() {
  return Shell::Get()->laser_pointer_controller()->is_enabled();
}

void ProjectorUiController::OnProjectorSessionActiveStateChanged(bool active) {
  if (!active)
    ResetTools();
}

void ProjectorUiController::OnLaserPointerStateChanged(bool enabled) {
  // If laser pointer is enabled, disable marker and magnifier.
  if (!enabled || !annotator_enabled_)
    return;

  ToggleAnnotator();
  annotator_enabled_ = false;
}

}  // namespace ash
