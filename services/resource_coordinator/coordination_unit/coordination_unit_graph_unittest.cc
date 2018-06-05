// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/coordination_unit_graph.h"
#include "services/resource_coordinator/coordination_unit/system_coordination_unit_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

TEST(CoordinationUnitGraphTest, FindOrCreateSystemCoordinationUnit) {
  CoordinationUnitGraph graph;

  SystemCoordinationUnitImpl* system_cu =
      graph.FindOrCreateSystemCoordinationUnit(nullptr);

  // A second request should return the same instance.
  EXPECT_EQ(system_cu, graph.FindOrCreateSystemCoordinationUnit(nullptr));

  // Destructing the system CU should be allowed.
  system_cu->Destruct();

  system_cu = graph.FindOrCreateSystemCoordinationUnit(nullptr);
  EXPECT_NE(nullptr, system_cu);
}

}  // namespace resource_coordinator
