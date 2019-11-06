/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <fcntl.h>

#include <chrono>
#include <condition_variable>
#include <functional>
#include <list>
#include <mutex>
#include <vector>

#include "perfetto/tracing.h"
#include "protos/perfetto/trace/test_event.pbzero.h"
#include "protos/perfetto/trace/trace.pb.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"
#include "test/gtest_and_gmock.h"

// Deliberately not pulling any non-public perfetto header to spot accidental
// header public -> non-public dependency while building this file.

// This is the only header allowed here, see comments in api_test_support.h.
#include "src/tracing/test/api_test_support.h"

#include "perfetto/tracing/core/data_source_descriptor.h"
#include "perfetto/tracing/core/trace_config.h"

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::StrEq;

// ------------------------------
// Declarations of helper classes
// ------------------------------
static constexpr auto kWaitEventTimeout = std::chrono::seconds(5);

struct WaitableTestEvent {
  std::mutex mutex_;
  std::condition_variable cv_;
  bool notified_ = false;

  bool notified() {
    std::unique_lock<std::mutex> lock(mutex_);
    return notified_;
  }

  void Wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!cv_.wait_for(lock, kWaitEventTimeout, [this] { return notified_; })) {
      fprintf(stderr, "Timed out while waiting for event\n");
      abort();
    }
  }

  void Notify() {
    std::unique_lock<std::mutex> lock(mutex_);
    notified_ = true;
    cv_.notify_one();
  }
};

class MockDataSource;

// We can't easily use gmock here because instances of data sources are lazily
// created by the service and are not owned by the test fixture.
struct TestDataSourceHandle {
  WaitableTestEvent on_create;
  WaitableTestEvent on_setup;
  WaitableTestEvent on_start;
  WaitableTestEvent on_stop;
  MockDataSource* instance;
  perfetto::DataSourceConfig config;
  bool handle_stop_asynchronously = false;
  std::function<void()> on_start_callback;
  std::function<void()> on_stop_callback;
  std::function<void()> async_stop_closure;
};

class MockDataSource : public perfetto::DataSource<MockDataSource> {
 public:
  void OnSetup(const SetupArgs&) override;
  void OnStart(const StartArgs&) override;
  void OnStop(const StopArgs&) override;
  TestDataSourceHandle* handle_ = nullptr;
};

class MockDataSource2 : public perfetto::DataSource<MockDataSource2> {
 public:
  void OnSetup(const SetupArgs&) override {}
  void OnStart(const StartArgs&) override {}
  void OnStop(const StopArgs&) override {}
};

struct TestIncrementalState {
  TestIncrementalState() { constructed = true; }
  // Note: a virtual destructor is not required for incremental state.
  ~TestIncrementalState() { destroyed = true; }

  int count = 100;
  static bool constructed;
  static bool destroyed;
};

bool TestIncrementalState::constructed;
bool TestIncrementalState::destroyed;

class TestIncrementalDataSource
    : public perfetto::DataSource<TestIncrementalDataSource,
                                  TestIncrementalState> {
 public:
  void OnSetup(const SetupArgs&) override {}
  void OnStart(const StartArgs&) override {}
  void OnStop(const StopArgs&) override {}
};

// A convenience wrapper around TracingSession that allows to do block on
//
struct TestTracingSessionHandle {
  perfetto::TracingSession* get() { return session.get(); }
  std::unique_ptr<perfetto::TracingSession> session;
  WaitableTestEvent on_stop;
};

// -------------------------
// Declaration of test class
// -------------------------
class PerfettoApiTest : public ::testing::Test {
 public:
  static PerfettoApiTest* instance;

  void SetUp() override {
    instance = this;
    // Perfetto can only be initialized once in a process.
    static bool was_initialized;
    if (!was_initialized) {
      perfetto::TracingInitArgs args;
      args.backends = perfetto::kInProcessBackend;
      perfetto::Tracing::Initialize(args);
      was_initialized = true;
      RegisterDataSource<MockDataSource>("my_data_source");
    }
    // Make sure our data source always has a valid handle.
    data_sources_["my_data_source"];
  }

  void TearDown() override { instance = nullptr; }

  template <typename DataSourceType>
  TestDataSourceHandle* RegisterDataSource(std::string name) {
    EXPECT_EQ(data_sources_.count(name), 0u);
    TestDataSourceHandle* handle = &data_sources_[name];
    perfetto::DataSourceDescriptor dsd;
    dsd.set_name(name);
    DataSourceType::Register(dsd);
    return handle;
  }

  TestTracingSessionHandle* NewTrace(const perfetto::TraceConfig& cfg,
                                     int fd = -1) {
    sessions_.emplace_back();
    TestTracingSessionHandle* handle = &sessions_.back();
    handle->session =
        perfetto::Tracing::NewTrace(perfetto::BackendType::kInProcessBackend);
    handle->session->SetOnStopCallback([handle] { handle->on_stop.Notify(); });
    handle->session->Setup(cfg, fd);
    return handle;
  }

  std::map<std::string, TestDataSourceHandle> data_sources_;
  std::list<TestTracingSessionHandle> sessions_;  // Needs stable pointers.
};

// ---------------------------------------------
// Definitions for non-inlineable helper methods
// ---------------------------------------------
PerfettoApiTest* PerfettoApiTest::instance;

void MockDataSource::OnSetup(const SetupArgs& args) {
  EXPECT_EQ(handle_, nullptr);
  auto it = PerfettoApiTest::instance->data_sources_.find(args.config->name());

  // We should not see an OnSetup for a data source that we didn't register
  // before via PerfettoApiTest::RegisterDataSource().
  EXPECT_NE(it, PerfettoApiTest::instance->data_sources_.end());
  handle_ = &it->second;
  handle_->config = *args.config;  // Deliberate copy.
  handle_->on_setup.Notify();
}

void MockDataSource::OnStart(const StartArgs&) {
  EXPECT_NE(handle_, nullptr);
  if (handle_->on_start_callback)
    handle_->on_start_callback();
  handle_->on_start.Notify();
}

void MockDataSource::OnStop(const StopArgs& args) {
  EXPECT_NE(handle_, nullptr);
  if (handle_->handle_stop_asynchronously)
    handle_->async_stop_closure = args.HandleStopAsynchronously();
  if (handle_->on_stop_callback)
    handle_->on_stop_callback();
  handle_->on_stop.Notify();
}

// -------------
// Test fixtures
// -------------

TEST_F(PerfettoApiTest, TrackEvent) {
  perfetto::TrackEvent::Initialize(/* TODO(skyostil): Register categories */);

  // Setup the trace config.
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("track_event");
  ds_cfg->set_legacy_config("test");

  // Create a new trace session.
  auto* tracing_session = NewTrace(cfg);
  tracing_session->get()->StartBlocking();

  // Emit one complete track event.
  perfetto::TrackEvent::Begin("test", "TestEvent");
  perfetto::TrackEvent::End("test");
  perfetto::TrackEvent::Flush();

  tracing_session->on_stop.Wait();
  std::vector<char> raw_trace = tracing_session->get()->ReadTraceBlocking();
  ASSERT_GE(raw_trace.size(), 0u);

  // Read back the trace, maintaining interning tables as we go.
  perfetto::protos::Trace trace;
  std::map<uint64_t, std::string> categories;
  std::map<uint64_t, std::string> event_names;
  ASSERT_TRUE(trace.ParseFromArray(raw_trace.data(), int(raw_trace.size())));

  bool incremental_state_was_cleared = false;
  bool begin_found = false;
  bool end_found = false;
  bool process_descriptor_found = false;
  bool thread_descriptor_found = false;
  auto now = perfetto::TrackEvent::GetTimeNs();
  uint32_t sequence_id = 0;
  int32_t cur_pid = perfetto::test::GetCurrentProcessId();
  for (const auto& packet : trace.packet()) {
    if (packet.has_process_descriptor()) {
      EXPECT_FALSE(process_descriptor_found);
      const auto& pd = packet.process_descriptor();
      EXPECT_EQ(cur_pid, pd.pid());
      process_descriptor_found = true;
    }
    if (packet.has_thread_descriptor()) {
      EXPECT_FALSE(thread_descriptor_found);
      const auto& td = packet.thread_descriptor();
      EXPECT_EQ(cur_pid, td.pid());
      EXPECT_NE(0, td.tid());
      thread_descriptor_found = true;
    }
    if (packet.incremental_state_cleared()) {
      incremental_state_was_cleared = true;
      categories.clear();
      event_names.clear();
    }

    if (!packet.has_track_event())
      continue;
    const auto& track_event = packet.track_event();

    // Make sure we only see track events on one sequence.
    if (packet.trusted_packet_sequence_id()) {
      if (!sequence_id)
        sequence_id = packet.trusted_packet_sequence_id();
      EXPECT_EQ(sequence_id, packet.trusted_packet_sequence_id());
    }

    // Update incremental state.
    if (packet.has_interned_data()) {
      const auto& interned_data = packet.interned_data();
      for (const auto& it : interned_data.event_categories())
        categories[it.iid()] = it.name();
      for (const auto& it : interned_data.event_names())
        event_names[it.iid()] = it.name();
    }

    EXPECT_GT(packet.timestamp(), 0u);
    EXPECT_LE(packet.timestamp(), now);
    EXPECT_EQ(track_event.category_iids().size(), 1);
    EXPECT_GE(track_event.category_iids().Get(0), 1u);

    if (track_event.type() == perfetto::protos::TrackEvent::TYPE_SLICE_BEGIN) {
      EXPECT_FALSE(begin_found);
      EXPECT_TRUE(track_event.has_legacy_event());
      EXPECT_EQ("test", categories[track_event.category_iids().Get(0)]);
      EXPECT_EQ("TestEvent",
                event_names[track_event.legacy_event().name_iid()]);
      begin_found = true;
    } else if (track_event.type() ==
               perfetto::protos::TrackEvent::TYPE_SLICE_END) {
      EXPECT_FALSE(end_found);
      EXPECT_FALSE(track_event.has_legacy_event());
      EXPECT_EQ("test", categories[track_event.category_iids().Get(0)]);
      end_found = true;
    }
  }
  EXPECT_TRUE(incremental_state_was_cleared);
  EXPECT_TRUE(process_descriptor_found);
  EXPECT_TRUE(thread_descriptor_found);
  EXPECT_TRUE(begin_found);
  EXPECT_TRUE(end_found);
}

TEST_F(PerfettoApiTest, OneDataSourceOneEvent) {
  auto* data_source = &data_sources_["my_data_source"];

  // Setup the trace config.
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("my_data_source");
  ds_cfg->set_legacy_config("test config");

  // Create a new trace session.
  auto* tracing_session = NewTrace(cfg);

  MockDataSource::Trace([](MockDataSource::TraceContext) {
    FAIL() << "Should not be called because the trace was not started";
  });

  tracing_session->get()->Start();
  data_source->on_setup.Wait();
  EXPECT_EQ(data_source->config.legacy_config(), "test config");
  data_source->on_start.Wait();

  // Emit one trace event.
  std::atomic<int> trace_lambda_calls{0};
  MockDataSource::Trace(
      [&trace_lambda_calls](MockDataSource::TraceContext ctx) {
        auto packet = ctx.NewTracePacket();
        packet->set_timestamp(42);
        packet->set_for_testing()->set_str("event 1");
        trace_lambda_calls++;
        packet->Finalize();

        // The SMB scraping logic will skip the last packet because it cannot
        // guarantee it's finalized. Create an empty packet so we get the
        // previous one and this empty one is ignored.
        packet = ctx.NewTracePacket();
      });

  data_source->on_stop.Wait();
  tracing_session->on_stop.Wait();
  EXPECT_EQ(trace_lambda_calls, 1);

  MockDataSource::Trace([](MockDataSource::TraceContext) {
    FAIL() << "Should not be called because the trace is now stopped";
  });

  std::vector<char> raw_trace = tracing_session->get()->ReadTraceBlocking();
  ASSERT_GE(raw_trace.size(), 0u);

  perfetto::protos::Trace trace;
  ASSERT_TRUE(trace.ParseFromArray(raw_trace.data(), int(raw_trace.size())));
  bool test_packet_found = false;
  for (const auto& packet : trace.packet()) {
    if (!packet.has_for_testing())
      continue;
    EXPECT_FALSE(test_packet_found);
    EXPECT_EQ(packet.timestamp(), 42U);
    EXPECT_EQ(packet.for_testing().str(), "event 1");
    test_packet_found = true;
  }
  EXPECT_TRUE(test_packet_found);
}

TEST_F(PerfettoApiTest, BlockingStartAndStop) {
  auto* data_source = &data_sources_["my_data_source"];

  // Register a second data source to get a bit more coverage.
  perfetto::DataSourceDescriptor dsd;
  dsd.set_name("my_data_source2");
  MockDataSource2::Register(dsd);

  // Setup the trace config.
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("my_data_source");
  ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("my_data_source2");

  // Create a new trace session.
  auto* tracing_session = NewTrace(cfg);

  tracing_session->get()->StartBlocking();
  EXPECT_TRUE(data_source->on_setup.notified());
  EXPECT_TRUE(data_source->on_start.notified());

  tracing_session->get()->StopBlocking();
  EXPECT_TRUE(data_source->on_stop.notified());
  EXPECT_TRUE(tracing_session->on_stop.notified());
}

TEST_F(PerfettoApiTest, BlockingStartAndStopOnEmptySession) {
  // Setup the trace config.
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("non_existent_data_source");

  // Create a new trace session.
  auto* tracing_session = NewTrace(cfg);
  tracing_session->get()->StartBlocking();
  tracing_session->get()->StopBlocking();
  EXPECT_TRUE(tracing_session->on_stop.notified());
}

TEST_F(PerfettoApiTest, WriteEventsAfterDeferredStop) {
  auto* data_source = &data_sources_["my_data_source"];
  data_source->handle_stop_asynchronously = true;

  // Setup the trace config and start the tracing session.
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("my_data_source");
  auto* tracing_session = NewTrace(cfg);
  tracing_session->get()->StartBlocking();

  // Stop and wait for the producer to have seen the stop event.
  WaitableTestEvent consumer_stop_signal;
  tracing_session->get()->SetOnStopCallback(
      [&consumer_stop_signal] { consumer_stop_signal.Notify(); });
  tracing_session->get()->Stop();
  data_source->on_stop.Wait();

  // At this point tracing should be still allowed because of the
  // HandleStopAsynchronously() call.
  bool lambda_called = false;

  // This usleep is here just to prevent that we accidentally pass the test
  // just by virtue of hitting some race. We should be able to trace up until
  // 5 seconds after seeing the stop when using the deferred stop mechanism.
  usleep(250 * 1000);

  MockDataSource::Trace([&lambda_called](MockDataSource::TraceContext ctx) {
    auto packet = ctx.NewTracePacket();
    packet->set_for_testing()->set_str("event written after OnStop");
    packet->Finalize();
    ctx.Flush();
    lambda_called = true;
  });
  ASSERT_TRUE(lambda_called);

  // Now call the async stop closure. This acks the stop to the service and
  // disallows further Trace() calls.
  EXPECT_TRUE(data_source->async_stop_closure);
  data_source->async_stop_closure();

  // Wait that the stop is propagated to the consumer.
  consumer_stop_signal.Wait();

  MockDataSource::Trace([](MockDataSource::TraceContext) {
    FAIL() << "Should not be called after the stop is acked";
  });

  // Check the contents of the trace.
  std::vector<char> raw_trace = tracing_session->get()->ReadTraceBlocking();
  ASSERT_GE(raw_trace.size(), 0u);
  perfetto::protos::Trace trace;
  ASSERT_TRUE(trace.ParseFromArray(raw_trace.data(), int(raw_trace.size())));
  int test_packet_found = 0;
  for (const auto& packet : trace.packet()) {
    if (!packet.has_for_testing())
      continue;
    EXPECT_EQ(packet.for_testing().str(), "event written after OnStop");
    test_packet_found++;
  }
  EXPECT_EQ(test_packet_found, 1);
}

TEST_F(PerfettoApiTest, RepeatedStartAndStop) {
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("my_data_source");

  for (int i = 0; i < 5; i++) {
    auto* tracing_session = NewTrace(cfg);
    tracing_session->get()->Start();
    std::atomic<bool> stop_called{false};
    tracing_session->get()->SetOnStopCallback(
        [&stop_called] { stop_called = true; });
    tracing_session->get()->StopBlocking();
    EXPECT_TRUE(stop_called);
  }
}

TEST_F(PerfettoApiTest, SetupWithFile) {
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
  char temp_file[] = "/data/local/tmp/perfetto-XXXXXXXX";
#else
  char temp_file[] = "/tmp/perfetto-XXXXXXXX";
#endif
  int fd = mkstemp(temp_file);
  ASSERT_TRUE(fd >= 0);

  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("my_data_source");
  // Write a trace into |fd|.
  auto* tracing_session = NewTrace(cfg, fd);
  tracing_session->get()->StartBlocking();
  tracing_session->get()->StopBlocking();
  // Check that |fd| didn't get closed.
  EXPECT_EQ(0, fcntl(fd, F_GETFD, 0));
  // Check that the trace got written.
  EXPECT_GT(lseek(fd, 0, SEEK_END), 0);
  EXPECT_EQ(0, close(fd));
  // Clean up.
  EXPECT_EQ(0, unlink(temp_file));
}

TEST_F(PerfettoApiTest, MultipleRegistrations) {
  // Attempt to register the same data source again.
  perfetto::DataSourceDescriptor dsd;
  dsd.set_name("my_data_source");
  EXPECT_TRUE(MockDataSource::Register(dsd));

  // Setup the trace config.
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("my_data_source");

  // Create a new trace session.
  auto* tracing_session = NewTrace(cfg);
  tracing_session->get()->StartBlocking();

  // Emit one trace event.
  std::atomic<int> trace_lambda_calls{0};
  MockDataSource::Trace([&trace_lambda_calls](MockDataSource::TraceContext) {
    trace_lambda_calls++;
  });

  // Make sure the data source got called only once.
  tracing_session->get()->StopBlocking();
  EXPECT_EQ(trace_lambda_calls, 1);
}

TEST_F(PerfettoApiTest, CustomIncrementalState) {
  perfetto::DataSourceDescriptor dsd;
  dsd.set_name("incr_data_source");
  TestIncrementalDataSource::Register(dsd);

  // Setup the trace config.
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("incr_data_source");

  // Create a new trace session.
  auto* tracing_session = NewTrace(cfg);
  tracing_session->get()->StartBlocking();

  // First emit a no-op trace event that initializes the incremental state as a
  // side effect.
  TestIncrementalDataSource::Trace(
      [](TestIncrementalDataSource::TraceContext) {});
  EXPECT_TRUE(TestIncrementalState::constructed);

  // Check that the incremental state is carried across trace events.
  TestIncrementalDataSource::Trace(
      [](TestIncrementalDataSource::TraceContext ctx) {
        auto* state = ctx.GetIncrementalState();
        EXPECT_TRUE(state);
        EXPECT_EQ(100, state->count);
        state->count++;
      });

  TestIncrementalDataSource::Trace(
      [](TestIncrementalDataSource::TraceContext ctx) {
        auto* state = ctx.GetIncrementalState();
        EXPECT_EQ(101, state->count);
      });

  // Make sure the incremental state gets cleaned up between sessions.
  tracing_session->get()->StopBlocking();
  tracing_session = NewTrace(cfg);
  tracing_session->get()->StartBlocking();
  TestIncrementalDataSource::Trace(
      [](TestIncrementalDataSource::TraceContext ctx) {
        auto* state = ctx.GetIncrementalState();
        EXPECT_TRUE(TestIncrementalState::destroyed);
        EXPECT_TRUE(state);
        EXPECT_EQ(100, state->count);
      });
  tracing_session->get()->StopBlocking();
}

// Regression test for b/139110180. Checks that GetDataSourceLocked() can be
// called from OnStart() and OnStop() callbacks without deadlocking.
TEST_F(PerfettoApiTest, GetDataSourceLockedFromCallbacks) {
  auto* data_source = &data_sources_["my_data_source"];

  // Setup the trace config.
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(1);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("my_data_source");

  // Create a new trace session.
  auto* tracing_session = NewTrace(cfg);

  data_source->on_start_callback = [] {
    MockDataSource::Trace([](MockDataSource::TraceContext ctx) {
      ctx.NewTracePacket()->set_for_testing()->set_str("on-start");
      auto ds = ctx.GetDataSourceLocked();
      ASSERT_TRUE(!!ds);
      ctx.NewTracePacket()->set_for_testing()->set_str("on-start-locked");
    });
  };

  data_source->on_stop_callback = [] {
    MockDataSource::Trace([](MockDataSource::TraceContext ctx) {
      ctx.NewTracePacket()->set_for_testing()->set_str("on-stop");
      auto ds = ctx.GetDataSourceLocked();
      ASSERT_TRUE(!!ds);
      ctx.NewTracePacket()->set_for_testing()->set_str("on-stop-locked");
      ctx.Flush();
    });
  };

  tracing_session->get()->Start();
  data_source->on_stop.Wait();
  tracing_session->on_stop.Wait();

  std::vector<char> raw_trace = tracing_session->get()->ReadTraceBlocking();
  ASSERT_GE(raw_trace.size(), 0u);

  perfetto::protos::Trace trace;
  ASSERT_TRUE(trace.ParseFromArray(raw_trace.data(), int(raw_trace.size())));
  int packets_found = 0;
  for (const auto& packet : trace.packet()) {
    if (!packet.has_for_testing())
      continue;
    packets_found |= packet.for_testing().str() == "on-start" ? 1 : 0;
    packets_found |= packet.for_testing().str() == "on-start-locked" ? 2 : 0;
    packets_found |= packet.for_testing().str() == "on-stop" ? 4 : 0;
    packets_found |= packet.for_testing().str() == "on-stop-locked" ? 8 : 0;
  }
  EXPECT_EQ(packets_found, 1 | 2 | 4 | 8);
}

}  // namespace

PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(MockDataSource);
PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(MockDataSource2);
PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(TestIncrementalDataSource,
                                           TestIncrementalState);
