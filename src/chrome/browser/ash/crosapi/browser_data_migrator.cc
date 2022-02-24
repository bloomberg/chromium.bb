// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_migrator.h"

#include <string>
#include <utility>

#include "ash/components/cryptohome/cryptohome_parameters.h"
#include "ash/constants/ash_switches.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/copy_migrator.h"
#include "chrome/browser/ash/crosapi/move_migrator.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"

namespace ash {
namespace {
// Flag values for `switches::kForceBrowserDataMigrationForTesting`.
const char kBrowserDataMigrationForceSkip[] = "force-skip";
const char kBrowserDataMigrationForceMigration[] = "force-migration";
}  // namespace

// static
bool BrowserDataMigratorImpl::MaybeRestartToMigrate(
    const AccountId& account_id,
    const std::string& user_id_hash,
    crosapi::browser_util::PolicyInitState policy_init_state) {
  // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises, remove this
  // log message.
  LOG(WARNING) << "MaybeRestartToMigrate() is called.";
  // If `MigrationStep` is not `kCheckStep`, `MaybeRestartToMigrate()` has
  // already moved on to later steps. Namely either in the middle of migration
  // or migration has already run.
  MigrationStep step = GetMigrationStep(g_browser_process->local_state());
  if (step != MigrationStep::kCheckStep) {
    switch (step) {
      case MigrationStep::kRestartCalled:
        LOG(ERROR) << "RestartToMigrate() was called but Migrate() was not. "
                      "This indicates that eitehr "
                      "SessionManagerClient::RequestBrowserDataMigration() "
                      "failed or ash crashed before reaching Migrate(). Check "
                      "the previous chrome log and the one before.";
        break;
      case MigrationStep::kStarted:
        LOG(ERROR) << "Migrate() was called but "
                      "MigrateInternalFinishedUIThread() was not indicating "
                      "that ash might have crashed during the migration.";
        break;
      case MigrationStep::kEnded:
      default:
        // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises,
        // remove
        // this log message or reduce to VLOG(1).
        LOG(WARNING)
            << "Migration has ended and either completed or failed. step = "
            << static_cast<int>(step);
        break;
    }

    return false;
  }

  // Check if the switch for testing is present.
  const std::string force_migration_switch =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kForceBrowserDataMigrationForTesting);
  if (force_migration_switch == kBrowserDataMigrationForceSkip)
    return false;
  if (force_migration_switch == kBrowserDataMigrationForceMigration) {
    LOG(WARNING) << "`kBrowserDataMigrationForceMigration` switch is present.";
    return RestartToMigrate(account_id, user_id_hash);
  }

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  // Check if user exists i.e. not a guest session.
  if (!user)
    return false;
  // Check if lacros is enabled. If not immediately return.
  if (!crosapi::browser_util::IsLacrosEnabledForMigration(user,
                                                          policy_init_state)) {
    // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises, remove
    // this log message.
    LOG(WARNING)
        << "Lacros is disabled. Call ClearMigrationAttemptCountForUser() so "
           "that the migration can be attempted again after once lacros is "
           "enabled again.";

    // If lacros is not enabled other than reaching the maximum retry count of
    // profile migration, clear the retry count and
    // `kProfileMigrationCompletedForUserPref`. This will allow users to retry
    // profile migration after disabling and re-enabling lacros.
    ClearMigrationAttemptCountForUser(g_browser_process->local_state(),
                                      user_id_hash);
    crosapi::browser_util::ClearProfileMigrationCompletedForUser(
        g_browser_process->local_state(), user_id_hash);
    return false;
  }

  //  Currently we turn on profile migration only for Googlers. To test profile
  //  migration without @google.com account, enable feature
  //  `kLacrosProfileMigrationForAnyUser` defined in browser_util.
  // TODO(crbug.com/1266669): Remove this check once profile migration is
  // enabled for all users.
  if (!crosapi::browser_util::IsProfileMigrationEnabled(account_id)) {
    // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises, remove
    // this log message.
    LOG(WARNING) << "Profile migration is disabled.";
    return false;
  }

  // If the user is a new user, then there shouldn't be anything to migrate.
  // Also mark the user as migration completed.
  if (user_manager::UserManager::Get()->IsCurrentUserNew()) {
    crosapi::browser_util::RecordDataVer(g_browser_process->local_state(),
                                         user_id_hash,
                                         version_info::GetVersion());

    crosapi::browser_util::SetProfileMigrationCompletedForUser(
        g_browser_process->local_state(), user_id_hash);
    // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises, remove
    // this log message.
    LOG(WARNING) << "Setting migration as completed since it is a new user.";
    return false;
  }

  int attempts = GetMigrationAttemptCountForUser(
      g_browser_process->local_state(), user_id_hash);
  // TODO(crbug.com/1178702): Once BrowserDataMigrator stabilises, reduce the
  // log level to VLOG(1).
  LOG(WARNING) << "Attempt #" << attempts;
  if (attempts >= kMaxMigrationAttemptCount) {
    // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises, remove
    // this log message.
    LOG(WARNING) << "Skipping profile migration since migration attemp count = "
                 << attempts << " has exceeded " << kMaxMigrationAttemptCount;
    return false;
  }

  if (crosapi::browser_util::IsDataWipeRequired(user_id_hash)) {
    // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises, remove
    // this log message.
    LOG(WARNING)
        << "Restarting to run profile migration since data wipe is required.";
    // If data wipe is required, no need for a further check to determine if
    // lacros data dir exists or not.
    return RestartToMigrate(account_id, user_id_hash);
  }

  if (crosapi::browser_util::IsProfileMigrationCompletedForUser(
          g_browser_process->local_state(), user_id_hash)) {
    // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises,
    // remove this log message.
    LOG(WARNING) << "Profile migration has been completed already.";
    return false;
  }

  return RestartToMigrate(account_id, user_id_hash);
}

// static
bool BrowserDataMigratorImpl::RestartToMigrate(
    const AccountId& account_id,
    const std::string& user_id_hash) {
  SetMigrationStep(g_browser_process->local_state(),
                   MigrationStep::kRestartCalled);

  UpdateMigrationAttemptCountForUser(g_browser_process->local_state(),
                                     user_id_hash);

  crosapi::browser_util::ClearProfileMigrationCompletedForUser(
      g_browser_process->local_state(), user_id_hash);

  g_browser_process->local_state()->CommitPendingWrite();

  // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises, remove
  // this log message.
  LOG(WARNING) << "Making a dbus method call to session_manager";
  bool success = SessionManagerClient::Get()->RequestBrowserDataMigration(
      cryptohome::CreateAccountIdentifierFromAccountId(account_id));

  if (!success)
    return false;

  chrome::AttemptRestart();
  return true;
}

BrowserDataMigratorImpl::BrowserDataMigratorImpl(
    const base::FilePath& original_profile_dir,
    const std::string& user_id_hash,
    const ProgressCallback& progress_callback,
    PrefService* local_state)
    : original_profile_dir_(original_profile_dir),
      user_id_hash_(user_id_hash),
      progress_tracker_(
          std::make_unique<MigrationProgressTrackerImpl>(progress_callback)),
      cancel_flag_(
          base::MakeRefCounted<browser_data_migrator_util::CancelFlag>()),
      local_state_(local_state) {
  DCHECK(local_state_);
}

BrowserDataMigratorImpl::~BrowserDataMigratorImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BrowserDataMigratorImpl::Migrate(MigrateCallback callback) {
  DCHECK(local_state_);
  DCHECK(completion_callback_.is_null());
  completion_callback_ = std::move(callback);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1178702): Once BrowserDataMigrator stabilises, reduce the
  // log level to VLOG(1).
  LOG(WARNING) << "BrowserDataMigratorImpl::Migrate() is called.";

  DCHECK(GetMigrationStep(local_state_) == MigrationStep::kRestartCalled);
  SetMigrationStep(local_state_, MigrationStep::kStarted);

  if (base::FeatureList::IsEnabled(kLacrosMoveProfileMigration)) {
    LOG(WARNING) << "Initializing MoveMigrator.";
    migrator_delegate_ = std::make_unique<MoveMigrator>(
        original_profile_dir_, user_id_hash_, std::move(progress_tracker_),
        cancel_flag_, local_state_,
        base::BindOnce(
            &BrowserDataMigratorImpl::MigrateInternalFinishedUIThread,
            weak_factory_.GetWeakPtr()));
  } else {
    LOG(WARNING) << "Initializing CopyMigrator.";
    migrator_delegate_ = std::make_unique<CopyMigrator>(
        original_profile_dir_, user_id_hash_, std::move(progress_tracker_),
        cancel_flag_,
        base::BindOnce(
            &BrowserDataMigratorImpl::MigrateInternalFinishedUIThread,
            weak_factory_.GetWeakPtr()));
  }

  migrator_delegate_->Migrate();
}

void BrowserDataMigratorImpl::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cancel_flag_->Set();
}

void BrowserDataMigratorImpl::MigrateInternalFinishedUIThread(
    MigrationResult result) {
  DCHECK(local_state_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetMigrationStep(local_state_) == MigrationStep::kStarted);
  SetMigrationStep(local_state_, MigrationStep::kEnded);

  // TODO(crbug.com/1178702): Once BrowserDataMigrator stabilises, reduce the
  // log level to VLOG(1).
  LOG(WARNING)
      << "MigrateInternalFinishedUIThread() called with results data wipe = "
      << static_cast<int>(result.data_wipe_result) << " and migration "
      << static_cast<int>(result.data_migration_result.kind);

  if (result.data_wipe_result != DataWipeResult::kFailed) {
    // kSkipped means that the directory did not exist so record the current
    // version as the data version.
    crosapi::browser_util::RecordDataVer(local_state_, user_id_hash_,
                                         version_info::GetVersion());
  }

  if (result.data_migration_result.kind == ResultKind::kSucceeded) {
    crosapi::browser_util::SetProfileMigrationCompletedForUser(local_state_,
                                                               user_id_hash_);

    ClearMigrationAttemptCountForUser(local_state_, user_id_hash_);
  }
  // If migration has failed or skipped, we silently relaunch ash and send them
  // to their home screen. In that case lacros will be disabled.

  local_state_->CommitPendingWrite();

  std::move(completion_callback_).Run(result.data_migration_result);
}

// static
void BrowserDataMigratorImpl::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kMigrationStep,
                                static_cast<int>(MigrationStep::kCheckStep));
  registry->RegisterDictionaryPref(kMigrationAttemptCountPref,
                                   base::DictionaryValue());
  // Register prefs for move migration.
  MoveMigrator::RegisterLocalStatePrefs(registry);
}

// static
void BrowserDataMigratorImpl::SetMigrationStep(
    PrefService* local_state,
    BrowserDataMigratorImpl::MigrationStep step) {
  local_state->SetInteger(kMigrationStep, static_cast<int>(step));
}

// static
void BrowserDataMigratorImpl::ClearMigrationStep(PrefService* local_state) {
  local_state->ClearPref(kMigrationStep);
}

// static
BrowserDataMigratorImpl::MigrationStep
BrowserDataMigratorImpl::GetMigrationStep(PrefService* local_state) {
  return static_cast<MigrationStep>(local_state->GetInteger(kMigrationStep));
}

// static
void BrowserDataMigratorImpl::UpdateMigrationAttemptCountForUser(
    PrefService* local_state,
    const std::string& user_id_hash) {
  int count = GetMigrationAttemptCountForUser(local_state, user_id_hash);
  count += 1;
  DictionaryPrefUpdate update(local_state, kMigrationAttemptCountPref);
  base::Value* dict = update.Get();
  dict->SetIntKey(user_id_hash, count);
}

// static
int BrowserDataMigratorImpl::GetMigrationAttemptCountForUser(
    PrefService* local_state,
    const std::string& user_id_hash) {
  return local_state->GetDictionary(kMigrationAttemptCountPref)
      ->FindIntPath(user_id_hash)
      .value_or(0);
}

// static
void BrowserDataMigratorImpl::ClearMigrationAttemptCountForUser(
    PrefService* local_state,
    const std::string& user_id_hash) {
  DictionaryPrefUpdate update(local_state, kMigrationAttemptCountPref);
  base::Value* dict = update.Get();
  dict->RemoveKey(user_id_hash);
}

}  // namespace ash
