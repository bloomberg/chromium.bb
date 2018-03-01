// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_TRACING_SERVICE_H_
#define SERVICES_TRACING_TRACING_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "services/tracing/agent_registry.h"
#include "services/tracing/coordinator.h"

namespace tracing {

class TracingService : public service_manager::Service {
 public:
  TracingService();
  ~TracingService() override;

  // service_manager::Service:
  // Factory function for use as an embedded service.
  static std::unique_ptr<service_manager::Service> Create();

  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  service_manager::ServiceContextRefFactory* ref_factory() {
    return ref_factory_.get();
  }

 private:
  service_manager::BinderRegistryWithArgs<
      const service_manager::BindSourceInfo&>
      registry_;
  std::unique_ptr<tracing::AgentRegistry> tracing_agent_registry_;
  std::unique_ptr<tracing::Coordinator> tracing_coordinator_;
  std::unique_ptr<service_manager::ServiceContextRefFactory> ref_factory_;

  // WeakPtrFactory members should always come last so WeakPtrs are destructed
  // before other members.
  base::WeakPtrFactory<TracingService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TracingService);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_TRACING_SERVICE_H_
