// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_TRACED_PROCESS_IMPL_H_
#define SERVICES_TRACING_PUBLIC_CPP_TRACED_PROCESS_IMPL_H_

#include <set>
#include <string>

#include "base/component_export.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/tracing/public/mojom/traced_process.mojom.h"
#include "services/tracing/public/mojom/tracing.mojom.h"

namespace tracing {

class BaseAgent;

// Each running process will bind this singleton
// to the mojom::TracedProcess interface, which will be
// connected to by the tracing service to enable tracing
// support.
class COMPONENT_EXPORT(TRACING_CPP) TracedProcessImpl
    : public mojom::TracedProcess {
 public:
  static TracedProcessImpl* GetInstance();

  void OnTracedProcessRequest(mojom::TracedProcessRequest request);

  // Set which taskrunner to bind any incoming requests on.
  void SetTaskRunner(scoped_refptr<base::SequencedTaskRunner> task_runner);

  void RegisterAgent(BaseAgent* agent);
  void UnregisterAgent(BaseAgent* agent);

  // Populate categories from all of the registered agents.
  void GetCategories(std::set<std::string>* category_set);

 private:
  friend class base::NoDestructor<TracedProcessImpl>;
  TracedProcessImpl();
  ~TracedProcessImpl() override;

  // tracing::mojom::TracedProcess:
  void ConnectToTracingService(
      mojom::ConnectToTracingRequestPtr request,
      ConnectToTracingServiceCallback callback) override;

  // Lock protecting binding_.
  base::Lock lock_;
  std::set<BaseAgent*> agents_;
  tracing::mojom::AgentRegistryPtr agent_registry_;
  mojo::Binding<tracing::mojom::TracedProcess> binding_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(TracedProcessImpl);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_TRACED_PROCESS_IMPL_H_
