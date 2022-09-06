// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/screen_time_controller_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/child_accounts/child_status_reporting_service_factory.h"
#include "chrome/browser/ash/child_accounts/screen_time_controller.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace ash {

// static
ScreenTimeController* ScreenTimeControllerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ScreenTimeController*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ScreenTimeControllerFactory* ScreenTimeControllerFactory::GetInstance() {
  static base::NoDestructor<ScreenTimeControllerFactory> factory;
  return factory.get();
}

ScreenTimeControllerFactory::ScreenTimeControllerFactory()
    : BrowserContextKeyedServiceFactory(
          "ScreenTimeControllerFactory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ChildStatusReportingServiceFactory::GetInstance());
}

ScreenTimeControllerFactory::~ScreenTimeControllerFactory() = default;

KeyedService* ScreenTimeControllerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ScreenTimeController(context);
}

}  // namespace ash
