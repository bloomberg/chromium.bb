// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/coordination_unit_provider_impl.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_impl.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace resource_coordinator {

CoordinationUnitProviderImpl::CoordinationUnitProviderImpl(
    service_manager::ServiceContextRefFactory* service_ref_factory)
    : service_ref_factory_(service_ref_factory) {
  DCHECK(service_ref_factory);
  service_ref_ = service_ref_factory->CreateRef();
}

CoordinationUnitProviderImpl::~CoordinationUnitProviderImpl() = default;

void CoordinationUnitProviderImpl::CreateCoordinationUnit(
    mojom::CoordinationUnitRequest request,
    const CoordinationUnitID& id) {
  // TODO(oysteine): A strong binding here means the first binding set up
  // to a CoordinationUnit via CoordinationUnitProvider, i.e. the authoritative
  // one in terms of setting the context, has to outlive all of the other
  // connections (as the rest are just duplicated and held within
  // CoordinationUnit). Make sure this assumption is correct, or refactor into
  // some kind of refcounted thing.

  // Once there's a need for custom code for various types of CUs (tabs,
  // processes, etc) then this could become a factory function and instantiate
  // different subclasses of CoordinationUnitImpl based on the id.type.
  mojo::MakeStrongBinding(base::MakeUnique<CoordinationUnitImpl>(
                              id, service_ref_factory_->CreateRef()),
                          std::move(request));
};

// static
void CoordinationUnitProviderImpl::Create(
    service_manager::ServiceContextRefFactory* service_ref_factory,
    resource_coordinator::mojom::CoordinationUnitProviderRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<CoordinationUnitProviderImpl>(service_ref_factory),
      std::move(request));
}

}  // namespace resource_coordinator
