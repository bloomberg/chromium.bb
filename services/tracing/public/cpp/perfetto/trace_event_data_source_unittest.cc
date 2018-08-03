// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/debug/leak_annotations.h"
#include "base/json/json_reader.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/trace_event/trace_event.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_stream_null_delegate.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_writer.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace tracing {

namespace {

const char kCategoryGroup[] = "foo";

class MockProducerClient : public ProducerClient {
 public:
  explicit MockProducerClient(
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
      const char* wanted_event_category)
      : delegate_(perfetto::base::kPageSize),
        stream_(&delegate_),
        main_thread_task_runner_(std::move(main_thread_task_runner)),
        wanted_event_category_(wanted_event_category) {
    trace_packet_.Reset(&stream_);
  }

  std::unique_ptr<perfetto::TraceWriter> CreateTraceWriter(
      perfetto::BufferID target_buffer) override;

  void FlushPacketIfPossible() {
    // GetNewBuffer() in ScatteredStreamWriterNullDelegate doesn't
    // actually return a new buffer, but rather lets us access the buffer
    // buffer already used by protozero to write the TracePacket into.
    protozero::ContiguousMemoryRange buffer = delegate_.GetNewBuffer();

    uint32_t message_size = trace_packet_.Finalize();
    if (message_size) {
      EXPECT_GE(buffer.size(), message_size);

      auto proto = std::make_unique<perfetto::protos::TracePacket>();
      EXPECT_TRUE(proto->ParseFromArray(buffer.begin, message_size));
      if (proto->has_chrome_events() &&
          proto->chrome_events().trace_events().size() > 0 &&
          proto->chrome_events().trace_events()[0].category_group_name() ==
              wanted_event_category_) {
        finalized_packets_.push_back(std::move(proto));
      } else if (proto->has_chrome_events() &&
                 proto->chrome_events().metadata().size() > 0) {
        metadata_packets_.push_back(std::move(proto));
      }
    }

    stream_.Reset(buffer);
    trace_packet_.Reset(&stream_);
  }

  perfetto::protos::pbzero::TracePacket* NewTracePacket() {
    FlushPacketIfPossible();

    return &trace_packet_;
  }

  size_t GetFinalizedPacketCount() {
    FlushPacketIfPossible();
    return finalized_packets_.size();
  }

  const google::protobuf::RepeatedPtrField<perfetto::protos::ChromeTraceEvent>
  GetChromeTraceEvents(size_t packet_index = 0) {
    FlushPacketIfPossible();
    EXPECT_GT(finalized_packets_.size(), packet_index);

    auto event_bundle = finalized_packets_[packet_index]->chrome_events();
    return event_bundle.trace_events();
  }

  const google::protobuf::RepeatedPtrField<perfetto::protos::ChromeMetadata>
  GetChromeMetadata(size_t packet_index = 0) {
    FlushPacketIfPossible();
    EXPECT_GT(metadata_packets_.size(), packet_index);

    auto event_bundle = metadata_packets_[packet_index]->chrome_events();
    return event_bundle.metadata();
  }

 private:
  std::vector<std::unique_ptr<perfetto::protos::TracePacket>>
      finalized_packets_;
  std::vector<std::unique_ptr<perfetto::protos::TracePacket>> metadata_packets_;
  perfetto::protos::pbzero::TracePacket trace_packet_;
  protozero::ScatteredStreamWriterNullDelegate delegate_;
  protozero::ScatteredStreamWriter stream_;
  scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;
  const char* wanted_event_category_;
};

// For sequences/threads other than our own, we just want to ignore
// any events coming in.
class DummyTraceWriter : public perfetto::TraceWriter {
 public:
  DummyTraceWriter()
      : delegate_(perfetto::base::kPageSize), stream_(&delegate_) {}

  perfetto::TraceWriter::TracePacketHandle NewTracePacket() override {
    stream_.Reset(delegate_.GetNewBuffer());
    trace_packet_.Reset(&stream_);

    return perfetto::TraceWriter::TracePacketHandle(&trace_packet_);
  }

  void Flush(std::function<void()> callback = {}) override {}

  perfetto::WriterID writer_id() const override {
    return perfetto::WriterID(0);
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
  // We attempt to destroy TraceWriters on thread shutdown in
  // ThreadLocalStorage::Slot, by posting them to the ProducerClient taskrunner,
  // but there's no guarantee that this will succeed if that taskrunner is also
  // shut down.
  ANNOTATE_SCOPED_MEMORY_LEAK;
  if (main_thread_task_runner_->RunsTasksInCurrentSequence()) {
    return std::make_unique<MockTraceWriter>(this);
  } else {
    return std::make_unique<DummyTraceWriter>();
  }
}

class TraceEventDataSourceTest : public testing::Test {
 public:
  void SetUp() override {
    ProducerClient::ResetTaskRunnerForTesting();
  }

  void TearDown() override {
    if (base::trace_event::TraceLog::GetInstance()->IsEnabled()) {
      base::RunLoop wait_for_tracelog_flush;

      TraceEventDataSource::GetInstance()->StopTracing(base::BindRepeating(
          [](const base::RepeatingClosure& quit_closure) {
            quit_closure.Run();
          },
          wait_for_tracelog_flush.QuitClosure()));

      wait_for_tracelog_flush.Run();
    }

    // As MockTraceWriter keeps a pointer to our MockProducerClient,
    // we need to make sure to clean it up from TLS. The other sequences
    // get DummyTraceWriters that we don't care about.
    TraceEventDataSource::GetInstance()->FlushCurrentThread();
    producer_client_.reset();
  }

  void CreateTraceEventDataSource(
      const char* wanted_event_category = kCategoryGroup) {
    producer_client_ = std::make_unique<MockProducerClient>(
        scoped_task_environment_.GetMainThreadTaskRunner(),
        wanted_event_category);

    auto data_source_config = mojom::DataSourceConfig::New();
    TraceEventDataSource::GetInstance()->StartTracing(producer_client(),
                                                      *data_source_config);
  }

  MockProducerClient* producer_client() { return producer_client_.get(); }

 private:
  std::unique_ptr<MockProducerClient> producer_client_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

void HasMetadataValue(const perfetto::protos::ChromeMetadata& entry,
                      const char* value) {
  EXPECT_TRUE(entry.has_string_value());
  EXPECT_EQ(entry.string_value(), value);
}

void HasMetadataValue(const perfetto::protos::ChromeMetadata& entry,
                      int value) {
  EXPECT_TRUE(entry.has_int_value());
  EXPECT_EQ(entry.int_value(), value);
}

void HasMetadataValue(const perfetto::protos::ChromeMetadata& entry,
                      bool value) {
  EXPECT_TRUE(entry.has_bool_value());
  EXPECT_EQ(entry.bool_value(), value);
}

void HasMetadataValue(const perfetto::protos::ChromeMetadata& entry,
                      const base::DictionaryValue& value) {
  EXPECT_TRUE(entry.has_json_value());

  std::unique_ptr<base::Value> child_dict =
      base::JSONReader::Read(entry.json_value());
  EXPECT_EQ(*child_dict, value);
}

template <typename T>
void MetadataHasNamedValue(const google::protobuf::RepeatedPtrField<
                               perfetto::protos::ChromeMetadata>& metadata,
                           const char* name,
                           const T& value) {
  for (int i = 0; i < metadata.size(); i++) {
    auto& entry = metadata[i];
    if (entry.name() == name) {
      HasMetadataValue(entry, value);
      return;
    }
  }

  NOTREACHED();
}

TEST_F(TraceEventDataSourceTest, MetadataSourceBasicTypes) {
  auto metadata_source = std::make_unique<TraceEventMetadataSource>();
  metadata_source->AddGeneratorFunction(base::BindRepeating([]() {
    auto metadata = std::make_unique<base::DictionaryValue>();
    metadata->SetInteger("foo_int", 42);
    metadata->SetString("foo_str", "bar");
    metadata->SetBoolean("foo_bool", true);

    auto child_dict = std::make_unique<base::DictionaryValue>();
    child_dict->SetString("child_str", "child_val");
    metadata->Set("child_dict", std::move(child_dict));
    return metadata;
  }));

  CreateTraceEventDataSource();

  auto data_source_config = mojom::DataSourceConfig::New();
  metadata_source->StartTracing(producer_client(), *data_source_config);

  base::RunLoop wait_for_flush;
  metadata_source->Flush(wait_for_flush.QuitClosure());
  wait_for_flush.Run();

  auto metadata = producer_client()->GetChromeMetadata();
  EXPECT_EQ(4, metadata.size());
  MetadataHasNamedValue(metadata, "foo_int", 42);
  MetadataHasNamedValue(metadata, "foo_str", "bar");
  MetadataHasNamedValue(metadata, "foo_bool", true);

  auto child_dict = std::make_unique<base::DictionaryValue>();
  child_dict->SetString("child_str", "child_val");
  MetadataHasNamedValue(metadata, "child_dict", *child_dict);
}

TEST_F(TraceEventDataSourceTest, TraceLogMetadataEvents) {
  CreateTraceEventDataSource("__metadata");

  base::RunLoop wait_for_flush;
  TraceEventDataSource::GetInstance()->StopTracing(
      wait_for_flush.QuitClosure());
  wait_for_flush.Run();

  bool has_process_uptime_event = false;
  for (size_t i = 0; i < producer_client()->GetFinalizedPacketCount(); ++i) {
    auto trace_events = producer_client()->GetChromeTraceEvents(i);
    for (auto& event : trace_events) {
      if (event.name() == "process_uptime_seconds") {
        has_process_uptime_event = true;
        break;
      }
    }
  }

  EXPECT_TRUE(has_process_uptime_event);
}

TEST_F(TraceEventDataSourceTest, BasicTraceEvent) {
  CreateTraceEventDataSource();

  TRACE_EVENT_BEGIN0(kCategoryGroup, "bar");

  auto trace_events = producer_client()->GetChromeTraceEvents();
  EXPECT_EQ(trace_events.size(), 1);

  auto trace_event = trace_events[0];
  EXPECT_EQ("bar", trace_event.name());
  EXPECT_EQ(kCategoryGroup, trace_event.category_group_name());
  EXPECT_EQ(TRACE_EVENT_PHASE_BEGIN, trace_event.phase());
}

TEST_F(TraceEventDataSourceTest, TimestampedTraceEvent) {
  CreateTraceEventDataSource();

  TRACE_EVENT_BEGIN_WITH_ID_TID_AND_TIMESTAMP0(
      kCategoryGroup, "bar", 42, 4242,
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(424242));

  auto trace_events = producer_client()->GetChromeTraceEvents();
  EXPECT_EQ(trace_events.size(), 1);

  auto trace_event = trace_events[0];
  EXPECT_EQ("bar", trace_event.name());
  EXPECT_EQ(kCategoryGroup, trace_event.category_group_name());
  EXPECT_EQ(42u, trace_event.id());
  EXPECT_EQ(4242, trace_event.thread_id());
  EXPECT_EQ(424242, trace_event.timestamp());
  EXPECT_EQ(TRACE_EVENT_PHASE_ASYNC_BEGIN, trace_event.phase());
}

TEST_F(TraceEventDataSourceTest, InstantTraceEvent) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT0(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD);

  auto trace_events = producer_client()->GetChromeTraceEvents();
  EXPECT_EQ(trace_events.size(), 1);

  auto trace_event = trace_events[0];
  EXPECT_EQ("bar", trace_event.name());
  EXPECT_EQ(kCategoryGroup, trace_event.category_group_name());
  EXPECT_EQ(TRACE_EVENT_SCOPE_THREAD, trace_event.flags());
  EXPECT_EQ(TRACE_EVENT_PHASE_INSTANT, trace_event.phase());
}

TEST_F(TraceEventDataSourceTest, EventWithStringArgs) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD,
                       "arg1_name", "arg1_val", "arg2_name", "arg2_val");

  auto trace_events = producer_client()->GetChromeTraceEvents();
  EXPECT_EQ(trace_events.size(), 1);

  auto trace_args = trace_events[0].args();
  EXPECT_EQ(trace_args.size(), 2);

  EXPECT_EQ("arg1_name", trace_args[0].name());
  EXPECT_EQ("arg1_val", trace_args[0].string_value());
  EXPECT_EQ("arg2_name", trace_args[1].name());
  EXPECT_EQ("arg2_val", trace_args[1].string_value());
}

TEST_F(TraceEventDataSourceTest, EventWithUIntArgs) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       42u, "bar", 4242u);

  auto trace_events = producer_client()->GetChromeTraceEvents();
  EXPECT_EQ(trace_events.size(), 1);

  auto trace_args = trace_events[0].args();
  EXPECT_EQ(trace_args.size(), 2);

  EXPECT_EQ(42u, trace_args[0].uint_value());
  EXPECT_EQ(4242u, trace_args[1].uint_value());
}

TEST_F(TraceEventDataSourceTest, EventWithIntArgs) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       42, "bar", 4242);

  auto trace_events = producer_client()->GetChromeTraceEvents();
  EXPECT_EQ(trace_events.size(), 1);

  auto trace_args = trace_events[0].args();
  EXPECT_EQ(trace_args.size(), 2);

  EXPECT_EQ(42, trace_args[0].int_value());
  EXPECT_EQ(4242, trace_args[1].int_value());
}

TEST_F(TraceEventDataSourceTest, EventWithBoolArgs) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       true, "bar", false);

  auto trace_events = producer_client()->GetChromeTraceEvents();
  EXPECT_EQ(trace_events.size(), 1);

  auto trace_args = trace_events[0].args();
  EXPECT_EQ(trace_args.size(), 2);

  EXPECT_TRUE(trace_args[0].has_bool_value());
  EXPECT_EQ(true, trace_args[0].bool_value());
  EXPECT_TRUE(trace_args[1].has_bool_value());
  EXPECT_EQ(false, trace_args[1].bool_value());
}

TEST_F(TraceEventDataSourceTest, EventWithDoubleArgs) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       42.42, "bar", 4242.42);

  auto trace_events = producer_client()->GetChromeTraceEvents();
  EXPECT_EQ(trace_events.size(), 1);

  auto trace_args = trace_events[0].args();
  EXPECT_EQ(trace_args.size(), 2);

  EXPECT_EQ(42.42, trace_args[0].double_value());
  EXPECT_EQ(4242.42, trace_args[1].double_value());
}

TEST_F(TraceEventDataSourceTest, EventWithPointerArgs) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       reinterpret_cast<void*>(0xBEEF), "bar",
                       reinterpret_cast<void*>(0xF00D));

  auto trace_events = producer_client()->GetChromeTraceEvents();
  EXPECT_EQ(trace_events.size(), 1);

  auto trace_args = trace_events[0].args();
  EXPECT_EQ(trace_args.size(), 2);

  EXPECT_EQ(static_cast<uintptr_t>(0xBEEF), trace_args[0].pointer_value());
  EXPECT_EQ(static_cast<uintptr_t>(0xF00D), trace_args[1].pointer_value());
}

TEST_F(TraceEventDataSourceTest, EventWithConvertableArgs) {
  static const char kArgValue1[] = "\"conv_value1\"";
  static const char kArgValue2[] = "\"conv_value2\"";

  CreateTraceEventDataSource();

  int num_calls = 0;

  class Convertable : public base::trace_event::ConvertableToTraceFormat {
   public:
    explicit Convertable(int* num_calls, const char* arg_value)
        : num_calls_(num_calls), arg_value_(arg_value) {}
    ~Convertable() override = default;

    void AppendAsTraceFormat(std::string* out) const override {
      (*num_calls_)++;
      out->append(arg_value_);
    }

   private:
    int* num_calls_;
    const char* arg_value_;
  };

  std::unique_ptr<Convertable> conv1(new Convertable(&num_calls, kArgValue1));
  std::unique_ptr<Convertable> conv2(new Convertable(&num_calls, kArgValue2));

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD,
                       "foo_arg1", std::move(conv1), "foo_arg2",
                       std::move(conv2));

  EXPECT_EQ(2, num_calls);

  auto trace_events = producer_client()->GetChromeTraceEvents();
  EXPECT_EQ(trace_events.size(), 1);

  auto trace_args = trace_events[0].args();
  EXPECT_EQ(trace_args.size(), 2);

  EXPECT_EQ(kArgValue1, trace_args[0].json_value());
  EXPECT_EQ(kArgValue2, trace_args[1].json_value());
}

TEST_F(TraceEventDataSourceTest, CompleteTraceEventsIntoSeparateBeginAndEnd) {
  static const char kEventName[] = "bar";

  CreateTraceEventDataSource();

  auto* category_group_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(kCategoryGroup);

  trace_event_internal::TraceID trace_event_trace_id =
      trace_event_internal::kNoId;

  auto handle = trace_event_internal::AddTraceEventWithThreadIdAndTimestamp(
      TRACE_EVENT_PHASE_COMPLETE, category_group_enabled, kEventName,
      trace_event_trace_id.scope(), trace_event_trace_id.raw_id(),
      1 /* thread_id */,
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(10),
      trace_event_trace_id.id_flags() | TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP,
      trace_event_internal::kNoId);

  base::trace_event::TraceLog::GetInstance()->UpdateTraceEventDurationExplicit(
      category_group_enabled, kEventName, handle,
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(20),
      base::ThreadTicks() + base::TimeDelta::FromMicroseconds(50));

  // TRACE_EVENT_PHASE_COMPLETE events should internally emit a
  // TRACE_EVENT_PHASE_BEGIN event first, and then a TRACE_EVENT_PHASE_END event
  // when the duration is attempted set on the first event.
  EXPECT_EQ(2u, producer_client()->GetFinalizedPacketCount());

  auto events_from_first_packet = producer_client()->GetChromeTraceEvents(0);
  EXPECT_EQ(events_from_first_packet.size(), 1);

  auto begin_trace_event = events_from_first_packet[0];
  EXPECT_EQ(TRACE_EVENT_PHASE_BEGIN, begin_trace_event.phase());
  EXPECT_EQ(10, begin_trace_event.timestamp());

  auto events_from_second_packet = producer_client()->GetChromeTraceEvents(1);
  EXPECT_EQ(events_from_second_packet.size(), 1);

  auto end_trace_event = events_from_second_packet[0];
  EXPECT_EQ(TRACE_EVENT_PHASE_END, end_trace_event.phase());
  EXPECT_EQ(20, end_trace_event.timestamp());
  EXPECT_EQ(50, end_trace_event.thread_timestamp());
}

}  // namespace

}  // namespace tracing
