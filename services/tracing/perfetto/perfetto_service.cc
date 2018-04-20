// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/perfetto_service.h"

#include <utility>

#include "base/task_scheduler/post_task.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/tracing/perfetto/producer_host.h"
#include "services/tracing/public/cpp/perfetto/shared_memory.h"

#include "third_party/perfetto/include/perfetto/tracing/core/service.h"

namespace tracing {

namespace {

const char kPerfettoProducerName[] = "org.chromium.perfetto_producer";

PerfettoService* g_perfetto_service;

// Just used to destroy disconnected clients.
template <typename T>
void OnClientDisconnect(std::unique_ptr<T>) {}

}  // namespace

// TODO(oysteine): Figure out if this is the correct TaskRunner to use.
PerfettoService::PerfettoService()
    : perfetto_task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  service_ = perfetto::Service::CreateInstance(
      std::make_unique<MojoSharedMemory::Factory>(), &perfetto_task_runner_);
  DCHECK(service_);
  DCHECK(!g_perfetto_service);
  g_perfetto_service = this;
}

PerfettoService::~PerfettoService() {
  DCHECK_EQ(g_perfetto_service, this);
  g_perfetto_service = nullptr;
}

// static
PerfettoService* PerfettoService::GetInstance() {
  return g_perfetto_service;
}

void PerfettoService::BindRequest(
    mojom::PerfettoServiceRequest request,
    const service_manager::BindSourceInfo& source_info) {
  bindings_.AddBinding(this, std::move(request), source_info.identity);
}

void PerfettoService::ConnectToProducerHost(
    mojom::ProducerClientPtr producer_client,
    mojom::ProducerHostRequest producer_host) {
  auto new_producer = std::make_unique<ProducerHost>();
  new_producer->Initialize(std::move(producer_client), std::move(producer_host),
                           service_.get(), kPerfettoProducerName);
  new_producer->set_connection_error_handler(base::BindOnce(
      &OnClientDisconnect<ProducerHost>, std::move(new_producer)));
}

}  // namespace tracing
