// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_manager_utils.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/simple_download_manager_coordinator_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/transition_manager/full_browser_transition_manager.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_url_loader_factory_getter_impl.h"
#include "components/download/public/common/in_progress_download_manager.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/common/service_manager_connection.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/download/download_manager_service.h"
#endif

namespace {
// A map for owning InProgressDownloadManagers before DownloadManagerImpl gets
// created.
using InProgressManagerMap =
    std::map<SimpleFactoryKey*,
             std::unique_ptr<download::InProgressDownloadManager>>;

InProgressManagerMap& GetInProgressManagerMap() {
  static base::NoDestructor<InProgressManagerMap> map;
  return *map;
}

// Ignores origin security check. DownloadManagerImpl will provide its own
// implementation when InProgressDownloadManager object is passed to it.
bool IgnoreOriginSecurityCheck(const GURL& url) {
  return true;
}

// Some ChromeOS browser tests doesn't initialize DownloadManager when profile
// is created, and cause the download request to fail. This method helps us
// ensure that the DownloadManager will be created after profile creation.
void GetDownloadManagerOnProfileCreation(Profile* profile) {
  content::DownloadManager* manager =
      content::BrowserContext::GetDownloadManager(profile);
  DCHECK(manager);
}

service_manager::Connector* GetServiceConnector() {
  auto* connection = content::ServiceManagerConnection::GetForProcess();
  if (!connection)
    return nullptr;
  return connection->GetConnector();
}

void CreateInProgressDownloadManager(SimpleFactoryKey* key) {
  auto& map = GetInProgressManagerMap();
  auto it = map.find(key);
  // Create the InProgressDownloadManager if it hasn't been created yet.
  if (it == map.end()) {
    auto in_progress_manager =
        std::make_unique<download::InProgressDownloadManager>(
            nullptr, key->IsOffTheRecord() ? base::FilePath() : key->GetPath(),
            base::BindRepeating(&IgnoreOriginSecurityCheck),
            base::BindRepeating(&content::DownloadRequestUtils::IsURLSafe),
            GetServiceConnector());
    download::SimpleDownloadManagerCoordinator* coordinator =
        SimpleDownloadManagerCoordinatorFactory::GetForKey(key);
    coordinator->SetSimpleDownloadManager(in_progress_manager.get(), false);
    scoped_refptr<network::SharedURLLoaderFactory> factory =
        SystemNetworkContextManager::GetInstance()->GetSharedURLLoaderFactory();
    in_progress_manager->set_url_loader_factory_getter(
        base::MakeRefCounted<download::DownloadURLLoaderFactoryGetterImpl>(
            factory->Clone()));
    map[key] = std::move(in_progress_manager);
  }
}

}  // namespace

download::InProgressDownloadManager*
DownloadManagerUtils::RetrieveInProgressDownloadManager(Profile* profile) {
  // TODO(qinmin): use the profile to retrieve SimpleFactoryKey and
  // create SimpleDownloadManagerCoordinator.
#if defined(OS_ANDROID)
  download::InProgressDownloadManager* manager =
      DownloadManagerService::GetInstance()->RetriveInProgressDownloadManager(
          profile);
  if (manager)
    return manager;
#endif
  SimpleFactoryKey* key = profile->GetProfileKey();
  CreateInProgressDownloadManager(key);
  auto& map = GetInProgressManagerMap();
  return map[key].release();
}

void DownloadManagerUtils::InitializeSimpleDownloadManager(
    SimpleFactoryKey* key) {
#if defined(OS_ANDROID)
  if (!g_browser_process) {
    DownloadManagerService::GetInstance()->CreateInProgressDownloadManager();
    return;
  }
#endif
  if (base::FeatureList::IsEnabled(
          download::features::
              kUseInProgressDownloadManagerForDownloadService)) {
    CreateInProgressDownloadManager(key);
  } else {
    FullBrowserTransitionManager::Get()->RegisterCallbackOnProfileCreation(
        key, base::BindOnce(&GetDownloadManagerOnProfileCreation));
  }
}
