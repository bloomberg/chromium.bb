// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_model_handler_factory.h"

#include "base/memory/singleton.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/permissions/prediction_service/prediction_model_handler.h"

// static
PredictionModelHandlerFactory* PredictionModelHandlerFactory::GetInstance() {
  return base::Singleton<PredictionModelHandlerFactory>::get();
}

// static
permissions::PredictionModelHandler*
PredictionModelHandlerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<permissions::PredictionModelHandler*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

PredictionModelHandlerFactory::PredictionModelHandlerFactory()
    : BrowserContextKeyedServiceFactory(
          "PredictionModelHandler",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

PredictionModelHandlerFactory::~PredictionModelHandlerFactory() = default;

KeyedService* PredictionModelHandlerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new permissions::PredictionModelHandler(
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context)),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE}));
}

content::BrowserContext* PredictionModelHandlerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}
