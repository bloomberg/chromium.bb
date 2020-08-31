// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/message_center_controller.h"

#include <utility>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/arc/arc_notification_manager.h"
#include "ash/system/message_center/arc_notification_manager_delegate_impl.h"
#include "ash/system/message_center/ash_message_center_lock_screen_controller.h"
#include "ash/system/message_center/fullscreen_notification_blocker.h"
#include "ash/system/message_center/inactive_user_notification_blocker.h"
#include "ash/system/message_center/session_state_notification_blocker.h"
#include "base/command_line.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/popup_timers_controller.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notifier_id.h"

using message_center::MessageCenter;
using message_center::NotifierId;

namespace ash {

// static
void MessageCenterController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(
      prefs::kMessageCenterLockScreenMode,
      prefs::kMessageCenterLockScreenModeHide,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

namespace {

bool DisableShortNotificationsForAccessibility(PrefService* pref_service) {
  DCHECK(pref_service);
  return (
      pref_service->GetBoolean(prefs::kAccessibilityAutoclickEnabled) ||
      pref_service->GetBoolean(prefs::kAccessibilityScreenMagnifierEnabled) ||
      pref_service->GetBoolean(prefs::kAccessibilitySelectToSpeakEnabled) ||
      pref_service->GetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled) ||
      pref_service->GetBoolean(prefs::kDockedMagnifierEnabled));
}

// A notification blocker that unconditionally blocks toasts. Implements
// --suppress-message-center-notifications.
class PopupNotificationBlocker : public message_center::NotificationBlocker {
 public:
  explicit PopupNotificationBlocker(MessageCenter* message_center)
      : NotificationBlocker(message_center) {}
  ~PopupNotificationBlocker() override = default;

  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PopupNotificationBlocker);
};

}  // namespace

MessageCenterController::MessageCenterController() {
  message_center::MessageCenter::Initialize(
      std::make_unique<AshMessageCenterLockScreenController>());

  fullscreen_notification_blocker_ =
      std::make_unique<FullscreenNotificationBlocker>(MessageCenter::Get());
  inactive_user_notification_blocker_ =
      std::make_unique<InactiveUserNotificationBlocker>(MessageCenter::Get());
  session_state_notification_blocker_ =
      std::make_unique<SessionStateNotificationBlocker>(MessageCenter::Get());

  Shell::Get()->session_controller()->AddObserver(this);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSuppressMessageCenterPopups)) {
    all_popup_blocker_ =
        std::make_unique<PopupNotificationBlocker>(MessageCenter::Get());
  }

  // Set the system notification source display name ("Chrome OS" or "Chromium
  // OS").
  message_center::MessageCenter::Get()->SetSystemNotificationAppName(
      l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME));
}

MessageCenterController::~MessageCenterController() {
  for (auto& observer : observers_)
    observer.OnArcNotificationInitializerDestroyed(this);

  // These members all depend on the MessageCenter instance, so must be
  // destroyed first.
  all_popup_blocker_.reset();
  session_state_notification_blocker_.reset();
  inactive_user_notification_blocker_.reset();
  fullscreen_notification_blocker_.reset();
  arc_notification_manager_.reset();

  Shell::Get()->session_controller()->RemoveObserver(this);

  message_center::MessageCenter::Shutdown();
}

void MessageCenterController::SetArcNotificationsInstance(
    mojo::PendingRemote<arc::mojom::NotificationsInstance>
        arc_notification_instance) {
  if (!arc_notification_manager_) {
    arc_notification_manager_ = std::make_unique<ArcNotificationManager>(
        std::make_unique<ArcNotificationManagerDelegateImpl>(),
        Shell::Get()
            ->session_controller()
            ->GetPrimaryUserSession()
            ->user_info.account_id,
        message_center::MessageCenter::Get());
  }
  arc_notification_manager_->SetInstance(std::move(arc_notification_instance));

  for (auto& observer : observers_)
    observer.OnSetArcNotificationsInstance(arc_notification_manager_.get());
}

void MessageCenterController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MessageCenterController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MessageCenterController::OnActiveUserPrefServiceChanged(
    PrefService* prefs) {
  if (!features::IsNotificationExperimentalShortTimeoutsEnabled() ||
      DisableShortNotificationsForAccessibility(prefs)) {
    return;
  }

  message_center::PopupTimersController::SetNotificationTimeouts(
      message_center::kAutocloseShortDelaySeconds,
      message_center::kAutocloseShortDelaySeconds);
}

}  // namespace ash
