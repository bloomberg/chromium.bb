// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/powerwash_requirements_checker.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace policy {

namespace {

namespace mc = ::message_center;

using RebootOnSignOutPolicy =
    ::enterprise_management::DeviceRebootOnUserSignoutProto;

PowerwashRequirementsChecker::State g_cached_cryptohome_powerwash_state =
    PowerwashRequirementsChecker::State::kUndefined;

constexpr NotificationHandler::Type kNotificationHandlerType =
    NotificationHandler::Type::TRANSIENT;
const char kNotificationArcId[] = "arc_powerwash_request_instead_of_run";
const char kNotificationCrostiniId[] =
    "crostini_powerwash_request_instead_of_run";
const char kNotificationCryptohomeErrorArcId[] =
    "arc_powerwash_request_cryptohome_error";
const char kNotificationCryptohomeErrorCrostiniId[] =
    "crostini_powerwash_request_cryptohome_error";
const gfx::VectorIcon& kNotificationIcon = vector_icons::kBusinessIcon;
constexpr mc::SystemNotificationWarningLevel kNotificationLevel =
    mc::SystemNotificationWarningLevel::NORMAL;
const char kNotificationLearnMoreLink[] =
    "https://support.google.com/chromebook?p=factory_reset";

base::string16 GetEnterpriseDisplayDomain() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return base::UTF8ToUTF16(connector->GetEnterpriseDisplayDomain());
}

void OnNotificationClicked(Profile* profile) {
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  NavigateParams params(displayer.browser(), GURL(kNotificationLearnMoreLink),
                        ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

void OnNotificationClickedCloseIt(Profile* profile,
                                  const char* notification_id) {
  NotificationDisplayService::GetForProfile(profile)->Close(
      kNotificationHandlerType, notification_id);
}

void OnCryptohomeCheckHealth(base::OnceClosure on_initialized_callback,
                             base::Optional<cryptohome::BaseReply> reply) {
  if (!reply || !reply->HasExtension(cryptohome::CheckHealthReply::reply)) {
    LOG(ERROR) << "Cryptohome failed to send health state";
  } else {
    const bool state = reply->GetExtension(cryptohome::CheckHealthReply::reply)
                           .requires_powerwash();
    g_cached_cryptohome_powerwash_state =
        state ? PowerwashRequirementsChecker::State::kRequired
              : PowerwashRequirementsChecker::State::kNotRequired;
  }

  if (on_initialized_callback)
    std::move(on_initialized_callback).Run();
}

void OnCryptohomeAvailability(base::OnceClosure on_initialized_callback,
                              bool is_available) {
  if (!is_available) {
    LOG(ERROR) << "Cryptohome is not available. Cannot request "
                  "powerwash_required state.";
    if (on_initialized_callback)
      std::move(on_initialized_callback).Run();
    return;
  }
  chromeos::CryptohomeClient::Get()->CheckHealth(
      cryptohome::CheckHealthRequest(),
      base::BindOnce(OnCryptohomeCheckHealth,
                     std::move(on_initialized_callback)));
}

}  // namespace

// static
void PowerwashRequirementsChecker::Initialize() {
  chromeos::CryptohomeClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(OnCryptohomeAvailability, base::OnceClosure{}));
}

// static
void PowerwashRequirementsChecker::InitializeSynchronouslyForTesting() {
  base::RunLoop run_loop;
  chromeos::CryptohomeClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(OnCryptohomeAvailability, run_loop.QuitClosure()));
  run_loop.Run();
}

// static
void PowerwashRequirementsChecker::ResetForTesting() {
  g_cached_cryptohome_powerwash_state = State::kUndefined;
}

PowerwashRequirementsChecker::PowerwashRequirementsChecker(
    PowerwashRequirementsChecker::Context context,
    Profile* profile)
    : context_(context), profile_(profile) {}

PowerwashRequirementsChecker::State PowerwashRequirementsChecker::GetState()
    const {
  if (!IsPolicySet() || IsUserAffiliated())
    return State::kNotRequired;

  return g_cached_cryptohome_powerwash_state;
}

bool PowerwashRequirementsChecker::IsPolicySet() const {
  int policy_value = RebootOnSignOutPolicy::NEVER;
  if (!chromeos::CrosSettings::Get()->GetInteger(
          chromeos::kDeviceRebootOnUserSignout, &policy_value)) {
    return false;
  }

  switch (policy_value) {
    case RebootOnSignOutPolicy::REBOOT_ON_SIGNOUT_MODE_UNSPECIFIED:
    case RebootOnSignOutPolicy::NEVER:
      return false;
    case RebootOnSignOutPolicy::ARC_SESSION:
      return context_ == Context::kArc;
    case RebootOnSignOutPolicy::VM_STARTED_OR_ARC_SESSION:
    case RebootOnSignOutPolicy::ALWAYS:
    default:
      return true;
  }
}

bool PowerwashRequirementsChecker::IsUserAffiliated() const {
  user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  return user->IsAffiliated();
}

void PowerwashRequirementsChecker::ShowNotification() {
  if (g_cached_cryptohome_powerwash_state == State::kUndefined) {
    LOG(ERROR) << "Cryptohome \"powerwash_required\" state not found.";
    // Cryptohome has not responded with powerwash status.
    // Show error notification instead of the normal one and try to request
    // again
    ShowCryptohomeErrorNotification();
    Initialize();
    return;
  }

  const char* notification_id = nullptr;
  const char* cryptohome_error_notification_id = nullptr;
  int message_id = 0;
  switch (context_) {
    case Context::kArc:
      notification_id = kNotificationArcId;
      cryptohome_error_notification_id = kNotificationCryptohomeErrorArcId;
      message_id = IDS_POWERWASH_REQUEST_MESSAGE_FOR_ARC;
      break;
    case Context::kCrostini:
      notification_id = kNotificationCrostiniId;
      cryptohome_error_notification_id = kNotificationCryptohomeErrorCrostiniId;
      message_id = IDS_POWERWASH_REQUEST_MESSAGE_FOR_CROSTINI;
      break;
  }

  mc::RichNotificationData rich_data;
  rich_data.buttons = std::vector<mc::ButtonInfo>{mc::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_POWERWASH_REQUEST_MESSAGE_BUTTON))};

  auto delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&OnNotificationClicked, profile_));

  auto notification = ash::CreateSystemNotification(
      mc::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringUTF16(IDS_POWERWASH_REQUEST_TITLE),
      l10n_util::GetStringFUTF16(message_id, GetEnterpriseDisplayDomain()),
      base::string16{}, GURL{},
      mc::NotifierId(mc::NotifierType::SYSTEM_COMPONENT, notification_id),
      std::move(rich_data), std::move(delegate), kNotificationIcon,
      kNotificationLevel);

  NotificationDisplayService::GetForProfile(profile_)->Close(
      kNotificationHandlerType, cryptohome_error_notification_id);
  NotificationDisplayService::GetForProfile(profile_)->Close(
      kNotificationHandlerType, notification_id);
  NotificationDisplayService::GetForProfile(profile_)->Display(
      kNotificationHandlerType, *notification,
      /*metadata=*/nullptr);
}

void PowerwashRequirementsChecker::ShowCryptohomeErrorNotification() {
  const char* notification_id = nullptr;
  int message_id = 0;
  switch (context_) {
    case Context::kArc:
      notification_id = kNotificationCryptohomeErrorArcId;
      message_id = IDS_POWERWASH_REQUEST_UNDEFINED_STATE_ERROR_MESSAGE_FOR_ARC;
      break;
    case Context::kCrostini:
      notification_id = kNotificationCryptohomeErrorCrostiniId;
      message_id =
          IDS_POWERWASH_REQUEST_UNDEFINED_STATE_ERROR_MESSAGE_FOR_CROSTINI;
      break;
  }

  auto delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&OnNotificationClickedCloseIt, profile_,
                              notification_id));

  auto notification = ash::CreateSystemNotification(
      mc::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringUTF16(
          IDS_POWERWASH_REQUEST_UNDEFINED_STATE_ERROR_TITLE),
      l10n_util::GetStringUTF16(message_id), base::string16{}, GURL{},
      mc::NotifierId(mc::NotifierType::SYSTEM_COMPONENT, notification_id), {},
      std::move(delegate), kNotificationIcon, kNotificationLevel);

  NotificationDisplayService::GetForProfile(profile_)->Close(
      kNotificationHandlerType, notification_id);
  NotificationDisplayService::GetForProfile(profile_)->Display(
      kNotificationHandlerType, *notification,
      /*metadata=*/nullptr);
}

}  // namespace policy
