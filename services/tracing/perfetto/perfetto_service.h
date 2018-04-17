// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PERFETTO_PERFETTO_SERVICE_H_
#define SERVICES_TRACING_PERFETTO_PERFETTO_SERVICE_H_

#include <memory>
#include <set>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/tracing/public/cpp/perfetto/task_runner.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"

namespace service_manager {
struct BindSourceInfo;
}  // namespace service_manager

namespace perfetto {
class Service;
}  // namespace perfetto

namespace tracing {

// This class serves two purposes: It wraps the use of the system-wide
// perfetto::Service instance, and serves as the main Mojo interface for
// connecting per-process ProducerClient with corresponding service-side
// ProducerHost.
class PerfettoService : public mojom::PerfettoService {
 public:
  PerfettoService();
  ~PerfettoService() override;

  static PerfettoService* GetInstance();

  void BindRequest(mojom::PerfettoServiceRequest request,
                   const service_manager::BindSourceInfo& source_info);

  // mojom::PerfettoService implementation.
  void ConnectToProducerHost(mojom::ProducerClientPtr producer_client,
                             mojom::ProducerHostRequest producer_host) override;

  perfetto::Service* service() const { return service_.get(); }

 private:
  PerfettoTaskRunner perfetto_task_runner_;
  std::unique_ptr<perfetto::Service> service_;
  mojo::BindingSet<mojom::PerfettoService, service_manager::Identity> bindings_;

  DISALLOW_COPY_AND_ASSIGN(PerfettoService);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PERFETTO_PERFETTO_SERVICE_H_
