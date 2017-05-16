// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory/coordinator/process_map.h"

#include "base/process/process_handle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/resource_coordinator/public/interfaces/memory/memory_instrumentation.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/interfaces/service_manager.mojom.h"

namespace memory_instrumentation {

ProcessMap::ProcessMap(service_manager::Connector* connector) : binding_(this) {
  if (connector) {
    service_manager::mojom::ServiceManagerPtr service_manager;
    connector->BindInterface(service_manager::mojom::kServiceName,
                             &service_manager);
    service_manager::mojom::ServiceManagerListenerPtr listener;
    service_manager::mojom::ServiceManagerListenerRequest request(
        mojo::MakeRequest(&listener));
    service_manager->AddListener(std::move(listener));

    binding_.Bind(std::move(request));
  }
}

ProcessMap::~ProcessMap() {}

void ProcessMap::OnInit(std::vector<RunningServiceInfoPtr> instances) {
  // This callback should only be called with an empty model.
  DCHECK(instances_.empty());
  for (size_t i = 0; i < instances.size(); ++i) {
    const service_manager::Identity& identity = instances[i]->identity;
    instances_.emplace(identity, instances[i]->pid);
  }
}

void ProcessMap::OnServiceCreated(RunningServiceInfoPtr instance) {
  if (instance->pid == base::kNullProcessId)
    return;

  const service_manager::Identity& identity = instance->identity;
  DCHECK(instances_.find(identity) == instances_.end());
  instances_.emplace(identity, instance->pid);
}

void ProcessMap::OnServiceStarted(const service_manager::Identity& identity,
                                  uint32_t pid) {
  instances_[identity] = pid;
}

void ProcessMap::OnServiceStopped(const service_manager::Identity& identity) {
  instances_.erase(identity);
}

base::ProcessId ProcessMap::GetProcessId(
    service_manager::Identity identity) const {
  auto instance = instances_.find(identity);
  if (instance == instances_.end())
    return base::kNullProcessId;
  return instance->second;
}

}  // namespace memory_instrumentation
