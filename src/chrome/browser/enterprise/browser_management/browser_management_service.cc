// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/browser_management/browser_management_service.h"

#include "chrome/browser/enterprise/browser_management/browser_management_status_provider.h"
#include "chrome/browser/profiles/profile.h"

namespace policy {

namespace {

std::vector<std::unique_ptr<ManagementStatusProvider>>
GetManagementStatusProviders(Profile* profile) {
  std::vector<std::unique_ptr<ManagementStatusProvider>> providers;
  providers.emplace_back(
      std::make_unique<BrowserCloudManagementStatusProvider>());
  providers.emplace_back(
      std::make_unique<LocalBrowserManagementStatusProvider>());
  providers.emplace_back(
      std::make_unique<ProfileCloudManagementStatusProvider>(profile));
  return providers;
}

}  // namespace

BrowserManagementService::BrowserManagementService(Profile* profile)
    : ManagementService(GetManagementStatusProviders(profile)) {}

BrowserManagementService::~BrowserManagementService() = default;

}  // namespace policy
