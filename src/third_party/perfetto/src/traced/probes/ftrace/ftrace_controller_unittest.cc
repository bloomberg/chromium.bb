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

#include "src/traced/probes/ftrace/ftrace_controller.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "src/traced/probes/ftrace/cpu_reader.h"
#include "src/traced/probes/ftrace/ftrace_config.h"
#include "src/traced/probes/ftrace/ftrace_config_muxer.h"
#include "src/traced/probes/ftrace/ftrace_data_source.h"
#include "src/traced/probes/ftrace/ftrace_procfs.h"
#include "src/traced/probes/ftrace/proto_translation_table.h"
#include "src/tracing/core/trace_writer_for_testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "perfetto/trace/trace_packet.pb.h"

#include "perfetto/trace/ftrace/ftrace_event_bundle.pbzero.h"
#include "perfetto/trace/ftrace/ftrace_stats.pbzero.h"
#include "perfetto/trace/trace_packet.pbzero.h"

using testing::_;
using testing::AnyNumber;
using testing::ByMove;
using testing::Invoke;
using testing::NiceMock;
using testing::MatchesRegex;
using testing::Return;
using testing::IsEmpty;
using testing::ElementsAre;
using testing::Pair;

using Table = perfetto::ProtoTranslationTable;
using FtraceEventBundle = perfetto::protos::pbzero::FtraceEventBundle;

namespace perfetto {

namespace {

constexpr char kFooEnablePath[] = "/root/events/group/foo/enable";
constexpr char kBarEnablePath[] = "/root/events/group/bar/enable";

class MockTaskRunner : public base::TaskRunner {
 public:
  MockTaskRunner() {
    ON_CALL(*this, PostTask(_))
        .WillByDefault(Invoke(this, &MockTaskRunner::OnPostTask));
    ON_CALL(*this, PostDelayedTask(_, _))
        .WillByDefault(Invoke(this, &MockTaskRunner::OnPostDelayedTask));
  }

  void OnPostTask(std::function<void()> task) {
    std::unique_lock<std::mutex> lock(lock_);
    EXPECT_FALSE(task_);
    task_ = std::move(task);
  }

  void OnPostDelayedTask(std::function<void()> task, int /*delay*/) {
    std::unique_lock<std::mutex> lock(lock_);
    EXPECT_FALSE(task_);
    task_ = std::move(task);
  }

  void RunLastTask() {
    auto task = TakeTask();
    if (task)
      task();
  }

  std::function<void()> TakeTask() {
    std::unique_lock<std::mutex> lock(lock_);
    auto task(std::move(task_));
    task_ = std::function<void()>();
    return task;
  }

  MOCK_METHOD1(PostTask, void(std::function<void()>));
  MOCK_METHOD2(PostDelayedTask, void(std::function<void()>, uint32_t delay_ms));
  MOCK_METHOD2(AddFileDescriptorWatch, void(int fd, std::function<void()>));
  MOCK_METHOD1(RemoveFileDescriptorWatch, void(int fd));
  MOCK_CONST_METHOD0(RunsTasksOnCurrentThread, bool());

 private:
  std::mutex lock_;
  std::function<void()> task_;
};

std::unique_ptr<Table> FakeTable(FtraceProcfs* ftrace) {
  std::vector<Field> common_fields;
  std::vector<Event> events;

  {
    Event event;
    event.name = "foo";
    event.group = "group";
    event.ftrace_event_id = 1;
    events.push_back(event);
  }

  {
    Event event;
    event.name = "bar";
    event.group = "group";
    event.ftrace_event_id = 10;
    events.push_back(event);
  }

  return std::unique_ptr<Table>(
      new Table(ftrace, events, std::move(common_fields),
                ProtoTranslationTable::DefaultPageHeaderSpecForTesting()));
}

std::unique_ptr<FtraceConfigMuxer> FakeModel(FtraceProcfs* ftrace,
                                             ProtoTranslationTable* table) {
  return std::unique_ptr<FtraceConfigMuxer>(
      new FtraceConfigMuxer(ftrace, table));
}

class MockFtraceProcfs : public FtraceProcfs {
 public:
  explicit MockFtraceProcfs(size_t cpu_count = 1) : FtraceProcfs("/root/") {
    ON_CALL(*this, NumberOfCpus()).WillByDefault(Return(cpu_count));
    EXPECT_CALL(*this, NumberOfCpus()).Times(AnyNumber());

    ON_CALL(*this, ReadFileIntoString("/root/trace_clock"))
        .WillByDefault(Return("local global [boot]"));
    EXPECT_CALL(*this, ReadFileIntoString("/root/trace_clock"))
        .Times(AnyNumber());

    ON_CALL(*this, ReadFileIntoString("/root/per_cpu/cpu0/stats"))
        .WillByDefault(Return(""));
    EXPECT_CALL(*this, ReadFileIntoString("/root/per_cpu/cpu0/stats"))
        .Times(AnyNumber());

    ON_CALL(*this, ReadFileIntoString("/root/events//not_an_event/format"))
        .WillByDefault(Return(""));
    EXPECT_CALL(*this, ReadFileIntoString("/root/events//not_an_event/format"))
        .Times(AnyNumber());

    ON_CALL(*this, ReadFileIntoString("/root/events/group/bar/format"))
        .WillByDefault(Return(""));
    EXPECT_CALL(*this, ReadFileIntoString("/root/events/group/bar/format"))
        .Times(AnyNumber());

    ON_CALL(*this, WriteToFile(_, _)).WillByDefault(Return(true));
    ON_CALL(*this, ClearFile(_)).WillByDefault(Return(true));

    ON_CALL(*this, WriteToFile("/root/tracing_on", _))
        .WillByDefault(Invoke(this, &MockFtraceProcfs::WriteTracingOn));
    ON_CALL(*this, ReadOneCharFromFile("/root/tracing_on"))
        .WillByDefault(Invoke(this, &MockFtraceProcfs::ReadTracingOn));
    EXPECT_CALL(*this, ReadOneCharFromFile("/root/tracing_on"))
        .Times(AnyNumber());
  }

  bool WriteTracingOn(const std::string& /*path*/, const std::string& value) {
    PERFETTO_CHECK(value == "1" || value == "0");
    tracing_on_ = value == "1";
    return true;
  }

  char ReadTracingOn(const std::string& /*path*/) {
    return tracing_on_ ? '1' : '0';
  }

  base::ScopedFile OpenPipeForCpu(size_t /*cpu*/) override {
    return base::ScopedFile(base::OpenFile("/dev/null", O_RDONLY));
  }

  MOCK_METHOD2(WriteToFile,
               bool(const std::string& path, const std::string& str));
  MOCK_CONST_METHOD0(NumberOfCpus, size_t());
  MOCK_METHOD1(ReadOneCharFromFile, char(const std::string& path));
  MOCK_METHOD1(ClearFile, bool(const std::string& path));
  MOCK_CONST_METHOD1(ReadFileIntoString, std::string(const std::string& path));

  bool is_tracing_on() { return tracing_on_; }

 private:
  bool tracing_on_ = false;
};

}  // namespace

class TestFtraceController : public FtraceController,
                             public FtraceController::Observer {
 public:
  TestFtraceController(std::unique_ptr<MockFtraceProcfs> ftrace_procfs,
                       std::unique_ptr<Table> table,
                       std::unique_ptr<FtraceConfigMuxer> model,
                       std::unique_ptr<MockTaskRunner> runner,
                       MockFtraceProcfs* raw_procfs)
      : FtraceController(std::move(ftrace_procfs),
                         std::move(table),
                         std::move(model),
                         runner.get(),
                         /*observer=*/this),
        runner_(std::move(runner)),
        procfs_(raw_procfs) {}

  MOCK_METHOD1(OnDrainCpuForTesting, void(size_t cpu));

  MockTaskRunner* runner() { return runner_.get(); }
  MockFtraceProcfs* procfs() { return procfs_; }

  uint64_t NowMs() const override { return now_ms; }

  uint32_t drain_period_ms() { return GetDrainPeriodMs(); }

  std::function<void()> GetDataAvailableCallback(size_t cpu) {
    int generation = generation_;
    auto* thread_sync = &thread_sync_;
    return [cpu, generation, thread_sync] {
      FtraceController::OnCpuReaderRead(cpu, generation, thread_sync);
    };
  }

  void WaitForData(size_t cpu) {
    for (;;) {
      {
        std::unique_lock<std::mutex> lock(thread_sync_.mutex);
        if (thread_sync_.cpus_to_drain[cpu])
          return;
      }
      usleep(5000);
    }
  }

  std::unique_ptr<FtraceDataSource> AddFakeDataSource(const FtraceConfig& cfg) {
    std::unique_ptr<FtraceDataSource> data_source(new FtraceDataSource(
        GetWeakPtr(), 0 /* session id */, cfg, nullptr /* trace_writer */));
    if (!AddDataSource(data_source.get()))
      return nullptr;
    return data_source;
  }

  void OnFtraceDataWrittenIntoDataSourceBuffers() override {}

  uint64_t now_ms = 0;

 private:
  TestFtraceController(const TestFtraceController&) = delete;
  TestFtraceController& operator=(const TestFtraceController&) = delete;

  std::unique_ptr<MockTaskRunner> runner_;
  MockFtraceProcfs* procfs_;
};

namespace {

std::unique_ptr<TestFtraceController> CreateTestController(
    bool runner_is_nice_mock,
    bool procfs_is_nice_mock,
    size_t cpu_count = 1) {
  std::unique_ptr<MockTaskRunner> runner;
  if (runner_is_nice_mock) {
    runner = std::unique_ptr<MockTaskRunner>(new NiceMock<MockTaskRunner>());
  } else {
    runner = std::unique_ptr<MockTaskRunner>(new MockTaskRunner());
  }

  std::unique_ptr<MockFtraceProcfs> ftrace_procfs;
  if (procfs_is_nice_mock) {
    ftrace_procfs = std::unique_ptr<MockFtraceProcfs>(
        new NiceMock<MockFtraceProcfs>(cpu_count));
  } else {
    ftrace_procfs =
        std::unique_ptr<MockFtraceProcfs>(new MockFtraceProcfs(cpu_count));
  }

  auto table = FakeTable(ftrace_procfs.get());

  auto model = FakeModel(ftrace_procfs.get(), table.get());

  MockFtraceProcfs* raw_procfs = ftrace_procfs.get();
  return std::unique_ptr<TestFtraceController>(new TestFtraceController(
      std::move(ftrace_procfs), std::move(table), std::move(model),
      std::move(runner), raw_procfs));
}

}  // namespace

TEST(FtraceControllerTest, NonExistentEventsDontCrash) {
  auto controller =
      CreateTestController(true /* nice runner */, true /* nice procfs */);

  FtraceConfig config = CreateFtraceConfig({"not_an_event"});
  EXPECT_TRUE(controller->AddFakeDataSource(config));
}

TEST(FtraceControllerTest, RejectsBadEventNames) {
  auto controller =
      CreateTestController(true /* nice runner */, true /* nice procfs */);

  FtraceConfig config = CreateFtraceConfig({"../try/to/escape"});
  EXPECT_FALSE(controller->AddFakeDataSource(config));
  config = CreateFtraceConfig({"/event"});
  EXPECT_FALSE(controller->AddFakeDataSource(config));
  config = CreateFtraceConfig({"event/"});
  EXPECT_FALSE(controller->AddFakeDataSource(config));
}

TEST(FtraceControllerTest, OneSink) {
  auto controller =
      CreateTestController(true /* nice runner */, false /* nice procfs */);

  FtraceConfig config = CreateFtraceConfig({"group/foo"});

  EXPECT_CALL(*controller->procfs(), WriteToFile(kFooEnablePath, "1"));
  EXPECT_CALL(*controller->procfs(), WriteToFile("/root/buffer_size_kb", _));
  auto data_source = controller->AddFakeDataSource(config);
  ASSERT_TRUE(data_source);

  EXPECT_CALL(*controller->procfs(), WriteToFile("/root/tracing_on", "1"));
  ASSERT_TRUE(controller->StartDataSource(data_source.get()));

  EXPECT_CALL(*controller->procfs(), WriteToFile("/root/buffer_size_kb", "0"));
  EXPECT_CALL(*controller->procfs(), ClearFile("/root/trace"))
      .WillOnce(Return(true));
  EXPECT_CALL(*controller->procfs(),
              ClearFile(MatchesRegex("/root/per_cpu/cpu[0-9]/trace")))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*controller->procfs(), WriteToFile(kFooEnablePath, "0"));
  EXPECT_CALL(*controller->procfs(), WriteToFile("/root/tracing_on", "0"));
  EXPECT_CALL(*controller->procfs(), WriteToFile("/root/events/enable", "0"));
  EXPECT_TRUE(controller->procfs()->is_tracing_on());

  data_source.reset();
  EXPECT_FALSE(controller->procfs()->is_tracing_on());
}

TEST(FtraceControllerTest, MultipleSinks) {
  auto controller =
      CreateTestController(false /* nice runner */, false /* nice procfs */);

  FtraceConfig configA = CreateFtraceConfig({"group/foo"});
  FtraceConfig configB = CreateFtraceConfig({"group/foo", "group/bar"});

  EXPECT_CALL(*controller->procfs(), WriteToFile("/root/buffer_size_kb", _));
  EXPECT_CALL(*controller->procfs(), WriteToFile(kFooEnablePath, "1"));
  auto data_sourceA = controller->AddFakeDataSource(configA);
  EXPECT_CALL(*controller->procfs(), WriteToFile(kBarEnablePath, "1"));
  auto data_sourceB = controller->AddFakeDataSource(configB);

  EXPECT_CALL(*controller->procfs(), WriteToFile("/root/tracing_on", "1"));
  ASSERT_TRUE(controller->StartDataSource(data_sourceA.get()));
  ASSERT_TRUE(controller->StartDataSource(data_sourceB.get()));

  data_sourceA.reset();

  EXPECT_CALL(*controller->procfs(), WriteToFile(kFooEnablePath, "0"));
  EXPECT_CALL(*controller->procfs(), WriteToFile(kBarEnablePath, "0"));
  EXPECT_CALL(*controller->procfs(), WriteToFile("/root/buffer_size_kb", "0"));
  EXPECT_CALL(*controller->procfs(), WriteToFile("/root/tracing_on", "0"));
  EXPECT_CALL(*controller->procfs(), WriteToFile("/root/events/enable", "0"));
  EXPECT_CALL(*controller->procfs(), ClearFile("/root/trace"));
  EXPECT_CALL(*controller->procfs(),
              ClearFile(MatchesRegex("/root/per_cpu/cpu[0-9]/trace")));
  data_sourceB.reset();
}

TEST(FtraceControllerTest, ControllerMayDieFirst) {
  auto controller =
      CreateTestController(false /* nice runner */, false /* nice procfs */);

  FtraceConfig config = CreateFtraceConfig({"group/foo"});

  EXPECT_CALL(*controller->procfs(), WriteToFile("/root/buffer_size_kb", _));
  EXPECT_CALL(*controller->procfs(), WriteToFile(kFooEnablePath, "1"));
  auto data_source = controller->AddFakeDataSource(config);

  EXPECT_CALL(*controller->procfs(), WriteToFile("/root/tracing_on", "1"));
  ASSERT_TRUE(controller->StartDataSource(data_source.get()));

  EXPECT_CALL(*controller->procfs(), WriteToFile(kFooEnablePath, "0"));
  EXPECT_CALL(*controller->procfs(), ClearFile("/root/trace"))
      .WillOnce(Return(true));
  EXPECT_CALL(*controller->procfs(),
              ClearFile(MatchesRegex("/root/per_cpu/cpu[0-9]/trace")))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*controller->procfs(), WriteToFile("/root/tracing_on", "0"));
  EXPECT_CALL(*controller->procfs(), WriteToFile("/root/buffer_size_kb", "0"));
  EXPECT_CALL(*controller->procfs(), WriteToFile("/root/events/enable", "0"));
  controller.reset();
  data_source.reset();
}

TEST(FtraceControllerTest, BackToBackEnableDisable) {
  auto controller =
      CreateTestController(false /* nice runner */, false /* nice procfs */);

  // For this test we don't care about calls to WriteToFile/ClearFile.
  EXPECT_CALL(*controller->procfs(), WriteToFile(_, _)).Times(AnyNumber());
  EXPECT_CALL(*controller->procfs(), ClearFile(_)).Times(AnyNumber());
  EXPECT_CALL(*controller->procfs(), ReadOneCharFromFile("/root/tracing_on"))
      .Times(AnyNumber());

  EXPECT_CALL(*controller->runner(), PostTask(_)).Times(2);
  EXPECT_CALL(*controller->runner(), PostDelayedTask(_, 100)).Times(2);
  FtraceConfig config = CreateFtraceConfig({"group/foo"});
  auto data_source = controller->AddFakeDataSource(config);
  ASSERT_TRUE(controller->StartDataSource(data_source.get()));

  auto on_data_available = controller->GetDataAvailableCallback(0u);
  std::thread worker([on_data_available] { on_data_available(); });
  controller->WaitForData(0u);

  // Disable the first data source and run the delayed task that it generated.
  // It should be a no-op.
  data_source.reset();
  controller->runner()->RunLastTask();
  controller->runner()->RunLastTask();
  worker.join();

  // Register another data source and wait for it to generate data.
  data_source = controller->AddFakeDataSource(config);
  ASSERT_TRUE(controller->StartDataSource(data_source.get()));

  on_data_available = controller->GetDataAvailableCallback(0u);
  std::thread worker2([on_data_available] { on_data_available(); });
  controller->WaitForData(0u);

  // This drain should also be a no-op after the data source is unregistered.
  data_source.reset();
  controller->runner()->RunLastTask();
  controller->runner()->RunLastTask();
  worker2.join();
}

TEST(FtraceControllerTest, BufferSize) {
  auto controller =
      CreateTestController(true /* nice runner */, false /* nice procfs */);

  // For this test we don't care about most calls to WriteToFile/ClearFile.
  EXPECT_CALL(*controller->procfs(), WriteToFile(_, _)).Times(AnyNumber());
  EXPECT_CALL(*controller->procfs(), ClearFile(_)).Times(AnyNumber());

  {
    // No buffer size -> good default.
    EXPECT_CALL(*controller->procfs(),
                WriteToFile("/root/buffer_size_kb", "2048"));
    FtraceConfig config = CreateFtraceConfig({"group/foo"});
    auto data_source = controller->AddFakeDataSource(config);
    ASSERT_TRUE(controller->StartDataSource(data_source.get()));
  }

  {
    // Way too big buffer size -> max size.
    EXPECT_CALL(*controller->procfs(),
                WriteToFile("/root/buffer_size_kb", "65536"));
    FtraceConfig config = CreateFtraceConfig({"group/foo"});
    config.set_buffer_size_kb(10 * 1024 * 1024);
    auto data_source = controller->AddFakeDataSource(config);
    ASSERT_TRUE(controller->StartDataSource(data_source.get()));
  }

  {
    // The limit is 64mb, 65mb is too much.
    EXPECT_CALL(*controller->procfs(),
                WriteToFile("/root/buffer_size_kb", "65536"));
    FtraceConfig config = CreateFtraceConfig({"group/foo"});
    ON_CALL(*controller->procfs(), NumberOfCpus()).WillByDefault(Return(2));
    config.set_buffer_size_kb(65 * 1024);
    auto data_source = controller->AddFakeDataSource(config);
    ASSERT_TRUE(controller->StartDataSource(data_source.get()));
  }

  {
    // Your size ends up with less than 1 page per cpu -> 1 page.
    EXPECT_CALL(*controller->procfs(),
                WriteToFile("/root/buffer_size_kb", "4"));
    FtraceConfig config = CreateFtraceConfig({"group/foo"});
    config.set_buffer_size_kb(1);
    auto data_source = controller->AddFakeDataSource(config);
    ASSERT_TRUE(controller->StartDataSource(data_source.get()));
  }

  {
    // You picked a good size -> your size rounded to nearest page.
    EXPECT_CALL(*controller->procfs(),
                WriteToFile("/root/buffer_size_kb", "40"));
    FtraceConfig config = CreateFtraceConfig({"group/foo"});
    config.set_buffer_size_kb(42);
    auto data_source = controller->AddFakeDataSource(config);
    ASSERT_TRUE(controller->StartDataSource(data_source.get()));
  }

  {
    // You picked a good size -> your size rounded to nearest page.
    EXPECT_CALL(*controller->procfs(),
                WriteToFile("/root/buffer_size_kb", "40"));
    FtraceConfig config = CreateFtraceConfig({"group/foo"});
    ON_CALL(*controller->procfs(), NumberOfCpus()).WillByDefault(Return(2));
    config.set_buffer_size_kb(42);
    auto data_source = controller->AddFakeDataSource(config);
    ASSERT_TRUE(controller->StartDataSource(data_source.get()));
  }
}

TEST(FtraceControllerTest, PeriodicDrainConfig) {
  auto controller =
      CreateTestController(true /* nice runner */, false /* nice procfs */);

  // For this test we don't care about calls to WriteToFile/ClearFile.
  EXPECT_CALL(*controller->procfs(), WriteToFile(_, _)).Times(AnyNumber());
  EXPECT_CALL(*controller->procfs(), ClearFile(_)).Times(AnyNumber());

  {
    // No period -> good default.
    FtraceConfig config = CreateFtraceConfig({"group/foo"});
    auto data_source = controller->AddFakeDataSource(config);
    EXPECT_EQ(100u, controller->drain_period_ms());
  }

  {
    // Pick a tiny value -> good default.
    FtraceConfig config = CreateFtraceConfig({"group/foo"});
    config.set_drain_period_ms(0);
    auto data_source = controller->AddFakeDataSource(config);
    EXPECT_EQ(100u, controller->drain_period_ms());
  }

  {
    // Pick a huge value -> good default.
    FtraceConfig config = CreateFtraceConfig({"group/foo"});
    config.set_drain_period_ms(1000 * 60 * 60);
    auto data_source = controller->AddFakeDataSource(config);
    EXPECT_EQ(100u, controller->drain_period_ms());
  }

  {
    // Pick a resonable value -> get that value.
    FtraceConfig config = CreateFtraceConfig({"group/foo"});
    config.set_drain_period_ms(200);
    auto data_source = controller->AddFakeDataSource(config);
    EXPECT_EQ(200u, controller->drain_period_ms());
  }
}

TEST(FtraceMetadataTest, Clear) {
  FtraceMetadata metadata;
  metadata.inode_and_device.push_back(std::make_pair(1, 1));
  metadata.pids.push_back(2);
  metadata.overwrite_count = 3;
  metadata.last_seen_device_id = 100;
  metadata.Clear();
  EXPECT_THAT(metadata.inode_and_device, IsEmpty());
  EXPECT_THAT(metadata.pids, IsEmpty());
  EXPECT_EQ(0u, metadata.overwrite_count);
  EXPECT_EQ(BlockDeviceID(0), metadata.last_seen_device_id);
}

TEST(FtraceMetadataTest, AddDevice) {
  FtraceMetadata metadata;
  metadata.AddDevice(1);
  EXPECT_EQ(BlockDeviceID(1), metadata.last_seen_device_id);
  metadata.AddDevice(3);
  EXPECT_EQ(BlockDeviceID(3), metadata.last_seen_device_id);
}

TEST(FtraceMetadataTest, AddInode) {
  FtraceMetadata metadata;
  metadata.AddCommonPid(getpid() + 1);
  metadata.AddDevice(3);
  metadata.AddInode(2);
  metadata.AddInode(1);
  metadata.AddCommonPid(getpid() + 1);
  metadata.AddDevice(4);
  metadata.AddInode(3);

  // Check activity from ourselves is excluded.
  metadata.AddCommonPid(getpid());
  metadata.AddDevice(5);
  metadata.AddInode(5);

  EXPECT_THAT(metadata.inode_and_device,
              ElementsAre(Pair(2, 3), Pair(1, 3), Pair(3, 4)));
}

TEST(FtraceMetadataTest, AddPid) {
  FtraceMetadata metadata;
  metadata.AddPid(1);
  metadata.AddPid(2);
  metadata.AddPid(2);
  metadata.AddPid(3);
  EXPECT_THAT(metadata.pids, ElementsAre(1, 2, 3));
}

TEST(FtraceStatsTest, Write) {
  FtraceStats stats{};
  FtraceCpuStats cpu_stats{};
  cpu_stats.cpu = 0;
  cpu_stats.entries = 1;
  cpu_stats.overrun = 2;
  stats.cpu_stats.push_back(cpu_stats);

  std::unique_ptr<TraceWriterForTesting> writer =
      std::unique_ptr<TraceWriterForTesting>(new TraceWriterForTesting());
  {
    auto packet = writer->NewTracePacket();
    auto* out = packet->set_ftrace_stats();
    stats.Write(out);
  }

  std::unique_ptr<protos::TracePacket> result_packet = writer->ParseProto();
  auto result = result_packet->ftrace_stats().cpu_stats(0);
  EXPECT_EQ(result.cpu(), 0);
  EXPECT_EQ(result.entries(), 1);
  EXPECT_EQ(result.overrun(), 2);
}

}  // namespace perfetto
