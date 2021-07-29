// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/prefs.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/prefs_impl.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/update_client/update_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

namespace {

const char kPrefQualified[] = "qualified";
const char kPrefSwapping[] = "swapping";
const char kPrefActiveVersion[] = "active_version";
const char kPrefServerStarts[] = "server_starts";

}  // namespace

const char kPrefUpdateTime[] = "update_time";

UpdaterPrefsImpl::UpdaterPrefsImpl(std::unique_ptr<ScopedPrefsLock> lock,
                                   std::unique_ptr<PrefService> prefs)
    : lock_(std::move(lock)), prefs_(std::move(prefs)) {}

UpdaterPrefsImpl::~UpdaterPrefsImpl() = default;

PrefService* UpdaterPrefsImpl::GetPrefService() const {
  return prefs_.get();
}

bool UpdaterPrefsImpl::GetQualified() const {
  return prefs_->GetBoolean(kPrefQualified);
}

void UpdaterPrefsImpl::SetQualified(bool value) {
  prefs_->SetBoolean(kPrefQualified, value);
}

std::string UpdaterPrefsImpl::GetActiveVersion() const {
  return prefs_->GetString(kPrefActiveVersion);
}

void UpdaterPrefsImpl::SetActiveVersion(std::string value) {
  prefs_->SetString(kPrefActiveVersion, value);
}

bool UpdaterPrefsImpl::GetSwapping() const {
  return prefs_->GetBoolean(kPrefSwapping);
}

void UpdaterPrefsImpl::SetSwapping(bool value) {
  prefs_->SetBoolean(kPrefSwapping, value);
}

int UpdaterPrefsImpl::CountServerStarts() {
  int starts = prefs_->GetInteger(kPrefServerStarts);
  if (starts <= kMaxServerStartsBeforeFirstReg)
    prefs_->SetInteger(kPrefServerStarts, ++starts);
  return starts;
}

scoped_refptr<GlobalPrefs> CreateGlobalPrefs(UpdaterScope scope) {
  std::unique_ptr<ScopedPrefsLock> lock =
      AcquireGlobalPrefsLock(scope, base::TimeDelta::FromMinutes(2));
  if (!lock)
    return nullptr;

  const absl::optional<base::FilePath> global_prefs_dir =
      GetBaseDirectory(scope);
  if (!global_prefs_dir)
    return nullptr;
  VLOG(1) << "global_prefs_dir: " << global_prefs_dir;

  PrefServiceFactory pref_service_factory;
  pref_service_factory.set_user_prefs(base::MakeRefCounted<JsonPrefStore>(
      global_prefs_dir->Append(FILE_PATH_LITERAL("prefs.json"))));

  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
  update_client::RegisterPrefs(pref_registry.get());
  pref_registry->RegisterBooleanPref(kPrefSwapping, false);
  pref_registry->RegisterStringPref(kPrefActiveVersion, "0");
  pref_registry->RegisterTimePref(kPrefUpdateTime, base::Time());
  pref_registry->RegisterIntegerPref(kPrefServerStarts, 0);

  return base::MakeRefCounted<UpdaterPrefsImpl>(
      std::move(lock), pref_service_factory.Create(pref_registry));
}

scoped_refptr<LocalPrefs> CreateLocalPrefs(UpdaterScope scope) {
  const absl::optional<base::FilePath> local_prefs_dir =
      GetVersionedDirectory(scope);
  if (!local_prefs_dir)
    return nullptr;

  PrefServiceFactory pref_service_factory;
  pref_service_factory.set_user_prefs(base::MakeRefCounted<JsonPrefStore>(
      local_prefs_dir->Append(FILE_PATH_LITERAL("prefs.json"))));

  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
  update_client::RegisterPrefs(pref_registry.get());
  pref_registry->RegisterBooleanPref(kPrefQualified, false);
  pref_registry->RegisterTimePref(kPrefUpdateTime, base::Time());

  return base::MakeRefCounted<UpdaterPrefsImpl>(
      nullptr, pref_service_factory.Create(pref_registry));
}

void PrefsCommitPendingWrites(PrefService* pref_service) {
  // Waits in the run loop until pending writes complete.
  base::RunLoop runloop;
  pref_service->CommitPendingWrite(base::BindOnce(
      [](base::OnceClosure quit_closure) { std::move(quit_closure).Run(); },
      runloop.QuitWhenIdleClosure()));
  runloop.Run();
}

}  // namespace updater
