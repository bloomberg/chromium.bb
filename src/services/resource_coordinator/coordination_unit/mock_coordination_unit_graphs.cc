// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/mock_coordination_unit_graphs.h"

#include <string>

#include "services/resource_coordinator/coordination_unit/coordination_unit_base.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/system_coordination_unit_impl.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"

namespace resource_coordinator {

MockSinglePageInSingleProcessCoordinationUnitGraph::
    MockSinglePageInSingleProcessCoordinationUnitGraph(
        CoordinationUnitGraph* graph)
    : system(TestCoordinationUnitWrapper<SystemCoordinationUnitImpl>::Create(
          graph)),
      frame(TestCoordinationUnitWrapper<FrameCoordinationUnitImpl>::Create(
          graph)),
      process(TestCoordinationUnitWrapper<ProcessCoordinationUnitImpl>::Create(
          graph)),
      page(TestCoordinationUnitWrapper<PageCoordinationUnitImpl>::Create(
          graph)) {
  page->AddFrame(frame->id());
  frame->SetProcess(process->id());
  process->SetPID(1);
}

MockSinglePageInSingleProcessCoordinationUnitGraph::
    ~MockSinglePageInSingleProcessCoordinationUnitGraph() = default;

MockMultiplePagesInSingleProcessCoordinationUnitGraph::
    MockMultiplePagesInSingleProcessCoordinationUnitGraph(
        CoordinationUnitGraph* graph)
    : MockSinglePageInSingleProcessCoordinationUnitGraph(graph),
      other_frame(
          TestCoordinationUnitWrapper<FrameCoordinationUnitImpl>::Create(
              graph)),
      other_page(TestCoordinationUnitWrapper<PageCoordinationUnitImpl>::Create(
          graph)) {
  other_page->AddFrame(other_frame->id());
  other_frame->SetProcess(process->id());
}

MockMultiplePagesInSingleProcessCoordinationUnitGraph::
    ~MockMultiplePagesInSingleProcessCoordinationUnitGraph() = default;

MockSinglePageWithMultipleProcessesCoordinationUnitGraph::
    MockSinglePageWithMultipleProcessesCoordinationUnitGraph(
        CoordinationUnitGraph* graph)
    : MockSinglePageInSingleProcessCoordinationUnitGraph(graph),
      child_frame(
          TestCoordinationUnitWrapper<FrameCoordinationUnitImpl>::Create(
              graph)),
      other_process(
          TestCoordinationUnitWrapper<ProcessCoordinationUnitImpl>::Create(
              graph)) {
  frame->AddChildFrame(child_frame->id());
  page->AddFrame(child_frame->id());
  child_frame->SetProcess(other_process->id());
  other_process->SetPID(2);
}

MockSinglePageWithMultipleProcessesCoordinationUnitGraph::
    ~MockSinglePageWithMultipleProcessesCoordinationUnitGraph() = default;

MockMultiplePagesWithMultipleProcessesCoordinationUnitGraph::
    MockMultiplePagesWithMultipleProcessesCoordinationUnitGraph(
        CoordinationUnitGraph* graph)
    : MockMultiplePagesInSingleProcessCoordinationUnitGraph(graph),
      child_frame(
          TestCoordinationUnitWrapper<FrameCoordinationUnitImpl>::Create(
              graph)),
      other_process(
          TestCoordinationUnitWrapper<ProcessCoordinationUnitImpl>::Create(
              graph)) {
  other_frame->AddChildFrame(child_frame->id());
  other_page->AddFrame(child_frame->id());
  child_frame->SetProcess(other_process->id());
  other_process->SetPID(2);
}

MockMultiplePagesWithMultipleProcessesCoordinationUnitGraph::
    ~MockMultiplePagesWithMultipleProcessesCoordinationUnitGraph() = default;

}  // namespace resource_coordinator
