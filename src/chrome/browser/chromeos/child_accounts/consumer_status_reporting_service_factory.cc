// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/consumer_status_reporting_service_factory.h"

#include "base/macros.h"
#include "chrome/browser/chromeos/child_accounts/consumer_status_reporting_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {

// static
ConsumerStatusReportingService*
ConsumerStatusReportingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ConsumerStatusReportingService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ConsumerStatusReportingServiceFactory*
ConsumerStatusReportingServiceFactory::GetInstance() {
  static base::NoDestructor<ConsumerStatusReportingServiceFactory> factory;
  return factory.get();
}

ConsumerStatusReportingServiceFactory::ConsumerStatusReportingServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ConsumerStatusReportingServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {
}

ConsumerStatusReportingServiceFactory::
    ~ConsumerStatusReportingServiceFactory() = default;

KeyedService* ConsumerStatusReportingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ConsumerStatusReportingService(context);
}

}  // namespace chromeos
