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
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/copy_migrator.h"
#include "chrome/browser/ash/crosapi/move_migrator.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/chrome_paths.h"
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

base::RepeatingClosure* g_attempt_restart = nullptr;

// Checks if the disk space is enough to run profile migration.
// Returns the bytes required to be freed. Specifically, on success
// returns 0.
uint64_t DiskCheck(const base::FilePath& profile_data_dir) {
  using browser_data_migrator_util::GetTargetItems;
  using browser_data_migrator_util::ItemType;
  using browser_data_migrator_util::TargetItems;
  TargetItems lacros_items =
      GetTargetItems(profile_data_dir, ItemType::kLacros);
  TargetItems need_copy_items =
      GetTargetItems(profile_data_dir, ItemType::kNeedCopyForMove);
  TargetItems deletable_items =
      GetTargetItems(profile_data_dir, ItemType::kDeletable);

  int64_t required_size = need_copy_items.total_size;
  if (!base::FeatureList::IsEnabled(kLacrosMoveProfileMigration))
    required_size += lacros_items.total_size;
  required_size -= deletable_items.total_size;

  return browser_data_migrator_util::ExtraBytesRequiredToBeFreed(
      required_size, profile_data_dir);
}

}  // namespace

ScopedRestartAttemptForTesting::ScopedRestartAttemptForTesting(
    base::RepeatingClosure callback) {
  DCHECK(!g_attempt_restart);
  g_attempt_restart = new base::RepeatingClosure(std::move(callback));
}

ScopedRestartAttemptForTesting::~ScopedRestartAttemptForTesting() {
  DCHECK(g_attempt_restart);
  delete g_attempt_restart;
  g_attempt_restart = nullptr;
}

bool BrowserDataMigratorImpl::MaybeForceResumeMoveMigration(
    PrefService* local_state,
    const AccountId& account_id,
    const std::string& user_id_hash) {
  LOG(WARNING) << "MaybeForceResumeMoveMigration() is called.";
  if (!MoveMigrator::ResumeRequired(local_state, user_id_hash))
    return false;

  return RestartToMigrate(account_id, user_id_hash, local_state);
}

// static
void BrowserDataMigratorImpl::AttemptRestart() {
  if (g_attempt_restart) {
    g_attempt_restart->Run();
    return;
  }

  chrome::AttemptRestart();
}

// static
bool BrowserDataMigratorImpl::MaybeRestartToMigrate(
    const AccountId& account_id,
    const std::string& user_id_hash,
    crosapi::browser_util::PolicyInitState policy_init_state) {
  if (!MaybeRestartToMigrateInternal(account_id, user_id_hash,
                                     policy_init_state)) {
    return false;
  }
  return RestartToMigrate(account_id, user_id_hash,
                          user_manager::UserManager::Get()->GetLocalState());
}

void BrowserDataMigratorImpl::MaybeRestartToMigrateWithDiskCheck(
    const AccountId& account_id,
    const std::string& user_id_hash,
    base::OnceCallback<void(bool, const absl::optional<uint64_t>&)> callback) {
  if (!MaybeRestartToMigrateInternal(
          account_id, user_id_hash,
          crosapi::browser_util::PolicyInitState::kAfterInit)) {
    std::move(callback).Run(false, absl::nullopt);
    return;
  }

  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    LOG(DFATAL) << "Could not get the original user data dir path.";
    std::move(callback).Run(false, absl::nullopt);
    return;
  }

  const base::FilePath profile_data_dir =
      user_data_dir.Append(ProfileHelper::GetUserProfileDir(user_id_hash));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&DiskCheck, profile_data_dir),
      base::BindOnce(&BrowserDataMigratorImpl::
                         MaybeRestartToMigrateWithDiskCheckAfterDiskCheck,
                     account_id, user_id_hash, std::move(callback)));
}

void BrowserDataMigratorImpl::MaybeRestartToMigrateWithDiskCheckAfterDiskCheck(
    const AccountId& account_id,
    const std::string& user_id_hash,
    base::OnceCallback<void(bool, const absl::optional<uint64_t>&)> callback,
    uint64_t required_size) {
  if (required_size > 0) {
    LOG(ERROR) << "Failed due to out of disk: " << required_size;
    std::move(callback).Run(false, required_size);
    return;
  }

  bool result =
      RestartToMigrate(account_id, user_id_hash,
                       user_manager::UserManager::Get()->GetLocalState());
  std::move(callback).Run(result, absl::nullopt);
}

bool BrowserDataMigratorImpl::MaybeRestartToMigrateInternal(
    const AccountId& account_id,
    const std::string& user_id_hash,
    crosapi::browser_util::PolicyInitState policy_init_state) {
  // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises, remove this
  // log message.
  LOG(WARNING) << "MaybeRestartToMigrateInternal() is called.";

  auto* user_manager = user_manager::UserManager::Get();
  auto* local_state = user_manager->GetLocalState();

  // If `MigrationStep` is not `kCheckStep`, `MaybeRestartToMigrate()` has
  // already moved on to later steps. Namely either in the middle of migration
  // or migration has already run.
  MigrationStep step = GetMigrationStep(local_state);
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
    return true;
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
    ClearMigrationAttemptCountForUser(local_state, user_id_hash);
    crosapi::browser_util::ClearProfileMigrationCompletedForUser(local_state,
                                                                 user_id_hash);
    MoveMigrator::ClearResumeStepForUser(local_state, user_id_hash);
    MoveMigrator::ClearResumeAttemptCountForUser(local_state, user_id_hash);
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
    crosapi::browser_util::RecordDataVer(local_state, user_id_hash,
                                         version_info::GetVersion());

    crosapi::browser_util::SetProfileMigrationCompletedForUser(local_state,
                                                               user_id_hash);
    // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises, remove
    // this log message.
    LOG(WARNING) << "Setting migration as completed since it is a new user.";
    return false;
  }

  int attempts = GetMigrationAttemptCountForUser(local_state, user_id_hash);
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
    return true;
  }

  if (crosapi::browser_util::IsProfileMigrationCompletedForUser(local_state,
                                                                user_id_hash)) {
    // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises,
    // remove this log message.
    LOG(WARNING) << "Profile migration has been completed already.";
    return false;
  }

  return true;
}

// static
bool BrowserDataMigratorImpl::RestartToMigrate(const AccountId& account_id,
                                               const std::string& user_id_hash,
                                               PrefService* local_state) {
  SetMigrationStep(local_state, MigrationStep::kRestartCalled);

  UpdateMigrationAttemptCountForUser(local_state, user_id_hash);

  crosapi::browser_util::ClearProfileMigrationCompletedForUser(local_state,
                                                               user_id_hash);

  local_state->CommitPendingWrite();

  // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises, remove
  // this log message.
  LOG(WARNING) << "Making a dbus method call to session_manager";
  bool success = SessionManagerClient::Get()->RequestBrowserDataMigration(
      cryptohome::CreateAccountIdentifierFromAccountId(account_id));

  // TODO(crbug.com/1261730): Add an UMA.
  if (!success)
    return false;

  AttemptRestart();
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

  if (base::FeatureList::IsEnabled(kLacrosMoveProfileMigration) ||
      MoveMigrator::ResumeRequired(local_state_, user_id_hash_)) {
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
