// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/history/print_job_reporting_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/ash/printing/history/print_job_reporting_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace ash {

// static
PrintJobReportingService* PrintJobReportingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PrintJobReportingService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PrintJobReportingServiceFactory*
PrintJobReportingServiceFactory::GetInstance() {
  return base::Singleton<PrintJobReportingServiceFactory>::get();
}

PrintJobReportingServiceFactory::PrintJobReportingServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PrintJobReportingServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {}

PrintJobReportingServiceFactory::~PrintJobReportingServiceFactory() = default;

KeyedService* PrintJobReportingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto reporting_service = PrintJobReportingService::Create();

  return reporting_service.release();
}

bool PrintJobReportingServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ash
