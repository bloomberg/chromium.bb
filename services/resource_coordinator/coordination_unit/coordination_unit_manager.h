// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_MANAGER_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_MANAGER_H_

#include "base/macros.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace service_manager {
class ServiceContextRefFactory;
}  // service_manager

namespace resource_coordinator {

// The CoordinationUnitManager is a singleton that encapsulates all
// aspects of Coordination Units within the ResourceCoordinatorService.
// All functionality for dealing with CoordinationUnits should be contained
// within this class or classes that are owned by it
class CoordinationUnitManager {
 public:
  CoordinationUnitManager();
  ~CoordinationUnitManager();

  void OnStart(service_manager::BinderRegistry* registry,
               service_manager::ServiceContextRefFactory* service_ref_factory);

 private:
  static void Create(
      service_manager::ServiceContextRefFactory* service_ref_factory);

  DISALLOW_COPY_AND_ASSIGN(CoordinationUnitManager);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_MANAGER_H_
