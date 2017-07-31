// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/coordination_unit_introspector_impl.h"

#include <vector>

#include "base/process/process_handle.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_impl.h"

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
    mojom::ProcessInfoPtr process_info(mojom::ProcessInfo::New());

    // The implicit contract for process CUs is that the |id| is the |pid|.
    process_info->pid = process_cu->id().id;
    DCHECK_NE(base::kNullProcessId, process_info->pid);

    std::vector<std::string> urls;
    std::set<CoordinationUnitImpl*> frame_cus =
        process_cu->GetAssociatedCoordinationUnitsOfType(
            CoordinationUnitType::kFrame);
    for (CoordinationUnitImpl* frame_cu : frame_cus) {
      base::Value url_value = frame_cu->GetProperty(
          resource_coordinator::mojom::PropertyType::kURL);
      if (url_value.is_string()) {
        urls.push_back(url_value.GetString());
      }
    }
    process_info->urls = std::move(urls);
    process_infos.push_back(std::move(process_info));
  }
  callback.Run(std::move(process_infos));
}

void CoordinationUnitIntrospectorImpl::BindToInterface(
    resource_coordinator::mojom::CoordinationUnitIntrospectorRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace resource_coordinator
