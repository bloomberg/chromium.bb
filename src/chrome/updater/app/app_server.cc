// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_server.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/version.h"
#include "chrome/updater/app/app_utils.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_impl.h"
#include "chrome/updater/update_service_impl_inactive.h"
#include "chrome/updater/update_service_internal.h"
#include "chrome/updater/update_service_internal_impl.h"
#include "chrome/updater/update_service_internal_impl_inactive.h"
#include "chrome/updater/update_service_internal_impl_qualifying.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "components/prefs/pref_service.h"

namespace updater {

namespace {

bool IsInternalService() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             kServerServiceSwitch) == kServerUpdateServiceInternalSwitchValue;
}

}  // namespace

AppServer::AppServer() : external_constants_(CreateExternalConstants()) {}

AppServer::~AppServer() = default;

void AppServer::Initialize() {
  first_task_ = ModeCheck();
}

base::OnceClosure AppServer::ModeCheck() {
  scoped_refptr<GlobalPrefs> global_prefs = CreateGlobalPrefs(updater_scope());
  if (!global_prefs) {
    return base::BindOnce(&AppServer::Shutdown, this,
                          kErrorFailedToLockPrefsMutex);
  }

  const base::Version this_version(kUpdaterVersion);
  const base::Version active_version(global_prefs->GetActiveVersion());

  VLOG(2) << "This version: " << this_version.GetString()
          << ", active version: " << active_version.GetString();

  if (this_version < active_version) {
    global_prefs = nullptr;
    uninstall_self_ = true;
    if (IsInternalService()) {
      return base::BindOnce(&AppServer::ActiveDutyInternal, this,
                            MakeInactiveUpdateServiceInternal());
    }
    return base::BindOnce(&AppServer::ActiveDuty, this,
                          MakeInactiveUpdateService());
  }

  if (active_version != base::Version("0") && active_version != this_version) {
    scoped_refptr<LocalPrefs> local_prefs = CreateLocalPrefs(updater_scope());
    if (!local_prefs->GetQualified()) {
      global_prefs = nullptr;
      prefs_ = local_prefs;
      return IsInternalService()
                 ? base::BindOnce(&AppServer::ActiveDutyInternal, this,
                                  MakeQualifyingUpdateServiceInternal(
                                      base::MakeRefCounted<Configurator>(
                                          prefs_, external_constants_),
                                      local_prefs))
                 : base::BindOnce(&AppServer::ActiveDuty, this,
                                  MakeInactiveUpdateService());
    }
  }

  if (this_version > active_version || global_prefs->GetSwapping()) {
    if (!SwapVersions(global_prefs.get()))
      return base::BindOnce(&AppServer::Shutdown, this, kErrorFailedToSwap);
  }

  if (IsInternalService()) {
    prefs_ = CreateLocalPrefs(updater_scope());
    return base::BindOnce(&AppServer::ActiveDutyInternal, this,
                          base::MakeRefCounted<UpdateServiceInternalImpl>());
  }

  server_starts_ = global_prefs->CountServerStarts();
  prefs_ = global_prefs;
  return base::BindOnce(
      &AppServer::ActiveDuty, this,
      base::MakeRefCounted<UpdateServiceImpl>(
          base::MakeRefCounted<Configurator>(prefs_, external_constants_)));
}

void AppServer::Uninitialize() {
  if (prefs_)
    PrefsCommitPendingWrites(prefs_->GetPrefService());
  if (uninstall_self_) {
    VLOG(1) << "Uninstalling version " << kUpdaterVersion;
    UninstallSelf();
  } else {
    MaybeUninstall();
  }
}

void AppServer::MaybeUninstall() {
  if (!prefs_)
    return;

  if (ShouldUninstall(
          base::MakeRefCounted<PersistedData>(prefs_->GetPrefService())
              ->GetAppIds(),
          server_starts_)) {
    base::CommandLine command_line(
        base::CommandLine::ForCurrentProcess()->GetProgram());
    command_line.AppendSwitch(kUninstallIfUnusedSwitch);
    if (updater_scope() == UpdaterScope::kSystem)
      command_line.AppendSwitch(kSystemSwitch);
    command_line.AppendSwitch(kEnableLoggingSwitch);
    command_line.AppendSwitchASCII(kLoggingModuleSwitch,
                                   kLoggingModuleSwitchValue);
    VLOG(2) << "Launching uninstall command: "
            << command_line.GetCommandLineString();

    base::Process process = base::LaunchProcess(command_line, {});
    if (!process.IsValid()) {
      VLOG(2) << "Invalid process launching command: "
              << command_line.GetCommandLineString();
    }
  }
}

void AppServer::FirstTaskRun() {
  std::move(first_task_).Run();
}

bool AppServer::SwapVersions(GlobalPrefs* global_prefs) {
  global_prefs->SetSwapping(true);
  PrefsCommitPendingWrites(global_prefs->GetPrefService());
  if (!SwapRPCInterfaces())
    return false;
  if (!ConvertLegacyUpdaters(base::BindRepeating(
          &PersistedData::RegisterApp, base::MakeRefCounted<PersistedData>(
                                           global_prefs->GetPrefService())))) {
    return false;
  }
  global_prefs->SetActiveVersion(kUpdaterVersion);
  global_prefs->SetSwapping(false);
  PrefsCommitPendingWrites(global_prefs->GetPrefService());
  return true;
}

}  // namespace updater
