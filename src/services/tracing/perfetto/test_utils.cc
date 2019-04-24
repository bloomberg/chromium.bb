// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "services/tracing/perfetto/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/tracing/core/commit_data_request.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_packet.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_writer.h"
#include "third_party/perfetto/protos/perfetto/common/commit_data_request.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/test_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace tracing {

TestDataSource::TestDataSource(const std::string& data_source_name,
                               size_t send_packet_count)
    : DataSourceBase(data_source_name), send_packet_count_(send_packet_count) {}

TestDataSource::~TestDataSource() = default;

void TestDataSource::WritePacketBigly() {
  std::unique_ptr<char[]> payload(new char[kLargeMessageSize]);
  memset(payload.get(), '.', kLargeMessageSize);
  payload.get()[kLargeMessageSize - 1] = 0;

  std::unique_ptr<perfetto::TraceWriter> writer =
      producer_client_->CreateTraceWriter(target_buffer_);
  CHECK(writer);

  writer->NewTracePacket()->set_for_testing()->set_str(payload.get(),
                                                       kLargeMessageSize);
}

void TestDataSource::StartTracing(
    ProducerClient* producer_client,
    const perfetto::DataSourceConfig& data_source_config) {
  producer_client_ = producer_client;
  target_buffer_ = data_source_config.target_buffer();

  if (send_packet_count_ > 0) {
    std::unique_ptr<perfetto::TraceWriter> writer =
        producer_client_->CreateTraceWriter(target_buffer_);
    CHECK(writer);

    for (size_t i = 0; i < send_packet_count_; i++) {
      writer->NewTracePacket()->set_for_testing()->set_str(kPerfettoTestString);
    }
  }
}

void TestDataSource::StopTracing(base::OnceClosure stop_complete_callback) {
  std::move(stop_complete_callback).Run();
}

void TestDataSource::Flush(base::RepeatingClosure flush_complete_callback) {
  if (flush_complete_callback) {
    flush_complete_callback.Run();
  }
}

MockProducerClient::MockProducerClient(
    size_t send_packet_count,
    base::OnceClosure client_enabled_callback,
    base::OnceClosure client_disabled_callback)
    : client_enabled_callback_(std::move(client_enabled_callback)),
      client_disabled_callback_(std::move(client_disabled_callback)),
      send_packet_count_(send_packet_count) {}

MockProducerClient::~MockProducerClient() = default;

void MockProducerClient::SetupDataSource(const std::string& data_source_name) {
  enabled_data_source_ =
      std::make_unique<TestDataSource>(data_source_name, send_packet_count_);
  AddDataSource(enabled_data_source_.get());
}

void MockProducerClient::StartDataSource(
    uint64_t id,
    const perfetto::DataSourceConfig& data_source_config) {
  ProducerClient::StartDataSource(id, std::move(data_source_config));

  if (client_enabled_callback_) {
    std::move(client_enabled_callback_).Run();
  }
}

void MockProducerClient::StopDataSource(uint64_t id,
                                        StopDataSourceCallback callback) {
  ProducerClient::StopDataSource(id, std::move(callback));

  if (client_disabled_callback_) {
    std::move(client_disabled_callback_).Run();
  }
}

void MockProducerClient::CommitData(const perfetto::CommitDataRequest& commit,
                                    CommitDataCallback callback) {
  // Only write out commits that have actual data in it; Perfetto
  // might send two commits from different threads (one always empty),
  // which causes TSan to complain.
  if (commit.chunks_to_patch_size() || commit.chunks_to_move_size()) {
    perfetto::protos::CommitDataRequest proto;
    commit.ToProto(&proto);
    std::string proto_string;
    CHECK(proto.SerializeToString(&proto_string));
    all_client_commit_data_requests_ += proto_string;
  }
  ProducerClient::CommitData(commit, callback);
}

void MockProducerClient::SetAgentEnabledCallback(
    base::OnceClosure client_enabled_callback) {
  client_enabled_callback_ = std::move(client_enabled_callback);
}

void MockProducerClient::SetAgentDisabledCallback(
    base::OnceClosure client_disabled_callback) {
  client_disabled_callback_ = std::move(client_disabled_callback);
}

MockConsumer::MockConsumer(std::string data_source_name,
                           perfetto::TracingService* service,
                           PacketReceivedCallback packet_received_callback)
    : packet_received_callback_(packet_received_callback),
      data_source_name_(data_source_name) {
  consumer_endpoint_ = service->ConnectConsumer(this, /*uid=*/0);
}

MockConsumer::~MockConsumer() = default;

void MockConsumer::ReadBuffers() {
  consumer_endpoint_->ReadBuffers();
}

void MockConsumer::StopTracing() {
  ReadBuffers();
  consumer_endpoint_->DisableTracing();
}

void MockConsumer::StartTracing() {
  perfetto::TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(1024 * 32);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name(data_source_name_);
  ds_config->set_target_buffer(0);

  consumer_endpoint_->EnableTracing(trace_config);
}

void MockConsumer::FreeBuffers() {
  consumer_endpoint_->FreeBuffers();
}

void MockConsumer::OnConnect() {
  StartTracing();
}
void MockConsumer::OnDisconnect() {}
void MockConsumer::OnTracingDisabled() {}

void MockConsumer::OnTraceData(std::vector<perfetto::TracePacket> packets,
                               bool has_more) {
  for (auto& encoded_packet : packets) {
    perfetto::protos::TracePacket packet;
    EXPECT_TRUE(encoded_packet.Decode(&packet));
    if (packet.for_testing().str() == kPerfettoTestString) {
      received_packets_++;
    }
  }

  packet_received_callback_(has_more);
}

void MockConsumer::OnDetach(bool /*success*/) {}
void MockConsumer::OnAttach(bool /*success*/, const perfetto::TraceConfig&) {}
void MockConsumer::OnTraceStats(bool /*success*/, const perfetto::TraceStats&) {
}

MockProducerHost::MockProducerHost(
    const std::string& data_source_name,
    perfetto::TracingService* service,
    MockProducerClient* producer_client,
    base::OnceClosure datasource_registered_callback)
    : datasource_registered_callback_(
          std::move(datasource_registered_callback)) {
  auto on_mojo_connected_callback =
      [](MockProducerClient* producer_client,
         const std::string& data_source_name, MockProducerHost* producer_host,
         perfetto::TracingService* service,
         mojom::ProducerClientPtr producer_client_pipe,
         mojom::ProducerHostRequest producer_host_pipe) {
        producer_host->OnMessagepipesReadyCallback(
            service, std::move(producer_client_pipe),
            std::move(producer_host_pipe));
        producer_client->SetupDataSource(data_source_name);
      };

  producer_client->CreateMojoMessagepipes(base::BindOnce(
      on_mojo_connected_callback, base::Unretained(producer_client),
      data_source_name, base::Unretained(this), base::Unretained(service)));
}

MockProducerHost::~MockProducerHost() = default;

void MockProducerHost::RegisterDataSource(
    const perfetto::DataSourceDescriptor& registration_info) {
  ProducerHost::RegisterDataSource(registration_info);

  on_commit_callback_for_testing_ =
      base::BindRepeating(&MockProducerHost::OnCommit, base::Unretained(this));

  if (datasource_registered_callback_) {
    std::move(datasource_registered_callback_).Run();
  }
}

void MockProducerHost::OnConnect() {}

void MockProducerHost::OnCommit(
    const perfetto::CommitDataRequest& commit_data_request) {
  if (!commit_data_request.chunks_to_patch_size() &&
      !commit_data_request.chunks_to_move_size()) {
    return;
  }

  perfetto::protos::CommitDataRequest proto;
  commit_data_request.ToProto(&proto);
  std::string proto_string;
  CHECK(proto.SerializeToString(&proto_string));
  all_host_commit_data_requests_ += proto_string;
}

void MockProducerHost::OnMessagepipesReadyCallback(
    perfetto::TracingService* perfetto_service,
    mojom::ProducerClientPtr producer_client_pipe,
    mojom::ProducerHostRequest producer_host_pipe) {
  Initialize(std::move(producer_client_pipe), perfetto_service,
             kPerfettoProducerName);
  binding_ = std::make_unique<mojo::Binding<mojom::ProducerHost>>(
      this, std::move(producer_host_pipe));
}

MockProducer::MockProducer(const std::string& data_source_name,
                           perfetto::TracingService* service,
                           base::OnceClosure on_datasource_registered,
                           base::OnceClosure on_tracing_started,
                           size_t num_packets) {
  producer_client_ = std::make_unique<MockProducerClient>(
      num_packets, std::move(on_tracing_started));

  producer_host_ = std::make_unique<MockProducerHost>(
      data_source_name, service, producer_client_.get(),
      std::move(on_datasource_registered));
}

MockProducer::~MockProducer() {
  ProducerClient::DeleteSoonForTesting(std::move(producer_client_));
}

void MockProducer::WritePacketBigly(base::OnceClosure on_write_complete) {
  producer_client_->GetTaskRunner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&TestDataSource::WritePacketBigly,
                     base::Unretained(producer_client_->data_source())),
      std::move(on_write_complete));
}

}  // namespace tracing
