// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_MANAGER_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace service_manager {
class ServiceContextRefFactory;
}  // service_manager

namespace resource_coordinator {

class CoordinationUnitImpl;
class CoordinationUnitGraphObserver;
class CoordinationUnitProviderImpl;

// The CoordinationUnitManager is a singleton that encapsulates all
// aspects of Coordination Units within the resource_coordinator service.
// All functionality for dealing with CoordinationUnits should be contained
// within this class or classes that are owned by it
class CoordinationUnitManager {
 public:
  CoordinationUnitManager();
  ~CoordinationUnitManager();

  void set_ukm_recorder(ukm::MojoUkmRecorder* ukm_recorder) {
    ukm_recorder_ = ukm_recorder;
  }
  ukm::MojoUkmRecorder* ukm_recorder() const { return ukm_recorder_; }

  void OnStart(service_manager::BinderRegistry* registry,
               service_manager::ServiceContextRefFactory* service_ref_factory);
  void RegisterObserver(
      std::unique_ptr<CoordinationUnitGraphObserver> observer);
  void OnCoordinationUnitCreated(CoordinationUnitImpl* coordination_unit);
  void OnBeforeCoordinationUnitDestroyed(
      CoordinationUnitImpl* coordination_unit);

  std::vector<std::unique_ptr<CoordinationUnitGraphObserver>>&
  observers_for_testing() {
    return observers_;
  }

 private:
  std::vector<std::unique_ptr<CoordinationUnitGraphObserver>> observers_;
  ukm::MojoUkmRecorder* ukm_recorder_ = nullptr;
  std::unique_ptr<CoordinationUnitProviderImpl> provider_;

  static void Create(
      service_manager::ServiceContextRefFactory* service_ref_factory);

  DISALLOW_COPY_AND_ASSIGN(CoordinationUnitManager);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_MANAGER_H_
