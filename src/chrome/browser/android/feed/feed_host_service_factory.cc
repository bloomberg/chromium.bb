// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/feed_host_service_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/image_decoder_impl.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "components/feed/content/feed_host_service.h"
#include "components/feed/content/feed_offline_host.h"
#include "components/feed/core/feed_content_database.h"
#include "components/feed/core/feed_image_manager.h"
#include "components/feed/core/feed_journal_database.h"
#include "components/feed/core/feed_networking_host.h"
#include "components/feed/core/feed_scheduler_host.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/google_api_keys.h"
#include "net/url_request/url_request_context_getter.h"

namespace feed {

namespace {
const char kFeedFolder[] = "feed";
}  // namespace

// static
FeedHostService* FeedHostServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<FeedHostService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
FeedHostServiceFactory* FeedHostServiceFactory::GetInstance() {
  return base::Singleton<FeedHostServiceFactory>::get();
}

FeedHostServiceFactory::FeedHostServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "FeedHostService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(offline_pages::OfflinePageModelFactory::GetInstance());
  DependsOn(offline_pages::PrefetchServiceFactory::GetInstance());
}

FeedHostServiceFactory::~FeedHostServiceFactory() = default;

KeyedService* FeedHostServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  content::StoragePartition* storage_partition =
      content::BrowserContext::GetDefaultStoragePartition(context);

  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  std::string api_key;
  if (google_apis::IsGoogleChromeAPIKeyUsed()) {
    bool is_stable_channel =
        chrome::GetChannel() == version_info::Channel::STABLE;
    api_key = is_stable_channel ? google_apis::GetAPIKey()
                                : google_apis::GetNonStableAPIKey();
  }
  auto networking_host = std::make_unique<FeedNetworkingHost>(
      identity_manager, api_key,
      storage_partition->GetURLLoaderFactoryForBrowserProcess());

  auto image_fetcher = std::make_unique<image_fetcher::ImageFetcherImpl>(
      std::make_unique<suggestions::ImageDecoderImpl>(),
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess());

  base::FilePath feed_dir(profile->GetPath().Append(kFeedFolder));
  auto image_database = std::make_unique<FeedImageDatabase>(feed_dir);

  auto image_manager = std::make_unique<FeedImageManager>(
      std::move(image_fetcher), std::move(image_database));

  auto scheduler_host = std::make_unique<FeedSchedulerHost>(
      profile->GetPrefs(), g_browser_process->local_state(),
      base::DefaultClock::GetInstance());

  auto content_database = std::make_unique<FeedContentDatabase>(feed_dir);
  auto journal_database = std::make_unique<FeedJournalDatabase>(feed_dir);

  offline_pages::OfflinePageModel* offline_page_model =
      offline_pages::OfflinePageModelFactory::GetForBrowserContext(profile);
  offline_pages::PrefetchService* prefetch_service =
      offline_pages::PrefetchServiceFactory::GetForBrowserContext(profile);
  auto offline_host = std::make_unique<FeedOfflineHost>(
      offline_page_model, prefetch_service, scheduler_host.get());

  return new FeedHostService(
      std::move(image_manager), std::move(networking_host),
      std::move(scheduler_host), std::move(content_database),
      std::move(journal_database), std::move(offline_host));
}

content::BrowserContext* FeedHostServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context->IsOffTheRecord() ? nullptr : context;
}

}  // namespace feed
