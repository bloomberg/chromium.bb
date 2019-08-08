// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"

#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/json/json_reader.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/trace_event/trace_event.h"
#include "components/tracing/common/tracing_switches.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_stream_null_delegate.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_stream_writer.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_writer.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

using TrackEvent = perfetto::protos::TrackEvent;

namespace tracing {

namespace {

constexpr const char kCategoryGroup[] = "foo";

class MockProducerClient : public ProducerClient {
 public:
  explicit MockProducerClient(
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner)
      : delegate_(perfetto::base::kPageSize),
        stream_(&delegate_),
        main_thread_task_runner_(std::move(main_thread_task_runner)) {
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
                 proto->chrome_events().metadata().size() > 0) {
        metadata_packets_.push_back(std::move(proto));
      } else {
        finalized_packets_.push_back(std::move(proto));
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

  const perfetto::protos::TracePacket* GetFinalizedPacket(
      size_t packet_index = 0) {
    FlushPacketIfPossible();
    EXPECT_GT(finalized_packets_.size(), packet_index);
    return finalized_packets_[packet_index].get();
  }

  const google::protobuf::RepeatedPtrField<perfetto::protos::ChromeMetadata>
  GetChromeMetadata(size_t packet_index = 0) {
    FlushPacketIfPossible();
    if (metadata_packets_.empty()) {
      return google::protobuf::RepeatedPtrField<
          perfetto::protos::ChromeMetadata>();
    }
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

  uint64_t written() const override { return 0u; }

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

  uint64_t written() const override { return 0u; }

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
    producer_client_ = std::make_unique<MockProducerClient>(
        scoped_task_environment_.GetMainThreadTaskRunner());
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

  void CreateTraceEventDataSource(bool privacy_filtering_enabled = false) {
    TraceEventDataSource::ResetForTesting();
    perfetto::DataSourceConfig config;
    config.mutable_chrome_config()->set_privacy_filtering_enabled(
        privacy_filtering_enabled);
    TraceEventDataSource::GetInstance()->StartTracing(producer_client(),
                                                      config);
  }

  MockProducerClient* producer_client() { return producer_client_.get(); }

  void ExpectThreadDescriptor(const perfetto::protos::TracePacket* packet,
                              int64_t min_timestamp = 1u,
                              int64_t min_thread_time = 1u) {
    EXPECT_TRUE(packet->has_thread_descriptor());
    EXPECT_NE(packet->thread_descriptor().pid(), 0);
    EXPECT_NE(packet->thread_descriptor().tid(), 0);
    EXPECT_GE(packet->thread_descriptor().reference_timestamp_us(),
              last_timestamp_);
    EXPECT_GE(packet->thread_descriptor().reference_thread_time_us(),
              last_thread_time_);
    EXPECT_LE(packet->thread_descriptor().reference_timestamp_us(),
              TRACE_TIME_TICKS_NOW().since_origin().InMicroseconds());
    if (base::ThreadTicks::IsSupported()) {
      EXPECT_LE(packet->thread_descriptor().reference_thread_time_us(),
                base::ThreadTicks::Now().since_origin().InMicroseconds());
    }

    last_timestamp_ = packet->thread_descriptor().reference_timestamp_us();
    last_thread_time_ = packet->thread_descriptor().reference_thread_time_us();

    EXPECT_EQ(packet->interned_data().event_categories_size(), 0);
    EXPECT_EQ(packet->interned_data().legacy_event_names_size(), 0);

    // ThreadDescriptor is only emitted when incremental state was reset, and
    // thus also always serves as indicator for the state reset to the consumer.
    EXPECT_TRUE(packet->incremental_state_cleared());
  }

  void ExpectTraceEvent(const perfetto::protos::TracePacket* packet,
                        uint32_t category_iid,
                        uint32_t name_iid,
                        char phase,
                        uint32_t flags = 0,
                        uint64_t id = 0,
                        int64_t absolute_timestamp = 0,
                        int32_t tid_override = 0,
                        int32_t pid_override = 0,
                        int64_t duration = 0) {
    EXPECT_TRUE(packet->has_track_event());

    if (absolute_timestamp > 0) {
      EXPECT_TRUE(packet->track_event().has_timestamp_absolute_us());
      EXPECT_EQ(packet->track_event().timestamp_absolute_us(),
                absolute_timestamp);
    } else {
      EXPECT_TRUE(packet->track_event().has_timestamp_delta_us());
      EXPECT_GE(packet->track_event().timestamp_delta_us(), 0);
      EXPECT_LE(last_timestamp_ + packet->track_event().timestamp_delta_us(),
                TRACE_TIME_TICKS_NOW().since_origin().InMicroseconds());
      last_timestamp_ += packet->track_event().timestamp_delta_us();
    }
    if (packet->track_event().has_thread_time_delta_us()) {
      EXPECT_LE(
          last_thread_time_ + packet->track_event().thread_time_delta_us(),
          TRACE_TIME_TICKS_NOW().since_origin().InMicroseconds());
      last_thread_time_ += packet->track_event().thread_time_delta_us();
    }

    EXPECT_EQ(packet->track_event().category_iids_size(), 1);
    EXPECT_EQ(packet->track_event().category_iids(0), category_iid);
    EXPECT_TRUE(packet->track_event().has_legacy_event());

    const auto& legacy_event = packet->track_event().legacy_event();
    EXPECT_EQ(legacy_event.name_iid(), name_iid);
    EXPECT_EQ(legacy_event.phase(), phase);
    EXPECT_EQ(legacy_event.duration_us(), duration);

    if (phase == TRACE_EVENT_PHASE_INSTANT) {
      switch (flags & TRACE_EVENT_FLAG_SCOPE_MASK) {
        case TRACE_EVENT_SCOPE_GLOBAL:
          EXPECT_EQ(legacy_event.instant_event_scope(),
                    TrackEvent::LegacyEvent::SCOPE_GLOBAL);
          break;

        case TRACE_EVENT_SCOPE_PROCESS:
          EXPECT_EQ(legacy_event.instant_event_scope(),
                    TrackEvent::LegacyEvent::SCOPE_PROCESS);
          break;

        case TRACE_EVENT_SCOPE_THREAD:
          EXPECT_EQ(legacy_event.instant_event_scope(),
                    TrackEvent::LegacyEvent::SCOPE_THREAD);
          break;
      }
    } else {
      EXPECT_EQ(legacy_event.instant_event_scope(),
                TrackEvent::LegacyEvent::SCOPE_UNSPECIFIED);
    }

    switch (flags & (TRACE_EVENT_FLAG_HAS_ID | TRACE_EVENT_FLAG_HAS_LOCAL_ID |
                     TRACE_EVENT_FLAG_HAS_GLOBAL_ID)) {
      case TRACE_EVENT_FLAG_HAS_ID:
        EXPECT_EQ(legacy_event.unscoped_id(), id);
        EXPECT_EQ(legacy_event.local_id(), 0u);
        EXPECT_EQ(legacy_event.global_id(), 0u);
        break;
      case TRACE_EVENT_FLAG_HAS_LOCAL_ID:
        EXPECT_EQ(legacy_event.unscoped_id(), 0u);
        EXPECT_EQ(legacy_event.local_id(), id);
        EXPECT_EQ(legacy_event.global_id(), 0u);
        break;
      case TRACE_EVENT_FLAG_HAS_GLOBAL_ID:
        EXPECT_EQ(legacy_event.unscoped_id(), 0u);
        EXPECT_EQ(legacy_event.local_id(), 0u);
        EXPECT_EQ(legacy_event.global_id(), id);
        break;
      default:
        EXPECT_EQ(legacy_event.unscoped_id(), 0u);
        EXPECT_EQ(legacy_event.local_id(), 0u);
        EXPECT_EQ(legacy_event.global_id(), 0u);
        break;
    }

    EXPECT_EQ(legacy_event.use_async_tts(), flags & TRACE_EVENT_FLAG_ASYNC_TTS);

    switch (flags & (TRACE_EVENT_FLAG_FLOW_OUT | TRACE_EVENT_FLAG_FLOW_IN)) {
      case TRACE_EVENT_FLAG_FLOW_OUT | TRACE_EVENT_FLAG_FLOW_IN:
        EXPECT_EQ(legacy_event.flow_direction(),
                  TrackEvent::LegacyEvent::FLOW_INOUT);
        break;
      case TRACE_EVENT_FLAG_FLOW_OUT:
        EXPECT_EQ(legacy_event.flow_direction(),
                  TrackEvent::LegacyEvent::FLOW_OUT);
        break;
      case TRACE_EVENT_FLAG_FLOW_IN:
        EXPECT_EQ(legacy_event.flow_direction(),
                  TrackEvent::LegacyEvent::FLOW_IN);
        break;
      default:
        EXPECT_EQ(legacy_event.flow_direction(),
                  TrackEvent::LegacyEvent::FLOW_UNSPECIFIED);
        break;
    }

    EXPECT_EQ(legacy_event.bind_to_enclosing(),
              flags & TRACE_EVENT_FLAG_BIND_TO_ENCLOSING);

    EXPECT_EQ(legacy_event.tid_override(), tid_override);
    EXPECT_EQ(legacy_event.pid_override(), pid_override);
  }

  void ExpectEventCategories(
      const perfetto::protos::TracePacket* packet,
      std::initializer_list<std::pair<uint32_t, std::string>> entries) {
    ExpectInternedNames(packet->interned_data().event_categories(), entries);
  }

  void ExpectEventNames(
      const perfetto::protos::TracePacket* packet,
      std::initializer_list<std::pair<uint32_t, std::string>> entries) {
    ExpectInternedNames(packet->interned_data().legacy_event_names(), entries);
  }

  void ExpectDebugAnnotationNames(
      const perfetto::protos::TracePacket* packet,
      std::initializer_list<std::pair<uint32_t, std::string>> entries) {
    ExpectInternedNames(packet->interned_data().debug_annotation_names(),
                        entries);
  }

  template <typename T>
  void ExpectInternedNames(
      const google::protobuf::RepeatedPtrField<T>& field,
      std::initializer_list<std::pair<uint32_t, std::string>> entries) {
    EXPECT_EQ(field.size(), static_cast<int>(entries.size()));
    int i = 0;
    for (const auto& entry : entries) {
      EXPECT_EQ(field[i].iid(), entry.first);
      EXPECT_EQ(field[i].name(), entry.second);
      i++;
    }
  }

 protected:
  std::unique_ptr<MockProducerClient> producer_client_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  int64_t last_timestamp_ = 0;
  int64_t last_thread_time_ = 0;
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
      base::JSONReader::ReadDeprecated(entry.json_value());
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

  metadata_source->StartTracing(producer_client(),
                                perfetto::DataSourceConfig());

  base::RunLoop wait_for_stop;
  metadata_source->StopTracing(wait_for_stop.QuitClosure());
  wait_for_stop.Run();

  auto metadata = producer_client()->GetChromeMetadata();
  EXPECT_EQ(4, metadata.size());
  MetadataHasNamedValue(metadata, "foo_int", 42);
  MetadataHasNamedValue(metadata, "foo_str", "bar");
  MetadataHasNamedValue(metadata, "foo_bool", true);

  auto child_dict = std::make_unique<base::DictionaryValue>();
  child_dict->SetString("child_str", "child_val");
  MetadataHasNamedValue(metadata, "child_dict", *child_dict);
}

TEST_F(TraceEventDataSourceTest, BasicTraceEvent) {
  CreateTraceEventDataSource();

  TRACE_EVENT_BEGIN0(kCategoryGroup, "bar");

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);

  auto* td_packet = producer_client()->GetFinalizedPacket();
  ExpectThreadDescriptor(td_packet);

  auto* e_packet = producer_client()->GetFinalizedPacket(1);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN);

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
}

TEST_F(TraceEventDataSourceTest, TraceLogMetadataEvents) {
  CreateTraceEventDataSource();

  base::RunLoop wait_for_flush;
  TraceEventDataSource::GetInstance()->StopTracing(
      wait_for_flush.QuitClosure());
  wait_for_flush.Run();

  bool has_process_uptime_event = false;
  for (size_t i = 0; i < producer_client()->GetFinalizedPacketCount(); ++i) {
    auto* packet = producer_client()->GetFinalizedPacket(i);
    for (auto& event_name : packet->interned_data().legacy_event_names()) {
      if (event_name.name() == "process_uptime_seconds") {
        has_process_uptime_event = true;
        break;
      }
    }
  }

  EXPECT_TRUE(has_process_uptime_event);
}

TEST_F(TraceEventDataSourceTest, TimestampedTraceEvent) {
  CreateTraceEventDataSource();

  TRACE_EVENT_BEGIN_WITH_ID_TID_AND_TIMESTAMP0(
      kCategoryGroup, "bar", 42, 4242,
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(424242));

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);

  auto* td_packet = producer_client()->GetFinalizedPacket();
  ExpectThreadDescriptor(td_packet);

  auto* e_packet = producer_client()->GetFinalizedPacket(1);
  ExpectTraceEvent(
      e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
      TRACE_EVENT_PHASE_ASYNC_BEGIN,
      TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP | TRACE_EVENT_FLAG_HAS_ID, /*id=*/42u,
      /*absolute_timestamp=*/424242, /*tid_override=*/4242);

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
}

TEST_F(TraceEventDataSourceTest, InstantTraceEvent) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT0(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD);

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);

  auto* td_packet = producer_client()->GetFinalizedPacket();
  ExpectThreadDescriptor(td_packet);

  auto* e_packet = producer_client()->GetFinalizedPacket(1);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
}

TEST_F(TraceEventDataSourceTest, EventWithStringArgs) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD,
                       "arg1_name", "arg1_val", "arg2_name", "arg2_val");

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);

  auto* td_packet = producer_client()->GetFinalizedPacket();
  ExpectThreadDescriptor(td_packet);

  auto* e_packet = producer_client()->GetFinalizedPacket(1);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_EQ(annotations[0].name_iid(), 1u);
  EXPECT_EQ(annotations[0].string_value(), "arg1_val");
  EXPECT_EQ(annotations[1].name_iid(), 2u);
  EXPECT_EQ(annotations[1].string_value(), "arg2_val");

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
  ExpectDebugAnnotationNames(e_packet, {{1u, "arg1_name"}, {2u, "arg2_name"}});
}

TEST_F(TraceEventDataSourceTest, EventWithCopiedStrings) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar",
                       TRACE_EVENT_SCOPE_THREAD | TRACE_EVENT_FLAG_COPY,
                       "arg1_name", "arg1_val", "arg2_name", "arg2_val");

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);

  auto* td_packet = producer_client()->GetFinalizedPacket();
  ExpectThreadDescriptor(td_packet);

  auto* e_packet = producer_client()->GetFinalizedPacket(1);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT,
                   TRACE_EVENT_SCOPE_THREAD | TRACE_EVENT_FLAG_COPY);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_EQ(annotations[0].name_iid(), 1u);
  EXPECT_EQ(annotations[0].string_value(), "arg1_val");
  EXPECT_EQ(annotations[1].name_iid(), 2u);
  EXPECT_EQ(annotations[1].string_value(), "arg2_val");

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
  ExpectDebugAnnotationNames(e_packet, {{1u, "arg1_name"}, {2u, "arg2_name"}});
}

TEST_F(TraceEventDataSourceTest, EventWithUIntArgs) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       42u, "bar", 4242u);

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);
  auto* e_packet = producer_client()->GetFinalizedPacket(1);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_EQ(annotations[0].uint_value(), 42u);
  EXPECT_EQ(annotations[1].uint_value(), 4242u);
}

TEST_F(TraceEventDataSourceTest, EventWithIntArgs) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       42, "bar", 4242);

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);
  auto* e_packet = producer_client()->GetFinalizedPacket(1);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_EQ(annotations[0].int_value(), 42);
  EXPECT_EQ(annotations[1].int_value(), 4242);
}

TEST_F(TraceEventDataSourceTest, EventWithBoolArgs) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       true, "bar", false);

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);
  auto* e_packet = producer_client()->GetFinalizedPacket(1);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_TRUE(annotations[0].has_bool_value());
  EXPECT_EQ(annotations[0].bool_value(), true);
  EXPECT_TRUE(annotations[1].has_bool_value());
  EXPECT_EQ(annotations[1].int_value(), false);
}

TEST_F(TraceEventDataSourceTest, EventWithDoubleArgs) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       42.42, "bar", 4242.42);

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);
  auto* e_packet = producer_client()->GetFinalizedPacket(1);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_EQ(annotations[0].double_value(), 42.42);
  EXPECT_EQ(annotations[1].double_value(), 4242.42);
}

TEST_F(TraceEventDataSourceTest, EventWithPointerArgs) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       reinterpret_cast<void*>(0xBEEF), "bar",
                       reinterpret_cast<void*>(0xF00D));

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);
  auto* e_packet = producer_client()->GetFinalizedPacket(1);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_EQ(annotations[0].pointer_value(), static_cast<uintptr_t>(0xBEEF));
  EXPECT_EQ(annotations[1].pointer_value(), static_cast<uintptr_t>(0xF00D));
}

TEST_F(TraceEventDataSourceTest, EventWithConvertableArgs) {
  CreateTraceEventDataSource();

  static const char kArgValue1[] = "\"conv_value1\"";
  static const char kArgValue2[] = "\"conv_value2\"";

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

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);
  auto* e_packet = producer_client()->GetFinalizedPacket(1);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 2);
  EXPECT_EQ(annotations[0].legacy_json_value(), kArgValue1);
  EXPECT_EQ(annotations[1].legacy_json_value(), kArgValue2);
}

TEST_F(TraceEventDataSourceTest, TaskExecutionEvent) {
  CreateTraceEventDataSource();

  INTERNAL_TRACE_EVENT_ADD(
      TRACE_EVENT_PHASE_INSTANT, "toplevel", "ThreadControllerImpl::RunTask",
      TRACE_EVENT_SCOPE_THREAD | TRACE_EVENT_FLAG_TYPED_PROTO_ARGS, "src_file",
      "my_file", "src_func", "my_func");
  INTERNAL_TRACE_EVENT_ADD(
      TRACE_EVENT_PHASE_INSTANT, "toplevel", "ThreadControllerImpl::RunTask",
      TRACE_EVENT_SCOPE_THREAD | TRACE_EVENT_FLAG_TYPED_PROTO_ARGS, "src_file",
      "my_file", "src_func", "my_func");

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 3u);
  auto* e_packet = producer_client()->GetFinalizedPacket(1);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 0);

  EXPECT_EQ(e_packet->track_event().task_execution().posted_from_iid(), 1u);
  const auto& locations = e_packet->interned_data().source_locations();
  EXPECT_EQ(locations.size(), 1);
  EXPECT_EQ(locations[0].file_name(), "my_file");
  EXPECT_EQ(locations[0].function_name(), "my_func");

  // Second event should refer to the same interning entries.
  auto* e_packet2 = producer_client()->GetFinalizedPacket(2);
  ExpectTraceEvent(e_packet2, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  EXPECT_EQ(e_packet2->track_event().task_execution().posted_from_iid(), 1u);
  EXPECT_EQ(e_packet2->interned_data().source_locations().size(), 0);
}

TEST_F(TraceEventDataSourceTest, TaskExecutionEventWithoutFunction) {
  CreateTraceEventDataSource();

  INTERNAL_TRACE_EVENT_ADD(
      TRACE_EVENT_PHASE_INSTANT, "toplevel", "ThreadControllerImpl::RunTask",
      TRACE_EVENT_SCOPE_THREAD | TRACE_EVENT_FLAG_TYPED_PROTO_ARGS, "src",
      "my_file");

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);
  auto* e_packet = producer_client()->GetFinalizedPacket(1);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 0);

  EXPECT_EQ(e_packet->track_event().task_execution().posted_from_iid(), 1u);
  const auto& locations = e_packet->interned_data().source_locations();
  EXPECT_EQ(locations.size(), 1);
  EXPECT_EQ(locations[0].file_name(), "my_file");
  EXPECT_FALSE(locations[0].has_function_name());
}

TEST_F(TraceEventDataSourceTest, UpdateDurationOfCompleteEvent) {
  CreateTraceEventDataSource();

  static const char kEventName[] = "bar";

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
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(30),
      base::ThreadTicks() + base::TimeDelta::FromMicroseconds(50));

  // The call to UpdateTraceEventDurationExplicit should have successfully
  // updated the duration of the event which was added in the
  // AddTraceEventWithThreadIdAndTimestamp call.
  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);
  auto* e_packet = producer_client()->GetFinalizedPacket(1);
  ExpectTraceEvent(
      e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
      TRACE_EVENT_PHASE_COMPLETE,
      TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP | TRACE_EVENT_FLAG_HAS_ID, /*id=*/0u,
      /*absolute_timestamp=*/10, /*tid_override=*/1, /*pid_override=*/0,
      /*duration=*/20);

  // Updating the duration of an invalid event should cause no further events to
  // be emitted.
  handle.event_index = 0;

  base::trace_event::TraceLog::GetInstance()->UpdateTraceEventDurationExplicit(
      category_group_enabled, kEventName, handle,
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(30),
      base::ThreadTicks() + base::TimeDelta::FromMicroseconds(50));

  // No further packets should have been emitted.
  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);
}

// TODO(eseckler): Add a test with multiple events + same strings (cat, name,
// arg names).

// TODO(eseckler): Add a test with multiple events + same strings with reset.

TEST_F(TraceEventDataSourceTest, InternedStrings) {
  CreateTraceEventDataSource();

  for (size_t i = 0; i < 2; i++) {
    TRACE_EVENT_INSTANT1("cat1", "e1", TRACE_EVENT_SCOPE_THREAD, "arg1", 4);
    TRACE_EVENT_INSTANT1("cat1", "e1", TRACE_EVENT_SCOPE_THREAD, "arg1", 2);
    TRACE_EVENT_INSTANT1("cat2", "e2", TRACE_EVENT_SCOPE_THREAD, "arg2", 1);

    EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 4u * (i + 1));

    auto* td_packet = producer_client()->GetFinalizedPacket(4 * i);
    ExpectThreadDescriptor(td_packet);

    // First packet needs to emit new interning entries
    auto* e_packet1 = producer_client()->GetFinalizedPacket(1 + (4 * i));
    ExpectTraceEvent(e_packet1, /*category_iid=*/1u, /*name_iid=*/1u,
                     TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

    const auto& annotations1 = e_packet1->track_event().debug_annotations();
    EXPECT_EQ(annotations1.size(), 1);
    EXPECT_EQ(annotations1[0].name_iid(), 1u);
    EXPECT_EQ(annotations1[0].int_value(), 4);

    ExpectEventCategories(e_packet1, {{1u, "cat1"}});
    ExpectEventNames(e_packet1, {{1u, "e1"}});
    ExpectDebugAnnotationNames(e_packet1, {{1u, "arg1"}});

    // Second packet refers to the interning entries from packet 1.
    auto* e_packet2 = producer_client()->GetFinalizedPacket(2 + (4 * i));
    ExpectTraceEvent(e_packet2, /*category_iid=*/1u, /*name_iid=*/1u,
                     TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

    const auto& annotations2 = e_packet2->track_event().debug_annotations();
    EXPECT_EQ(annotations2.size(), 1);
    EXPECT_EQ(annotations2[0].name_iid(), 1u);
    EXPECT_EQ(annotations2[0].int_value(), 2);

    ExpectEventCategories(e_packet2, {});
    ExpectEventNames(e_packet2, {});
    ExpectDebugAnnotationNames(e_packet2, {});

    // Third packet uses different names, so emits new entries.
    auto* e_packet3 = producer_client()->GetFinalizedPacket(3 + (4 * i));
    ExpectTraceEvent(e_packet3, /*category_iid=*/2u, /*name_iid=*/2u,
                     TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

    const auto& annotations3 = e_packet3->track_event().debug_annotations();
    EXPECT_EQ(annotations3.size(), 1);
    EXPECT_EQ(annotations3[0].name_iid(), 2u);
    EXPECT_EQ(annotations3[0].int_value(), 1);

    ExpectEventCategories(e_packet3, {{2u, "cat2"}});
    ExpectEventNames(e_packet3, {{2u, "e2"}});
    ExpectDebugAnnotationNames(e_packet3, {{2u, "arg2"}});

    // Resetting the interning state causes ThreadDescriptor and interning
    // entries to be emitted again, with the same interning IDs.
    TraceEventDataSource::GetInstance()->ResetIncrementalStateForTesting();
  }
}

TEST_F(TraceEventDataSourceTest, FilteringSimpleTraceEvent) {
  CreateTraceEventDataSource(/* privacy_filtering_enabled =*/true);
  TRACE_EVENT_BEGIN0(kCategoryGroup, "bar");

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);

  auto* td_packet = producer_client()->GetFinalizedPacket();
  ExpectThreadDescriptor(td_packet);

  auto* e_packet = producer_client()->GetFinalizedPacket(1);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_BEGIN);

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
  ExpectDebugAnnotationNames(e_packet, {});
}

TEST_F(TraceEventDataSourceTest, FilteringEventWithArgs) {
  CreateTraceEventDataSource(/* privacy_filtering_enabled =*/true);
  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar", TRACE_EVENT_SCOPE_THREAD, "foo",
                       42, "bar", "string_val");

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);
  auto* e_packet = producer_client()->GetFinalizedPacket(1);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 0);

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectEventNames(e_packet, {{1u, "bar"}});
  ExpectDebugAnnotationNames(e_packet, {});
}

TEST_F(TraceEventDataSourceTest, FilteringEventWithFlagCopy) {
  CreateTraceEventDataSource(/* privacy_filtering_enabled =*/true);
  TRACE_EVENT_INSTANT2(kCategoryGroup, "bar",
                       TRACE_EVENT_SCOPE_THREAD | TRACE_EVENT_FLAG_COPY,
                       "arg1_name", "arg1_val", "arg2_name", "arg2_val");

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 2u);
  auto* e_packet = producer_client()->GetFinalizedPacket(1);
  ExpectTraceEvent(e_packet, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations = e_packet->track_event().debug_annotations();
  EXPECT_EQ(annotations.size(), 0);

  ExpectEventCategories(e_packet, {{1u, kCategoryGroup}});
  ExpectEventNames(e_packet, {{1u, "PRIVACY_FILTERED"}});
  ExpectDebugAnnotationNames(e_packet, {});
}

TEST_F(TraceEventDataSourceTest, FilteringMetadataSource) {
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

  perfetto::DataSourceConfig config;
  config.mutable_chrome_config()->set_privacy_filtering_enabled(true);
  metadata_source->StartTracing(producer_client(), config);

  base::RunLoop wait_for_stop;
  metadata_source->StopTracing(wait_for_stop.QuitClosure());
  wait_for_stop.Run();

  auto metadata = producer_client()->GetChromeMetadata();
  EXPECT_EQ(0, metadata.size());
}

class TraceEventDataSourceNoInterningTest : public TraceEventDataSourceTest {
 public:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kPerfettoDisableInterning);
    TraceEventDataSourceTest::SetUp();
  }
};

TEST_F(TraceEventDataSourceNoInterningTest, InterningScopedToPackets) {
  CreateTraceEventDataSource();

  TRACE_EVENT_INSTANT1("cat1", "e1", TRACE_EVENT_SCOPE_THREAD, "arg1", 4);
  TRACE_EVENT_INSTANT1("cat1", "e1", TRACE_EVENT_SCOPE_THREAD, "arg1", 2);
  TRACE_EVENT_INSTANT1("cat2", "e2", TRACE_EVENT_SCOPE_THREAD, "arg2", 1);

  EXPECT_EQ(producer_client()->GetFinalizedPacketCount(), 4u);

  auto* td_packet = producer_client()->GetFinalizedPacket(0);
  ExpectThreadDescriptor(td_packet);

  // First packet needs to emit new interning entries
  auto* e_packet1 = producer_client()->GetFinalizedPacket(1);
  ExpectTraceEvent(e_packet1, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations1 = e_packet1->track_event().debug_annotations();
  EXPECT_EQ(annotations1.size(), 1);
  EXPECT_EQ(annotations1[0].name_iid(), 1u);
  EXPECT_EQ(annotations1[0].int_value(), 4);

  ExpectEventCategories(e_packet1, {{1u, "cat1"}});
  ExpectEventNames(e_packet1, {{1u, "e1"}});
  ExpectDebugAnnotationNames(e_packet1, {{1u, "arg1"}});

  // Second packet reemits the entries the same way.
  auto* e_packet2 = producer_client()->GetFinalizedPacket(2);
  ExpectTraceEvent(e_packet2, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations2 = e_packet2->track_event().debug_annotations();
  EXPECT_EQ(annotations2.size(), 1);
  EXPECT_EQ(annotations2[0].name_iid(), 1u);
  EXPECT_EQ(annotations2[0].int_value(), 2);

  ExpectEventCategories(e_packet1, {{1u, "cat1"}});
  ExpectEventNames(e_packet1, {{1u, "e1"}});
  ExpectDebugAnnotationNames(e_packet1, {{1u, "arg1"}});

  // Third packet emits entries with the same IDs but different strings.
  auto* e_packet3 = producer_client()->GetFinalizedPacket(3);
  ExpectTraceEvent(e_packet3, /*category_iid=*/1u, /*name_iid=*/1u,
                   TRACE_EVENT_PHASE_INSTANT, TRACE_EVENT_SCOPE_THREAD);

  const auto& annotations3 = e_packet3->track_event().debug_annotations();
  EXPECT_EQ(annotations3.size(), 1);
  EXPECT_EQ(annotations3[0].name_iid(), 1u);
  EXPECT_EQ(annotations3[0].int_value(), 1);

  ExpectEventCategories(e_packet3, {{1u, "cat2"}});
  ExpectEventNames(e_packet3, {{1u, "e2"}});
  ExpectDebugAnnotationNames(e_packet3, {{1u, "arg2"}});
}

// TODO(eseckler): Add startup tracing unittests.

}  // namespace

}  // namespace tracing
