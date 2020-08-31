// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/update_service.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/browser/updater/extension_update_data.h"
#include "extensions/browser/updater/update_data_provider.h"
#include "extensions/browser/updater/update_service_factory.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_url_handlers.h"

namespace extensions {

namespace {

UpdateService* update_service_override = nullptr;

// This set contains all Omaha attributes that is associated with extensions.
constexpr const char* kOmahaAttributes[] = {"_malware"};

void SendUninstallPingCompleteCallback(update_client::Error error) {}

}  // namespace

UpdateService::InProgressUpdate::InProgressUpdate(base::OnceClosure callback,
                                                  bool install_immediately)
    : callback(std::move(callback)), install_immediately(install_immediately) {}
UpdateService::InProgressUpdate::~InProgressUpdate() = default;

UpdateService::InProgressUpdate::InProgressUpdate(
    UpdateService::InProgressUpdate&& other) = default;
UpdateService::InProgressUpdate& UpdateService::InProgressUpdate::operator=(
    UpdateService::InProgressUpdate&& other) = default;

// static
void UpdateService::SupplyUpdateServiceForTest(UpdateService* service) {
  update_service_override = service;
}

// static
UpdateService* UpdateService::Get(content::BrowserContext* context) {
  return update_service_override == nullptr
             ? UpdateServiceFactory::GetForBrowserContext(context)
             : update_service_override;
}

void UpdateService::Shutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (update_data_provider_) {
    update_data_provider_->Shutdown();
    update_data_provider_ = nullptr;
  }
  RemoveUpdateClientObserver(this);
  update_client_ = nullptr;
  browser_context_ = nullptr;
}

void UpdateService::SendUninstallPing(const std::string& id,
                                      const base::Version& version,
                                      int reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(update_client_);
  update_client_->SendUninstallPing(
      id, version, reason, base::BindOnce(&SendUninstallPingCompleteCallback));
}

bool UpdateService::CanUpdate(const std::string& extension_id) const {
  // Won't update extensions with empty IDs.
  if (extension_id.empty())
    return false;

  // We can only update extensions that have been installed on the system.
  const ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  const Extension* extension = registry->GetInstalledExtension(extension_id);
  if (extension == nullptr)
    return false;

  // Furthermore, we can only update extensions that were installed from the
  // default webstore or extensions with empty update URLs not converted from
  // user scripts.
  const GURL& update_url = ManifestURL::GetUpdateURL(extension);
  if (update_url.is_empty())
    return !extension->converted_from_user_script();
  return extension_urls::IsWebstoreUpdateUrl(update_url);
}

void UpdateService::OnEvent(Events event, const std::string& extension_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << "UpdateService::OnEvent " << static_cast<int>(event) << " "
          << extension_id;

  bool should_perform_action_on_omaha_attributes = false;
  switch (event) {
    case Events::COMPONENT_NOT_UPDATED:
      // Attributes is currently only added when no_update is true in the update
      // client config.
      should_perform_action_on_omaha_attributes = true;
      break;
    case Events::COMPONENT_UPDATE_FOUND:
      // This flag is set since it makes sense to update attributes when an
      // update is found even though the server currently doesn not serve
      // attributes for an extension with an update.
      should_perform_action_on_omaha_attributes = true;
      HandleComponentUpdateFoundEvent(extension_id);
      break;
    case Events::COMPONENT_CHECKING_FOR_UPDATES:
    case Events::COMPONENT_WAIT:
    case Events::COMPONENT_UPDATE_READY:
    case Events::COMPONENT_UPDATE_DOWNLOADING:
    case Events::COMPONENT_UPDATE_UPDATING:
    case Events::COMPONENT_UPDATED:
    case Events::COMPONENT_UPDATE_ERROR:
      break;
  }

  base::Value attributes(base::Value::Type::DICTIONARY);
  if (should_perform_action_on_omaha_attributes &&
      base::FeatureList::IsEnabled(
          extensions_features::kDisableMalwareExtensionsRemotely)) {
    attributes = GetExtensionOmahaAttributes(extension_id);
  }
  ExtensionSystem::Get(browser_context_)
      ->PerformActionBasedOnOmahaAttributes(extension_id, attributes);
}

UpdateService::UpdateService(
    content::BrowserContext* browser_context,
    scoped_refptr<update_client::UpdateClient> update_client)
    : browser_context_(browser_context), update_client_(update_client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  update_data_provider_ =
      base::MakeRefCounted<UpdateDataProvider>(browser_context_);
  AddUpdateClientObserver(this);
}

UpdateService::~UpdateService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (update_client_)
    update_client_->RemoveObserver(this);
}

void UpdateService::StartUpdateCheck(
    const ExtensionUpdateCheckParams& update_params,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!update_params.update_info.empty());

  VLOG(2) << "UpdateService::StartUpdateCheck";

  if (!ExtensionsBrowserClient::Get()->IsBackgroundUpdateAllowed()) {
    VLOG(1) << "UpdateService - Extension update not allowed.";
    if (!callback.is_null())
      std::move(callback).Run();
    return;
  }

  InProgressUpdate update =
      InProgressUpdate(std::move(callback), update_params.install_immediately);

  ExtensionUpdateDataMap update_data;
  std::vector<ExtensionId> update_ids;
  update_ids.reserve(update_params.update_info.size());
  for (const auto& update_info : update_params.update_info) {
    const std::string& extension_id = update_info.first;

    DCHECK(!extension_id.empty());

    update.pending_extension_ids.insert(extension_id);

    ExtensionUpdateData data = update_info.second;
    if (data.is_corrupt_reinstall) {
      data.install_source = "reinstall";
    } else if (data.install_source.empty() &&
               update_params.priority ==
                   ExtensionUpdateCheckParams::FOREGROUND) {
      data.install_source = "ondemand";
    }
    update_ids.push_back(extension_id);
    update_data.insert(std::make_pair(extension_id, data));
  }

  update_client_->Update(
      update_ids,
      base::BindOnce(&UpdateDataProvider::GetData, update_data_provider_,
                     update_params.install_immediately, std::move(update_data)),
      {}, update_params.priority == ExtensionUpdateCheckParams::FOREGROUND,
      base::BindOnce(&UpdateService::UpdateCheckComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(update)));
}

void UpdateService::UpdateCheckComplete(InProgressUpdate update,
                                        update_client::Error error) {
  VLOG(2) << "UpdateService::UpdateCheckComplete";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // When no update is found or there's an update error, a previous update
  // check might have queued an update for this extension because it was in
  // use at the time. We should ask for the install of the queued update now
  // if it's ready.
  if (update.install_immediately) {
    for (const ExtensionId& extension_id : update.pending_extension_ids) {
      ExtensionSystem::Get(browser_context_)
          ->FinishDelayedInstallationIfReady(extension_id,
                                             true /*install_immediately*/);
    }
  }

  if (!update.callback.is_null())
    std::move(update.callback).Run();
}

void UpdateService::AddUpdateClientObserver(
    update_client::UpdateClient::Observer* observer) {
  if (update_client_)
    update_client_->AddObserver(observer);
}

void UpdateService::RemoveUpdateClientObserver(
    update_client::UpdateClient::Observer* observer) {
  if (update_client_)
    update_client_->RemoveObserver(observer);
}

void UpdateService::HandleComponentUpdateFoundEvent(
    const std::string& extension_id) const {
  update_client::CrxUpdateItem update_item;
  if (!update_client_->GetCrxUpdateState(extension_id, &update_item)) {
    return;
  }

  VLOG(3) << "UpdateService::OnEvent COMPONENT_UPDATE_FOUND: " << extension_id
          << " " << update_item.next_version.GetString();
  UpdateDetails update_info(extension_id, update_item.next_version);
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_UPDATE_FOUND,
      content::NotificationService::AllBrowserContextsAndSources(),
      content::Details<UpdateDetails>(&update_info));
}

base::Value UpdateService::GetExtensionOmahaAttributes(
    const std::string& extension_id) {
  DCHECK(base::FeatureList::IsEnabled(
      extensions_features::kDisableMalwareExtensionsRemotely));

  update_client::CrxUpdateItem update_item;
  base::Value attributes(base::Value::Type::DICTIONARY);
  if (!update_client_->GetCrxUpdateState(extension_id, &update_item))
    return attributes;

  for (const char* key : kOmahaAttributes) {
    auto iter = update_item.custom_updatecheck_data.find(key);
    // This is assuming that the values of the keys are "true", "false",
    // or does not exist.
    if (iter != update_item.custom_updatecheck_data.end())
      attributes.SetKey(key, base::Value(iter->second == "true"));
  }
  return attributes;
}
}  // namespace extensions
