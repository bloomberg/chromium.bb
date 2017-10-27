// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/service_callbacks_impl.h"

#include <utility>

#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/resource_coordinator/resource_coordinator_service.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace resource_coordinator {

ServiceCallbacksImpl::ServiceCallbacksImpl(
    service_manager::ServiceContextRefFactory* service_ref_factory,
    ResourceCoordinatorService* resource_coordinator_service)
    : resource_coordinator_service_(resource_coordinator_service) {
  DCHECK(service_ref_factory);
  service_ref_ = service_ref_factory->CreateRef();
}

ServiceCallbacksImpl::~ServiceCallbacksImpl() = default;

// static
void ServiceCallbacksImpl::Create(
    service_manager::ServiceContextRefFactory* service_ref_factory,
    ResourceCoordinatorService* resource_coordinator_service,
    resource_coordinator::mojom::ServiceCallbacksRequest request,
    const service_manager::BindSourceInfo& source_info) {
  mojo::MakeStrongBinding(
      std::make_unique<ServiceCallbacksImpl>(service_ref_factory,
                                             resource_coordinator_service),
      std::move(request));
}

void ServiceCallbacksImpl::SetUkmRecorderInterface(
    ukm::mojom::UkmRecorderInterfacePtr ukm_recorder_interface) {
  resource_coordinator_service_->SetUkmRecorder(
      std::make_unique<ukm::MojoUkmRecorder>(
          std::move(ukm_recorder_interface)));
}

void ServiceCallbacksImpl::IsUkmRecorderInterfaceInitialized(
    const IsUkmRecorderInterfaceInitializedCallback& callback) {
  callback.Run(resource_coordinator_service_->ukm_recorder() != nullptr);
}

}  // namespace resource_coordinator
