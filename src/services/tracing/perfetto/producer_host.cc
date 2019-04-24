// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/producer_host.h"

#include <utility>

#include "base/bind.h"
#include "services/tracing/public/cpp/perfetto/shared_memory.h"
#include "third_party/perfetto/include/perfetto/tracing/core/commit_data_request.h"
#include "third_party/perfetto/include/perfetto/tracing/core/data_source_descriptor.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"

namespace tracing {

ProducerHost::ProducerHost() = default;
ProducerHost::~ProducerHost() = default;

void ProducerHost::Initialize(mojom::ProducerClientPtr producer_client,
                              perfetto::TracingService* service,
                              const std::string& name) {
  DCHECK(service);
  DCHECK(!producer_endpoint_);

  producer_client_ = std::move(producer_client);

  // TODO(oysteine): Figure out an uid once we need it.
  // TODO(oysteine): Figure out a good buffer size.
  producer_endpoint_ = service->ConnectProducer(
      this, 0 /* uid */, name,
      4 * 1024 * 1024 /* shared_memory_size_hint_bytes */);
  DCHECK(producer_endpoint_);

  producer_client_.set_connection_error_handler(
      base::BindOnce(&ProducerHost::OnConnectionError, base::Unretained(this)));
}

void ProducerHost::OnConnectionError() {
  // Manually reset to prevent any callbacks from the ProducerEndpoint
  // when we're in a half-destructed state.
  producer_endpoint_.reset();
}

void ProducerHost::OnConnect() {
}

void ProducerHost::OnDisconnect() {
  // Deliberately empty, this is invoked by the |service_| business logic after
  // we destroy the |producer_endpoint|.
}

void ProducerHost::OnTracingSetup() {
  MojoSharedMemory* shared_memory =
      static_cast<MojoSharedMemory*>(producer_endpoint_->shared_memory());
  DCHECK(shared_memory);
  DCHECK(producer_client_);
  mojo::ScopedSharedBufferHandle shm = shared_memory->Clone();
  DCHECK(shm.is_valid());

  producer_client_->OnTracingStart(std::move(shm));
}

void ProducerHost::SetupDataSource(perfetto::DataSourceInstanceID,
                                   const perfetto::DataSourceConfig&) {
  // TODO(primiano): plumb call through mojo.
}

void ProducerHost::StartDataSource(perfetto::DataSourceInstanceID id,
                                   const perfetto::DataSourceConfig& config) {
  // The type traits will send the base fields in the DataSourceConfig and also
  // the ChromeConfig other configs are dropped.
  producer_client_->StartDataSource(id, config);
}

void ProducerHost::StopDataSource(perfetto::DataSourceInstanceID id) {
  if (producer_client_) {
    producer_client_->StopDataSource(
        id,
        base::BindOnce(
            [](ProducerHost* producer_host, perfetto::DataSourceInstanceID id) {
              producer_host->producer_endpoint_->NotifyDataSourceStopped(id);
            },
            base::Unretained(this), id));
  }
}

void ProducerHost::Flush(
    perfetto::FlushRequestID id,
    const perfetto::DataSourceInstanceID* raw_data_source_ids,
    size_t num_data_sources) {
  DCHECK(producer_client_);
  std::vector<uint64_t> data_source_ids(raw_data_source_ids,
                                        raw_data_source_ids + num_data_sources);
  DCHECK_EQ(data_source_ids.size(), num_data_sources);
  producer_client_->Flush(id, data_source_ids);
}

// This data can come from a malicious child process. We don't do any
// sanitization here because ProducerEndpoint::CommitData() (And any other
// ProducerEndpoint methods) are designed to deal with malformed / malicious
// inputs.
void ProducerHost::CommitData(const perfetto::CommitDataRequest& data_request) {
  if (on_commit_callback_for_testing_) {
    on_commit_callback_for_testing_.Run(data_request);
  }
  producer_endpoint_->CommitData(data_request);
}

void ProducerHost::RegisterDataSource(
    const perfetto::DataSourceDescriptor& registration_info) {
  producer_endpoint_->RegisterDataSource(registration_info);
}

void ProducerHost::NotifyFlushComplete(uint64_t flush_request_id) {
  producer_endpoint_->NotifyFlushComplete(flush_request_id);
}

void ProducerHost::RegisterTraceWriter(uint32_t writer_id,
                                       uint32_t target_buffer) {
  producer_endpoint_->RegisterTraceWriter(writer_id, target_buffer);
}

void ProducerHost::UnregisterTraceWriter(uint32_t writer_id) {
  producer_endpoint_->UnregisterTraceWriter(writer_id);
}

}  // namespace tracing
