// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/trace_event/trace_event.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_stream_null_delegate.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_writer.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace tracing {

namespace {

class MockProducerClient : public ProducerClient {
 public:
  MockProducerClient()
      : delegate_(perfetto::base::kPageSize), stream_(&delegate_) {}

  std::unique_ptr<perfetto::TraceWriter> CreateTraceWriter(
      perfetto::BufferID target_buffer) override;

  perfetto::protos::pbzero::TracePacket* NewTracePacket() {
    trace_packet_.Reset(&stream_);
    return &trace_packet_;
  }

  std::unique_ptr<perfetto::protos::TracePacket> GetPreviousPacket() {
    uint32_t message_size = trace_packet_.Finalize();
    protozero::ContiguousMemoryRange buffer = delegate_.GetNewBuffer();
    EXPECT_GE(buffer.size(), message_size);

    auto proto = std::make_unique<perfetto::protos::TracePacket>();
    proto->ParseFromArray(buffer.begin, buffer.size());

    return proto;
  }

  const google::protobuf::RepeatedPtrField<perfetto::protos::ChromeTraceEvent>
  GetChromeTraceEvents() {
    auto proto = GetPreviousPacket();

    auto event_bundle = proto->chrome_events();
    return event_bundle.trace_events();
  }

 private:
  perfetto::protos::pbzero::TracePacket trace_packet_;
  protozero::ScatteredStreamWriterNullDelegate delegate_;
  protozero::ScatteredStreamWriter stream_;
};

class MockTraceWriter : public perfetto::TraceWriter {
 public:
  explicit MockTraceWriter(MockProducerClient* producer_client)
      : producer_client_(producer_client) {}

  perfetto::TraceWriter::TracePacketHandle NewTracePacket() override {
    return perfetto::TraceWriter::TracePacketHandle(
        producer_client_->NewTracePacket());
  }

  void Flush(std::function<void()> callback = {}) override {}

  perfetto::WriterID writer_id() const override {
    return perfetto::WriterID(0);
  }

 private:
  MockProducerClient* producer_client_;
};

std::unique_ptr<perfetto::TraceWriter> MockProducerClient::CreateTraceWriter(
    perfetto::BufferID target_buffer) {
  return std::make_unique<MockTraceWriter>(this);
}

class TraceEventDataSourceTest : public testing::Test {
 public:
  void SetUp() override {
    message_loop_ = std::make_unique<base::MessageLoop>();
    producer_client_ = std::make_unique<MockProducerClient>();
  }

  void TearDown() override {
    base::RunLoop wait_for_tracelog_flush;

    TraceEventDataSource::GetInstance()->StopTracing(base::BindRepeating(
        [](const base::RepeatingClosure& quit_closure) { quit_closure.Run(); },
        wait_for_tracelog_flush.QuitClosure()));

    wait_for_tracelog_flush.Run();

    producer_client_.reset();
    message_loop_.reset();
  }

  void CreateTraceEventDataSource() {
    auto data_source_config = mojom::DataSourceConfig::New();
    TraceEventDataSource::GetInstance()->StartTracing(producer_client_.get(),
                                                      *data_source_config);
  }

  MockProducerClient* producer_client() { return producer_client_.get(); }

 private:
  std::unique_ptr<MockProducerClient> producer_client_;
  std::unique_ptr<base::MessageLoop> message_loop_;
};

TEST_F(TraceEventDataSourceTest, BasicTraceEvent) {
  CreateTraceEventDataSource();

  TRACE_EVENT_BEGIN0("foo", "bar");

  auto trace_events = producer_client()->GetChromeTraceEvents();
  EXPECT_EQ(trace_events.size(), 1);

  auto trace_event = trace_events[0];
  EXPECT_EQ("bar", trace_event.name());
  EXPECT_EQ("foo", trace_event.category_group_name());
  EXPECT_EQ(TRACE_EVENT_PHASE_BEGIN, trace_event.phase());
}

TEST_F(TraceEventDataSourceTest, TimestampedTraceEvent) {
  CreateTraceEventDataSource();

  TRACE_EVENT_BEGIN_WITH_ID_TID_AND_TIMESTAMP0(
      "foo", "bar", 42, 4242,
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(424242));

  auto trace_events = producer_client()->GetChromeTraceEvents();
  EXPECT_EQ(trace_events.size(), 1);

  auto trace_event = trace_events[0];
  EXPECT_EQ("bar", trace_event.name());
  EXPECT_EQ("foo", trace_event.category_group_name());
  EXPECT_EQ(42u, trace_event.id());
  EXPECT_EQ(4242, trace_event.thread_id());
  EXPECT_EQ(424242, trace_event.timestamp());
  EXPECT_EQ(TRACE_EVENT_PHASE_ASYNC_BEGIN, trace_event.phase());
}

TEST_F(TraceEventDataSourceTest, InstantTraceEvent) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT0("foo", "bar", TRACE_EVENT_SCOPE_THREAD);

  auto trace_events = producer_client()->GetChromeTraceEvents();
  EXPECT_EQ(trace_events.size(), 1);

  auto trace_event = trace_events[0];
  EXPECT_EQ("bar", trace_event.name());
  EXPECT_EQ("foo", trace_event.category_group_name());
  EXPECT_EQ(TRACE_EVENT_SCOPE_THREAD, trace_event.flags());
  EXPECT_EQ(TRACE_EVENT_PHASE_INSTANT, trace_event.phase());
}

}  // namespace

}  // namespace tracing
