// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/coordination_unit_provider_impl.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_factory.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_graph_observer.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_impl.h"
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

void CoordinationUnitProviderImpl::CreateCoordinationUnit(
    mojom::CoordinationUnitRequest request,
    const CoordinationUnitID& id) {
  std::unique_ptr<CoordinationUnitImpl> coordination_unit =
      coordination_unit_factory::CreateCoordinationUnit(
          id, service_ref_factory_->CreateRef());

  CoordinationUnitImpl* coordination_unit_impl = coordination_unit.get();

  auto coordination_unit_binding =
      mojo::MakeStrongBinding(std::move(coordination_unit), std::move(request));

  coordination_unit_manager_->OnCoordinationUnitCreated(coordination_unit_impl);

  coordination_unit_binding->set_connection_error_handler(
      base::Bind(&CoordinationUnitManager::OnCoordinationUnitWillBeDestroyed,
                 base::Unretained(coordination_unit_manager_),
                 base::Unretained(coordination_unit_impl)));
}

// static
void CoordinationUnitProviderImpl::Create(
    service_manager::ServiceContextRefFactory* service_ref_factory,
    CoordinationUnitManager* coordination_unit_manager,
    const service_manager::BindSourceInfo& source_info,
    resource_coordinator::mojom::CoordinationUnitProviderRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<CoordinationUnitProviderImpl>(
                              service_ref_factory, coordination_unit_manager),
                          std::move(request));
}

}  // namespace resource_coordinator
