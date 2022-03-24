// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_managed_app_manager.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace web_app {

ExternallyManagedAppManager::InstallResult::InstallResult() = default;

ExternallyManagedAppManager::InstallResult::InstallResult(
    InstallResultCode code,
    absl::optional<AppId> app_id,
    bool did_uninstall_and_replace)
    : code(code),
      app_id(std::move(app_id)),
      did_uninstall_and_replace(did_uninstall_and_replace) {}

ExternallyManagedAppManager::InstallResult::InstallResult(
    const InstallResult&) = default;

ExternallyManagedAppManager::InstallResult::~InstallResult() = default;

bool ExternallyManagedAppManager::InstallResult::operator==(
    const InstallResult& other) const {
  return std::tie(code, app_id, did_uninstall_and_replace) ==
         std::tie(other.code, other.app_id, other.did_uninstall_and_replace);
}

ExternallyManagedAppManager::SynchronizeRequest::SynchronizeRequest(
    SynchronizeCallback callback,
    std::vector<ExternalInstallOptions> pending_installs,
    int remaining_uninstall_requests)
    : callback(std::move(callback)),
      remaining_install_requests(pending_installs.size()),
      pending_installs(std::move(pending_installs)),
      remaining_uninstall_requests(remaining_uninstall_requests) {}

ExternallyManagedAppManager::SynchronizeRequest::~SynchronizeRequest() =
    default;

ExternallyManagedAppManager::SynchronizeRequest&
ExternallyManagedAppManager::SynchronizeRequest::operator=(
    ExternallyManagedAppManager::SynchronizeRequest&&) = default;

ExternallyManagedAppManager::SynchronizeRequest::SynchronizeRequest(
    SynchronizeRequest&& other) = default;

ExternallyManagedAppManager::ExternallyManagedAppManager() = default;

ExternallyManagedAppManager::~ExternallyManagedAppManager() {
  DCHECK(!registration_callback_);
}

void ExternallyManagedAppManager::SetSubsystems(
    WebAppRegistrar* registrar,
    OsIntegrationManager* os_integration_manager,
    WebAppUiManager* ui_manager,
    WebAppInstallFinalizer* finalizer,
    WebAppInstallManager* install_manager) {
  registrar_ = registrar;
  os_integration_manager_ = os_integration_manager;
  ui_manager_ = ui_manager;
  finalizer_ = finalizer;
  install_manager_ = install_manager;
}

void ExternallyManagedAppManager::SynchronizeInstalledApps(
    std::vector<ExternalInstallOptions> desired_apps_install_options,
    ExternalInstallSource install_source,
    SynchronizeCallback callback) {
  DCHECK(registrar_);
  DCHECK(std::all_of(
      desired_apps_install_options.begin(), desired_apps_install_options.end(),
      [&install_source](const ExternalInstallOptions& install_options) {
        return install_options.install_source == install_source;
      }));
  // Only one concurrent SynchronizeInstalledApps() expected per
  // ExternalInstallSource.
  DCHECK(!base::Contains(synchronize_requests_, install_source));

  std::vector<GURL> installed_urls;
  for (auto apps_it : registrar_->GetExternallyInstalledApps(install_source))
    installed_urls.push_back(apps_it.second);

  std::sort(installed_urls.begin(), installed_urls.end());

  std::vector<GURL> desired_urls;
  for (const auto& info : desired_apps_install_options)
    desired_urls.push_back(info.install_url);

  std::sort(desired_urls.begin(), desired_urls.end());

  auto urls_to_remove =
      base::STLSetDifference<std::vector<GURL>>(installed_urls, desired_urls);

  // Run callback immediately if there's no work to be done.
  if (urls_to_remove.empty() && desired_apps_install_options.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::map<GURL, InstallResult>(),
                       std::map<GURL, bool>()));
    return;
  }

  // Add the callback to a map and call once all installs/uninstalls finish.
  synchronize_requests_.insert_or_assign(
      install_source,
      SynchronizeRequest(std::move(callback),
                         std::move(desired_apps_install_options),
                         urls_to_remove.size()));

  if (urls_to_remove.empty()) {
    // If there are no uninstalls, this will trigger the installs.
    ContinueOrCompleteSynchronization(install_source);
  } else {
    UninstallApps(
        urls_to_remove, install_source,
        base::BindRepeating(
            &ExternallyManagedAppManager::UninstallForSynchronizeCallback,
            weak_ptr_factory_.GetWeakPtr(), install_source));
  }
}

void ExternallyManagedAppManager::SetRegistrationCallbackForTesting(
    RegistrationCallback callback) {
  registration_callback_ = callback;
}

void ExternallyManagedAppManager::ClearRegistrationCallbackForTesting() {
  registration_callback_ = RegistrationCallback();
}

void ExternallyManagedAppManager::SetRegistrationsCompleteCallbackForTesting(
    base::OnceClosure callback) {
  registrations_complete_callback_ = std::move(callback);
}

void ExternallyManagedAppManager::OnRegistrationFinished(
    const GURL& install_url,
    RegistrationResultCode result) {
  if (registration_callback_)
    registration_callback_.Run(install_url, result);
}

void ExternallyManagedAppManager::InstallForSynchronizeCallback(
    ExternalInstallSource source,
    const GURL& app_url,
    ExternallyManagedAppManager::InstallResult result) {
  if (!IsSuccess(result.code)) {
    LOG(ERROR) << app_url << " from install source " << static_cast<int>(source)
               << " failed to install with reason "
               << static_cast<int>(result.code);
  }

  auto source_and_request = synchronize_requests_.find(source);
  DCHECK(source_and_request != synchronize_requests_.end());
  SynchronizeRequest& request = source_and_request->second;
  request.install_results[app_url] = result;
  --request.remaining_install_requests;
  DCHECK_GE(request.remaining_install_requests, 0);

  ContinueOrCompleteSynchronization(source);
}

void ExternallyManagedAppManager::UninstallForSynchronizeCallback(
    ExternalInstallSource source,
    const GURL& app_url,
    bool succeeded) {
  auto source_and_request = synchronize_requests_.find(source);
  DCHECK(source_and_request != synchronize_requests_.end());
  SynchronizeRequest& request = source_and_request->second;
  request.uninstall_results[app_url] = succeeded;
  --request.remaining_uninstall_requests;
  DCHECK_GE(request.remaining_uninstall_requests, 0);

  ContinueOrCompleteSynchronization(source);
}

void ExternallyManagedAppManager::ContinueOrCompleteSynchronization(
    ExternalInstallSource source) {
  auto source_and_request = synchronize_requests_.find(source);
  DCHECK(source_and_request != synchronize_requests_.end());

  SynchronizeRequest& request = source_and_request->second;

  if (request.remaining_uninstall_requests > 0)
    return;

  // Installs only take place after all uninstalls.
  if (!request.pending_installs.empty()) {
    DCHECK_GT(request.remaining_install_requests, 0);
    // Note: It is intentional that std::move(request.pending_installs) clears
    // the vector in `request`, preventing this branch from triggering again.
    InstallApps(std::move(request.pending_installs),
                base::BindRepeating(
                    &ExternallyManagedAppManager::InstallForSynchronizeCallback,
                    weak_ptr_factory_.GetWeakPtr(), source));
    return;
  }

  if (request.remaining_install_requests > 0)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(request.callback),
                                std::move(request.install_results),
                                std::move(request.uninstall_results)));
  synchronize_requests_.erase(source);
}
void ExternallyManagedAppManager::ClearSynchronizeRequestsForTesting() {
  synchronize_requests_.erase(synchronize_requests_.begin(),
                              synchronize_requests_.end());
}

}  // namespace web_app
