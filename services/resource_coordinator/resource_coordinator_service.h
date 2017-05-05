// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SPEED_SPEED_SERVICE_H_
#define SERVICES_SPEED_SPEED_SERVICE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace resource_coordinator {

class ResourceCoordinatorService : public service_manager::Service {
 public:
  ResourceCoordinatorService();
  ~ResourceCoordinatorService() override;

  // service_manager::Service:
  // Factory function for use as an embedded service.
  static std::unique_ptr<service_manager::Service> Create();

  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

 private:
  service_manager::BinderRegistry registry_;
  std::unique_ptr<service_manager::ServiceContextRefFactory> ref_factory_;

  // WeakPtrFactory members should always come last so WeakPtrs are destructed
  // before other members.
  base::WeakPtrFactory<ResourceCoordinatorService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ResourceCoordinatorService);
};

}  // namespace resource_coordinator

#endif  // SERVICES_SPEED_SPEED_SERVICE_H_
