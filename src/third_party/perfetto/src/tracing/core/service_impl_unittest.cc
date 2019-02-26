/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/tracing/core/tracing_service_impl.h"

#include <string.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "perfetto/base/file_utils.h"
#include "perfetto/base/temp_file.h"
#include "perfetto/base/utils.h"
#include "perfetto/tracing/core/consumer.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "perfetto/tracing/core/data_source_descriptor.h"
#include "perfetto/tracing/core/producer.h"
#include "perfetto/tracing/core/shared_memory.h"
#include "perfetto/tracing/core/trace_packet.h"
#include "perfetto/tracing/core/trace_writer.h"
#include "src/base/test/test_task_runner.h"
#include "src/tracing/test/mock_consumer.h"
#include "src/tracing/test/mock_producer.h"
#include "src/tracing/test/test_shared_memory.h"

#include "perfetto/trace/test_event.pbzero.h"
#include "perfetto/trace/trace.pb.h"
#include "perfetto/trace/trace_packet.pb.h"
#include "perfetto/trace/trace_packet.pbzero.h"

using ::testing::_;
using ::testing::Contains;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::Property;
using ::testing::StrictMock;

namespace perfetto {

namespace {
constexpr size_t kDefaultShmSizeKb = TracingServiceImpl::kDefaultShmSize / 1024;
constexpr size_t kMaxShmSizeKb = TracingServiceImpl::kMaxShmSize / 1024;
}  // namespace

class TracingServiceImplTest : public testing::Test {
 public:
  TracingServiceImplTest() {
    auto shm_factory =
        std::unique_ptr<SharedMemory::Factory>(new TestSharedMemory::Factory());
    svc.reset(static_cast<TracingServiceImpl*>(
        TracingService::CreateInstance(std::move(shm_factory), &task_runner)
            .release()));
    svc->min_write_period_ms_ = 1;
  }

  std::unique_ptr<MockProducer> CreateMockProducer() {
    return std::unique_ptr<MockProducer>(
        new StrictMock<MockProducer>(&task_runner));
  }

  std::unique_ptr<MockConsumer> CreateMockConsumer() {
    return std::unique_ptr<MockConsumer>(
        new StrictMock<MockConsumer>(&task_runner));
  }

  ProducerID* last_producer_id() { return &svc->last_producer_id_; }

  uid_t GetProducerUid(ProducerID producer_id) {
    return svc->GetProducer(producer_id)->uid_;
  }

  TracingServiceImpl::TracingSession* tracing_session() {
    auto* session = svc->GetTracingSession(svc->last_tracing_session_id_);
    EXPECT_NE(nullptr, session);
    return session;
  }

  size_t GetNumPendingFlushes() {
    return tracing_session()->pending_flushes.size();
  }

  void WaitForNextSyncMarker() {
    tracing_session()->last_snapshot_time = base::TimeMillis(0);
    static int attempt = 0;
    while (tracing_session()->last_snapshot_time == base::TimeMillis(0)) {
      auto checkpoint_name = "wait_snapshot_" + std::to_string(attempt++);
      auto timer_expired = task_runner.CreateCheckpoint(checkpoint_name);
      task_runner.PostDelayedTask([timer_expired] { timer_expired(); }, 1);
      task_runner.RunUntilCheckpoint(checkpoint_name);
    }
  }

  base::TestTaskRunner task_runner;
  std::unique_ptr<TracingServiceImpl> svc;
};

TEST_F(TracingServiceImplTest, RegisterAndUnregister) {
  std::unique_ptr<MockProducer> mock_producer_1 = CreateMockProducer();
  std::unique_ptr<MockProducer> mock_producer_2 = CreateMockProducer();

  mock_producer_1->Connect(svc.get(), "mock_producer_1", 123u /* uid */);
  mock_producer_2->Connect(svc.get(), "mock_producer_2", 456u /* uid */);

  ASSERT_EQ(2u, svc->num_producers());
  ASSERT_EQ(mock_producer_1->endpoint(), svc->GetProducer(1));
  ASSERT_EQ(mock_producer_2->endpoint(), svc->GetProducer(2));
  ASSERT_EQ(123u, GetProducerUid(1));
  ASSERT_EQ(456u, GetProducerUid(2));

  mock_producer_1->RegisterDataSource("foo");
  mock_producer_2->RegisterDataSource("bar");

  mock_producer_1->UnregisterDataSource("foo");
  mock_producer_2->UnregisterDataSource("bar");

  mock_producer_1.reset();
  ASSERT_EQ(1u, svc->num_producers());
  ASSERT_EQ(nullptr, svc->GetProducer(1));

  mock_producer_2.reset();
  ASSERT_EQ(nullptr, svc->GetProducer(2));

  ASSERT_EQ(0u, svc->num_producers());
}

TEST_F(TracingServiceImplTest, EnableAndDisableTracing) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");
  consumer->EnableTracing(trace_config);

  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  // Calling StartTracing() should be a noop (% a DLOG statement) because the
  // trace config didn't have the |deferred_start| flag set.
  consumer->StartTracing();

  consumer->DisableTracing();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();
}

TEST_F(TracingServiceImplTest, LockdownMode) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer_sameuid", geteuid());
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");
  trace_config.set_lockdown_mode(
      TraceConfig::LockdownModeOperation::LOCKDOWN_SET);
  consumer->EnableTracing(trace_config);

  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  std::unique_ptr<MockProducer> producer_otheruid = CreateMockProducer();
  auto x = svc->ConnectProducer(producer_otheruid.get(), geteuid() + 1,
                                "mock_producer_ouid");
  EXPECT_CALL(*producer_otheruid, OnConnect()).Times(0);
  task_runner.RunUntilIdle();
  Mock::VerifyAndClearExpectations(producer_otheruid.get());

  consumer->DisableTracing();
  consumer->FreeBuffers();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();

  trace_config.set_lockdown_mode(
      TraceConfig::LockdownModeOperation::LOCKDOWN_CLEAR);
  consumer->EnableTracing(trace_config);
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  std::unique_ptr<MockProducer> producer_otheruid2 = CreateMockProducer();
  producer_otheruid->Connect(svc.get(), "mock_producer_ouid2", geteuid() + 1);

  consumer->DisableTracing();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();
}

TEST_F(TracingServiceImplTest, DisconnectConsumerWhileTracing) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");
  consumer->EnableTracing(trace_config);

  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  // Disconnecting the consumer while tracing should trigger data source
  // teardown.
  consumer.reset();
  producer->WaitForDataSourceStop("data_source");
}

TEST_F(TracingServiceImplTest, ReconnectProducerWhileTracing) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");
  consumer->EnableTracing(trace_config);

  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  // Disconnecting and reconnecting a producer with a matching data source.
  // The Producer should see that data source getting enabled again.
  producer.reset();
  producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer_2");
  producer->RegisterDataSource("data_source");
  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");
}

TEST_F(TracingServiceImplTest, ProducerIDWrapping) {
  std::vector<std::unique_ptr<MockProducer>> producers;
  producers.push_back(nullptr);

  auto connect_producer_and_get_id = [&producers,
                                      this](const std::string& name) {
    producers.emplace_back(CreateMockProducer());
    producers.back()->Connect(svc.get(), "mock_producer_" + name);
    return *last_producer_id();
  };

  // Connect producers 1-4.
  for (ProducerID i = 1; i <= 4; i++)
    ASSERT_EQ(i, connect_producer_and_get_id(std::to_string(i)));

  // Disconnect producers 1,3.
  producers[1].reset();
  producers[3].reset();

  *last_producer_id() = kMaxProducerID - 1;
  ASSERT_EQ(kMaxProducerID, connect_producer_and_get_id("maxid"));
  ASSERT_EQ(1u, connect_producer_and_get_id("1_again"));
  ASSERT_EQ(3u, connect_producer_and_get_id("3_again"));
  ASSERT_EQ(5u, connect_producer_and_get_id("5"));
  ASSERT_EQ(6u, connect_producer_and_get_id("6"));
}

// Note: file_write_period_ms is set to a large enough to have exactly one flush
// of the tracing buffers (and therefore at most one synchronization section),
// unless the test runs unrealistically slowly, or the implementation of the
// tracing snapshot packets changes.
TEST_F(TracingServiceImplTest, WriteIntoFileAndStopOnMaxSize) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(4096);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");
  ds_config->set_target_buffer(0);
  trace_config.set_write_into_file(true);
  trace_config.set_file_write_period_ms(100000);  // 100s
  const uint64_t kMaxFileSize = 1024;
  trace_config.set_max_file_size_bytes(kMaxFileSize);
  base::TempFile tmp_file = base::TempFile::Create();
  consumer->EnableTracing(trace_config, base::ScopedFile(dup(tmp_file.fd())));

  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  static const int kNumPreamblePackets = 4;
  static const int kNumTestPackets = 10;
  static const char kPayload[] = "1234567890abcdef-";

  std::unique_ptr<TraceWriter> writer =
      producer->CreateTraceWriter("data_source");
  // Tracing service will emit a preamble of packets (a synchronization section,
  // followed by a tracing config packet). The preamble and these test packets
  // should fit within kMaxFileSize.
  for (int i = 0; i < kNumTestPackets; i++) {
    auto tp = writer->NewTracePacket();
    std::string payload(kPayload);
    payload.append(std::to_string(i));
    tp->set_for_testing()->set_str(payload.c_str(), payload.size());
  }

  // Finally add a packet that overflows kMaxFileSize. This should cause the
  // implicit stop of the trace and should *not* be written in the trace.
  {
    auto tp = writer->NewTracePacket();
    char big_payload[kMaxFileSize] = "BIG!";
    tp->set_for_testing()->set_str(big_payload, sizeof(big_payload));
  }
  writer->Flush();
  writer.reset();

  consumer->DisableTracing();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();

  // Verify the contents of the file.
  std::string trace_raw;
  ASSERT_TRUE(base::ReadFile(tmp_file.path().c_str(), &trace_raw));
  protos::Trace trace;
  ASSERT_TRUE(trace.ParseFromString(trace_raw));

  ASSERT_EQ(trace.packet_size(), kNumPreamblePackets + kNumTestPackets);
  for (int i = 0; i < kNumTestPackets; i++) {
    const protos::TracePacket& tp = trace.packet(kNumPreamblePackets + i);
    ASSERT_EQ(kPayload + std::to_string(i++), tp.for_testing().str());
  }
}

// Test the logic that allows the trace config to set the shm total size and
// page size from the trace config. Also check that, if the config doesn't
// specify a value we fall back on the hint provided by the producer.
TEST_F(TracingServiceImplTest, ProducerShmAndPageSizeOverriddenByTraceConfig) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());
  const size_t kConfigPageSizesKb[] = /****/ {16, 16, 4, 0, 16, 8, 3, 4096, 4};
  const size_t kExpectedPageSizesKb[] = /**/ {16, 16, 4, 4, 16, 8, 4, 64, 4};

  const size_t kConfigSizesKb[] = /**/ {0, 16, 0, 20, 32, 7, 0, 96, 4096000};
  const size_t kHintSizesKb[] = /****/ {0, 0, 16, 32, 16, 0, 7, 96, 4096000};
  const size_t kExpectedSizesKb[] = {
      kDefaultShmSizeKb,  // Both hint and config are 0, use default.
      16,                 // Hint is 0, use config.
      16,                 // Config is 0, use hint.
      20,                 // Hint is takes precedence over the config.
      32,                 // Ditto, even if config is higher than hint.
      kDefaultShmSizeKb,  // Config is invalid and hint is 0, use default.
      kDefaultShmSizeKb,  // Config is 0 and hint is invalid, use default.
      kDefaultShmSizeKb,  // 96 KB isn't a multiple of the page size (64 KB).
      kMaxShmSizeKb       // Too big, cap at kMaxShmSize.
  };

  const size_t kNumProducers = base::ArraySize(kHintSizesKb);
  std::unique_ptr<MockProducer> producer[kNumProducers];
  for (size_t i = 0; i < kNumProducers; i++) {
    auto name = "mock_producer_" + std::to_string(i);
    producer[i] = CreateMockProducer();
    producer[i]->Connect(svc.get(), name, geteuid(), kHintSizesKb[i] * 1024);
    producer[i]->RegisterDataSource("data_source");
  }

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");
  for (size_t i = 0; i < kNumProducers; i++) {
    auto* producer_config = trace_config.add_producers();
    producer_config->set_producer_name("mock_producer_" + std::to_string(i));
    producer_config->set_shm_size_kb(static_cast<uint32_t>(kConfigSizesKb[i]));
    producer_config->set_page_size_kb(
        static_cast<uint32_t>(kConfigPageSizesKb[i]));
  }

  consumer->EnableTracing(trace_config);
  size_t actual_shm_sizes_kb[kNumProducers]{};
  size_t actual_page_sizes_kb[kNumProducers]{};
  for (size_t i = 0; i < kNumProducers; i++) {
    producer[i]->WaitForTracingSetup();
    producer[i]->WaitForDataSourceSetup("data_source");
    actual_shm_sizes_kb[i] =
        producer[i]->endpoint()->shared_memory()->size() / 1024;
    actual_page_sizes_kb[i] =
        producer[i]->endpoint()->shared_buffer_page_size_kb();
  }
  for (size_t i = 0; i < kNumProducers; i++) {
    producer[i]->WaitForDataSourceStart("data_source");
  }
  ASSERT_THAT(actual_page_sizes_kb, ElementsAreArray(kExpectedPageSizesKb));
  ASSERT_THAT(actual_shm_sizes_kb, ElementsAreArray(kExpectedSizesKb));
}

TEST_F(TracingServiceImplTest, ExplicitFlush) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  std::unique_ptr<TraceWriter> writer =
      producer->CreateTraceWriter("data_source");
  {
    auto tp = writer->NewTracePacket();
    tp->set_for_testing()->set_str("payload");
  }

  auto flush_request = consumer->Flush();
  producer->WaitForFlush(writer.get());
  ASSERT_TRUE(flush_request.WaitForReply());

  consumer->DisableTracing();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();
  EXPECT_THAT(
      consumer->ReadBuffers(),
      Contains(Property(&protos::TracePacket::for_testing,
                        Property(&protos::TestEvent::str, Eq("payload")))));
}

TEST_F(TracingServiceImplTest, ImplicitFlushOnTimedTraces) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");
  trace_config.set_duration_ms(1);

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  std::unique_ptr<TraceWriter> writer =
      producer->CreateTraceWriter("data_source");
  {
    auto tp = writer->NewTracePacket();
    tp->set_for_testing()->set_str("payload");
  }

  producer->WaitForFlush(writer.get());

  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();

  EXPECT_THAT(
      consumer->ReadBuffers(),
      Contains(Property(&protos::TracePacket::for_testing,
                        Property(&protos::TestEvent::str, Eq("payload")))));
}

// Tests the monotonic semantic of flush request IDs, i.e., once a producer
// acks flush request N, all flush requests <= N are considered successful and
// acked to the consumer.
TEST_F(TracingServiceImplTest, BatchFlushes) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  std::unique_ptr<TraceWriter> writer =
      producer->CreateTraceWriter("data_source");
  {
    auto tp = writer->NewTracePacket();
    tp->set_for_testing()->set_str("payload");
  }

  auto flush_req_1 = consumer->Flush();
  auto flush_req_2 = consumer->Flush();
  auto flush_req_3 = consumer->Flush();

  // We'll deliberately let the 4th flush request timeout. Use a lower timeout
  // to keep test time short.
  auto flush_req_4 = consumer->Flush(/*timeout_ms=*/10);
  ASSERT_EQ(4u, GetNumPendingFlushes());

  // Make the producer reply only to the 3rd flush request.
  testing::InSequence seq;
  producer->WaitForFlush(nullptr);       // Will NOT reply to flush id == 1.
  producer->WaitForFlush(nullptr);       // Will NOT reply to flush id == 2.
  producer->WaitForFlush(writer.get());  // Will reply only to flush id == 3.
  producer->WaitForFlush(nullptr);       // Will NOT reply to flush id == 4.

  // Even if the producer explicily replied only to flush ID == 3, all the
  // previous flushed < 3 should be implicitly acked.
  ASSERT_TRUE(flush_req_1.WaitForReply());
  ASSERT_TRUE(flush_req_2.WaitForReply());
  ASSERT_TRUE(flush_req_3.WaitForReply());

  // At this point flush id == 4 should still be pending and should fail because
  // of reaching its timeout.
  ASSERT_FALSE(flush_req_4.WaitForReply());

  consumer->DisableTracing();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();
  EXPECT_THAT(
      consumer->ReadBuffers(),
      Contains(Property(&protos::TracePacket::for_testing,
                        Property(&protos::TestEvent::str, Eq("payload")))));
}

TEST_F(TracingServiceImplTest, PeriodicFlush) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.set_flush_period_ms(1);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  std::unique_ptr<TraceWriter> writer =
      producer->CreateTraceWriter("data_source");

  const int kNumFlushes = 3;
  auto checkpoint = task_runner.CreateCheckpoint("all_flushes_done");
  int flushes_seen = 0;
  EXPECT_CALL(*producer, Flush(_, _, _))
      .WillRepeatedly(Invoke([&producer, &writer, &flushes_seen, checkpoint](
                                 FlushRequestID flush_req_id,
                                 const DataSourceInstanceID*, size_t) {
        {
          auto tp = writer->NewTracePacket();
          char payload[32];
          sprintf(payload, "f_%d", flushes_seen);
          tp->set_for_testing()->set_str(payload);
        }
        writer->Flush();
        producer->endpoint()->NotifyFlushComplete(flush_req_id);
        if (++flushes_seen == kNumFlushes)
          checkpoint();
      }));
  task_runner.RunUntilCheckpoint("all_flushes_done");

  consumer->DisableTracing();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();
  auto trace_packets = consumer->ReadBuffers();
  for (int i = 0; i < kNumFlushes; i++) {
    EXPECT_THAT(trace_packets,
                Contains(Property(&protos::TracePacket::for_testing,
                                  Property(&protos::TestEvent::str,
                                           Eq("f_" + std::to_string(i))))));
  }
}

// Creates a tracing session where some of the data sources set the
// |will_notify_on_stop| flag and checks that the OnTracingDisabled notification
// to the consumer is delayed until the acks are received.
TEST_F(TracingServiceImplTest, OnTracingDisabledWaitsForDataSourceStopAcks) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("ds_will_ack_1", /*ack_stop=*/true);
  producer->RegisterDataSource("ds_wont_ack");
  producer->RegisterDataSource("ds_will_ack_2", /*ack_stop=*/true);

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_data_sources()->mutable_config()->set_name("ds_will_ack_1");
  trace_config.add_data_sources()->mutable_config()->set_name("ds_wont_ack");
  trace_config.add_data_sources()->mutable_config()->set_name("ds_will_ack_2");
  trace_config.set_duration_ms(1);

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();

  producer->WaitForDataSourceSetup("ds_will_ack_1");
  producer->WaitForDataSourceSetup("ds_wont_ack");
  producer->WaitForDataSourceSetup("ds_will_ack_2");

  producer->WaitForDataSourceStart("ds_will_ack_1");
  producer->WaitForDataSourceStart("ds_wont_ack");
  producer->WaitForDataSourceStart("ds_will_ack_2");

  std::unique_ptr<TraceWriter> writer =
      producer->CreateTraceWriter("ds_wont_ack");
  producer->WaitForFlush(writer.get());

  DataSourceInstanceID id1 = producer->GetDataSourceInstanceId("ds_will_ack_1");
  DataSourceInstanceID id2 = producer->GetDataSourceInstanceId("ds_will_ack_2");

  producer->WaitForDataSourceStop("ds_will_ack_1");
  producer->WaitForDataSourceStop("ds_wont_ack");
  producer->WaitForDataSourceStop("ds_will_ack_2");

  producer->endpoint()->NotifyDataSourceStopped(id1);
  producer->endpoint()->NotifyDataSourceStopped(id2);

  // Wait for at most half of the service timeout, so that this test fails if
  // the service falls back on calling the OnTracingDisabled() because some of
  // the expected acks weren't received.
  consumer->WaitForTracingDisabled(
      TracingServiceImpl::kDataSourceStopTimeoutMs / 2);
}

// Creates a tracing session where a second data source
// is added while the service is waiting for DisableTracing
// acks; the service should not enable the new datasource
// and should not hit any asserts when the consumer is
// subsequently destroyed.
TEST_F(TracingServiceImplTest, OnDataSourceAddedWhilePendingDisableAcks) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("ds_will_ack", /*ack_stop=*/true);

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_data_sources()->mutable_config()->set_name("ds_will_ack");
  trace_config.add_data_sources()->mutable_config()->set_name("ds_wont_ack");

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();

  consumer->DisableTracing();

  producer->RegisterDataSource("ds_wont_ack");

  consumer.reset();
}

// Similar to OnTracingDisabledWaitsForDataSourceStopAcks, but deliberately
// skips the ack and checks that the service invokes the OnTracingDisabled()
// after the timeout.
TEST_F(TracingServiceImplTest, OnTracingDisabledCalledAnywaysInCaseOfTimeout) {
  svc->override_data_source_test_timeout_ms_for_testing = 1;
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source", /*ack_stop=*/true);

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_data_sources()->mutable_config()->set_name("data_source");
  trace_config.set_duration_ms(1);

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  std::unique_ptr<TraceWriter> writer =
      producer->CreateTraceWriter("data_source");
  producer->WaitForFlush(writer.get());

  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();
}

// Tests the session_id logic. Two data sources in the same tracing session
// should see the same session id.
TEST_F(TracingServiceImplTest, SessionId) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer1 = CreateMockProducer();
  producer1->Connect(svc.get(), "mock_producer1");
  producer1->RegisterDataSource("ds_1A");
  producer1->RegisterDataSource("ds_1B");

  std::unique_ptr<MockProducer> producer2 = CreateMockProducer();
  producer2->Connect(svc.get(), "mock_producer2");
  producer2->RegisterDataSource("ds_2A");

  testing::InSequence seq;
  TracingSessionID last_session_id = 0;
  for (int i = 0; i < 3; i++) {
    TraceConfig trace_config;
    trace_config.add_buffers()->set_size_kb(128);
    trace_config.add_data_sources()->mutable_config()->set_name("ds_1A");
    trace_config.add_data_sources()->mutable_config()->set_name("ds_1B");
    trace_config.add_data_sources()->mutable_config()->set_name("ds_2A");
    trace_config.set_duration_ms(1);

    consumer->EnableTracing(trace_config);

    if (i == 0)
      producer1->WaitForTracingSetup();

    producer1->WaitForDataSourceSetup("ds_1A");
    producer1->WaitForDataSourceSetup("ds_1B");
    if (i == 0)
      producer2->WaitForTracingSetup();
    producer2->WaitForDataSourceSetup("ds_2A");

    producer1->WaitForDataSourceStart("ds_1A");
    producer1->WaitForDataSourceStart("ds_1B");
    producer2->WaitForDataSourceStart("ds_2A");

    auto* ds1 = producer1->GetDataSourceInstance("ds_1A");
    auto* ds2 = producer1->GetDataSourceInstance("ds_1B");
    auto* ds3 = producer2->GetDataSourceInstance("ds_2A");
    ASSERT_EQ(ds1->session_id, ds2->session_id);
    ASSERT_EQ(ds1->session_id, ds3->session_id);
    ASSERT_NE(ds1->session_id, last_session_id);
    last_session_id = ds1->session_id;

    auto writer1 = producer1->CreateTraceWriter("ds_1A");
    producer1->WaitForFlush(writer1.get());

    auto writer2 = producer2->CreateTraceWriter("ds_2A");
    producer2->WaitForFlush(writer2.get());

    producer1->WaitForDataSourceStop("ds_1A");
    producer1->WaitForDataSourceStop("ds_1B");
    producer2->WaitForDataSourceStop("ds_2A");
    consumer->WaitForTracingDisabled();
    consumer->FreeBuffers();
  }
}

// Writes a long trace and then tests that the trace parsed in partitions
// derived by the synchronization markers is identical to the whole trace parsed
// in one go.
TEST_F(TracingServiceImplTest, ResynchronizeTraceStreamUsingSyncMarker) {
  // Setup tracing.
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());
  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");
  producer->RegisterDataSource("data_source");
  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(4096);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("data_source");
  trace_config.set_write_into_file(true);
  trace_config.set_file_write_period_ms(1);
  base::TempFile tmp_file = base::TempFile::Create();
  consumer->EnableTracing(trace_config, base::ScopedFile(dup(tmp_file.fd())));
  producer->WaitForTracingSetup();
  producer->WaitForDataSourceSetup("data_source");
  producer->WaitForDataSourceStart("data_source");

  // Write some variable length payload, waiting for sync markers every now
  // and then.
  const int kNumMarkers = 5;
  auto writer = producer->CreateTraceWriter("data_source");
  for (int i = 1; i <= 100; i++) {
    std::string payload(i, 'A' + (i % 25));
    writer->NewTracePacket()->set_for_testing()->set_str(payload.c_str());
    if (i % (100 / kNumMarkers) == 0) {
      writer->Flush();
      WaitForNextSyncMarker();
    }
  }
  writer->Flush();
  writer.reset();
  consumer->DisableTracing();
  producer->WaitForDataSourceStop("data_source");
  consumer->WaitForTracingDisabled();

  std::string trace_raw;
  ASSERT_TRUE(base::ReadFile(tmp_file.path().c_str(), &trace_raw));

  const auto kMarkerSize = sizeof(TracingServiceImpl::kSyncMarker);
  const std::string kSyncMarkerStr(
      reinterpret_cast<const char*>(TracingServiceImpl::kSyncMarker),
      kMarkerSize);

  // Read back the trace in partitions derived from the marker.
  // The trace should look like this:
  // [uid, marker] [event] [event] [uid, marker] [event] [event]
  size_t num_markers = 0;
  size_t start = 0;
  size_t end = 0;
  protos::Trace merged_trace;
  for (size_t pos = 0; pos != std::string::npos; start = end) {
    pos = trace_raw.find(kSyncMarkerStr, pos + 1);
    num_markers++;
    end = (pos == std::string::npos) ? trace_raw.size() : pos + kMarkerSize;
    int size = static_cast<int>(end - start);
    ASSERT_GT(size, 0);
    protos::Trace trace_partition;
    ASSERT_TRUE(trace_partition.ParseFromArray(trace_raw.data() + start, size));
    merged_trace.MergeFrom(trace_partition);
  }
  EXPECT_GE(num_markers, static_cast<size_t>(kNumMarkers));

  protos::Trace whole_trace;
  ASSERT_TRUE(whole_trace.ParseFromString(trace_raw));

  ASSERT_EQ(whole_trace.packet_size(), merged_trace.packet_size());
  EXPECT_EQ(whole_trace.SerializeAsString(), merged_trace.SerializeAsString());
}

// Creates a tracing session with |deferred_start| and checks that data sources
// are started only after calling StartTracing().
TEST_F(TracingServiceImplTest, DeferredStart) {
  std::unique_ptr<MockConsumer> consumer = CreateMockConsumer();
  consumer->Connect(svc.get());

  std::unique_ptr<MockProducer> producer = CreateMockProducer();
  producer->Connect(svc.get(), "mock_producer");

  // Create two data sources but enable only one of them.
  producer->RegisterDataSource("ds_1");
  producer->RegisterDataSource("ds_2");

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(128);
  trace_config.add_data_sources()->mutable_config()->set_name("ds_1");
  trace_config.set_deferred_start(true);
  trace_config.set_duration_ms(1);

  consumer->EnableTracing(trace_config);
  producer->WaitForTracingSetup();

  producer->WaitForDataSourceSetup("ds_1");

  // Make sure we don't get unexpected DataSourceStart() notifications yet.
  task_runner.RunUntilIdle();

  consumer->StartTracing();

  producer->WaitForDataSourceStart("ds_1");

  auto writer1 = producer->CreateTraceWriter("ds_1");
  producer->WaitForFlush(writer1.get());

  producer->WaitForDataSourceStop("ds_1");
  consumer->WaitForTracingDisabled();
}

}  // namespace perfetto
