// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/coordination_unit_test_harness.h"

#include "base/bind.h"
#include "base/run_loop.h"

namespace resource_coordinator {

CoordinationUnitTestHarness::CoordinationUnitTestHarness()
    : task_env_(base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
                base::test::ScopedTaskEnvironment::ExecutionMode::QUEUED),
      service_keepalive_(static_cast<service_manager::ServiceBinding*>(nullptr),
                         base::nullopt /* idle_timeout */),
      provider_(&service_keepalive_, &coordination_unit_graph_) {}

CoordinationUnitTestHarness::~CoordinationUnitTestHarness() = default;

void CoordinationUnitTestHarness::TearDown() {
  base::RunLoop().RunUntilIdle();
}

}  // namespace resource_coordinator
