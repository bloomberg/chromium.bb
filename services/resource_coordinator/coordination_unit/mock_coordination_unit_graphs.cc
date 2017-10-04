// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/mock_coordination_unit_graphs.h"

#include <string>

#include "services/resource_coordinator/coordination_unit/coordination_unit_base.h"
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
      CoordinationUnitBase::CreateCoordinationUnit(cu_id, nullptr));
}

}  // namespace

MockSinglePageInSingleProcessCoordinationUnitGraph::
    MockSinglePageInSingleProcessCoordinationUnitGraph()
    : frame(CreateCoordinationUnit(CoordinationUnitType::kFrame)),
      process(CreateCoordinationUnit(CoordinationUnitType::kProcess)),
      page(CreateCoordinationUnit(CoordinationUnitType::kPage)) {
  page->AddChild(frame->id());
  process->AddChild(frame->id());
}

MockSinglePageInSingleProcessCoordinationUnitGraph::
    ~MockSinglePageInSingleProcessCoordinationUnitGraph() = default;

MockMultiplePagesInSingleProcessCoordinationUnitGraph::
    MockMultiplePagesInSingleProcessCoordinationUnitGraph()
    : other_frame(CreateCoordinationUnit(CoordinationUnitType::kFrame)),
      other_page(CreateCoordinationUnit(CoordinationUnitType::kPage)) {
  other_page->AddChild(other_frame->id());
  process->AddChild(other_frame->id());
}

MockMultiplePagesInSingleProcessCoordinationUnitGraph::
    ~MockMultiplePagesInSingleProcessCoordinationUnitGraph() = default;

MockSinglePageWithMultipleProcessesCoordinationUnitGraph::
    MockSinglePageWithMultipleProcessesCoordinationUnitGraph()
    : child_frame(CreateCoordinationUnit(CoordinationUnitType::kFrame)),
      other_process(CreateCoordinationUnit(CoordinationUnitType::kProcess)) {
  frame->AddChild(child_frame->id());
  page->AddChild(child_frame->id());
  other_process->AddChild(child_frame->id());
}

MockSinglePageWithMultipleProcessesCoordinationUnitGraph::
    ~MockSinglePageWithMultipleProcessesCoordinationUnitGraph() = default;

MockMultiplePagesWithMultipleProcessesCoordinationUnitGraph::
    MockMultiplePagesWithMultipleProcessesCoordinationUnitGraph()
    : child_frame(CreateCoordinationUnit(CoordinationUnitType::kFrame)),
      other_process(CreateCoordinationUnit(CoordinationUnitType::kProcess)) {
  other_frame->AddChild(child_frame->id());
  other_page->AddChild(child_frame->id());
  other_process->AddChild(child_frame->id());
}

MockMultiplePagesWithMultipleProcessesCoordinationUnitGraph::
    ~MockMultiplePagesWithMultipleProcessesCoordinationUnitGraph() = default;

}  // namespace resource_coordinator
