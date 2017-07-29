// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/mock_coordination_unit_graphs.h"

#include <string>

#include "services/resource_coordinator/coordination_unit/coordination_unit_impl.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"

namespace service_manager {
class ServiceContextRef;
}

namespace resource_coordinator {

namespace {

TestCoordinationUnitWrapper CreateCoordinationUnit(CoordinationUnitType type) {
  CoordinationUnitID cu_id(type, std::string());
  return TestCoordinationUnitWrapper(
      CoordinationUnitImpl::CreateCoordinationUnit(cu_id, nullptr));
}

}  // namespace

MockSingleTabInSingleProcessCoordinationUnitGraph::
    MockSingleTabInSingleProcessCoordinationUnitGraph()
    : frame(CreateCoordinationUnit(CoordinationUnitType::kFrame)),
      process(CreateCoordinationUnit(CoordinationUnitType::kProcess)),
      tab(CreateCoordinationUnit(CoordinationUnitType::kWebContents)) {
  tab->AddChild(frame->id());
  process->AddChild(frame->id());
}

MockSingleTabInSingleProcessCoordinationUnitGraph::
    ~MockSingleTabInSingleProcessCoordinationUnitGraph() = default;

MockMultipleTabsInSingleProcessCoordinationUnitGraph::
    MockMultipleTabsInSingleProcessCoordinationUnitGraph()
    : other_frame(CreateCoordinationUnit(CoordinationUnitType::kFrame)),
      other_tab(CreateCoordinationUnit(CoordinationUnitType::kWebContents)) {
  other_tab->AddChild(other_frame->id());
  process->AddChild(other_frame->id());
}

MockMultipleTabsInSingleProcessCoordinationUnitGraph::
    ~MockMultipleTabsInSingleProcessCoordinationUnitGraph() = default;

MockSingleTabWithMultipleProcessesCoordinationUnitGraph::
    MockSingleTabWithMultipleProcessesCoordinationUnitGraph()
    : child_frame(CreateCoordinationUnit(CoordinationUnitType::kFrame)),
      other_process(CreateCoordinationUnit(CoordinationUnitType::kProcess)) {
  frame->AddChild(child_frame->id());
  tab->AddChild(child_frame->id());
  other_process->AddChild(child_frame->id());
}

MockSingleTabWithMultipleProcessesCoordinationUnitGraph::
    ~MockSingleTabWithMultipleProcessesCoordinationUnitGraph() = default;

MockMultipleTabsWithMultipleProcessesCoordinationUnitGraph::
    MockMultipleTabsWithMultipleProcessesCoordinationUnitGraph()
    : child_frame(CreateCoordinationUnit(CoordinationUnitType::kFrame)),
      other_process(CreateCoordinationUnit(CoordinationUnitType::kProcess)) {
  other_frame->AddChild(child_frame->id());
  other_tab->AddChild(child_frame->id());
  other_process->AddChild(child_frame->id());
}

MockMultipleTabsWithMultipleProcessesCoordinationUnitGraph::
    ~MockMultipleTabsWithMultipleProcessesCoordinationUnitGraph() = default;

}  // namespace resource_coordinator
