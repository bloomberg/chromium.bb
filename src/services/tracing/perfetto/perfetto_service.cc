// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/perfetto_service.h"

#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/tracing/perfetto/producer_host.h"
#include "services/tracing/public/cpp/perfetto/shared_memory.h"
#include "third_party/perfetto/include/perfetto/tracing/core/tracing_service.h"

namespace tracing {

// static
PerfettoService* PerfettoService::GetInstance() {
  static base::NoDestructor<PerfettoService> perfetto_service;
  return perfetto_service.get();
}

PerfettoService::PerfettoService(
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_testing)
    : perfetto_task_runner_(task_runner_for_testing
                                ? std::move(task_runner_for_testing)
                                : base::SequencedTaskRunnerHandle::Get()) {
  service_ = perfetto::TracingService::CreateInstance(
      std::make_unique<MojoSharedMemory::Factory>(), &perfetto_task_runner_);
  // Chromium uses scraping of the shared memory chunks to ensure that data
  // from threads without a MessageLoop doesn't get lost.
  service_->SetSMBScrapingEnabled(true);
  DCHECK(service_);
}

PerfettoService::~PerfettoService() = default;

perfetto::TracingService* PerfettoService::GetService() const {
  return service_.get();
}

void PerfettoService::BindRequest(mojom::PerfettoServiceRequest request,
                                  uint32_t pid) {
  bindings_.AddBinding(this, std::move(request), pid);
}

void PerfettoService::ConnectToProducerHost(
    mojom::ProducerClientPtr producer_client,
    mojom::ProducerHostRequest producer_host_request) {
  auto new_producer = std::make_unique<ProducerHost>();
  uint32_t producer_pid = bindings_.dispatch_context();
  new_producer->Initialize(std::move(producer_client), service_.get(),
                           base::StrCat({mojom::kPerfettoProducerName, ".",
                                         base::NumberToString(producer_pid)}));
  producer_bindings_.AddBinding(std::move(new_producer),
                                std::move(producer_host_request));
}

}  // namespace tracing
