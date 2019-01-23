// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PERFETTO_TEST_UTILS_H_
#define SERVICES_TRACING_PERFETTO_TEST_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "services/tracing/perfetto/producer_host.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "third_party/perfetto/include/perfetto/tracing/core/consumer.h"

namespace tracing {

const char kPerfettoTestDataSourceName[] =
    "org.chromium.chrome_integration_unittest";
const char kPerfettoProducerName[] = "chrome_producer_test";
const char kPerfettoTestString[] = "d00df00d";
const size_t kLargeMessageSize = 1 * 1024 * 1024;

class TestDataSource : public ProducerClient::DataSourceBase {
 public:
  TestDataSource(const std::string& data_source_name, size_t send_packet_count);
  ~TestDataSource() override;

  void WritePacketBigly();

  // DataSourceBase implementation
  void StartTracing(ProducerClient* producer_client,
                    const mojom::DataSourceConfig& data_source_config) override;
  void StopTracing(
      base::OnceClosure stop_complete_callback = base::OnceClosure()) override;
  void Flush(base::RepeatingClosure flush_complete_callback) override;

 private:
  ProducerClient* producer_client_ = nullptr;
  const size_t send_packet_count_;
  uint32_t target_buffer_ = 0;
};

class MockProducerClient : public ProducerClient {
 public:
  MockProducerClient(
      size_t send_packet_count,
      base::OnceClosure client_enabled_callback = base::OnceClosure(),
      base::OnceClosure client_disabled_callback = base::OnceClosure());
  ~MockProducerClient() override;

  void SetupDataSource(const std::string& data_source_name);

  void StartDataSource(uint64_t id,
                       mojom::DataSourceConfigPtr data_source_config) override;

  void StopDataSource(uint64_t id, StopDataSourceCallback callback) override;

  void CommitData(const perfetto::CommitDataRequest& commit,
                  CommitDataCallback callback = {}) override;

  void SetAgentEnabledCallback(base::OnceClosure client_enabled_callback);

  void SetAgentDisabledCallback(base::OnceClosure client_disabled_callback);

  const std::string& all_client_commit_data_requests() const {
    return all_client_commit_data_requests_;
  }

  TestDataSource* data_source() { return enabled_data_source_.get(); }

 private:
  base::OnceClosure client_enabled_callback_;
  base::OnceClosure client_disabled_callback_;
  size_t send_packet_count_;
  std::string all_client_commit_data_requests_;
  std::unique_ptr<TestDataSource> enabled_data_source_;
};

class MockConsumer : public perfetto::Consumer {
 public:
  using PacketReceivedCallback = std::function<void(bool)>;
  MockConsumer(std::string data_source_name,
               perfetto::TracingService* service,
               PacketReceivedCallback packet_received_callback);
  ~MockConsumer() override;

  void ReadBuffers();

  void StopTracing();

  void StartTracing();

  void FreeBuffers();

  size_t received_packets() const { return received_packets_; }

  // perfetto::Consumer implementation
  void OnConnect() override;
  void OnDisconnect() override;
  void OnTracingDisabled() override;

  void OnTraceData(std::vector<perfetto::TracePacket> packets,
                   bool has_more) override;
  void OnDetach(bool success) override;
  void OnAttach(bool success, const perfetto::TraceConfig&) override;
  void OnTraceStats(bool success, const perfetto::TraceStats&) override;

 private:
  std::unique_ptr<perfetto::TracingService::ConsumerEndpoint>
      consumer_endpoint_;
  size_t received_packets_ = 0;
  PacketReceivedCallback packet_received_callback_;
  std::string data_source_name_;
};

class MockProducerHost : public ProducerHost {
 public:
  MockProducerHost(
      const std::string& data_source_name,
      perfetto::TracingService* service,
      MockProducerClient* producer_client,
      base::OnceClosure datasource_registered_callback = base::OnceClosure());
  ~MockProducerHost() override;

  void RegisterDataSource(
      mojom::DataSourceRegistrationPtr registration_info) override;

  void OnConnect() override;

  void OnCommit(const perfetto::CommitDataRequest& commit_data_request);

  void OnMessagepipesReadyCallback(
      perfetto::TracingService* perfetto_service,
      mojom::ProducerClientPtr producer_client_pipe,
      mojom::ProducerHostRequest producer_host_pipe);

  const std::string& all_host_commit_data_requests() const {
    return all_host_commit_data_requests_;
  }

 protected:
  base::OnceClosure datasource_registered_callback_;
  const std::string data_source_name_;
  std::string all_host_commit_data_requests_;
  std::unique_ptr<mojo::Binding<mojom::ProducerHost>> binding_;
};

class MockProducer {
 public:
  MockProducer(const std::string& data_source_name,
               perfetto::TracingService* service,
               base::OnceClosure on_datasource_registered,
               base::OnceClosure on_tracing_started,
               size_t num_packets = 10);
  virtual ~MockProducer();

  void WritePacketBigly(base::OnceClosure on_write_complete);

 private:
  std::unique_ptr<MockProducerClient> producer_client_;
  std::unique_ptr<MockProducerHost> producer_host_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PERFETTO_TEST_UTILS_H_
