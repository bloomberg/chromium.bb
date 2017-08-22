// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/coordination_unit_introspector_impl.h"

#include <vector>

#include "base/process/process_handle.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"

namespace resource_coordinator {

CoordinationUnitIntrospectorImpl::CoordinationUnitIntrospectorImpl() = default;

CoordinationUnitIntrospectorImpl::~CoordinationUnitIntrospectorImpl() = default;

void CoordinationUnitIntrospectorImpl::GetProcessToURLMap(
    const GetProcessToURLMapCallback& callback) {
  std::vector<resource_coordinator::mojom::ProcessInfoPtr> process_infos;
  std::vector<CoordinationUnitImpl*> process_cus =
      CoordinationUnitImpl::GetCoordinationUnitsOfType(
          CoordinationUnitType::kProcess);
  for (CoordinationUnitImpl* process_cu : process_cus) {
    int64_t pid;
    if (!process_cu->GetProperty(mojom::PropertyType::kPID, &pid))
      continue;

    mojom::ProcessInfoPtr process_info(mojom::ProcessInfo::New());
    process_info->pid = pid;
    DCHECK_NE(base::kNullProcessId, process_info->pid);

    std::set<CoordinationUnitImpl*> web_contents_cus =
        process_cu->GetAssociatedCoordinationUnitsOfType(
            CoordinationUnitType::kWebContents);
    for (CoordinationUnitImpl* web_contents_cu : web_contents_cus) {
      int64_t ukm_source_id;
      if (web_contents_cu->GetProperty(
              resource_coordinator::mojom::PropertyType::kUKMSourceId,
              &ukm_source_id)) {
        process_info->ukm_source_ids.push_back(ukm_source_id);
      }
    }
    process_infos.push_back(std::move(process_info));
  }
  callback.Run(std::move(process_infos));
}

void CoordinationUnitIntrospectorImpl::BindToInterface(
    resource_coordinator::mojom::CoordinationUnitIntrospectorRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace resource_coordinator
