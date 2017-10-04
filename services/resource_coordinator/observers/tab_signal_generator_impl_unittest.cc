// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/observers/tab_signal_generator_impl.h"

#include "services/resource_coordinator/coordination_unit/coordination_unit_base.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_test_harness.h"
#include "services/resource_coordinator/coordination_unit/mock_coordination_unit_graphs.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

class MockTabSignalGeneratorImpl : public TabSignalGeneratorImpl {
 public:
  // Overridden from TabSignalGeneratorImpl.
  void OnPagePropertyChanged(const PageCoordinationUnitImpl* coordination_unit,
                             const mojom::PropertyType property_type,
                             int64_t value) override {
    if (property_type == mojom::PropertyType::kExpectedTaskQueueingDuration)
      ++eqt_change_count_;
  }

  size_t eqt_change_count() const { return eqt_change_count_; }

 private:
  size_t eqt_change_count_ = 0;
};

class TabSignalGeneratorImplTest : public CoordinationUnitTestHarness {
 protected:
  MockTabSignalGeneratorImpl* tab_signal_generator() {
    return &tab_signal_generator_;
  }

 private:
  MockTabSignalGeneratorImpl tab_signal_generator_;
};

TEST_F(TabSignalGeneratorImplTest,
       CalculateTabEQTForSingleTabWithMultipleProcesses) {
  MockSinglePageWithMultipleProcessesCoordinationUnitGraph cu_graph;
  cu_graph.page->AddObserver(tab_signal_generator());

  cu_graph.process->SetProperty(
      mojom::PropertyType::kExpectedTaskQueueingDuration, 1);
  cu_graph.other_process->SetProperty(
      mojom::PropertyType::kExpectedTaskQueueingDuration, 10);

  // The |other_process| is not for the main frame so its EQT values does not
  // propagate to the page.
  EXPECT_EQ(1u, tab_signal_generator()->eqt_change_count());
  int64_t eqt;
  ASSERT_TRUE(cu_graph.page->GetProperty(
      mojom::PropertyType::kExpectedTaskQueueingDuration, &eqt));
  EXPECT_EQ(1, eqt);
}

}  // namespace resource_coordinator
