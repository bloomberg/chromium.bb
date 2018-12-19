// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_BASE_AGENT_H_
#define SERVICES_TRACING_PUBLIC_CPP_BASE_AGENT_H_

#include <string>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/tracing/public/mojom/tracing.mojom.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

// This class is a minimal implementation of mojom::Agent to reduce boilerplate
// code in tracing agents. A tracing agent can inherit from this class and only
// override methods that actually do something, in most cases only StartTracing
// and StopAndFlush.
namespace tracing {
class COMPONENT_EXPORT(TRACING_CPP) BaseAgent : public mojom::Agent {
 public:
  ~BaseAgent() override;

  virtual void Connect(service_manager::Connector* connector);

 protected:
  BaseAgent(const std::string& label,
            mojom::TraceDataType type,
            base::ProcessId pid);

 private:
  // tracing::mojom::Agent:
  void StartTracing(const std::string& config,
                    base::TimeTicks coordinator_time,
                    Agent::StartTracingCallback callback) override;
  void StopAndFlush(tracing::mojom::RecorderPtr recorder) override;
  void GetCategories(Agent::GetCategoriesCallback callback) override;
  void RequestBufferStatus(
      Agent::RequestBufferStatusCallback callback) override;

  mojo::Binding<tracing::mojom::Agent> binding_;

  const std::string label_;
  const mojom::TraceDataType type_;
  const base::ProcessId pid_;

  DISALLOW_COPY_AND_ASSIGN(BaseAgent);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_BASE_AGENT_H_
