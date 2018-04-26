// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/json_trace_exporter.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/trace_event/trace_event.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_packet.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"

namespace tracing {

class MockService : public perfetto::Service {
 public:
  explicit MockService(base::MessageLoop* message_loop);

  void OnTracingEnabled(const std::string& config);
  void WaitForTracingEnabled();

  void OnTracingDisabled();
  void WaitForTracingDisabled();

  const std::string& tracing_enabled_with_config() const {
    return tracing_enabled_with_config_;
  }

  // perfetto::Service implementation.
  std::unique_ptr<ProducerEndpoint> ConnectProducer(
      perfetto::Producer*,
      uid_t uid,
      const std::string& name,
      size_t shared_buffer_size_hint_bytes = 0) override;

  std::unique_ptr<ConsumerEndpoint> ConnectConsumer(
      perfetto::Consumer*) override;

 private:
  base::MessageLoop* message_loop_;

  base::RunLoop wait_for_tracing_enabled_;
  base::RunLoop wait_for_tracing_disabled_;
  std::string tracing_enabled_with_config_;
};

class MockConsumerEndpoint : public perfetto::Service::ConsumerEndpoint {
 public:
  explicit MockConsumerEndpoint(MockService* mock_service)
      : mock_service_(mock_service) {
    CHECK(mock_service);
  }

  void EnableTracing(
      const perfetto::TraceConfig& config,
      perfetto::base::ScopedFile = perfetto::base::ScopedFile()) override {
    EXPECT_EQ(1, config.data_sources_size());
    mock_service_->OnTracingEnabled(
        config.data_sources()[0].config().chrome_config().trace_config());
  }

  void DisableTracing() override { mock_service_->OnTracingDisabled(); }
  void ReadBuffers() override {}
  void FreeBuffers() override {}
  void Flush(uint32_t timeout_ms, FlushCallback) override {}

 private:
  MockService* mock_service_;
};

MockService::MockService(base::MessageLoop* message_loop)
    : message_loop_(message_loop) {
  DCHECK(message_loop);
}

void MockService::OnTracingEnabled(const std::string& config) {
  tracing_enabled_with_config_ = config;
  wait_for_tracing_enabled_.Quit();
}

void MockService::WaitForTracingEnabled() {
  wait_for_tracing_enabled_.Run();
}

void MockService::OnTracingDisabled() {
  wait_for_tracing_disabled_.Quit();
}

void MockService::WaitForTracingDisabled() {
  wait_for_tracing_disabled_.Run();
}

// perfetto::Service implementation.
std::unique_ptr<perfetto::Service::ProducerEndpoint>
MockService::ConnectProducer(perfetto::Producer*,
                             uid_t uid,
                             const std::string& name,
                             size_t shared_buffer_size_hint_bytes) {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<perfetto::Service::ConsumerEndpoint>
MockService::ConnectConsumer(perfetto::Consumer* consumer) {
  message_loop_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&perfetto::Consumer::OnConnect,
                                base::Unretained(consumer)));

  return std::make_unique<MockConsumerEndpoint>(this);
}

class JSONTraceExporterTest : public testing::Test {
 public:
  void SetUp() override {
    message_loop_ = std::make_unique<base::MessageLoop>();
    service_ = std::make_unique<MockService>(message_loop_.get());
  }

  void TearDown() override {
    service_.reset();
    json_trace_exporter_.reset();
    message_loop_.reset();
  }

  void CreateJSONTraceExporter(const std::string& config) {
    json_trace_exporter_.reset(new JSONTraceExporter(config, service_.get()));
  }

  void StopAndFlush() {
    json_trace_exporter_->StopAndFlush(base::BindRepeating(
        &JSONTraceExporterTest::OnTraceEventJSON, base::Unretained(this)));
  }

  void OnTraceEventJSON(const std::string& json, bool has_more) {
    last_received_json_ = json;
  }

  void EmitTestPacket() {
    perfetto::protos::TracePacket trace_packet_proto;

    auto* event_bundle = trace_packet_proto.mutable_chrome_events();
    auto* new_trace_event = event_bundle->add_trace_events();

    new_trace_event->set_name("name");
    new_trace_event->set_timestamp(42);
    new_trace_event->set_flags(TRACE_EVENT_FLAG_HAS_GLOBAL_ID |
                               TRACE_EVENT_FLAG_FLOW_OUT);

    new_trace_event->set_process_id(2);
    new_trace_event->set_thread_id(3);
    new_trace_event->set_category_group_name("cat");
    new_trace_event->set_phase(TRACE_EVENT_PHASE_INSTANT);
    new_trace_event->set_duration(4);
    new_trace_event->set_thread_duration(5);
    new_trace_event->set_thread_timestamp(6);
    new_trace_event->set_id(7);
    new_trace_event->set_bind_id(8);

    std::string scope;
    scope += TRACE_EVENT_SCOPE_NAME_GLOBAL;
    new_trace_event->set_scope(scope);

    perfetto::TracePacket trace_packet;
    std::string ser_buf = trace_packet_proto.SerializeAsString();
    trace_packet.AddSlice(&ser_buf[0], ser_buf.size());

    std::vector<perfetto::TracePacket> packets;
    packets.emplace_back(std::move(trace_packet));

    json_trace_exporter_->OnTraceData(std::move(packets), false);
  }

  void ValidateTestPacket() {
    EXPECT_EQ(
        "{\"traceEvents\":[{\"pid\":2,\"tid\":3,\"ts\":42,\"ph\":\"I\",\"cat\":"
        "\"cat\",\"name\":\"name\",\"dur\":4,\"tdur\":5,\"tts\":6,\"scope\":"
        "\"g\",\"id2\":{\"global\":\"0x7\"},\"bind_id\":\"0x8\",\"flow_out\":"
        "true,\"s\":\"g\",\"args\":\"__stripped__\"}]}",
        last_received_json_);
  }

  std::unique_ptr<MockService> service_;

 private:
  std::unique_ptr<JSONTraceExporter> json_trace_exporter_;
  std::unique_ptr<base::MessageLoop> message_loop_;
  std::string last_received_json_;
};

TEST_F(JSONTraceExporterTest, EnableTracingWithGivenConfig) {
  const char kDummyTraceConfig[] = "trace_all_the_things";
  CreateJSONTraceExporter(kDummyTraceConfig);
  service_->WaitForTracingEnabled();
  EXPECT_EQ(kDummyTraceConfig, service_->tracing_enabled_with_config());
}

TEST_F(JSONTraceExporterTest, TestEvent) {
  CreateJSONTraceExporter("foo");
  service_->WaitForTracingEnabled();

  StopAndFlush();
  EmitTestPacket();

  service_->WaitForTracingDisabled();
  ValidateTestPacket();
}

}  // namespace tracing
