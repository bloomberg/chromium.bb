// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/resource_coordinator_service.h"

#include "base/macros.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_provider_impl.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace resource_coordinator {

std::unique_ptr<service_manager::Service> ResourceCoordinatorService::Create() {
  return base::MakeUnique<ResourceCoordinatorService>();
}

ResourceCoordinatorService::ResourceCoordinatorService()
    : weak_factory_(this) {}

ResourceCoordinatorService::~ResourceCoordinatorService() = default;

void ResourceCoordinatorService::OnStart() {
  ref_factory_.reset(new service_manager::ServiceContextRefFactory(
      base::Bind(&service_manager::ServiceContext::RequestQuit,
                 base::Unretained(context()))));

  registry_.AddInterface(base::Bind(&CoordinationUnitProviderImpl::Create,
                                    base::Unretained(ref_factory_.get())));
}

void ResourceCoordinatorService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(source_info, interface_name,
                          std::move(interface_pipe));
}

}  // namespace speed
