// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/tab_signal_generator_impl.h"

#include "services/resource_coordinator/coordination_unit/coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_impl_unittest_util.h"
#include "services/resource_coordinator/coordination_unit/mock_coordination_unit_graphs.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

class MockTabSignalGeneratorImpl : public TabSignalGeneratorImpl {
 public:
  // Overridden from TabSignalGeneratorImpl.
  void OnWebContentsPropertyChanged(
      const WebContentsCoordinationUnitImpl* coordination_unit,
      const mojom::PropertyType property_type,
      int64_t value) override {
    if (property_type == mojom::PropertyType::kExpectedTaskQueueingDuration)
      ++eqt_change_count_;
  }

  size_t eqt_change_count() const { return eqt_change_count_; }

 private:
  size_t eqt_change_count_ = 0;
};

class TabSignalGeneratorImplTest : public CoordinationUnitImplTestBase {
 protected:
  MockTabSignalGeneratorImpl* tab_signal_generator() {
    return &tab_signal_generator_;
  }

 private:
  MockTabSignalGeneratorImpl tab_signal_generator_;
};

TEST_F(TabSignalGeneratorImplTest,
       CalculateTabEQTForSingleTabWithMultipleProcesses) {
  MockSingleTabWithMultipleProcessesCoordinationUnitGraph cu_graph;
  cu_graph.tab->AddObserver(tab_signal_generator());

  cu_graph.process->SetProperty(
      mojom::PropertyType::kExpectedTaskQueueingDuration, 1);
  cu_graph.other_process->SetProperty(
      mojom::PropertyType::kExpectedTaskQueueingDuration, 10);

  // The |other_process| is not for the main frame so its EQT values does not
  // propagate to the tab.
  EXPECT_EQ(1u, tab_signal_generator()->eqt_change_count());
  int64_t eqt;
  ASSERT_TRUE(cu_graph.tab->GetProperty(
      mojom::PropertyType::kExpectedTaskQueueingDuration, &eqt));
  EXPECT_EQ(1, eqt);
}

}  // namespace resource_coordinator
