// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_MEMORY_DUMP_MANAGER_DELEGATE_IMPL_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_MEMORY_DUMP_MANAGER_DELEGATE_IMPL_H_

#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/resource_coordinator/public/cpp/memory/coordinator.h"
#include "services/resource_coordinator/public/interfaces/memory/memory_instrumentation.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace memory_instrumentation {

// This is the bridge between MemoryDumpManager and the Coordinator service.
// This indirection is needed to avoid a dependency from //base, where
// MemoryDumpManager lives, to //services, where the Coordinator service lives.
//
// This cannot just be implemented by the Coordinator service, because there is
// no Coordinator service in child processes. So, in a child process, the
// delegate remotely connects to the Coordinator service. In the browser
// process, the delegate locally connects to the Coordinator service.
class MemoryDumpManagerDelegateImpl
    : public base::trace_event::MemoryDumpManagerDelegate,
      public mojom::ProcessLocalDumpManager {
 public:
  class Config {
   public:
    Config(service_manager::Connector* connector,
           const std::string& service_name)
        : connector_(connector),
          service_name_(service_name),
          coordinator_(nullptr) {}
    Config(Coordinator* coordinator)
        : connector_(nullptr), coordinator_(coordinator) {}
    ~Config();

    service_manager::Connector* connector() const { return connector_; }

    const std::string& service_name() const { return service_name_; }

    Coordinator* coordinator() const { return coordinator_; }

   private:
    service_manager::Connector* connector_;
    const std::string service_name_;
    Coordinator* coordinator_;
    bool is_test_config_;
  };

  explicit MemoryDumpManagerDelegateImpl(const Config& config);
  ~MemoryDumpManagerDelegateImpl() override;

  bool IsCoordinator() const override;

  // The base::trace_event::MemoryDumpManager calls this.
  void RequestGlobalMemoryDump(
      const base::trace_event::MemoryDumpRequestArgs& args,
      const base::trace_event::MemoryDumpCallback& callback) override;

  Config config() { return config_; }
  void SetAsNonCoordinatorForTesting();

 private:
  friend std::default_delete<MemoryDumpManagerDelegateImpl>;  // For testing
  friend class MemoryDumpManagerDelegateImplTest;             // For testing

  // The ProcessLocalDumpManager interface. The coordinator calls this.
  void RequestProcessMemoryDump(
      const base::trace_event::MemoryDumpRequestArgs& args,
      const RequestProcessMemoryDumpCallback& callback) override;

  // A proxy callback for updating |pending_memory_dump_guid_|.
  void MemoryDumpCallbackProxy(
      const base::trace_event::MemoryDumpCallback& callback,
      uint64_t dump_guid,
      bool success);

  mojom::CoordinatorPtr coordinator_;
  mojo::Binding<mojom::ProcessLocalDumpManager> binding_;
  const Config config_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  uint64_t pending_memory_dump_guid_;

  // Prevents racy access to |pending_memory_dump_guid_|.
  base::Lock pending_memory_dump_guid_lock_;

  // Prevents racy access to |initialized_|.
  base::Lock initialized_lock_;

  DISALLOW_COPY_AND_ASSIGN(MemoryDumpManagerDelegateImpl);
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_MEMORY_DUMP_MANAGER_DELEGATE_IMPL_H_
