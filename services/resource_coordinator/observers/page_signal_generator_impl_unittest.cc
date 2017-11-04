// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/observers/page_signal_generator_impl.h"

#include "services/resource_coordinator/coordination_unit/coordination_unit_test_harness.h"
#include "services/resource_coordinator/coordination_unit/mock_coordination_unit_graphs.h"
#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

class MockPageSignalGeneratorImpl : public PageSignalGeneratorImpl {
 public:
  // Overridden from PageSignalGeneratorImpl.
  void OnProcessPropertyChanged(const ProcessCoordinationUnitImpl* process_cu,
                                const mojom::PropertyType property_type,
                                int64_t value) override {
    if (property_type == mojom::PropertyType::kExpectedTaskQueueingDuration)
      ++eqt_change_count_;
  }

  size_t eqt_change_count() const { return eqt_change_count_; }

 private:
  size_t eqt_change_count_ = 0;
};

class PageSignalGeneratorImplTest : public CoordinationUnitTestHarness {
 protected:
  MockPageSignalGeneratorImpl* page_signal_generator() {
    return &page_signal_generator_;
  }

 private:
  MockPageSignalGeneratorImpl page_signal_generator_;
};

TEST_F(PageSignalGeneratorImplTest,
       CalculatePageEQTForSinglePageWithMultipleProcesses) {
  MockSinglePageWithMultipleProcessesCoordinationUnitGraph cu_graph;
  cu_graph.process->AddObserver(page_signal_generator());

  cu_graph.process->SetExpectedTaskQueueingDuration(
      base::TimeDelta::FromMilliseconds(1));
  cu_graph.other_process->SetExpectedTaskQueueingDuration(
      base::TimeDelta::FromMilliseconds(10));

  // The |other_process| is not for the main frame so its EQT values does not
  // propagate to the page.
  EXPECT_EQ(1u, page_signal_generator()->eqt_change_count());
  int64_t eqt;
  EXPECT_TRUE(cu_graph.page->GetExpectedTaskQueueingDuration(&eqt));
  EXPECT_EQ(1, eqt);
}

}  // namespace resource_coordinator
