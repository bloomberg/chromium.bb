// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/coordination_unit_test_harness.h"

#include <string>

#include "base/bind.h"
#include "base/run_loop.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_base.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit.mojom.h"

namespace resource_coordinator {

namespace {

void OnLastServiceRefDestroyed() {
  // No-op. This is required by service_manager::ServiceContextRefFactory
  // construction but not needed for the tests.
}

}  // namespace

CoordinationUnitTestHarness::CoordinationUnitTestHarness()
    : service_ref_factory_(base::Bind(&OnLastServiceRefDestroyed)),
      provider_(&service_ref_factory_, &coordination_unit_manager_) {}

CoordinationUnitTestHarness::~CoordinationUnitTestHarness() = default;

void CoordinationUnitTestHarness::TearDown() {
  base::RunLoop().RunUntilIdle();
}

TestCoordinationUnitWrapper CoordinationUnitTestHarness::CreateCoordinationUnit(
    CoordinationUnitID cu_id) {
  return TestCoordinationUnitWrapper(
      CoordinationUnitBase::CreateCoordinationUnit(
          cu_id, service_context_ref_factory()->CreateRef()));
}

TestCoordinationUnitWrapper CoordinationUnitTestHarness::CreateCoordinationUnit(
    CoordinationUnitType type) {
  CoordinationUnitID cu_id(type, std::string());
  return CreateCoordinationUnit(cu_id);
}

}  // namespace resource_coordinator
