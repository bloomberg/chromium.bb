// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/coordination_unit_impl_unittest_util.h"

#include <string>

#include "base/bind.h"
#include "base/run_loop.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_impl.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit.mojom.h"

namespace resource_coordinator {

namespace {

void OnLastServiceRefDestroyed() {
  // No-op. This is required by service_manager::ServiceContextRefFactory
  // construction but not needed for the tests.
}

}  // namespace

CoordinationUnitImplTestBase::CoordinationUnitImplTestBase()
    : service_ref_factory_(base::Bind(&OnLastServiceRefDestroyed)),
      provider_(&service_ref_factory_, &coordination_unit_manager_) {}

CoordinationUnitImplTestBase::~CoordinationUnitImplTestBase() = default;

void CoordinationUnitImplTestBase::TearDown() {
  base::RunLoop().RunUntilIdle();
}

TestCoordinationUnitWrapper
CoordinationUnitImplTestBase::CreateCoordinationUnit(CoordinationUnitID cu_id) {
  return TestCoordinationUnitWrapper(
      CoordinationUnitImpl::CreateCoordinationUnit(
          cu_id, service_context_ref_factory()->CreateRef()));
}

TestCoordinationUnitWrapper
CoordinationUnitImplTestBase::CreateCoordinationUnit(
    CoordinationUnitType type) {
  CoordinationUnitID cu_id(type, std::string());
  return CreateCoordinationUnit(cu_id);
}

}  // namespace resource_coordinator
