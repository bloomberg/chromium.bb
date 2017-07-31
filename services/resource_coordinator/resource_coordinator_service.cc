// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/resource_coordinator_service.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "services/resource_coordinator/coordination_unit/tab_signal_generator_impl.h"
#include "services/resource_coordinator/service_callbacks_impl.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace resource_coordinator {

std::unique_ptr<service_manager::Service> ResourceCoordinatorService::Create() {
  auto resource_coordinator_service =
      base::MakeUnique<ResourceCoordinatorService>();

  return resource_coordinator_service;
}

ResourceCoordinatorService::ResourceCoordinatorService()
    : weak_factory_(this) {}

ResourceCoordinatorService::~ResourceCoordinatorService() {
  ref_factory_.reset();
}

void ResourceCoordinatorService::OnStart() {
  ref_factory_.reset(new service_manager::ServiceContextRefFactory(
      base::Bind(&service_manager::ServiceContext::RequestQuit,
                 base::Unretained(context()))));

  registry_.AddInterface(base::Bind(ServiceCallbacksImpl::Create,
                                    base::Unretained(ref_factory_.get()),
                                    base::Unretained(this)));

  registry_.AddInterface(
      base::Bind(&CoordinationUnitIntrospectorImpl::BindToInterface,
                 base::Unretained(&introspector_)));

  // Register new |CoordinationUnitGraphObserver| implementations here.
  auto tab_signal_generator_impl = base::MakeUnique<TabSignalGeneratorImpl>();
  registry_.AddInterface(
      base::Bind(&TabSignalGeneratorImpl::BindToInterface,
                 base::Unretained(tab_signal_generator_impl.get())));
  coordination_unit_manager_.RegisterObserver(
      std::move(tab_signal_generator_impl));

  coordination_unit_manager_.OnStart(&registry_, ref_factory_.get());
}

void ResourceCoordinatorService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void ResourceCoordinatorService::SetUkmRecorder(
    std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder) {
  ukm_recorder_ = std::move(ukm_recorder);
  coordination_unit_manager_.set_ukm_recorder(ukm_recorder_.get());
}

}  // namespace resource_coordinator
