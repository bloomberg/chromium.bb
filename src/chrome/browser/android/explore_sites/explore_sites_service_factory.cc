// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_service_factory.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"

#include "chrome/browser/android/explore_sites/explore_sites_service.h"
#include "chrome/browser/android/explore_sites/explore_sites_service_impl.h"
#include "chrome/browser/android/explore_sites/explore_sites_store.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace explore_sites {
const base::FilePath::CharType kExploreSitesStoreDirname[] =
    FILE_PATH_LITERAL("Explore");

ExploreSitesServiceFactory::ExploreSitesServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ExploreSitesService",
          BrowserContextDependencyManager::GetInstance()) {}

// static
ExploreSitesServiceFactory* ExploreSitesServiceFactory::GetInstance() {
  return base::Singleton<ExploreSitesServiceFactory>::get();
}

// static
ExploreSitesService* ExploreSitesServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExploreSitesService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

KeyedService* ExploreSitesServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()});
  base::FilePath store_path =
      profile->GetPath().Append(kExploreSitesStoreDirname);
  auto explore_sites_store =
      std::make_unique<ExploreSitesStore>(background_task_runner, store_path);

  return new ExploreSitesServiceImpl(std::move(explore_sites_store));
}

}  // namespace explore_sites
