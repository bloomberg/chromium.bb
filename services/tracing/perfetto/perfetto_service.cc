// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/perfetto_service.h"

#include <utility>

#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/tracing/perfetto/producer_host.h"
#include "services/tracing/public/cpp/perfetto/shared_memory.h"
#include "third_party/perfetto/include/perfetto/tracing/core/tracing_service.h"

namespace tracing {

namespace {

const char kPerfettoProducerName[] = "org.chromium.perfetto_producer";

}  // namespace

/*
TODO(oysteine): Right now the Perfetto service runs on a dedicated
thread for a couple of reasons:
* The sequence needs to be locked to a specific thread, or Perfetto's
  thread-checker will barf.
* The PerfettoTracingCoordinator uses
  mojo::BlockingCopyFromString to pass the string to the tracing
  controller, which requires the WithBaseSyncPrimitives task trait and
  SingleThreadTaskRunners which use this trait need to be running on a
  dedicated trait to avoid blocking other sequences.
* If a client fills up its Shared Memory Buffer when writing a Perfetto
  event proto, it'll stall until the Perfetto service clears up space.
  This won't happen if the client and the service happens to run on the same
  thread (the Mojo calls will never be executed).

The above should be resolved before we move the Perfetto usage out from the
flag so we can run this on non-thread-bound sequence.
*/

// static
PerfettoService* PerfettoService::GetInstance() {
  static base::NoDestructor<PerfettoService> perfetto_service;
  return perfetto_service.get();
}

PerfettoService::PerfettoService(
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_testing)
    : perfetto_task_runner_(
          task_runner_for_testing
              ? task_runner_for_testing
              : base::CreateSingleThreadTaskRunnerWithTraits(
                    {base::MayBlock(),
                     base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
                     base::WithBaseSyncPrimitives(),
                     base::TaskPriority::BEST_EFFORT},
                    base::SingleThreadTaskRunnerThreadMode::DEDICATED)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  perfetto_task_runner_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&PerfettoService::CreateServiceOnSequence,
                                base::Unretained(this)));
}

PerfettoService::~PerfettoService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PerfettoService::CreateServiceOnSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_ = perfetto::TracingService::CreateInstance(
      std::make_unique<MojoSharedMemory::Factory>(), &perfetto_task_runner_);
  // Chromium uses scraping of the shared memory chunks to ensure that data
  // from threads without a MessageLoop doesn't get lost.
  service_->SetSMBScrapingEnabled(true);
  DCHECK(service_);
}

perfetto::TracingService* PerfettoService::GetService() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return service_.get();
}

void PerfettoService::BindRequest(mojom::PerfettoServiceRequest request) {
  perfetto_task_runner_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&PerfettoService::BindOnSequence,
                                base::Unretained(this), std::move(request)));
}

void PerfettoService::BindOnSequence(mojom::PerfettoServiceRequest request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bindings_.AddBinding(this, std::move(request));
}

void PerfettoService::ConnectToProducerHost(
    mojom::ProducerClientPtr producer_client,
    mojom::ProducerHostRequest producer_host_request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto new_producer = std::make_unique<ProducerHost>();
  new_producer->Initialize(std::move(producer_client), service_.get(),
                           kPerfettoProducerName);
  producer_bindings_.AddBinding(std::move(new_producer),
                                std::move(producer_host_request));
}

}  // namespace tracing
