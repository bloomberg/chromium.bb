// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_MOCK_COORDINATION_UNIT_GRAPHS_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_MOCK_COORDINATION_UNIT_GRAPHS_H_

#include "services/resource_coordinator/coordination_unit/coordination_unit_test_harness.h"

namespace resource_coordinator {

class FrameCoordinationUnitImpl;
class PageCoordinationUnitImpl;
class ProcessCoordinationUnitImpl;

// The following coordination unit graph topology is created to emulate a
// scenario when a single page executes in a single process:
//
// P'  P
//  \ /
//   F
//
// Where:
// F: frame
// P: process
// P': page
struct MockSinglePageInSingleProcessCoordinationUnitGraph {
  MockSinglePageInSingleProcessCoordinationUnitGraph();
  ~MockSinglePageInSingleProcessCoordinationUnitGraph();
  TestCoordinationUnitWrapper<FrameCoordinationUnitImpl> frame;
  TestCoordinationUnitWrapper<ProcessCoordinationUnitImpl> process;
  TestCoordinationUnitWrapper<PageCoordinationUnitImpl> page;
};

// The following coordination unit graph topology is created to emulate a
// scenario where multiple pages are executing in a single process:
//
// P'  P  OP'
//  \ / \ /
//   F  OF
//
// Where:
// F: frame
// OF: other_frame
// P': page
// OP': other_page
// P: process
struct MockMultiplePagesInSingleProcessCoordinationUnitGraph
    : public MockSinglePageInSingleProcessCoordinationUnitGraph {
  MockMultiplePagesInSingleProcessCoordinationUnitGraph();
  ~MockMultiplePagesInSingleProcessCoordinationUnitGraph();
  TestCoordinationUnitWrapper<FrameCoordinationUnitImpl> other_frame;
  TestCoordinationUnitWrapper<PageCoordinationUnitImpl> other_page;
};

// The following coordination unit graph topology is created to emulate a
// scenario where a single page that has frames is executing in different
// processes (e.g. out-of-process iFrames):
//
// P'  P
// |\ /
// | F  OP
// |  \ /
// |__CF
//
// Where:
// F: frame
// CF: chid_frame
// P': page
// P: process
// OP: other_process
struct MockSinglePageWithMultipleProcessesCoordinationUnitGraph
    : public MockSinglePageInSingleProcessCoordinationUnitGraph {
  MockSinglePageWithMultipleProcessesCoordinationUnitGraph();
  ~MockSinglePageWithMultipleProcessesCoordinationUnitGraph();
  TestCoordinationUnitWrapper<FrameCoordinationUnitImpl> child_frame;
  TestCoordinationUnitWrapper<ProcessCoordinationUnitImpl> other_process;
};

// The following coordination unit graph topology is created to emulate a
// scenario where multiple pages are utilizing multiple processes (e.g.
// out-of-process iFrames and multiple pages in a process):
//
// P'  P  OP'___
//  \ / \ /     |
//   F   OF  OP |
//        \ /   |
//         CF___|
//
// Where:
// F: frame_coordination_unit
// OF: other_frame_coordination_unit
// CF: another_frame_coordination_unit
// P': page_coordination_unit
// OP': other_page_coordination_unit
// P: process_coordination_unit
// OP: other_process_coordination_unit
struct MockMultiplePagesWithMultipleProcessesCoordinationUnitGraph
    : public MockMultiplePagesInSingleProcessCoordinationUnitGraph {
  MockMultiplePagesWithMultipleProcessesCoordinationUnitGraph();
  ~MockMultiplePagesWithMultipleProcessesCoordinationUnitGraph();
  TestCoordinationUnitWrapper<FrameCoordinationUnitImpl> child_frame;
  TestCoordinationUnitWrapper<ProcessCoordinationUnitImpl> other_process;
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_MOCK_COORDINATION_UNIT_GRAPHS_H_
