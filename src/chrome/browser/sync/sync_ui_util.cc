// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_ui_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync/engine/sync_status.h"
#include "components/sync/protocol/sync_protocol_error.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ui/base/l10n/l10n_util.h"

namespace sync_ui_util {

namespace {

// Returns the message that should be displayed when the user is authenticated
// and can connect to the sync server. If Sync hasn't finished initializing yet,
// an empty string is returned.
base::string16 GetSyncedStateStatusLabel(const syncer::SyncService* service) {
  DCHECK(service);
  if (service->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY)) {
    return l10n_util::GetStringUTF16(IDS_SIGNED_IN_WITH_SYNC_DISABLED);
  }
  if (!service->GetUserSettings()->IsSyncRequested()) {
    return l10n_util::GetStringUTF16(IDS_SIGNED_IN_WITH_SYNC_SUPPRESSED);
  }
  if (!service->IsSyncFeatureActive()) {
    // Sync is still initializing.
    return base::string16();
  }

  return l10n_util::GetStringUTF16(
      service->GetUserSettings()->IsSyncEverythingEnabled()
          ? IDS_SYNC_ACCOUNT_SYNCING
          : IDS_SYNC_ACCOUNT_SYNCING_CUSTOM_DATA_TYPES);
}

// Returns whether there is a non-empty status for the given |action|.
bool GetStatusForActionableError(syncer::ClientAction action,
                                 base::string16* status_label,
                                 base::string16* link_label,
                                 ActionType* action_type) {
  switch (action) {
    case syncer::UPGRADE_CLIENT:
      if (status_label) {
        *status_label = l10n_util::GetStringUTF16(IDS_SYNC_UPGRADE_CLIENT);
      }
      if (link_label) {
        *link_label =
            l10n_util::GetStringUTF16(IDS_SYNC_UPGRADE_CLIENT_LINK_LABEL);
      }
      if (action_type) {
        *action_type = UPGRADE_CLIENT;
      }
      return true;
    case syncer::ENABLE_SYNC_ON_ACCOUNT:
      if (status_label) {
        *status_label =
            l10n_util::GetStringUTF16(IDS_SYNC_STATUS_ENABLE_SYNC_ON_ACCOUNT);
      }
      return true;
    default:
      if (status_label) {
        *status_label = base::string16();
      }
      return false;
  }
}

void GetStatusForUnrecoverableError(bool is_user_signout_allowed,
                                    syncer::ClientAction action,
                                    base::string16* status_label,
                                    base::string16* link_label,
                                    ActionType* action_type) {
  // Unrecoverable error is sometimes accompanied by actionable error.
  // If status message is set display that message, otherwise show generic
  // unrecoverable error message.
  if (!GetStatusForActionableError(action, status_label, link_label,
                                   action_type)) {
    if (action_type) {
      *action_type = REAUTHENTICATE;
    }
    if (link_label) {
      *link_label = l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_LINK_LABEL);
    }

    if (status_label) {
#if !defined(OS_CHROMEOS)
      if (is_user_signout_allowed) {
        *status_label =
            l10n_util::GetStringUTF16(IDS_SYNC_STATUS_UNRECOVERABLE_ERROR);
      } else {
        // The message for managed accounts is the same as that on ChromeOS.
        *status_label = l10n_util::GetStringUTF16(
            IDS_SYNC_STATUS_UNRECOVERABLE_ERROR_NEEDS_SIGNOUT);
      }
#else
      *status_label = l10n_util::GetStringUTF16(
          IDS_SYNC_STATUS_UNRECOVERABLE_ERROR_NEEDS_SIGNOUT);
#endif
    }
  }
}

// Depending on the authentication state, returns labels to be used to display
// information about the sync status.
void GetStatusForAuthError(const GoogleServiceAuthError& auth_error,
                           base::string16* status_label,
                           base::string16* link_label,
                           ActionType* action_type) {
  switch (auth_error.state()) {
    case GoogleServiceAuthError::NONE:
      NOTREACHED();
      break;
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      if (status_label) {
        *status_label = l10n_util::GetStringUTF16(IDS_SYNC_SERVICE_UNAVAILABLE);
      }
      break;
    case GoogleServiceAuthError::CONNECTION_FAILED:
      if (status_label) {
        *status_label =
            l10n_util::GetStringUTF16(IDS_SYNC_SERVER_IS_UNREACHABLE);
      }
      // Note that there is little the user can do if the server is not
      // reachable. Since attempting to re-connect is done automatically by
      // the Syncer, we do not show the (re)login link.
      break;
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::SERVICE_ERROR:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
    default:
      if (status_label) {
        *status_label = l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_ERROR);
      }
      if (link_label) {
        *link_label = l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_LINK_LABEL);
      }
      if (action_type) {
        *action_type = REAUTHENTICATE;
      }
      break;
  }
}

MessageType GetStatusLabelsImpl(
    const syncer::SyncService* service,
    bool is_user_signout_allowed,
    const GoogleServiceAuthError& auth_error,
    base::string16* status_label,
    base::string16* link_label,
    ActionType* action_type) {
  DCHECK(service);

  if (!service->IsAuthenticatedAccountPrimary()) {
    return PRE_SYNCED;
  }

  // If local Sync were enabled, then the SyncService shouldn't report having a
  // primary (or any) account.
  DCHECK(!service->IsLocalSyncEnabled());

  syncer::SyncStatus status;
  service->QueryDetailedSyncStatus(&status);

  // First check for an unrecoverable error.
  if (service->HasUnrecoverableError()) {
    GetStatusForUnrecoverableError(is_user_signout_allowed,
                                   status.sync_protocol_error.action,
                                   status_label, link_label, action_type);
    return SYNC_ERROR;
  }

  // Then check for an auth error.
  if (auth_error.state() != GoogleServiceAuthError::NONE &&
      auth_error.state() != GoogleServiceAuthError::TWO_FACTOR) {
    GetStatusForAuthError(auth_error, status_label, link_label, action_type);
    return SYNC_ERROR;
  }

  // TODO(crbug.com/911153): What's the intended meaning of this condition?
  // Should other disable reasons also be checked?
  if (service->GetUserSettings()->IsFirstSetupComplete() ||
      !service->GetUserSettings()->IsSyncRequested() ||
      service->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY)) {
    // Check for an actionable protocol error.
    if (GetStatusForActionableError(status.sync_protocol_error.action,
                                    status_label, link_label, action_type)) {
      return SYNC_ERROR;
    }

    // Check for a passphrase error.
    if (service->GetUserSettings()->IsPassphraseRequiredForDecryption()) {
      if (status_label) {
        *status_label =
            l10n_util::GetStringUTF16(IDS_SYNC_STATUS_NEEDS_PASSWORD);
      }
      if (link_label) {
        *link_label = l10n_util::GetStringUTF16(
            IDS_SYNC_STATUS_NEEDS_PASSWORD_LINK_LABEL);
      }
      if (action_type) {
        *action_type = ENTER_PASSPHRASE;
      }
      return SYNC_ERROR;
    }

    // At this point, there is no Sync error, but there might still be a message
    // we want to show.
    // TODO(crbug.com/911153): This also covers the "disabled by policy" and
    // "not requested" cases, which doesn't seem right.
    if (status_label) {
      *status_label = GetSyncedStateStatusLabel(service);
    }

    // Check to see if sync has been disabled via the dasboard and needs to be
    // set up once again.
    if (!service->GetUserSettings()->IsSyncRequested() &&
        status.sync_protocol_error.error_type == syncer::NOT_MY_BIRTHDAY) {
      return PRE_SYNCED;
    }

    return SYNCED;
  }

  // If first setup is in progress, show an "in progress" message.
  if (service->IsFirstSetupInProgress()) {
    if (status_label) {
      *status_label = l10n_util::GetStringUTF16(IDS_SYNC_NTP_SETUP_IN_PROGRESS);
    }
    return PRE_SYNCED;
  }

  // At this point we've ruled out all other cases - all that's left is a
  // missing Sync confirmation.
  DCHECK(ShouldRequestSyncConfirmation(service));
  if (status_label) {
    *status_label = l10n_util::GetStringUTF16(IDS_SYNC_SETTINGS_NOT_CONFIRMED);
  }
  if (link_label) {
    *link_label = l10n_util::GetStringUTF16(
        IDS_SYNC_ERROR_USER_MENU_CONFIRM_SYNC_SETTINGS_BUTTON);
  }
  if (action_type) {
    *action_type = CONFIRM_SYNC_SETTINGS;
  }
  return SYNC_ERROR;
}

}  // namespace

MessageType GetStatusLabels(Profile* profile,
                            base::string16* status_label,
                            base::string16* link_label,
                            ActionType* action_type) {
  DCHECK(profile);
  syncer::SyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  if (!service) {
    // This can happen if Sync is disabled via the command line.
    return PRE_SYNCED;
  }
  const bool is_user_signout_allowed =
      signin_util::IsUserSignoutAllowedForProfile(profile);
  GoogleServiceAuthError auth_error =
      SigninErrorControllerFactory::GetForProfile(profile)->auth_error();
  return GetStatusLabelsImpl(service, is_user_signout_allowed, auth_error,
                             status_label, link_label, action_type);
}

MessageType GetStatus(Profile* profile) {
  return GetStatusLabels(profile, /*status_label=*/nullptr,
                         /*link_label=*/nullptr, /*action_type=*/nullptr);
}

#if !defined(OS_CHROMEOS)
AvatarSyncErrorType GetMessagesForAvatarSyncError(
    Profile* profile,
    int* content_string_id,
    int* button_string_id) {
  const syncer::SyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile);

  // The order or priority is going to be: 1. Unrecoverable errors.
  // 2. Auth errors. 3. Protocol errors. 4. Passphrase errors.
  if (service && service->HasUnrecoverableError()) {
    // An unrecoverable error is sometimes accompanied by an actionable error.
    // If an actionable error is not set to be UPGRADE_CLIENT, then show a
    // generic unrecoverable error message.
    syncer::SyncStatus status;
    service->QueryDetailedSyncStatus(&status);
    if (status.sync_protocol_error.action != syncer::UPGRADE_CLIENT) {
      // Display different messages and buttons for managed accounts.
      if (!signin_util::IsUserSignoutAllowedForProfile(profile)) {
        // For a managed user, the user is directed to the signout
        // confirmation dialogue in the settings page.
        *content_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNOUT_MESSAGE;
        *button_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNOUT_BUTTON;
        return MANAGED_USER_UNRECOVERABLE_ERROR;
      }
      // For a non-managed user, we sign out on the user's behalf and prompt
      // the user to sign in again.
      *content_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNIN_AGAIN_MESSAGE;
      *button_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNIN_AGAIN_BUTTON;
      return UNRECOVERABLE_ERROR;
    }
  }

  // Check for an auth error.
  SigninErrorController* signin_error_controller =
      SigninErrorControllerFactory::GetForProfile(profile);
  if (signin_error_controller && signin_error_controller->HasError()) {
    // The user can reauth to resolve the signin error.
    *content_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNIN_MESSAGE;
    *button_string_id = IDS_SYNC_ERROR_USER_MENU_SIGNIN_BUTTON;
    return AUTH_ERROR;
  }

  // Check for sync errors if the sync service is enabled.
  if (service) {
    // Check for an actionable UPGRADE_CLIENT error.
    syncer::SyncStatus status;
    service->QueryDetailedSyncStatus(&status);
    if (status.sync_protocol_error.action == syncer::UPGRADE_CLIENT) {
      *content_string_id = IDS_SYNC_ERROR_USER_MENU_UPGRADE_MESSAGE;
      *button_string_id = IDS_SYNC_ERROR_USER_MENU_UPGRADE_BUTTON;
      return UPGRADE_CLIENT_ERROR;
    }

    // Check for a sync passphrase error.
    if (ShouldShowPassphraseError(service)) {
      *content_string_id = IDS_SYNC_ERROR_USER_MENU_PASSPHRASE_MESSAGE;
      *button_string_id = IDS_SYNC_ERROR_USER_MENU_PASSPHRASE_BUTTON;
      return PASSPHRASE_ERROR;
    }

    // Check for a sync confirmation error.
    if (ShouldRequestSyncConfirmation(service)) {
      *content_string_id = IDS_SYNC_SETTINGS_NOT_CONFIRMED;
      *button_string_id = IDS_SYNC_ERROR_USER_MENU_CONFIRM_SYNC_SETTINGS_BUTTON;
      return SETTINGS_UNCONFIRMED_ERROR;
    }
  }

  // There is no error.
  return NO_SYNC_ERROR;
}
#endif  // !defined(OS_CHROMEOS)

bool ShouldRequestSyncConfirmation(const syncer::SyncService* service) {
  return !service->IsLocalSyncEnabled() &&
         service->GetUserSettings()->IsSyncRequested() &&
         service->IsAuthenticatedAccountPrimary() &&
         !service->IsSetupInProgress() &&
         !service->GetUserSettings()->IsFirstSetupComplete();
}

bool ShouldShowPassphraseError(const syncer::SyncService* service) {
  return service->GetUserSettings()->IsFirstSetupComplete() &&
         service->GetUserSettings()->IsPassphraseRequiredForDecryption();
}

}  // namespace sync_ui_util
