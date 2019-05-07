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
#include "services/tracing/perfetto/consumer_host.h"
#include "services/tracing/perfetto/producer_host.h"
#include "services/tracing/public/cpp/perfetto/shared_memory.h"
#include "third_party/perfetto/include/perfetto/tracing/core/tracing_service.h"

namespace tracing {

namespace {

bool StringToProcessId(const std::string& input, base::ProcessId* output) {
  // Pid is encoded as uint in the string.
  return base::StringToUint(input, reinterpret_cast<uint32_t*>(output));
}

}  // namespace

// static
bool PerfettoService::ParsePidFromProducerName(const std::string& producer_name,
                                               base::ProcessId* pid) {
  if (!base::StartsWith(producer_name, mojom::kPerfettoProducerNamePrefix,
                        base::CompareCase::SENSITIVE)) {
    LOG(DFATAL) << "Unexpected producer name: " << producer_name;
    return false;
  }

  static const size_t kPrefixLength =
      strlen(mojom::kPerfettoProducerNamePrefix);
  if (!StringToProcessId(producer_name.substr(kPrefixLength), pid)) {
    LOG(DFATAL) << "Unexpected producer name: " << producer_name;
    return false;
  }
  return true;
}

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

  // Trace events emitted from the taskqueue and other places can cause the
  // Perfetto task runner to queue tasks instead of directly posting them to the
  // taskrunner; we start a timer to periodically flush these (rare) tasks.
  // This is needed in addition to the timer we start in
  // TraceEventDataSource::BeginTracing as the SharedMemoryArbiter will post
  // tasks to the taskrunner used by the Perfetto service rather than the
  // taskrunner used by the ProducerClient, when running in in-process mode.
  perfetto_task_runner_.StartDeferredTasksDrainTimer();
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
                           base::StrCat({mojom::kPerfettoProducerNamePrefix,
                                         base::NumberToString(producer_pid)}));
  producer_bindings_.AddBinding(std::move(new_producer),
                                std::move(producer_host_request));
}

void PerfettoService::AddActiveServicePid(base::ProcessId pid) {
  active_service_pids_.insert(pid);
  for (auto* tracing_session : tracing_sessions_) {
    tracing_session->OnActiveServicePidAdded(pid);
  }
}

void PerfettoService::RemoveActiveServicePid(base::ProcessId pid) {
  active_service_pids_.erase(pid);
  for (auto* tracing_session : tracing_sessions_) {
    tracing_session->OnActiveServicePidRemoved(pid);
  }
}

void PerfettoService::SetActiveServicePidsInitialized() {
  active_service_pids_initialized_ = true;
  for (auto* tracing_session : tracing_sessions_) {
    tracing_session->OnActiveServicePidsInitialized();
  }
}

void PerfettoService::RegisterTracingSession(
    ConsumerHost::TracingSession* tracing_session) {
  tracing_sessions_.insert(tracing_session);
}

void PerfettoService::UnregisterTracingSession(
    ConsumerHost::TracingSession* tracing_session) {
  tracing_sessions_.erase(tracing_session);
}

}  // namespace tracing
