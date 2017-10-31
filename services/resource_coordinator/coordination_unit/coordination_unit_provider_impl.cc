// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/coordination_unit_provider_impl.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_base.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace resource_coordinator {

CoordinationUnitProviderImpl::CoordinationUnitProviderImpl(
    service_manager::ServiceContextRefFactory* service_ref_factory,
    CoordinationUnitManager* coordination_unit_manager)
    : service_ref_factory_(service_ref_factory),
      coordination_unit_manager_(coordination_unit_manager) {
  DCHECK(service_ref_factory);
  service_ref_ = service_ref_factory->CreateRef();
}

CoordinationUnitProviderImpl::~CoordinationUnitProviderImpl() = default;

void CoordinationUnitProviderImpl::OnConnectionError(
    CoordinationUnitBase* coordination_unit) {
  coordination_unit_manager_->OnBeforeCoordinationUnitDestroyed(
      coordination_unit);
  coordination_unit->Destruct();
}

void CoordinationUnitProviderImpl::CreateCoordinationUnit(
    mojom::CoordinationUnitRequest request,
    const CoordinationUnitID& id) {
  CoordinationUnitBase* coordination_unit =
      CoordinationUnitBase::CreateCoordinationUnit(
          id, service_ref_factory_->CreateRef());

  coordination_unit->Bind(std::move(request));
  coordination_unit_manager_->OnCoordinationUnitCreated(coordination_unit);
  auto& coordination_unit_binding = coordination_unit->binding();

  coordination_unit_binding.set_connection_error_handler(
      base::BindOnce(&CoordinationUnitProviderImpl::OnConnectionError,
                     base::Unretained(this), coordination_unit));
}

void CoordinationUnitProviderImpl::Bind(
    resource_coordinator::mojom::CoordinationUnitProviderRequest request,
    const service_manager::BindSourceInfo& source_info) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace resource_coordinator
