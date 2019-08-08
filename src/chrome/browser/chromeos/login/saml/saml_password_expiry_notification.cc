// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/saml/saml_password_expiry_notification.h"

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/session/session_activation_observer.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/insession_password_change_ui.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

using message_center::HandleNotificationClickDelegate;
using message_center::Notification;
using message_center::NotificationType;
using message_center::NotifierId;
using message_center::NotifierType;
using message_center::RichNotificationData;
using message_center::SystemNotificationWarningLevel;

namespace chromeos {

namespace {

// Unique ID for this notification.
const char kNotificationId[] = "saml.password-expiry-notification";

// NotifierId for histogram reporting.
const base::NoDestructor<NotifierId> kNotifierId(
    message_center::NotifierType::SYSTEM_COMPONENT,
    kNotificationId);

// Simplest type of notification - has text but no other UI elements.
const NotificationType kNotificationType =
    message_center::NOTIFICATION_TYPE_SIMPLE;

// Generic type for notifications that are not from web pages etc.
const NotificationHandler::Type kNotificationHandlerType =
    NotificationHandler::Type::TRANSIENT;

// The icon to use for this notification - looks like an office building.
const gfx::VectorIcon& kIcon = vector_icons::kBusinessIcon;

// Warning level NORMAL means the notification heading is blue.
const SystemNotificationWarningLevel kWarningLevel =
    SystemNotificationWarningLevel::NORMAL;

// Leaving this empty means the notification is attributed to the system -
// ie "Chromium OS" or similar.
const base::NoDestructor<base::string16> kDisplaySource;

// These extra attributes aren't needed, so they are left empty.
const base::NoDestructor<GURL> kOriginUrl;
const base::NoDestructor<RichNotificationData> kRichNotificationData;

// Line separator in the notification body.
const base::NoDestructor<base::string16> kLineSeparator(
    base::string16(1, '\n'));

base::string16 GetTitleText(int less_than_n_days) {
  const bool hasExpired = (less_than_n_days <= 0);
  return hasExpired ? l10n_util::GetStringUTF16(IDS_PASSWORD_HAS_EXPIRED_TITLE)
                    : l10n_util::GetStringUTF16(IDS_PASSWORD_WILL_EXPIRE_TITLE);
}

base::string16 GetBodyText(int less_than_n_days) {
  const std::vector<base::string16> body_lines = {
      l10n_util::GetPluralStringFUTF16(IDS_PASSWORD_EXPIRY_DAYS_BODY,
                                       std::max(less_than_n_days, 0)),
      l10n_util::GetStringUTF16(IDS_PASSWORD_EXPIRY_CHOOSE_NEW_PASSWORD_LINK)};

  return base::JoinString(body_lines, *kLineSeparator);
}

// A time delta of length one hour.
const base::TimeDelta kOneHour = base::TimeDelta::FromHours(1);
// A time delta of length one day.
const base::TimeDelta kOneDay = base::TimeDelta::FromDays(1);

// Traits for running RecheckTask. Runs from the UI thread to show notification.
const base::TaskTraits kRecheckTaskTraits = {
    content::BrowserThread::UI, base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

// Returns true if |profile| is still valid.
bool IsValidProfile(Profile* profile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager && profile_manager->IsValidProfile(profile);
}

// Returns true if showing the notification is enabled for this profile.
bool IsEnabledForProfile(Profile* profile) {
  return chromeos::ProfileHelper::IsPrimaryProfile(profile) &&
         profile->GetPrefs()->GetBoolean(
             prefs::kSamlInSessionPasswordChangeEnabled);
}

// The Rechecker checks periodically if the notification should be shown or
// updated. When it checks depends on when the password will expire.
// There is only at most one Rechecker at a time - for the primary user.
class Rechecker : public ash::SessionActivationObserver {
 public:
  // Checks again if the notification should be shown, and maybe shows it.
  void Recheck();

  // Calls recheck after the given |delay|.
  void RecheckAfter(base::TimeDelta delay);

  // Cancels any pending tasks to call Recheck again.
  void CancelPendingRecheck();

  // ash::SessionActivationObserver:
  void OnSessionActivated(bool activated) override {}  // Not needed.
  void OnLockStateChanged(bool locked) override;

  // Ensures the singleton is initialized and started for the given profile.
  static void StartForProfile(Profile* profile);
  // Stops and deletes the Rechecker singleton.
  static void Stop();

 private:
  // The constructor and destructor also maintain the singleton instance.
  Rechecker(Profile* profile, const AccountId& account_id);
  ~Rechecker() override;

  // Singleton, since we only ever need one instance_ for the primary user.
  static Rechecker* instance_;

  Profile* profile_;
  const AccountId account_id_;
  base::WeakPtrFactory<Rechecker> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Rechecker);
};

void Rechecker::Recheck() {
  // This cancels any pending call to Recheck - we don't want some bug to cause
  // us to queue up lots of calls to Recheck in the future, we only want one.
  CancelPendingRecheck();

  // In case the profile has been deleted since this task was posted.
  if (!IsValidProfile(profile_)) {
    delete this;  // No need to keep calling recheck.
    return;
  }

  PrefService* prefs = profile_->GetPrefs();
  SamlPasswordAttributes attrs = SamlPasswordAttributes::LoadFromPrefs(prefs);
  if (!IsEnabledForProfile(profile_) || !attrs.has_expiration_time()) {
    // Feature is not enabled for this profile, or profile is not primary, or
    // there is no expiration time. Dismiss if shown, and stop checking.
    DismissSamlPasswordExpiryNotification(profile_);
    delete this;
    return;
  }

  // Calculate how many days until the password will expire.
  const base::TimeDelta time_until_expiry =
      attrs.expiration_time() - base::Time::Now();
  const int less_than_n_days =
      std::max(0, time_until_expiry.InDaysFloored() + 1);
  const int advance_warning_days = std::max(
      0, prefs->GetInteger(prefs::kSamlPasswordExpirationAdvanceWarningDays));

  if (less_than_n_days <= advance_warning_days) {
    // The password is expired, or expires in less than |advance_warning_days|.
    // So we show a notification immediately.
    ShowSamlPasswordExpiryNotification(profile_, less_than_n_days);
    // We check again whether to reshow / update the notification after one day:
    RecheckAfter(kOneDay);

  } else {
    // We have not yet reached the advance warning threshold. Check again
    // once we have arrived at expiry_time minus advance_warning_days...
    base::TimeDelta recheck_delay =
        time_until_expiry - base::TimeDelta::FromDays(advance_warning_days);
    // But, wait an extra hour so that when this code is next run, it is clear
    // we are now inside advance_warning_days (and not right on the boundary).
    recheck_delay += kOneHour;
    RecheckAfter(recheck_delay);
  }
}

void Rechecker::RecheckAfter(base::TimeDelta delay) {
  base::PostDelayedTaskWithTraits(
      FROM_HERE, kRecheckTaskTraits,
      base::BindOnce(&Rechecker::Recheck, weak_ptr_factory_.GetWeakPtr()),
      std::max(delay, kOneHour));
  // This always waits at least one hour before calling Recheck again - we don't
  // want some bug to cause this code to run every millisecond.
}

void Rechecker::CancelPendingRecheck() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void Rechecker::OnLockStateChanged(bool locked) {
  if (!locked) {
    // TODO(olsen): Reshow the notification when screen is unlocked.
  }
}

// static
void Rechecker::StartForProfile(Profile* profile) {
  if (!instance_) {
    const AccountId account_id =
        ProfileHelper::Get()->GetUserByProfile(profile)->GetAccountId();
    new Rechecker(profile, account_id);
  }
  DCHECK(instance_ && instance_->profile_ == profile);
  instance_->Recheck();
}

// static
void Rechecker::Stop() {
  delete instance_;
}

// static
Rechecker* Rechecker::instance_ = nullptr;

Rechecker::Rechecker(Profile* profile, const AccountId& account_id)
    : profile_(profile), account_id_(account_id) {
  // There must not be an existing singleton instance.
  DCHECK(!instance_);
  instance_ = this;

  // Add |this| as a SessionActivationObserver to see when the screen is locked.
  auto* session_controller = ash::SessionController::Get();
  if (session_controller) {
    session_controller->AddSessionActivationObserverForAccountId(account_id_,
                                                                 this);
  }
}

Rechecker::~Rechecker() {
  // Remove this as a SessionActivationObserver.
  auto* session_controller = ash::SessionController::Get();
  if (session_controller) {
    session_controller->RemoveSessionActivationObserverForAccountId(account_id_,
                                                                    this);
  }

  // This should still be the singleton instance.
  DCHECK_EQ(this, instance_);
  instance_ = nullptr;
}

}  // namespace

void MaybeShowSamlPasswordExpiryNotification(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (IsEnabledForProfile(profile)) {
    Rechecker::StartForProfile(profile);
  }
}

void ShowSamlPasswordExpiryNotification(Profile* profile,
                                        int less_than_n_days) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::string16 title = GetTitleText(less_than_n_days);
  const base::string16 body = GetBodyText(less_than_n_days);

  auto click_delegate = base::MakeRefCounted<HandleNotificationClickDelegate>(
      base::BindRepeating(&PasswordChangeDialog::Show, profile));

  std::unique_ptr<Notification> notification = ash::CreateSystemNotification(
      kNotificationType, kNotificationId, title, body, *kDisplaySource,
      *kOriginUrl, *kNotifierId, *kRichNotificationData, click_delegate, kIcon,
      kWarningLevel);

  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      kNotificationHandlerType, *notification, /*metadata=*/nullptr);
}

void DismissSamlPasswordExpiryNotification(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NotificationDisplayServiceFactory::GetForProfile(profile)->Close(
      kNotificationHandlerType, kNotificationId);
}

void ResetSamlPasswordExpiryNotificationForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Rechecker::Stop();
}

}  // namespace chromeos
