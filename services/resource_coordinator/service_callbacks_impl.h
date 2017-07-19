// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_SERVICE_CALLBACKS_IMPL_H_
#define SERVICES_RESOURCE_COORDINATOR_SERVICE_CALLBACKS_IMPL_H_

#include <memory>

#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/metrics/public/interfaces/ukm_interface.mojom.h"
#include "services/resource_coordinator/public/interfaces/service_callbacks.mojom.h"

namespace service_manager {
class ServiceContextRefFactory;
class ServiceContextRef;
}  // namespace service_manager

namespace resource_coordinator {

class ResourceCoordinatorService;

class ServiceCallbacksImpl : public mojom::ServiceCallbacks {
 public:
  ServiceCallbacksImpl(
      service_manager::ServiceContextRefFactory* service_ref_factory,
      ResourceCoordinatorService* resource_coordinator_service);
  ~ServiceCallbacksImpl() override;

  static void Create(
      service_manager::ServiceContextRefFactory* service_ref_factory,
      ResourceCoordinatorService* resource_coordinator_service,
      resource_coordinator::mojom::ServiceCallbacksRequest request);

  void IsUkmRecorderInterfaceInitialized(
      const IsUkmRecorderInterfaceInitializedCallback& callback) override;
  void SetUkmRecorderInterface(ukm::mojom::UkmRecorderInterfacePtr
                                   ukm_recorder_interface_request) override;

 private:
  ResourceCoordinatorService* resource_coordinator_service_;
  std::unique_ptr<service_manager::ServiceContextRef> service_ref_;

  DISALLOW_COPY_AND_ASSIGN(ServiceCallbacksImpl);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_SERVICE_CALLBACKS_IMPL_H_
