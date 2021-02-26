// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/scanning/scan_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/scanning/lorgnette_scanner_manager_factory.h"
#include "chrome/browser/chromeos/scanning/scan_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

// static
ScanService* ScanServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ScanService*>(
      ScanServiceFactory::GetInstance()->GetServiceForBrowserContext(
          context, /*create=*/true));
}

// static
ScanServiceFactory* ScanServiceFactory::GetInstance() {
  return base::Singleton<ScanServiceFactory>::get();
}

ScanServiceFactory::ScanServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ScanService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(LorgnetteScannerManagerFactory::GetInstance());
}

ScanServiceFactory::~ScanServiceFactory() = default;

KeyedService* ScanServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // Prevent an instance of ScanService from being created on the lock screen.
  Profile* profile = Profile::FromBrowserContext(context);
  if (ProfileHelper::IsLockScreenAppProfile(profile) ||
      ProfileHelper::IsSigninProfile(profile)) {
    return nullptr;
  }

  auto* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  bool drive_available = integration_service &&
                         integration_service->is_enabled() &&
                         integration_service->IsMounted();
  return new ScanService(
      LorgnetteScannerManagerFactory::GetForBrowserContext(context),
      file_manager::util::GetMyFilesFolderForProfile(profile),
      drive_available ? integration_service->GetMountPointPath()
                      : base::FilePath());
}

bool ScanServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool ScanServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace chromeos
