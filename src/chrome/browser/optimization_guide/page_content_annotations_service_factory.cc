// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/page_content_annotations_service_factory.h"

#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static
optimization_guide::PageContentAnnotationsService*
PageContentAnnotationsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<optimization_guide::PageContentAnnotationsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PageContentAnnotationsServiceFactory*
PageContentAnnotationsServiceFactory::GetInstance() {
  static base::NoDestructor<PageContentAnnotationsServiceFactory> factory;
  return factory.get();
}

PageContentAnnotationsServiceFactory::PageContentAnnotationsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PageContentAnnotationsService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
}

PageContentAnnotationsServiceFactory::~PageContentAnnotationsServiceFactory() =
    default;

KeyedService* PageContentAnnotationsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!optimization_guide::features::IsPageContentAnnotationEnabled())
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(context);

  auto* proto_db_provider = profile->GetOriginalProfile()
                                ->GetDefaultStoragePartition()
                                ->GetProtoDatabaseProvider();
  base::FilePath profile_path = profile->GetOriginalProfile()->GetPath();

  // The optimization guide and history services must be available for the page
  // content annotations service to work.
  OptimizationGuideKeyedService* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::IMPLICIT_ACCESS);
  if (optimization_guide_keyed_service && history_service) {
    return new optimization_guide::PageContentAnnotationsService(
        g_browser_process->GetApplicationLocale(),
        optimization_guide_keyed_service, history_service, proto_db_provider,
        profile_path,
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
  }
  return nullptr;
}

bool PageContentAnnotationsServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return optimization_guide::features::IsPageContentAnnotationEnabled();
}

bool PageContentAnnotationsServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
