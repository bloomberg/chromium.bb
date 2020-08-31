// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_in_process.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/installer.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/registration_data.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"

namespace updater {

namespace {

// The functions below are various adaptors between |update_client| and
// |UpdateService| types.
update_client::Callback MakeUpdateClientCallback(
    UpdateService::Callback callback) {
  return base::BindOnce(
      [](UpdateService::Callback callback, update_client::Error error) {
        std::move(callback).Run(static_cast<UpdateService::Result>(error));
      },
      std::move(callback));
}

UpdateService::UpdateState::State ToUpdateState(
    update_client::ComponentState component_state) {
  switch (component_state) {
    case update_client::ComponentState::kNew:
      return UpdateService::UpdateState::State::kNotStarted;

    case update_client::ComponentState::kChecking:
      return UpdateService::UpdateState::State::kCheckingForUpdates;

    case update_client::ComponentState::kDownloading:
    case update_client::ComponentState::kDownloadingDiff:
    case update_client::ComponentState::kDownloaded:
      return UpdateService::UpdateState::State::kDownloading;

    case update_client::ComponentState::kCanUpdate:
      return UpdateService::UpdateState::State::kUpdateAvailable;

    case update_client::ComponentState::kUpdating:
    case update_client::ComponentState::kUpdatingDiff:
      return UpdateService::UpdateState::State::kInstalling;

    case update_client::ComponentState::kUpdated:
      return UpdateService::UpdateState::State::kUpdated;

    case update_client::ComponentState::kUpToDate:
      return UpdateService::UpdateState::State::kNoUpdate;

    case update_client::ComponentState::kUpdateError:
      return UpdateService::UpdateState::State::kUpdateError;

    case update_client::ComponentState::kUninstalled:
    case update_client::ComponentState::kRun:
    case update_client::ComponentState::kLastStatus:
      NOTREACHED();
      return UpdateService::UpdateState::State::kUnknown;
  }
}

UpdateService::ErrorCategory ToErrorCategory(
    update_client::ErrorCategory error_category) {
  switch (error_category) {
    case update_client::ErrorCategory::kNone:
      return UpdateService::ErrorCategory::kNone;
    case update_client::ErrorCategory::kDownload:
      return UpdateService::ErrorCategory::kDownload;
    case update_client::ErrorCategory::kUnpack:
      return UpdateService::ErrorCategory::kUnpack;
    case update_client::ErrorCategory::kInstall:
      return UpdateService::ErrorCategory::kInstall;
    case update_client::ErrorCategory::kService:
      return UpdateService::ErrorCategory::kService;
    case update_client::ErrorCategory::kUpdateCheck:
      return UpdateService::ErrorCategory::kUpdateCheck;
  }
}

update_client::UpdateClient::CrxStateChangeCallback
MakeUpdateClientCrxStateChangeCallback(
    UpdateService::StateChangeCallback callback) {
  return base::BindRepeating(
      [](UpdateService::StateChangeCallback callback,
         update_client::CrxUpdateItem crx_update_item) {
        UpdateService::UpdateState update_state;
        update_state.app_id = crx_update_item.id;
        update_state.state = ToUpdateState(crx_update_item.state);
        update_state.next_version = crx_update_item.next_version;
        update_state.downloaded_bytes = crx_update_item.downloaded_bytes;
        update_state.total_bytes = crx_update_item.total_bytes;
        update_state.error_category =
            ToErrorCategory(crx_update_item.error_category);
        update_state.error_code = crx_update_item.error_code;
        update_state.extra_code1 = crx_update_item.extra_code1;
        callback.Run(update_state);
      },
      callback);
}

std::vector<base::Optional<update_client::CrxComponent>> GetComponents(
    scoped_refptr<PersistedData> persisted_data,
    const std::vector<std::string>& ids) {
  std::vector<base::Optional<update_client::CrxComponent>> components;
  for (const auto& id : ids) {
    components.push_back(base::MakeRefCounted<Installer>(id, persisted_data)
                             ->MakeCrxComponent());
  }
  return components;
}

}  // namespace

UpdateServiceInProcess::UpdateServiceInProcess(
    scoped_refptr<update_client::Configurator> config)
    : config_(config),
      persisted_data_(
          base::MakeRefCounted<PersistedData>(config_->GetPrefService())),
      main_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      update_client_(update_client::UpdateClientFactory(config)) {}

void UpdateServiceInProcess::RegisterApp(
    const RegistrationRequest& request,
    base::OnceCallback<void(const RegistrationResponse&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  persisted_data_->RegisterApp(request);

  // Result of registration. Currently there's no error handling in
  // PersistedData, so we assume success every time, which is why we respond
  // with 0.
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), RegistrationResponse(0)));
}

void UpdateServiceInProcess::UpdateAll(StateChangeCallback state_update,
                                       Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto app_ids = persisted_data_->GetAppIds();
  DCHECK(base::Contains(app_ids, kUpdaterAppId));

  update_client_->Update(app_ids,
                         base::BindOnce(&GetComponents, persisted_data_),
                         MakeUpdateClientCrxStateChangeCallback(state_update),
                         false, MakeUpdateClientCallback(std::move(callback)));
}

void UpdateServiceInProcess::Update(const std::string& app_id,
                                    Priority priority,
                                    StateChangeCallback state_update,
                                    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  update_client_->Update({app_id},
                         base::BindOnce(&GetComponents, persisted_data_),
                         MakeUpdateClientCrxStateChangeCallback(state_update),
                         priority == Priority::kForeground,
                         MakeUpdateClientCallback(std::move(callback)));
}

void UpdateServiceInProcess::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PrefsCommitPendingWrites(config_->GetPrefService());
}

UpdateServiceInProcess::~UpdateServiceInProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_->GetPrefService()->SchedulePendingLossyWrites();
}

}  // namespace updater
