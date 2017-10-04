// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_base.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_factory.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_test_harness.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

class ProcessCoordinationUnitImplTest : public CoordinationUnitTestHarness {};

}  // namespace

TEST_F(ProcessCoordinationUnitImplTest, MeasureCPUUsage) {
  base::ProcessId current_pid = base::GetCurrentProcId();
  CoordinationUnitID cu_id(CoordinationUnitType::kProcess, current_pid);

  std::unique_ptr<CoordinationUnitBase> coordination_unit_ =
      coordination_unit_factory::CreateCoordinationUnit(
          cu_id, service_context_ref_factory()->CreateRef());

  base::RunLoop().RunUntilIdle();

  base::Value cpu_usage_value =
      coordination_unit_->GetProperty(mojom::PropertyType::kCPUUsage);
  EXPECT_LE(0.0, cpu_usage_value.GetDouble());
}

}  // namespace resource_coordinator
