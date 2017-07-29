// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_MOCK_COORDINATION_UNIT_GRAPHS_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_MOCK_COORDINATION_UNIT_GRAPHS_H_

#include <memory>

#include "services/resource_coordinator/coordination_unit/coordination_unit_impl_unittest_util.h"

namespace resource_coordinator {

// The following coordination unit graph topology is created to emulate a
// scenario when a single tab are executes in a single process:
//
// T   P
//  \ /
//   F
//
// Where:
// T: tab
// F: frame
// P: process
struct MockSingleTabInSingleProcessCoordinationUnitGraph {
  MockSingleTabInSingleProcessCoordinationUnitGraph();
  ~MockSingleTabInSingleProcessCoordinationUnitGraph();
  TestCoordinationUnitWrapper frame;
  TestCoordinationUnitWrapper process;
  TestCoordinationUnitWrapper tab;
};

// The following coordination unit graph topology is created to emulate a
// scenario where multiple tabs are executing in a single process:
//
// T   P  OT
//  \ / \ /
//   F  OF
//
// Where:
// T: tab
// OT: other_tab
// F: frame
// OF: other_frame
// P: process
struct MockMultipleTabsInSingleProcessCoordinationUnitGraph
    : public MockSingleTabInSingleProcessCoordinationUnitGraph {
  MockMultipleTabsInSingleProcessCoordinationUnitGraph();
  ~MockMultipleTabsInSingleProcessCoordinationUnitGraph();
  TestCoordinationUnitWrapper other_frame;
  TestCoordinationUnitWrapper other_tab;
};

// The following coordination unit graph topology is created to emulate a
// scenario where a single tab that has frames executing in different
// processes (e.g. out-of-process iFrames):
//
// T   P
// |\ /
// | F  OP
// |  \ /
// |__CF
//
// Where:
// T: tab
// F: frame
// CF: chid_frame
// P: process
// OP: other_process
struct MockSingleTabWithMultipleProcessesCoordinationUnitGraph
    : public MockSingleTabInSingleProcessCoordinationUnitGraph {
  MockSingleTabWithMultipleProcessesCoordinationUnitGraph();
  ~MockSingleTabWithMultipleProcessesCoordinationUnitGraph();
  TestCoordinationUnitWrapper child_frame;
  TestCoordinationUnitWrapper other_process;
};

// The following coordination unit graph topology is created to emulate a
// scenario where multiple tabs are utilizing multiple processes (e.g.
// out-of-process iFrames and multiple tabs in a process):
//
// T   P  OT____
//  \ / \ /     |
//   F   OF  OP |
//        \ /   |
//         CF___|
//
// Where:
// T: tab_coordination_unit
// OT: other_tab_coordination_unit
// F: frame_coordination_unit
// OF: other_frame_coordination_unit
// CF: another_frame_coordination_unit
// P: process_coordination_unit
// OP: other_process_coordination_unit
struct MockMultipleTabsWithMultipleProcessesCoordinationUnitGraph
    : public MockMultipleTabsInSingleProcessCoordinationUnitGraph {
  MockMultipleTabsWithMultipleProcessesCoordinationUnitGraph();
  ~MockMultipleTabsWithMultipleProcessesCoordinationUnitGraph();
  TestCoordinationUnitWrapper child_frame;
  TestCoordinationUnitWrapper other_process;
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_MOCK_COORDINATION_UNIT_GRAPHS_H_
