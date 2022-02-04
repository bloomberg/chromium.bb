// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/platform_management_service.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "components/policy/core/common/management/platform_management_status_provider_win.h"
#endif

namespace policy {

namespace {
std::vector<std::unique_ptr<ManagementStatusProvider>>
GetPlatformManagementSatusProviders() {
  std::vector<std::unique_ptr<ManagementStatusProvider>> providers;
#if BUILDFLAG(IS_WIN)
  providers.emplace_back(std::make_unique<DomainEnrollmentStatusProvider>());
  providers.emplace_back(
      std::make_unique<EnterpriseMDMManagementStatusProvider>());
#endif
  return providers;
}

}  // namespace

PlatformManagementService::PlatformManagementService()
    : ManagementService(GetPlatformManagementSatusProviders()) {}

PlatformManagementService::~PlatformManagementService() = default;

}  // namespace policy
