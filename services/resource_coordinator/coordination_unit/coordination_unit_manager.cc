// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/coordination_unit_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_provider_impl.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace resource_coordinator {

class CoordinationUnitImpl;

CoordinationUnitManager::CoordinationUnitManager() = default;

CoordinationUnitManager::~CoordinationUnitManager() = default;

void CoordinationUnitManager::OnStart(
    service_manager::BinderRegistry* registry,
    service_manager::ServiceContextRefFactory* service_ref_factory) {
  registry->AddInterface(base::Bind(&CoordinationUnitProviderImpl::Create,
                                    base::Unretained(service_ref_factory)));
}

}  // namespace resource_coordinator
