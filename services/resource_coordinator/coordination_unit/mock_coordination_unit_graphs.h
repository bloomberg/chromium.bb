// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_MOCK_COORDINATION_UNIT_GRAPHS_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_MOCK_COORDINATION_UNIT_GRAPHS_H_

#include <memory>

namespace resource_coordinator {

class CoordinationUnitImpl;

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
  std::unique_ptr<CoordinationUnitImpl> frame;
  std::unique_ptr<CoordinationUnitImpl> process;
  std::unique_ptr<CoordinationUnitImpl> tab;
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
  std::unique_ptr<CoordinationUnitImpl> other_frame;
  std::unique_ptr<CoordinationUnitImpl> other_tab;
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
  std::unique_ptr<CoordinationUnitImpl> child_frame;
  std::unique_ptr<CoordinationUnitImpl> other_process;
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
  std::unique_ptr<CoordinationUnitImpl> child_frame;
  std::unique_ptr<CoordinationUnitImpl> other_process;
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_MOCK_COORDINATION_UNIT_GRAPHS_H_
