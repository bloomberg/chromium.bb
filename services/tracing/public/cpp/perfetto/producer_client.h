// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PRODUCER_CLIENT_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PRODUCER_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/atomicops.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/tracing/public/cpp/perfetto/task_runner.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/core/service.h"

namespace perfetto {
class SharedMemoryArbiter;
}  // namespace perfetto

namespace tracing {

class MojoSharedMemory;

// This class is the per-process client side of the Perfetto
// producer, and is responsible for creating specific kinds
// of DataSources (like ChromeTracing) on demand, and provide
// them with TraceWriters and a configuration to start logging.

// Implementations of new DataSources should:
// * Implement ProducerClient::DataSourceBase.
// * Add a new data source name in perfetto_service.mojom.
// * Register the data source with Perfetto in ProducerHost::OnConnect.
// * Construct the new implementation when requested to
//   in ProducerClient::CreateDataSourceInstance.
class COMPONENT_EXPORT(TRACING_CPP) ProducerClient
    : public mojom::ProducerClient,
      public perfetto::Service::ProducerEndpoint {
 public:
  ProducerClient();
  ~ProducerClient() override;

  static void DeleteSoon(std::unique_ptr<ProducerClient>);

  // Connect to the service-side ProducerHost which will
  // let Perfetto know we're ready to start logging
  // our data.
  mojom::ProducerClientPtr CreateAndBindProducerClient();
  mojom::ProducerHostRequest CreateProducerHostRequest();

  bool has_connected_producer_host_for_testing() const {
    return !!producer_host_;
  }

  // mojom::ProducerClient implementation.
  // Called through Mojo by the ProducerHost on the service-side to control
  // tracing and toggle specific DataSources.
  void OnTracingStart(mojo::ScopedSharedBufferHandle shared_memory) override;
  void CreateDataSourceInstance(
      uint64_t id,
      mojom::DataSourceConfigPtr data_source_config) override;

  void TearDownDataSourceInstance(uint64_t id) override;
  void Flush(uint64_t flush_request_id,
             const std::vector<uint64_t>& data_source_ids) override;

  // perfetto::Service::ProducerEndpoint implementation.
  // Used by the TraceWriters
  // to signal Perfetto that shared memory chunks are ready
  // for consumption.
  void CommitData(const perfetto::CommitDataRequest& commit,
                  CommitDataCallback callback) override;
  // Used by the DataSource implementations to create TraceWriters
  // for writing their protobufs
  std::unique_ptr<perfetto::TraceWriter> CreateTraceWriter(
      perfetto::BufferID target_buffer) override;
  perfetto::SharedMemory* shared_memory() const override;

  // These ProducerEndpoint functions are only used on the service
  // side and should not be called on the clients.
  void RegisterDataSource(const perfetto::DataSourceDescriptor&) override;
  void UnregisterDataSource(const std::string& name) override;
  size_t shared_buffer_page_size_kb() const override;
  void NotifyFlushComplete(perfetto::FlushRequestID) override;

 protected:
  base::SequencedTaskRunner* GetTaskRunner();

 private:
  void BindOnSequence(mojom::ProducerClientRequest request);
  void CommitDataOnSequence(mojom::CommitDataRequestPtr request);

  // Keep the TaskRunner first in the member list so it outlives
  // everything else and no dependent classes will try to use
  // an invalid taskrunner.
  PerfettoTaskRunner perfetto_task_runner_;
  std::unique_ptr<mojo::Binding<mojom::ProducerClient>> binding_;
  std::unique_ptr<perfetto::SharedMemoryArbiter> shared_memory_arbiter_;
  mojom::ProducerHostPtr producer_host_;
  std::unique_ptr<MojoSharedMemory> shared_memory_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ProducerClient);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PRODUCER_CLIENT_H_
