// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/identity_service.h"

#include "services/identity/identity_manager.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace identity {

IdentityService::IdentityService(SigninManagerBase* signin_manager)
    : signin_manager_(signin_manager) {}

IdentityService::~IdentityService() {}

void IdentityService::OnStart() {}

bool IdentityService::OnConnect(const service_manager::ServiceInfo& remote_info,
                                service_manager::InterfaceRegistry* registry) {
  registry->AddInterface<mojom::IdentityManager>(this);
  return true;
}

void IdentityService::Create(const service_manager::Identity& remote_identity,
                             mojom::IdentityManagerRequest request) {
  IdentityManager::Create(std::move(request), signin_manager_);
}

}  // namespace identity
