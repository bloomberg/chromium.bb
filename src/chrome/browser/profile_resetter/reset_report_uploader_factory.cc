// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/reset_report_uploader_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/profile_resetter/reset_report_uploader.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static
ResetReportUploaderFactory* ResetReportUploaderFactory::GetInstance() {
  return base::Singleton<ResetReportUploaderFactory>::get();
}

// static
ResetReportUploader* ResetReportUploaderFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ResetReportUploader*>(
      GetInstance()->GetServiceForBrowserContext(context, true /* create */));
}

ResetReportUploaderFactory::ResetReportUploaderFactory()
    : BrowserContextKeyedServiceFactory(
          "ResetReportUploaderFactory",
          BrowserContextDependencyManager::GetInstance()) {}

ResetReportUploaderFactory::~ResetReportUploaderFactory() {}

KeyedService* ResetReportUploaderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ResetReportUploader(
      content::BrowserContext::GetDefaultStoragePartition(context)
          ->GetURLLoaderFactoryForBrowserProcess());
}
