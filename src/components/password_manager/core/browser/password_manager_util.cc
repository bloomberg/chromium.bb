// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_util.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/credentials_cleaner.h"
#include "components/password_manager/core/browser/credentials_cleaner_runner.h"
#include "components/password_manager/core/browser/http_credentials_cleaner.h"
#include "components/password_manager/core/browser/old_google_credentials_cleaner.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"

using autofill::password_generation::PasswordGenerationType;
using password_manager::PasswordForm;

namespace password_manager_util {
namespace {

// Return true if 1.|lhs| is non-PSL match, |rhs| is PSL match or 2.|lhs| and
// |rhs| have the same value of |is_public_suffix_match|, and |lhs| is more
// recently used than |rhs|.
bool IsBetterMatch(const PasswordForm* lhs, const PasswordForm* rhs) {
  return std::make_pair(!lhs->is_public_suffix_match, lhs->date_last_used) >
         std::make_pair(!rhs->is_public_suffix_match, rhs->date_last_used);
}

}  // namespace

// Update |credential| to reflect usage.
void UpdateMetadataForUsage(PasswordForm* credential) {
  ++credential->times_used;

  // Remove alternate usernames. At this point we assume that we have found
  // the right username.
  credential->all_possible_usernames.clear();
}

password_manager::SyncState GetPasswordSyncState(
    const syncer::SyncService* sync_service) {
  if (!sync_service ||
      !sync_service->GetActiveDataTypes().Has(syncer::PASSWORDS)) {
    return password_manager::SyncState::kNotSyncing;
  }

  if (sync_service->IsSyncFeatureActive()) {
    return sync_service->GetUserSettings()->IsUsingExplicitPassphrase()
               ? password_manager::SyncState::kSyncingWithCustomPassphrase
               : password_manager::SyncState::kSyncingNormalEncryption;
  }

  DCHECK(base::FeatureList::IsEnabled(
      password_manager::features::kEnablePasswordsAccountStorage));
  // Account passwords are enabled only for users with normal encryption at
  // the moment. Data types won't become active for non-sync users with custom
  // passphrase.
  return password_manager::SyncState::kAccountPasswordsActiveNormalEncryption;
}

void TrimUsernameOnlyCredentials(
    std::vector<std::unique_ptr<PasswordForm>>* android_credentials) {
  // Remove username-only credentials which are not federated.
  base::EraseIf(*android_credentials,
                [](const std::unique_ptr<PasswordForm>& form) {
                  return form->scheme == PasswordForm::Scheme::kUsernameOnly &&
                         form->federation_origin.opaque();
                });

  // Set "skip_zero_click" on federated credentials.
  std::for_each(android_credentials->begin(), android_credentials->end(),
                [](const std::unique_ptr<PasswordForm>& form) {
                  if (form->scheme == PasswordForm::Scheme::kUsernameOnly)
                    form->skip_zero_click = true;
                });
}

bool IsLoggingActive(const password_manager::PasswordManagerClient* client) {
  const autofill::LogManager* log_manager = client->GetLogManager();
  return log_manager && log_manager->IsLoggingActive();
}

bool ManualPasswordGenerationEnabled(
    password_manager::PasswordManagerDriver* driver) {
  password_manager::PasswordGenerationFrameHelper* password_generation_manager =
      driver ? driver->GetPasswordGenerationHelper() : nullptr;
  if (!password_generation_manager ||
      !password_generation_manager->IsGenerationEnabled(false /*logging*/)) {
    return false;
  }

  LogPasswordGenerationEvent(
      autofill::password_generation::PASSWORD_GENERATION_CONTEXT_MENU_SHOWN);
  return true;
}

bool ShowAllSavedPasswordsContextMenuEnabled(
    password_manager::PasswordManagerDriver* driver) {
  password_manager::PasswordManager* password_manager =
      driver ? driver->GetPasswordManager() : nullptr;
  if (!password_manager)
    return false;

  password_manager::PasswordManagerClient* client = password_manager->client();
  if (!client ||
      !client->IsFillingFallbackEnabled(driver->GetLastCommittedURL()))
    return false;

  return true;
}

void UserTriggeredManualGenerationFromContextMenu(
    password_manager::PasswordManagerClient* password_manager_client) {
  if (!password_manager_client->GetPasswordFeatureManager()
           ->ShouldShowAccountStorageOptIn()) {
    password_manager_client->GeneratePassword(PasswordGenerationType::kManual);
    LogPasswordGenerationEvent(autofill::password_generation::
                                   PASSWORD_GENERATION_CONTEXT_MENU_PRESSED);
    return;
  }
  // The client ensures the callback won't be run if it is destroyed, so
  // base::Unretained is safe.
  password_manager_client->TriggerReauthForPrimaryAccount(
      signin_metrics::ReauthAccessPoint::kGeneratePasswordContextMenu,
      base::BindOnce(
          [](password_manager::PasswordManagerClient* client,
             password_manager::PasswordManagerClient::ReauthSucceeded
                 succeeded) {
            if (succeeded) {
              client->GeneratePassword(PasswordGenerationType::kManual);
              LogPasswordGenerationEvent(
                  autofill::password_generation::
                      PASSWORD_GENERATION_CONTEXT_MENU_PRESSED);
            }
          },
          base::Unretained(password_manager_client)));
}

// TODO(http://crbug.com/890318): Add unitests to check cleaners are correctly
// created.
void RemoveUselessCredentials(
    password_manager::CredentialsCleanerRunner* cleaning_tasks_runner,
    scoped_refptr<password_manager::PasswordStore> store,
    PrefService* prefs,
    base::TimeDelta delay,
    base::RepeatingCallback<network::mojom::NetworkContext*()>
        network_context_getter) {
  DCHECK(cleaning_tasks_runner);

#if !defined(OS_IOS)
  // Can be null for some unittests.
  if (!network_context_getter.is_null()) {
    cleaning_tasks_runner->MaybeAddCleaningTask(
        std::make_unique<password_manager::HttpCredentialCleaner>(
            store, network_context_getter, prefs));
  }
#endif  // !defined(OS_IOS)

  // TODO(crbug.com/450621): Remove this when enough number of clients switch
  // to the new version of Chrome.
  cleaning_tasks_runner->MaybeAddCleaningTask(
      std::make_unique<password_manager::OldGoogleCredentialCleaner>(store,
                                                                     prefs));

  if (cleaning_tasks_runner->HasPendingTasks()) {
    // The runner will delete itself once the clearing tasks are done, thus we
    // are releasing ownership here.
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &password_manager::CredentialsCleanerRunner::StartCleaning,
            cleaning_tasks_runner->GetWeakPtr()),
        delay);
  }
}

base::StringPiece GetSignonRealmWithProtocolExcluded(const PasswordForm& form) {
  base::StringPiece signon_realm = form.signon_realm;

  // Find the web origin (with protocol excluded) in the signon_realm.
  const size_t after_protocol = signon_realm.find(form.url.host_piece());

  // Keep the string starting with position |after_protocol|.
  return signon_realm.substr(std::min(after_protocol, signon_realm.size()));
}

void FindBestMatches(
    const std::vector<const PasswordForm*>& non_federated_matches,
    PasswordForm::Scheme scheme,
    std::vector<const PasswordForm*>* non_federated_same_scheme,
    std::vector<const PasswordForm*>* best_matches,
    const PasswordForm** preferred_match) {
  DCHECK(base::ranges::none_of(non_federated_matches,
                               &PasswordForm::blocked_by_user));
  DCHECK(non_federated_same_scheme);
  DCHECK(best_matches);
  DCHECK(preferred_match);

  *preferred_match = nullptr;
  best_matches->clear();
  non_federated_same_scheme->clear();

  for (auto* match : non_federated_matches) {
    if (match->scheme == scheme)
      non_federated_same_scheme->push_back(match);
  }

  if (non_federated_same_scheme->empty())
    return;

  std::sort(non_federated_same_scheme->begin(),
            non_federated_same_scheme->end(), IsBetterMatch);

  std::set<std::pair<PasswordForm::Store, std::u16string>> store_usernames;
  for (const auto* match : *non_federated_same_scheme) {
    auto store_username =
        std::make_pair(match->in_store, match->username_value);
    // The first match for |store_username| in the sorted array is best
    // match.
    if (!base::Contains(store_usernames, store_username)) {
      store_usernames.insert(store_username);
      best_matches->push_back(match);
    }
  }

  *preferred_match = *non_federated_same_scheme->begin();
}

const PasswordForm* FindFormByUsername(
    const std::vector<const PasswordForm*>& forms,
    const std::u16string& username_value) {
  for (const PasswordForm* form : forms) {
    if (form->username_value == username_value)
      return form;
  }
  return nullptr;
}

const PasswordForm* GetMatchForUpdating(
    const PasswordForm& submitted_form,
    const std::vector<const PasswordForm*>& credentials) {
  // This is the case for the credential management API. It should not depend on
  // form managers. Once that's the case, this should be turned into a DCHECK.
  // TODO(crbug/947030): turn it into a DCHECK.
  if (!submitted_form.federation_origin.opaque())
    return nullptr;

  // Try to return form with matching |username_value|.
  const PasswordForm* username_match =
      FindFormByUsername(credentials, submitted_form.username_value);
  if (username_match) {
    if (!username_match->is_public_suffix_match)
      return username_match;

    const auto& password_to_save = submitted_form.new_password_value.empty()
                                       ? submitted_form.password_value
                                       : submitted_form.new_password_value;
    // Normally, the copy of the PSL matched credentials, adapted for the
    // current domain, is saved automatically without asking the user, because
    // the copy likely represents the same account, i.e., the one for which
    // the user already agreed to store a password.
    //
    // However, if the user changes the suggested password, it might indicate
    // that the autofilled credentials and |submitted_password_form|
    // actually correspond to two different accounts (see
    // http://crbug.com/385619).
    return password_to_save == username_match->password_value ? username_match
                                                              : nullptr;
  }

  // Next attempt is to find a match by password value. It should not be tried
  // when the username was actually detected.
  if (submitted_form.type == PasswordForm::Type::kApi ||
      !submitted_form.username_value.empty()) {
    return nullptr;
  }

  for (const PasswordForm* stored_match : credentials) {
    if (stored_match->password_value == submitted_form.password_value)
      return stored_match;
  }

  // Last try. The submitted form had no username but a password. Assume that
  // it's an existing credential.
  return credentials.empty() ? nullptr : credentials.front();
}

PasswordForm MakeNormalizedBlocklistedForm(
    password_manager::PasswordFormDigest digest) {
  PasswordForm result;
  result.blocked_by_user = true;
  result.scheme = std::move(digest.scheme);
  result.signon_realm = std::move(digest.signon_realm);
  // In case |digest| corresponds to an Android credential copy the origin as
  // is, otherwise clear out the path by calling GetOrigin().
  if (password_manager::FacetURI::FromPotentiallyInvalidSpec(digest.url.spec())
          .IsValidAndroidFacetURI()) {
    result.url = std::move(digest.url);
  } else {
    // GetOrigin() will return an empty GURL if the origin is not valid or
    // standard. DCHECK that this will not happen.
    DCHECK(digest.url.is_valid());
    DCHECK(digest.url.IsStandard());
    result.url = digest.url.GetOrigin();
  }
  return result;
}

bool CanUseBiometricAuth(
    password_manager::BiometricAuthenticator* authenticator) {
  return authenticator &&
         authenticator->CanAuthenticate() ==
             password_manager::BiometricsAvailability::kAvailable &&
         base::FeatureList::IsEnabled(
             password_manager::features::kBiometricTouchToFill);
}

}  // namespace password_manager_util
