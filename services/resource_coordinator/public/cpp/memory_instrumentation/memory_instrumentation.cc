// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace memory_instrumentation {

namespace {
MemoryInstrumentation* g_instance = nullptr;

void DestroyCoordinatorTLS(void* tls_object) {
  delete reinterpret_cast<mojom::CoordinatorPtr*>(tls_object);
};
}  // namespace

// static
void MemoryInstrumentation::CreateInstance(
    service_manager::Connector* connector,
    const std::string& service_name) {
  DCHECK(!g_instance);
  g_instance = new MemoryInstrumentation(connector, service_name);
}

// static
MemoryInstrumentation* MemoryInstrumentation::GetInstance() {
  return g_instance;
}

MemoryInstrumentation::MemoryInstrumentation(
    service_manager::Connector* connector,
    const std::string& service_name)
    : connector_(connector),
      connector_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      tls_coordinator_(&DestroyCoordinatorTLS),
      service_name_(service_name),
      dump_id_() {
  DCHECK(connector_task_runner_);
}

MemoryInstrumentation::~MemoryInstrumentation() {
  g_instance = nullptr;
}

void MemoryInstrumentation::RequestGlobalDumpAndAppendToTrace(
    MemoryDumpType dump_type,
    MemoryDumpLevelOfDetail level_of_detail,
    RequestGlobalDumpAndAppendToTraceCallback callback) {
  const auto& coordinator = GetCoordinatorBindingForCurrentThread();
  auto callback_adapter = [](RequestGlobalDumpAndAppendToTraceCallback callback,
                             uint64_t dump_id, bool success,
                             mojom::GlobalMemoryDumpPtr) {
    if (callback)
      callback.Run(success, dump_id);
  };
  // TODO(primiano): get rid of dump_id. It should be a return-only argument.
  base::trace_event::MemoryDumpRequestArgs args = {++dump_id_, dump_type,
                                                   level_of_detail};
  coordinator->RequestGlobalMemoryDump(args,
                                       base::Bind(callback_adapter, callback));
}

const mojom::CoordinatorPtr&
MemoryInstrumentation::GetCoordinatorBindingForCurrentThread() {
  mojom::CoordinatorPtr* coordinator =
      reinterpret_cast<mojom::CoordinatorPtr*>(tls_coordinator_.Get());
  if (!coordinator) {
    coordinator = new mojom::CoordinatorPtr();
    tls_coordinator_.Set(coordinator);
    mojom::CoordinatorRequest coordinator_req = mojo::MakeRequest(coordinator);

    // The connector is not thread safe and BindInterface must be called on its
    // own thread. Thankfully, the binding can happen *after* having started
    // invoking methos on the |coordinator| proxy objects, as requests will be
    // internally queued by Mojo and flushed when the BindInterface() happens.
    connector_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(
            &MemoryInstrumentation::BindCoordinatorRequestOnConnectorThread,
            base::Unretained(this), base::Passed(std::move(coordinator_req))));
  }
  DCHECK(*coordinator);
  return *coordinator;
}

void MemoryInstrumentation::BindCoordinatorRequestOnConnectorThread(
    mojom::CoordinatorRequest coordinator_request) {
  connector_->BindInterface(service_name_, std::move(coordinator_request));
}

}  // namespace memory_instrumentation
