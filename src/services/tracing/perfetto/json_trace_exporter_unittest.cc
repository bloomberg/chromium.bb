// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/json_trace_exporter.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/trace_event_analyzer.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_packet.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"

namespace tracing {

class MockService : public perfetto::TracingService {
 public:
  explicit MockService(base::MessageLoop* message_loop);

  void OnTracingEnabled(const std::string& config);
  void WaitForTracingEnabled();

  void OnTracingDisabled();
  void WaitForTracingDisabled();

  const std::string& tracing_enabled_with_config() const {
    return tracing_enabled_with_config_;
  }

  // perfetto::TracingService implementation.
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

class MockConsumerEndpoint : public perfetto::TracingService::ConsumerEndpoint {
 public:
  explicit MockConsumerEndpoint(MockService* mock_service)
      : mock_service_(mock_service) {
    CHECK(mock_service);
  }

  void EnableTracing(
      const perfetto::TraceConfig& config,
      perfetto::base::ScopedFile = perfetto::base::ScopedFile()) override {
    EXPECT_EQ(mojom::kTraceEventDataSourceName,
              config.data_sources()[0].config().name());
    mock_service_->OnTracingEnabled(
        config.data_sources()[0].config().chrome_config().trace_config());
  }
  void DisableTracing() override { mock_service_->OnTracingDisabled(); }
  void ReadBuffers() override {}
  void FreeBuffers() override {}
  void Flush(uint32_t timeout_ms, FlushCallback callback) override {
    callback(true);
  }

  // Unused in chrome, only meaningful when using TraceConfig.deferred_start.
  void StartTracing() override {}

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

// perfetto::TracingService implementation.
std::unique_ptr<perfetto::TracingService::ProducerEndpoint>
MockService::ConnectProducer(perfetto::Producer*,
                             uid_t uid,
                             const std::string& name,
                             size_t shared_buffer_size_hint_bytes) {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<perfetto::TracingService::ConsumerEndpoint>
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

  void OnTraceEventJSON(const std::string& json,
                        base::DictionaryValue* metadata,
                        bool has_more) {
    CHECK(!has_more);

    parsed_trace_data_ =
        base::DictionaryValue::From(base::JSONReader::Read(json));
    EXPECT_TRUE(parsed_trace_data_);
    if (!parsed_trace_data_) {
      LOG(ERROR) << "Couldn't parse json: \n" << json;
    }

    // The TraceAnalyzer expects the raw trace output, without the
    // wrapping root-node.
    std::string raw_events;
    auto* events_value = parsed_trace_data_->FindKey("traceEvents");
    ASSERT_TRUE(events_value);
    base::JSONWriter::Write(*events_value, &raw_events);

    trace_analyzer_.reset(trace_analyzer::TraceAnalyzer::Create(raw_events));
  }

  void SetTestPacketBasicData(
      perfetto::protos::ChromeTraceEvent* new_trace_event) {
    new_trace_event->set_name("foo_name");
    new_trace_event->set_timestamp(42);
    new_trace_event->set_flags(TRACE_EVENT_FLAG_HAS_GLOBAL_ID |
                               TRACE_EVENT_FLAG_FLOW_OUT);

    new_trace_event->set_process_id(2);
    new_trace_event->set_thread_id(3);
    new_trace_event->set_category_group_name("cat_name");
    new_trace_event->set_phase(TRACE_EVENT_PHASE_COMPLETE);
    new_trace_event->set_duration(4);
    new_trace_event->set_thread_duration(5);
    new_trace_event->set_thread_timestamp(6);
    new_trace_event->set_id(7);
    new_trace_event->set_bind_id(8);

    std::string scope;
    scope += TRACE_EVENT_SCOPE_NAME_GLOBAL;
    new_trace_event->set_scope(scope);
  }

  void FinalizePacket(const perfetto::protos::TracePacket& trace_packet_proto) {
    perfetto::TracePacket trace_packet;
    std::string ser_buf = trace_packet_proto.SerializeAsString();
    trace_packet.AddSlice(&ser_buf[0], ser_buf.size());

    std::vector<perfetto::TracePacket> packets;
    packets.emplace_back(std::move(trace_packet));

    json_trace_exporter_->OnTraceData(std::move(packets), false);
  }

  const trace_analyzer::TraceEvent* ValidateAndGetBasicTestPacket() {
    const trace_analyzer::TraceEvent* trace_event =
        trace_analyzer_->FindFirstOf(
            trace_analyzer::Query(trace_analyzer::Query::EVENT_NAME) ==
            trace_analyzer::Query::String("foo_name"));
    EXPECT_TRUE(trace_event);

    EXPECT_EQ(2, trace_event->thread.process_id);
    EXPECT_EQ(3, trace_event->thread.thread_id);
    EXPECT_EQ(42, trace_event->timestamp);
    EXPECT_EQ('X', trace_event->phase);
    EXPECT_EQ("foo_name", trace_event->name);
    EXPECT_EQ("cat_name", trace_event->category);
    EXPECT_EQ(4, trace_event->duration);
    EXPECT_EQ(5, trace_event->thread_duration);
    EXPECT_EQ(6, trace_event->thread_timestamp);
    EXPECT_EQ("g", trace_event->scope);
    EXPECT_EQ("0x7", trace_event->global_id2);
    EXPECT_EQ("0x8", trace_event->bind_id);
    EXPECT_TRUE(trace_event->flow_out);

    return trace_event;
  }

  trace_analyzer::TraceAnalyzer* trace_analyzer() {
    return trace_analyzer_.get();
  }

  MockService* service() { return service_.get(); }
  const base::DictionaryValue* parsed_trace_data() const {
    return parsed_trace_data_.get();
  }

 private:
  std::unique_ptr<MockService> service_;
  std::unique_ptr<JSONTraceExporter> json_trace_exporter_;
  std::unique_ptr<base::MessageLoop> message_loop_;
  std::unique_ptr<trace_analyzer::TraceAnalyzer> trace_analyzer_;
  std::unique_ptr<base::DictionaryValue> parsed_trace_data_;
};

TEST_F(JSONTraceExporterTest, EnableTracingWithGivenConfig) {
  const char kDummyTraceConfig[] = "trace_all_the_things";
  CreateJSONTraceExporter(kDummyTraceConfig);
  service()->WaitForTracingEnabled();
  EXPECT_EQ(kDummyTraceConfig, service()->tracing_enabled_with_config());
}

TEST_F(JSONTraceExporterTest, TestMetadata) {
  CreateJSONTraceExporter("foo");
  service()->WaitForTracingEnabled();
  StopAndFlush();

  perfetto::protos::TracePacket trace_packet_proto;

  {
    auto* new_metadata =
        trace_packet_proto.mutable_chrome_events()->add_metadata();
    new_metadata->set_name("int_metadata");
    new_metadata->set_int_value(42);
  }

  {
    auto* new_metadata =
        trace_packet_proto.mutable_chrome_events()->add_metadata();
    new_metadata->set_name("string_metadata");
    new_metadata->set_string_value("met_val");
  }

  {
    auto* new_metadata =
        trace_packet_proto.mutable_chrome_events()->add_metadata();
    new_metadata->set_name("bool_metadata");
    new_metadata->set_bool_value(true);
  }

  {
    auto* new_metadata =
        trace_packet_proto.mutable_chrome_events()->add_metadata();
    new_metadata->set_name("dict_metadata");
    new_metadata->set_json_value("{\"child_dict\": \"foo\"}");
  }

  FinalizePacket(trace_packet_proto);

  service()->WaitForTracingDisabled();
  auto* metadata = parsed_trace_data()->FindKey("metadata");
  EXPECT_TRUE(metadata);
  EXPECT_EQ(metadata->FindKey("int_metadata")->GetInt(), 42);
  EXPECT_EQ(metadata->FindKey("string_metadata")->GetString(), "met_val");
  EXPECT_EQ(metadata->FindKey("bool_metadata")->GetBool(), true);
  EXPECT_EQ(
      metadata->FindKey("dict_metadata")->FindKey("child_dict")->GetString(),
      "foo");
}

TEST_F(JSONTraceExporterTest, TestBasicEvent) {
  CreateJSONTraceExporter("foo");
  service()->WaitForTracingEnabled();
  StopAndFlush();

  perfetto::protos::TracePacket trace_packet_proto;
  auto* new_trace_event =
      trace_packet_proto.mutable_chrome_events()->add_trace_events();
  SetTestPacketBasicData(new_trace_event);
  FinalizePacket(trace_packet_proto);

  service()->WaitForTracingDisabled();
  ValidateAndGetBasicTestPacket();
}

TEST_F(JSONTraceExporterTest, TestStringTable) {
  CreateJSONTraceExporter("foo");
  service()->WaitForTracingEnabled();
  StopAndFlush();

  perfetto::protos::TracePacket trace_packet_proto;
  auto* new_trace_event =
      trace_packet_proto.mutable_chrome_events()->add_trace_events();

  {
    auto* string_table_entry =
        trace_packet_proto.mutable_chrome_events()->add_string_table();
    string_table_entry->set_index(1);
    string_table_entry->set_value("foo_name");
  }

  {
    auto* string_table_entry =
        trace_packet_proto.mutable_chrome_events()->add_string_table();
    string_table_entry->set_index(2);
    string_table_entry->set_value("foo_cat");
  }

  {
    auto* string_table_entry =
        trace_packet_proto.mutable_chrome_events()->add_string_table();
    string_table_entry->set_index(3);
    string_table_entry->set_value("foo_arg");
  }

  new_trace_event->set_name_index(1);
  new_trace_event->set_category_group_name_index(2);

  auto* new_arg = new_trace_event->add_args();
  new_arg->set_name_index(3);
  new_arg->set_bool_value(true);

  FinalizePacket(trace_packet_proto);

  service()->WaitForTracingDisabled();

  auto* trace_event = trace_analyzer()->FindFirstOf(
      trace_analyzer::Query(trace_analyzer::Query::EVENT_NAME) ==
      trace_analyzer::Query::String("foo_name"));
  EXPECT_TRUE(trace_event);

  EXPECT_EQ("foo_name", trace_event->name);
  EXPECT_EQ("foo_cat", trace_event->category);

  EXPECT_TRUE(trace_event->GetKnownArgAsBool("foo_arg"));
}

TEST_F(JSONTraceExporterTest, TestEventWithBoolArgs) {
  CreateJSONTraceExporter("foo");
  service()->WaitForTracingEnabled();
  StopAndFlush();

  perfetto::protos::TracePacket trace_packet_proto;
  auto* new_trace_event =
      trace_packet_proto.mutable_chrome_events()->add_trace_events();
  SetTestPacketBasicData(new_trace_event);

  {
    auto* new_arg = new_trace_event->add_args();
    new_arg->set_name("foo1");
    new_arg->set_bool_value(true);
  }

  {
    auto* new_arg = new_trace_event->add_args();
    new_arg->set_name("foo2");
    new_arg->set_bool_value(false);
  }

  FinalizePacket(trace_packet_proto);

  service()->WaitForTracingDisabled();
  auto* trace_event = ValidateAndGetBasicTestPacket();

  EXPECT_TRUE(trace_event->GetKnownArgAsBool("foo1"));
  EXPECT_FALSE(trace_event->GetKnownArgAsBool("foo2"));
}

TEST_F(JSONTraceExporterTest, TestEventWithUintArgs) {
  CreateJSONTraceExporter("foo");
  service()->WaitForTracingEnabled();
  StopAndFlush();

  perfetto::protos::TracePacket trace_packet_proto;
  auto* new_trace_event =
      trace_packet_proto.mutable_chrome_events()->add_trace_events();
  SetTestPacketBasicData(new_trace_event);

  {
    auto* new_arg = new_trace_event->add_args();
    new_arg->set_name("foo1");
    new_arg->set_uint_value(1);
  }

  {
    auto* new_arg = new_trace_event->add_args();
    new_arg->set_name("foo2");
    new_arg->set_uint_value(2);
  }

  FinalizePacket(trace_packet_proto);

  service()->WaitForTracingDisabled();
  auto* trace_event = ValidateAndGetBasicTestPacket();

  EXPECT_EQ(1, trace_event->GetKnownArgAsDouble("foo1"));
  EXPECT_EQ(2, trace_event->GetKnownArgAsDouble("foo2"));
}

TEST_F(JSONTraceExporterTest, TestEventWithIntArgs) {
  CreateJSONTraceExporter("foo");
  service()->WaitForTracingEnabled();
  StopAndFlush();

  perfetto::protos::TracePacket trace_packet_proto;
  auto* new_trace_event =
      trace_packet_proto.mutable_chrome_events()->add_trace_events();
  SetTestPacketBasicData(new_trace_event);

  {
    auto* new_arg = new_trace_event->add_args();
    new_arg->set_name("foo1");
    new_arg->set_int_value(1);
  }

  {
    auto* new_arg = new_trace_event->add_args();
    new_arg->set_name("foo2");
    new_arg->set_int_value(2);
  }

  FinalizePacket(trace_packet_proto);

  service()->WaitForTracingDisabled();
  auto* trace_event = ValidateAndGetBasicTestPacket();

  EXPECT_EQ(1, trace_event->GetKnownArgAsDouble("foo1"));
  EXPECT_EQ(2, trace_event->GetKnownArgAsDouble("foo2"));
}

TEST_F(JSONTraceExporterTest, TestEventWithDoubleArgs) {
  CreateJSONTraceExporter("foo");
  service()->WaitForTracingEnabled();
  StopAndFlush();

  perfetto::protos::TracePacket trace_packet_proto;
  auto* new_trace_event =
      trace_packet_proto.mutable_chrome_events()->add_trace_events();
  SetTestPacketBasicData(new_trace_event);

  {
    auto* new_arg = new_trace_event->add_args();
    new_arg->set_name("foo1");
    new_arg->set_double_value(1.0);
  }

  {
    auto* new_arg = new_trace_event->add_args();
    new_arg->set_name("foo2");
    new_arg->set_double_value(2.0);
  }

  FinalizePacket(trace_packet_proto);

  service()->WaitForTracingDisabled();
  auto* trace_event = ValidateAndGetBasicTestPacket();

  EXPECT_EQ(1.0, trace_event->GetKnownArgAsDouble("foo1"));
  EXPECT_EQ(2.0, trace_event->GetKnownArgAsDouble("foo2"));
}

TEST_F(JSONTraceExporterTest, TestEventWithStringArgs) {
  CreateJSONTraceExporter("foo");
  service()->WaitForTracingEnabled();
  StopAndFlush();

  perfetto::protos::TracePacket trace_packet_proto;
  auto* new_trace_event =
      trace_packet_proto.mutable_chrome_events()->add_trace_events();
  SetTestPacketBasicData(new_trace_event);

  {
    auto* new_arg = new_trace_event->add_args();
    new_arg->set_name("foo1");
    new_arg->set_string_value("bar1");
  }

  {
    auto* new_arg = new_trace_event->add_args();
    new_arg->set_name("foo2");
    new_arg->set_string_value("bar2");
  }

  FinalizePacket(trace_packet_proto);

  service()->WaitForTracingDisabled();
  auto* trace_event = ValidateAndGetBasicTestPacket();

  EXPECT_EQ("bar1", trace_event->GetKnownArgAsString("foo1"));
  EXPECT_EQ("bar2", trace_event->GetKnownArgAsString("foo2"));
}

TEST_F(JSONTraceExporterTest, TestEventWithPointerArgs) {
  CreateJSONTraceExporter("foo");
  service()->WaitForTracingEnabled();
  StopAndFlush();

  perfetto::protos::TracePacket trace_packet_proto;
  auto* new_trace_event =
      trace_packet_proto.mutable_chrome_events()->add_trace_events();
  SetTestPacketBasicData(new_trace_event);

  {
    auto* new_arg = new_trace_event->add_args();
    new_arg->set_name("foo1");
    new_arg->set_pointer_value(0x1);
  }

  {
    auto* new_arg = new_trace_event->add_args();
    new_arg->set_name("foo2");
    new_arg->set_pointer_value(0x2);
  }

  FinalizePacket(trace_packet_proto);

  service()->WaitForTracingDisabled();
  auto* trace_event = ValidateAndGetBasicTestPacket();

  EXPECT_EQ("0x1", trace_event->GetKnownArgAsString("foo1"));
  EXPECT_EQ("0x2", trace_event->GetKnownArgAsString("foo2"));
}

TEST_F(JSONTraceExporterTest, TestEventWithConvertableArgs) {
  CreateJSONTraceExporter("foo");
  service()->WaitForTracingEnabled();
  StopAndFlush();

  perfetto::protos::TracePacket trace_packet_proto;
  auto* new_trace_event =
      trace_packet_proto.mutable_chrome_events()->add_trace_events();
  SetTestPacketBasicData(new_trace_event);

  {
    auto* new_arg = new_trace_event->add_args();
    new_arg->set_name("foo1");
    new_arg->set_json_value("\"conv_value1\"");
  }

  {
    auto* new_arg = new_trace_event->add_args();
    new_arg->set_name("foo2");
    new_arg->set_json_value("\"conv_value2\"");
  }

  FinalizePacket(trace_packet_proto);

  service()->WaitForTracingDisabled();
  auto* trace_event = ValidateAndGetBasicTestPacket();

  EXPECT_EQ("conv_value1", trace_event->GetKnownArgAsString("foo1"));
  EXPECT_EQ("conv_value2", trace_event->GetKnownArgAsString("foo2"));
}

TEST_F(JSONTraceExporterTest, TestEventWithTracedValueArg) {
  CreateJSONTraceExporter("foo");
  service()->WaitForTracingEnabled();
  StopAndFlush();

  perfetto::protos::TracePacket trace_packet_proto;
  auto* new_trace_event =
      trace_packet_proto.mutable_chrome_events()->add_trace_events();
  SetTestPacketBasicData(new_trace_event);

  auto* new_arg = new_trace_event->add_args();
  new_arg->set_name("foo1");
  auto* traced_value = new_arg->mutable_traced_value();
  traced_value->add_dict_keys("bool");
  traced_value->add_dict_values()->set_bool_value(true);

  FinalizePacket(trace_packet_proto);

  service()->WaitForTracingDisabled();
  auto* trace_event = ValidateAndGetBasicTestPacket();

  auto arg_value = trace_event->GetKnownArgAsValue("foo1");
  EXPECT_EQ(true, arg_value->FindKey("bool")->GetBool());
}

TEST_F(JSONTraceExporterTest, TracedValueFlatDictionary) {
  perfetto::protos::ChromeTracedValue traced_value;

  {
    traced_value.add_dict_keys("bool");
    traced_value.add_dict_values()->set_bool_value(true);
  }

  {
    traced_value.add_dict_keys("double");
    traced_value.add_dict_values()->set_double_value(8.0);
  }

  {
    traced_value.add_dict_keys("int");
    traced_value.add_dict_values()->set_int_value(2014);
  }

  {
    traced_value.add_dict_keys("string");
    traced_value.add_dict_values()->set_string_value("bar");
  }

  std::string json;
  AppendProtoDictAsJSON(&json, traced_value);

  EXPECT_EQ("{\"bool\":true,\"double\":8.0,\"int\":2014,\"string\":\"bar\"}",
            json);
}

TEST_F(JSONTraceExporterTest, TracedValueHierarchy) {
  perfetto::protos::ChromeTracedValue traced_value;

  {
    traced_value.add_dict_keys("a1");
    auto* a1_array = traced_value.add_dict_values();
    a1_array->set_nested_type(perfetto::protos::ChromeTracedValue::ARRAY);

    a1_array->add_array_values()->set_int_value(1);
    a1_array->add_array_values()->set_bool_value(true);

    auto* sub_dict = a1_array->add_array_values();
    sub_dict->set_nested_type(perfetto::protos::ChromeTracedValue::DICT);
    sub_dict->add_dict_keys("i2");
    sub_dict->add_dict_values()->set_int_value(3);
  }

  {
    traced_value.add_dict_keys("b0");
    traced_value.add_dict_values()->set_bool_value(true);
  }

  {
    traced_value.add_dict_keys("d0");
    traced_value.add_dict_values()->set_double_value(6.0);
  }

  {
    traced_value.add_dict_keys("a1");
    auto* dict1_subdict = traced_value.add_dict_values();
    dict1_subdict->set_nested_type(perfetto::protos::ChromeTracedValue::DICT);

    dict1_subdict->add_dict_keys("dict2");
    auto* dict2_sub_sub_dict = dict1_subdict->add_dict_values();
    dict2_sub_sub_dict->set_nested_type(
        perfetto::protos::ChromeTracedValue::DICT);

    dict2_sub_sub_dict->add_dict_keys("b2");
    dict2_sub_sub_dict->add_dict_values()->set_bool_value(true);

    dict1_subdict->add_dict_keys("i1");
    dict1_subdict->add_dict_values()->set_int_value(2014);

    dict1_subdict->add_dict_keys("s1");
    dict1_subdict->add_dict_values()->set_string_value("foo");
  }

  {
    traced_value.add_dict_keys("i0");
    traced_value.add_dict_values()->set_int_value(2014);
  }

  {
    traced_value.add_dict_keys("s0");
    traced_value.add_dict_values()->set_string_value("foo");
  }

  std::string json;
  AppendProtoDictAsJSON(&json, traced_value);

  EXPECT_EQ(
      "{\"a1\":[1,true,{\"i2\":3}],\"b0\":true,\"d0\":6.0,\"a1\":{\"dict2\":{"
      "\"b2\":true},\"i1\":2014,\"s1\":\"foo\"},\"i0\":2014,\"s0\":\"foo\"}",
      json);
}

TEST_F(JSONTraceExporterTest, TestLegacyUserTrace) {
  CreateJSONTraceExporter("foo");
  service()->WaitForTracingEnabled();
  StopAndFlush();

  perfetto::protos::TracePacket trace_packet_proto;

  auto* new_trace_event =
      trace_packet_proto.mutable_chrome_events()->add_trace_events();
  SetTestPacketBasicData(new_trace_event);

  auto* json_trace =
      trace_packet_proto.mutable_chrome_events()->add_legacy_json_trace();
  json_trace->set_type(perfetto::protos::ChromeLegacyJsonTrace::USER_TRACE);
  json_trace->set_data(
      "{\"pid\":10,\"tid\":11,\"ts\":23,\"ph\":\"I\""
      ",\"cat\":\"cat_name2\",\"name\":\"bar_name\""
      ",\"id2\":{\"global\":\"0x5\"},\"args\":{}}");

  FinalizePacket(trace_packet_proto);

  service()->WaitForTracingDisabled();
  ValidateAndGetBasicTestPacket();

  const trace_analyzer::TraceEvent* trace_event = trace_analyzer()->FindFirstOf(
      trace_analyzer::Query(trace_analyzer::Query::EVENT_NAME) ==
      trace_analyzer::Query::String("bar_name"));
  EXPECT_TRUE(trace_event);

  EXPECT_EQ(10, trace_event->thread.process_id);
  EXPECT_EQ(11, trace_event->thread.thread_id);
  EXPECT_EQ(23, trace_event->timestamp);
  EXPECT_EQ('I', trace_event->phase);
  EXPECT_EQ("bar_name", trace_event->name);
  EXPECT_EQ("cat_name2", trace_event->category);
  EXPECT_EQ("0x5", trace_event->global_id2);
}

TEST_F(JSONTraceExporterTest, TestLegacySystemFtrace) {
  CreateJSONTraceExporter("foo");
  service()->WaitForTracingEnabled();
  StopAndFlush();

  std::string ftrace = "#dummy data";

  perfetto::protos::TracePacket trace_packet_proto;
  trace_packet_proto.mutable_chrome_events()->add_legacy_ftrace_output(ftrace);
  FinalizePacket(trace_packet_proto);

  service()->WaitForTracingDisabled();
  auto* sys_trace = parsed_trace_data()->FindKey("systemTraceEvents");
  EXPECT_TRUE(sys_trace);
  EXPECT_EQ(sys_trace->GetString(), ftrace);
}

TEST_F(JSONTraceExporterTest, TestLegacySystemTraceEvents) {
  CreateJSONTraceExporter("foo");
  service()->WaitForTracingEnabled();
  StopAndFlush();

  perfetto::protos::TracePacket trace_packet_proto;

  auto* json_trace =
      trace_packet_proto.mutable_chrome_events()->add_legacy_json_trace();
  json_trace->set_type(perfetto::protos::ChromeLegacyJsonTrace::SYSTEM_TRACE);
  json_trace->set_data(
      "\"name\":\"MySysTrace\",\"content\":["
      "{\"pid\":10,\"tid\":11,\"ts\":23,\"ph\":\"I\""
      ",\"cat\":\"cat_name2\",\"name\":\"bar_name\""
      ",\"id2\":{\"global\":\"0x5\"},\"args\":{}}]");

  FinalizePacket(trace_packet_proto);

  service()->WaitForTracingDisabled();

  auto* sys_trace = parsed_trace_data()->FindKey("systemTraceEvents");
  EXPECT_TRUE(sys_trace);
  EXPECT_EQ(sys_trace->FindKey("name")->GetString(), "MySysTrace");
  auto* content = sys_trace->FindKey("content");
  EXPECT_EQ(content->GetList().size(), 1u);
  EXPECT_EQ(content->GetList()[0].FindKey("pid")->GetInt(), 10);
  EXPECT_EQ(content->GetList()[0].FindKey("tid")->GetInt(), 11);
  EXPECT_EQ(content->GetList()[0].FindKey("name")->GetString(), "bar_name");
}

}  // namespace tracing
