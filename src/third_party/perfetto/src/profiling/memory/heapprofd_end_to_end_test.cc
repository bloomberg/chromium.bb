/*
 * Copyright (C) 2018 The Android Open Source Project
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
#include <sys/system_properties.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "perfetto/base/build_config.h"
#include "perfetto/base/pipe.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "src/base/test/test_task_runner.h"
#include "src/profiling/memory/heapprofd_producer.h"
#include "src/tracing/ipc/default_socket.h"
#include "test/test_helper.h"

#include "perfetto/config/profiling/heapprofd_config.pbzero.h"

// This test only works when run on Android using an Android Q version of
// Bionic.
#if !PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
#error "This test can only be used on Android."
#endif

namespace perfetto {
namespace profiling {
namespace {

constexpr useconds_t kMsToUs = 1000;

using ::testing::AnyOf;
using ::testing::Eq;

class HeapprofdDelegate : public ThreadDelegate {
 public:
  HeapprofdDelegate(const std::string& producer_socket)
      : producer_socket_(producer_socket) {}
  ~HeapprofdDelegate() override = default;

  void Initialize(base::TaskRunner* task_runner) override {
    producer_.reset(
        new HeapprofdProducer(HeapprofdMode::kCentral, task_runner));
    producer_->ConnectWithRetries(producer_socket_.c_str());
  }

 private:
  std::string producer_socket_;
  std::unique_ptr<HeapprofdProducer> producer_;
};

constexpr const char* kEnableHeapprofdProperty = "persist.heapprofd.enable";
constexpr const char* kHeapprofdModeProperty = "heapprofd.userdebug.mode";

std::string ReadProperty(const std::string& name, std::string def) {
  const prop_info* pi = __system_property_find(name.c_str());
  if (pi) {
    __system_property_read_callback(
        pi,
        [](void* cookie, const char*, const char* value, uint32_t) {
          *reinterpret_cast<std::string*>(cookie) = value;
        },
        &def);
  }
  return def;
}

int __attribute__((unused)) SetModeProperty(std::string* value) {
  if (value) {
    __system_property_set(kHeapprofdModeProperty, value->c_str());
    delete value;
  }
  return 0;
}

base::ScopedResource<std::string*, SetModeProperty, nullptr> EnableFork() {
  std::string prev_property_value = ReadProperty(kHeapprofdModeProperty, "");
  __system_property_set(kHeapprofdModeProperty, "fork");
  return base::ScopedResource<std::string*, SetModeProperty, nullptr>(
      new std::string(prev_property_value));
}

base::ScopedResource<std::string*, SetModeProperty, nullptr> DisableFork() {
  std::string prev_property_value = ReadProperty(kHeapprofdModeProperty, "");
  __system_property_set(kHeapprofdModeProperty, "");
  return base::ScopedResource<std::string*, SetModeProperty, nullptr>(
      new std::string(prev_property_value));
}

int __attribute__((unused)) SetEnableProperty(std::string* value) {
  if (value) {
    __system_property_set(kEnableHeapprofdProperty, value->c_str());
    delete value;
  }
  return 0;
}

constexpr size_t kStartupAllocSize = 10;

void AllocateAndFree(size_t bytes) {
  // This volatile is needed to prevent the compiler from trying to be
  // helpful and compiling a "useless" malloc + free into a noop.
  volatile char* x = static_cast<char*>(malloc(bytes));
  if (x) {
    x[1] = 'x';
    free(const_cast<char*>(x));
  }
}

void __attribute__((noreturn)) ContinuousMalloc(size_t bytes) {
  for (;;) {
    AllocateAndFree(bytes);
    usleep(10 * kMsToUs);
  }
}

pid_t ForkContinuousMalloc(size_t bytes) {
  // Make sure forked process does not get reparented to init.
  setsid();
  pid_t pid = fork();
  switch (pid) {
    case -1:
      PERFETTO_FATAL("Failed to fork.");
    case 0:
      ContinuousMalloc(bytes);
    default:
      break;
  }
  return pid;
}

void __attribute__((constructor)) RunContinuousMalloc() {
  if (getenv("HEAPPROFD_TESTING_RUN_MALLOC") != nullptr)
    ContinuousMalloc(kStartupAllocSize);
}

std::unique_ptr<TestHelper> GetHelper(base::TestTaskRunner* task_runner) {
  std::unique_ptr<TestHelper> helper(new TestHelper(task_runner));
  helper->ConnectConsumer();
  helper->WaitForConsumerConnect();
  return helper;
}

std::string FormatHistogram(const protos::ProfilePacket_Histogram& hist) {
  std::string out;
  std::string prev_upper_limit = "-inf";
  for (const auto& bucket : hist.buckets()) {
    std::string upper_limit;
    if (bucket.max_bucket())
      upper_limit = "inf";
    else
      upper_limit = std::to_string(bucket.upper_limit());

    out += "[" + prev_upper_limit + ", " + upper_limit +
           "]: " + std::to_string(bucket.count()) + "; ";
    prev_upper_limit = std::move(upper_limit);
  }
  return out + "\n";
}

std::string FormatStats(const protos::ProfilePacket_ProcessStats& stats) {
  return std::string("unwinding_errors: ") +
         std::to_string(stats.unwinding_errors()) + "\n" +
         "heap_samples: " + std::to_string(stats.heap_samples()) + "\n" +
         "map_reparses: " + std::to_string(stats.map_reparses()) + "\n" +
         "unwinding_time_us: " + FormatHistogram(stats.unwinding_time_us());
}

class HeapprofdEndToEnd : public ::testing::Test {
 public:
  HeapprofdEndToEnd() {
    // This is not needed for correctness, but works around a init behavior that
    // makes this test take much longer. If persist.heapprofd.enable is set to 0
    // and then set to 1 again too quickly, init decides that the service is
    // "restarting" and waits before restarting it.
    usleep(50000);
  }

 protected:
  base::TestTaskRunner task_runner;

  std::unique_ptr<TestHelper> Trace(const TraceConfig& trace_config) {
    auto helper = GetHelper(&task_runner);

    helper->StartTracing(trace_config);
    helper->WaitForTracingDisabled(20000);

    helper->ReadData();
    helper->WaitForReadData();
    return helper;
  }

  void PrintStats(TestHelper* helper) {
    const auto& packets = helper->trace();
    for (const protos::TracePacket& packet : packets) {
      for (const auto& dump : packet.profile_packet().process_dumps()) {
        // protobuf uint64 does not like the PRIu64 formatter.
        PERFETTO_LOG("Stats for %s: %s", std::to_string(dump.pid()).c_str(),
                     FormatStats(dump.stats()).c_str());
      }
    }
  }

  void ValidateSampleSizes(TestHelper* helper,
                           uint64_t pid,
                           uint64_t alloc_size) {
    const auto& packets = helper->trace();
    for (const protos::TracePacket& packet : packets) {
      for (const auto& dump : packet.profile_packet().process_dumps()) {
        if (dump.pid() != pid)
          continue;
        for (const auto& sample : dump.samples()) {
          EXPECT_EQ(sample.self_allocated() % alloc_size, 0);
          EXPECT_EQ(sample.self_freed() % alloc_size, 0);
          EXPECT_THAT(sample.self_allocated() - sample.self_freed(),
                      AnyOf(Eq(0), Eq(alloc_size)));
        }
      }
    }
  }

  void ValidateFromStartup(TestHelper* helper,
                           uint64_t pid,
                           bool from_startup) {
    const auto& packets = helper->trace();
    for (const protos::TracePacket& packet : packets) {
      for (const auto& dump : packet.profile_packet().process_dumps()) {
        if (dump.pid() != pid)
          continue;
        EXPECT_EQ(dump.from_startup(), from_startup);
      }
    }
  }

  void ValidateRejectedConcurrent(TestHelper* helper,
                                  uint64_t pid,
                                  bool rejected_concurrent) {
    const auto& packets = helper->trace();
    for (const protos::TracePacket& packet : packets) {
      for (const auto& dump : packet.profile_packet().process_dumps()) {
        if (dump.pid() != pid)
          continue;
        EXPECT_EQ(dump.rejected_concurrent(), rejected_concurrent);
      }
    }
  }

  void ValidateHasSamples(TestHelper* helper, uint64_t pid) {
    const auto& packets = helper->trace();
    ASSERT_GT(packets.size(), 0u);
    size_t profile_packets = 0;
    size_t samples = 0;
    uint64_t last_allocated = 0;
    uint64_t last_freed = 0;
    for (const protos::TracePacket& packet : packets) {
      for (const auto& dump : packet.profile_packet().process_dumps()) {
        if (dump.pid() != pid)
          continue;
        for (const auto& sample : dump.samples()) {
          last_allocated = sample.self_allocated();
          last_freed = sample.self_freed();
          samples++;
        }
        profile_packets++;
      }
    }
    EXPECT_GT(profile_packets, 0);
    EXPECT_GT(samples, 0);
    EXPECT_GT(last_allocated, 0);
    EXPECT_GT(last_freed, 0);
  }

  void ValidateOnlyPID(TestHelper* helper, uint64_t pid) {
    size_t dumps = 0;
    const auto& packets = helper->trace();
    for (const protos::TracePacket& packet : packets) {
      for (const auto& dump : packet.profile_packet().process_dumps()) {
        EXPECT_EQ(dump.pid(), pid);
        dumps++;
      }
    }
    EXPECT_GT(dumps, 0);
  }

  void Smoke() {
    constexpr size_t kAllocSize = 1024;

    pid_t pid = ForkContinuousMalloc(kAllocSize);

    TraceConfig trace_config;
    trace_config.add_buffers()->set_size_kb(10 * 1024);
    trace_config.set_duration_ms(2000);
    trace_config.set_flush_timeout_ms(10000);

    auto* ds_config = trace_config.add_data_sources()->mutable_config();
    ds_config->set_name("android.heapprofd");
    ds_config->set_target_buffer(0);

    protozero::HeapBuffered<protos::pbzero::HeapprofdConfig> heapprofd_config;
    heapprofd_config->set_sampling_interval_bytes(1);
    heapprofd_config->add_pid(static_cast<uint64_t>(pid));
    heapprofd_config->set_all(false);
    auto* cont_config = heapprofd_config->set_continuous_dump_config();
    cont_config->set_dump_phase_ms(0);
    cont_config->set_dump_interval_ms(100);
    ds_config->set_heapprofd_config_raw(heapprofd_config.SerializeAsString());

    auto helper = Trace(trace_config);
    PrintStats(helper.get());
    ValidateHasSamples(helper.get(), static_cast<uint64_t>(pid));
    ValidateOnlyPID(helper.get(), static_cast<uint64_t>(pid));
    ValidateSampleSizes(helper.get(), static_cast<uint64_t>(pid), kAllocSize);

    PERFETTO_CHECK(kill(pid, SIGKILL) == 0);
    PERFETTO_CHECK(PERFETTO_EINTR(waitpid(pid, nullptr, 0)) == pid);
  }

  void TwoProcesses() {
    constexpr size_t kAllocSize = 1024;
    constexpr size_t kAllocSize2 = 7;

    pid_t pid = ForkContinuousMalloc(kAllocSize);
    pid_t pid2 = ForkContinuousMalloc(kAllocSize2);

    TraceConfig trace_config;
    trace_config.add_buffers()->set_size_kb(10 * 1024);
    trace_config.set_duration_ms(2000);
    trace_config.set_flush_timeout_ms(10000);

    auto* ds_config = trace_config.add_data_sources()->mutable_config();
    ds_config->set_name("android.heapprofd");
    ds_config->set_target_buffer(0);

    protozero::HeapBuffered<protos::pbzero::HeapprofdConfig> heapprofd_config;
    heapprofd_config->set_sampling_interval_bytes(1);
    heapprofd_config->add_pid(static_cast<uint64_t>(pid));
    heapprofd_config->add_pid(static_cast<uint64_t>(pid2));
    heapprofd_config->set_all(false);
    ds_config->set_heapprofd_config_raw(heapprofd_config.SerializeAsString());

    auto helper = Trace(trace_config);
    PrintStats(helper.get());
    ValidateHasSamples(helper.get(), static_cast<uint64_t>(pid));
    ValidateSampleSizes(helper.get(), static_cast<uint64_t>(pid), kAllocSize);
    ValidateHasSamples(helper.get(), static_cast<uint64_t>(pid2));
    ValidateSampleSizes(helper.get(), static_cast<uint64_t>(pid2), kAllocSize2);

    PERFETTO_CHECK(kill(pid, SIGKILL) == 0);
    PERFETTO_CHECK(PERFETTO_EINTR(waitpid(pid, nullptr, 0)) == pid);
    PERFETTO_CHECK(kill(pid2, SIGKILL) == 0);
    PERFETTO_CHECK(PERFETTO_EINTR(waitpid(pid2, nullptr, 0)) == pid2);
  }

  void FinalFlush() {
    constexpr size_t kAllocSize = 1024;

    pid_t pid = ForkContinuousMalloc(kAllocSize);

    TraceConfig trace_config;
    trace_config.add_buffers()->set_size_kb(10 * 1024);
    trace_config.set_duration_ms(2000);
    trace_config.set_flush_timeout_ms(10000);

    auto* ds_config = trace_config.add_data_sources()->mutable_config();
    ds_config->set_name("android.heapprofd");
    ds_config->set_target_buffer(0);

    protozero::HeapBuffered<protos::pbzero::HeapprofdConfig> heapprofd_config;
    heapprofd_config->set_sampling_interval_bytes(1);
    heapprofd_config->add_pid(static_cast<uint64_t>(pid));
    heapprofd_config->set_all(false);
    ds_config->set_heapprofd_config_raw(heapprofd_config.SerializeAsString());

    auto helper = Trace(trace_config);
    PrintStats(helper.get());
    ValidateHasSamples(helper.get(), static_cast<uint64_t>(pid));
    ValidateOnlyPID(helper.get(), static_cast<uint64_t>(pid));
    ValidateSampleSizes(helper.get(), static_cast<uint64_t>(pid), kAllocSize);

    PERFETTO_CHECK(kill(pid, SIGKILL) == 0);
    PERFETTO_CHECK(PERFETTO_EINTR(waitpid(pid, nullptr, 0)) == pid);
  }

  void NativeStartup() {
    auto helper = GetHelper(&task_runner);

    TraceConfig trace_config;
    trace_config.add_buffers()->set_size_kb(10 * 1024);
    trace_config.set_duration_ms(5000);
    trace_config.set_flush_timeout_ms(10000);

    auto* ds_config = trace_config.add_data_sources()->mutable_config();
    ds_config->set_name("android.heapprofd");

    protozero::HeapBuffered<protos::pbzero::HeapprofdConfig> heapprofd_config;
    heapprofd_config->set_sampling_interval_bytes(1);
    heapprofd_config->add_process_cmdline("heapprofd_continuous_malloc");
    heapprofd_config->set_all(false);
    ds_config->set_heapprofd_config_raw(heapprofd_config.SerializeAsString());

    helper->StartTracing(trace_config);

    // Wait to guarantee that the process forked below is hooked by the profiler
    // by virtue of the startup check, and not by virtue of being seen as a
    // running process. This sleep is here to prevent that, accidentally, the
    // test gets to the fork()+exec() too soon, before the heap profiling daemon
    // has received the trace config.
    sleep(1);

    // Make sure the forked process does not get reparented to init.
    setsid();
    pid_t pid = fork();
    switch (pid) {
      case -1:
        PERFETTO_FATAL("Failed to fork.");
      case 0: {
        const char* envp[] = {"HEAPPROFD_TESTING_RUN_MALLOC=1", nullptr};
        int null = open("/dev/null", O_RDWR);
        dup2(null, STDIN_FILENO);
        dup2(null, STDOUT_FILENO);
        dup2(null, STDERR_FILENO);
        PERFETTO_CHECK(execle("/proc/self/exe", "heapprofd_continuous_malloc",
                              nullptr, envp) == 0);
        break;
      }
      default:
        break;
    }

    helper->WaitForTracingDisabled(20000);

    helper->ReadData();
    helper->WaitForReadData();

    PERFETTO_CHECK(kill(pid, SIGKILL) == 0);
    PERFETTO_CHECK(PERFETTO_EINTR(waitpid(pid, nullptr, 0)) == pid);

    const auto& packets = helper->trace();
    ASSERT_GT(packets.size(), 0u);
    size_t profile_packets = 0;
    size_t samples = 0;
    uint64_t total_allocated = 0;
    uint64_t total_freed = 0;
    for (const protos::TracePacket& packet : packets) {
      if (packet.has_profile_packet() &&
          packet.profile_packet().process_dumps().size() > 0) {
        const auto& dumps = packet.profile_packet().process_dumps();
        ASSERT_EQ(dumps.size(), 1);
        const protos::ProfilePacket_ProcessHeapSamples& dump = dumps.Get(0);
        EXPECT_EQ(dump.pid(), pid);
        profile_packets++;
        for (const auto& sample : dump.samples()) {
          samples++;
          total_allocated += sample.self_allocated();
          total_freed += sample.self_freed();
        }
      }
    }
    EXPECT_EQ(profile_packets, 1);
    EXPECT_GT(samples, 0);
    EXPECT_GT(total_allocated, 0);
    EXPECT_GT(total_freed, 0);
  }

  void NativeStartupDenormalizedCmdline() {
    auto helper = GetHelper(&task_runner);

    TraceConfig trace_config;
    trace_config.add_buffers()->set_size_kb(10 * 1024);
    trace_config.set_duration_ms(5000);
    trace_config.set_flush_timeout_ms(10000);

    auto* ds_config = trace_config.add_data_sources()->mutable_config();
    ds_config->set_name("android.heapprofd");

    protozero::HeapBuffered<protos::pbzero::HeapprofdConfig> heapprofd_config;
    heapprofd_config->set_sampling_interval_bytes(1);
    heapprofd_config->add_process_cmdline("heapprofd_continuous_malloc@1.2.3");
    heapprofd_config->set_all(false);
    ds_config->set_heapprofd_config_raw(heapprofd_config.SerializeAsString());

    helper->StartTracing(trace_config);

    // Wait to guarantee that the process forked below is hooked by the profiler
    // by virtue of the startup check, and not by virtue of being seen as a
    // running process. This sleep is here to prevent that, accidentally, the
    // test gets to the fork()+exec() too soon, before the heap profiling daemon
    // has received the trace config.
    sleep(1);

    // Make sure the forked process does not get reparented to init.
    setsid();
    pid_t pid = fork();
    switch (pid) {
      case -1:
        PERFETTO_FATAL("Failed to fork.");
      case 0: {
        const char* envp[] = {"HEAPPROFD_TESTING_RUN_MALLOC=1", nullptr};
        int null = open("/dev/null", O_RDWR);
        dup2(null, STDIN_FILENO);
        dup2(null, STDOUT_FILENO);
        dup2(null, STDERR_FILENO);
        PERFETTO_CHECK(execle("/proc/self/exe", "heapprofd_continuous_malloc",
                              nullptr, envp) == 0);
        break;
      }
      default:
        break;
    }

    helper->WaitForTracingDisabled(20000);

    helper->ReadData();
    helper->WaitForReadData();

    PERFETTO_CHECK(kill(pid, SIGKILL) == 0);
    PERFETTO_CHECK(PERFETTO_EINTR(waitpid(pid, nullptr, 0)) == pid);

    const auto& packets = helper->trace();
    ASSERT_GT(packets.size(), 0u);
    size_t profile_packets = 0;
    size_t samples = 0;
    uint64_t total_allocated = 0;
    uint64_t total_freed = 0;
    for (const protos::TracePacket& packet : packets) {
      if (packet.has_profile_packet() &&
          packet.profile_packet().process_dumps().size() > 0) {
        const auto& dumps = packet.profile_packet().process_dumps();
        ASSERT_EQ(dumps.size(), 1);
        const protos::ProfilePacket_ProcessHeapSamples& dump = dumps.Get(0);
        EXPECT_EQ(dump.pid(), pid);
        profile_packets++;
        for (const auto& sample : dump.samples()) {
          samples++;
          total_allocated += sample.self_allocated();
          total_freed += sample.self_freed();
        }
      }
    }
    EXPECT_EQ(profile_packets, 1);
    EXPECT_GT(samples, 0);
    EXPECT_GT(total_allocated, 0);
    EXPECT_GT(total_freed, 0);
  }

  void DiscoverByName() {
    auto helper = GetHelper(&task_runner);

    TraceConfig trace_config;
    trace_config.add_buffers()->set_size_kb(10 * 1024);
    trace_config.set_duration_ms(5000);
    trace_config.set_flush_timeout_ms(10000);

    auto* ds_config = trace_config.add_data_sources()->mutable_config();
    ds_config->set_name("android.heapprofd");

    protozero::HeapBuffered<protos::pbzero::HeapprofdConfig> heapprofd_config;
    heapprofd_config->set_sampling_interval_bytes(1);
    heapprofd_config->add_process_cmdline("heapprofd_continuous_malloc");
    heapprofd_config->set_all(false);
    ds_config->set_heapprofd_config_raw(heapprofd_config.SerializeAsString());

    // Make sure the forked process does not get reparented to init.
    setsid();
    pid_t pid = fork();
    switch (pid) {
      case -1:
        PERFETTO_FATAL("Failed to fork.");
      case 0: {
        const char* envp[] = {"HEAPPROFD_TESTING_RUN_MALLOC=1", nullptr};
        int null = open("/dev/null", O_RDWR);
        dup2(null, STDIN_FILENO);
        dup2(null, STDOUT_FILENO);
        dup2(null, STDERR_FILENO);
        PERFETTO_CHECK(execle("/proc/self/exe", "heapprofd_continuous_malloc",
                              nullptr, envp) == 0);
        break;
      }
      default:
        break;
    }

    // Wait to make sure process is fully initialized, so we do not accidentally
    // match it by the startup logic.
    sleep(1);

    helper->StartTracing(trace_config);
    helper->WaitForTracingDisabled(20000);

    helper->ReadData();
    helper->WaitForReadData();

    PERFETTO_CHECK(kill(pid, SIGKILL) == 0);
    PERFETTO_CHECK(PERFETTO_EINTR(waitpid(pid, nullptr, 0)) == pid);

    const auto& packets = helper->trace();
    ASSERT_GT(packets.size(), 0u);
    size_t profile_packets = 0;
    size_t samples = 0;
    uint64_t total_allocated = 0;
    uint64_t total_freed = 0;
    for (const protos::TracePacket& packet : packets) {
      if (packet.has_profile_packet() &&
          packet.profile_packet().process_dumps().size() > 0) {
        const auto& dumps = packet.profile_packet().process_dumps();
        ASSERT_EQ(dumps.size(), 1);
        const protos::ProfilePacket_ProcessHeapSamples& dump = dumps.Get(0);
        EXPECT_EQ(dump.pid(), pid);
        profile_packets++;
        for (const auto& sample : dump.samples()) {
          samples++;
          total_allocated += sample.self_allocated();
          total_freed += sample.self_freed();
        }
      }
    }
    EXPECT_EQ(profile_packets, 1);
    EXPECT_GT(samples, 0);
    EXPECT_GT(total_allocated, 0);
    EXPECT_GT(total_freed, 0);
  }

  void DiscoverByNameDenormalizedCmdline() {
    auto helper = GetHelper(&task_runner);

    TraceConfig trace_config;
    trace_config.add_buffers()->set_size_kb(10 * 1024);
    trace_config.set_duration_ms(5000);
    trace_config.set_flush_timeout_ms(10000);

    auto* ds_config = trace_config.add_data_sources()->mutable_config();
    ds_config->set_name("android.heapprofd");

    protozero::HeapBuffered<protos::pbzero::HeapprofdConfig> heapprofd_config;
    heapprofd_config->set_sampling_interval_bytes(1);
    heapprofd_config->add_process_cmdline("heapprofd_continuous_malloc@1.2.3");
    heapprofd_config->set_all(false);
    ds_config->set_heapprofd_config_raw(heapprofd_config.SerializeAsString());

    // Make sure the forked process does not get reparented to init.
    setsid();
    pid_t pid = fork();
    switch (pid) {
      case -1:
        PERFETTO_FATAL("Failed to fork.");
      case 0: {
        const char* envp[] = {"HEAPPROFD_TESTING_RUN_MALLOC=1", nullptr};
        int null = open("/dev/null", O_RDWR);
        dup2(null, STDIN_FILENO);
        dup2(null, STDOUT_FILENO);
        dup2(null, STDERR_FILENO);
        PERFETTO_CHECK(execle("/proc/self/exe", "heapprofd_continuous_malloc",
                              nullptr, envp) == 0);
        break;
      }
      default:
        break;
    }

    // Wait to make sure process is fully initialized, so we do not accidentally
    // match it by the startup logic.
    sleep(1);

    helper->StartTracing(trace_config);
    helper->WaitForTracingDisabled(20000);

    helper->ReadData();
    helper->WaitForReadData();

    PERFETTO_CHECK(kill(pid, SIGKILL) == 0);
    PERFETTO_CHECK(PERFETTO_EINTR(waitpid(pid, nullptr, 0)) == pid);

    const auto& packets = helper->trace();
    ASSERT_GT(packets.size(), 0u);
    size_t profile_packets = 0;
    size_t samples = 0;
    uint64_t total_allocated = 0;
    uint64_t total_freed = 0;
    for (const protos::TracePacket& packet : packets) {
      if (packet.has_profile_packet() &&
          packet.profile_packet().process_dumps().size() > 0) {
        const auto& dumps = packet.profile_packet().process_dumps();
        ASSERT_EQ(dumps.size(), 1);
        const protos::ProfilePacket_ProcessHeapSamples& dump = dumps.Get(0);
        EXPECT_EQ(dump.pid(), pid);
        profile_packets++;
        for (const auto& sample : dump.samples()) {
          samples++;
          total_allocated += sample.self_allocated();
          total_freed += sample.self_freed();
        }
      }
    }
    EXPECT_EQ(profile_packets, 1);
    EXPECT_GT(samples, 0);
    EXPECT_GT(total_allocated, 0);
    EXPECT_GT(total_freed, 0);
  }

  void ReInit() {
    constexpr uint64_t kFirstIterationBytes = 5;
    constexpr uint64_t kSecondIterationBytes = 7;

    base::Pipe signal_pipe = base::Pipe::Create(base::Pipe::kBothNonBlock);
    base::Pipe ack_pipe = base::Pipe::Create(base::Pipe::kBothBlock);

    pid_t pid = fork();
    switch (pid) {
      case -1:
        PERFETTO_FATAL("Failed to fork.");
      case 0: {
        uint64_t bytes = kFirstIterationBytes;
        signal_pipe.wr.reset();
        ack_pipe.rd.reset();
        for (;;) {
          AllocateAndFree(bytes);
          char buf[1];
          if (bool(signal_pipe.rd) &&
              read(*signal_pipe.rd, buf, sizeof(buf)) == 0) {
            // make sure the client has noticed that the session has stopped
            AllocateAndFree(bytes);

            bytes = kSecondIterationBytes;
            signal_pipe.rd.reset();
            ack_pipe.wr.reset();
          }
          usleep(10 * kMsToUs);
        }
        PERFETTO_FATAL("Should be unreachable");
      }
      default:
        break;
    }

    signal_pipe.rd.reset();
    ack_pipe.wr.reset();

    TraceConfig trace_config;
    trace_config.add_buffers()->set_size_kb(10 * 1024);
    trace_config.set_duration_ms(2000);
    trace_config.set_flush_timeout_ms(10000);

    auto* ds_config = trace_config.add_data_sources()->mutable_config();
    ds_config->set_name("android.heapprofd");
    ds_config->set_target_buffer(0);

    protozero::HeapBuffered<protos::pbzero::HeapprofdConfig> heapprofd_config;
    heapprofd_config->set_sampling_interval_bytes(1);
    heapprofd_config->add_pid(static_cast<uint64_t>(pid));
    heapprofd_config->set_all(false);
    ds_config->set_heapprofd_config_raw(heapprofd_config.SerializeAsString());

    auto helper = Trace(trace_config);
    PrintStats(helper.get());
    ValidateHasSamples(helper.get(), static_cast<uint64_t>(pid));
    ValidateOnlyPID(helper.get(), static_cast<uint64_t>(pid));
    ValidateSampleSizes(helper.get(), static_cast<uint64_t>(pid),
                        kFirstIterationBytes);

    signal_pipe.wr.reset();
    char buf[1];
    ASSERT_EQ(read(*ack_pipe.rd, buf, sizeof(buf)), 0);
    ack_pipe.rd.reset();

    // A brief sleep to allow the client to notice that the profiling session is
    // to be torn down (as it rejects concurrent sessions).
    usleep(500 * kMsToUs);

    PERFETTO_LOG("HeapprofdEndToEnd::Reinit: Starting second");
    helper = Trace(trace_config);
    PrintStats(helper.get());
    ValidateHasSamples(helper.get(), static_cast<uint64_t>(pid));
    ValidateOnlyPID(helper.get(), static_cast<uint64_t>(pid));
    ValidateSampleSizes(helper.get(), static_cast<uint64_t>(pid),
                        kSecondIterationBytes);

    PERFETTO_CHECK(kill(pid, SIGKILL) == 0);
    PERFETTO_CHECK(PERFETTO_EINTR(waitpid(pid, nullptr, 0)) == pid);
  }

  void ConcurrentSession() {
    constexpr size_t kAllocSize = 1024;

    pid_t pid = ForkContinuousMalloc(kAllocSize);

    TraceConfig trace_config;
    trace_config.add_buffers()->set_size_kb(10 * 1024);
    trace_config.set_duration_ms(5000);
    trace_config.set_flush_timeout_ms(10000);

    auto* ds_config = trace_config.add_data_sources()->mutable_config();
    ds_config->set_name("android.heapprofd");
    ds_config->set_target_buffer(0);

    protozero::HeapBuffered<protos::pbzero::HeapprofdConfig> heapprofd_config;
    heapprofd_config->set_sampling_interval_bytes(1);
    heapprofd_config->add_pid(static_cast<uint64_t>(pid));
    heapprofd_config->set_all(false);
    auto* cont_config = heapprofd_config->set_continuous_dump_config();
    cont_config->set_dump_phase_ms(0);
    cont_config->set_dump_interval_ms(100);
    ds_config->set_heapprofd_config_raw(heapprofd_config.SerializeAsString());

    auto helper = GetHelper(&task_runner);
    helper->StartTracing(trace_config);
    sleep(1);
    auto helper_concurrent = GetHelper(&task_runner);
    helper_concurrent->StartTracing(trace_config);

    helper->WaitForTracingDisabled(20000);
    helper->ReadData();
    helper->WaitForReadData();
    PrintStats(helper.get());
    ValidateHasSamples(helper.get(), static_cast<uint64_t>(pid));
    ValidateOnlyPID(helper.get(), static_cast<uint64_t>(pid));
    ValidateSampleSizes(helper.get(), static_cast<uint64_t>(pid), kAllocSize);
    ValidateRejectedConcurrent(helper_concurrent.get(),
                               static_cast<uint64_t>(pid), false);

    helper_concurrent->WaitForTracingDisabled(20000);
    helper_concurrent->ReadData();
    helper_concurrent->WaitForReadData();
    PrintStats(helper.get());
    ValidateOnlyPID(helper_concurrent.get(), static_cast<uint64_t>(pid));
    ValidateRejectedConcurrent(helper_concurrent.get(),
                               static_cast<uint64_t>(pid), true);

    PERFETTO_CHECK(kill(pid, SIGKILL) == 0);
    PERFETTO_CHECK(PERFETTO_EINTR(waitpid(pid, nullptr, 0)) == pid);
  }

  // TODO(rsavitski): fold exit status assertions into existing tests where
  // possible.
  void NativeProfilingActiveAtProcessExit() {
    constexpr uint64_t kTestAllocSize = 128;
    base::Pipe start_pipe = base::Pipe::Create(base::Pipe::kBothBlock);

    pid_t pid = fork();
    if (pid == 0) {  // child
      start_pipe.rd.reset();
      start_pipe.wr.reset();
      for (int i = 0; i < 200; i++) {
        // malloc and leak, otherwise the free batching will cause us to filter
        // out the allocations (as we don't see the interleaved frees).
        volatile char* x = static_cast<char*>(malloc(kTestAllocSize));
        if (x) {
          x[0] = 'x';
        }
        usleep(10 * kMsToUs);
      }
      exit(0);
    }

    ASSERT_NE(pid, -1) << "Failed to fork.";
    start_pipe.wr.reset();

    // Construct tracing config (without starting profiling).
    auto helper = GetHelper(&task_runner);
    TraceConfig trace_config;
    trace_config.add_buffers()->set_size_kb(10 * 1024);
    trace_config.set_duration_ms(5000);
    trace_config.set_flush_timeout_ms(10000);

    auto* ds_config = trace_config.add_data_sources()->mutable_config();
    ds_config->set_name("android.heapprofd");

    protozero::HeapBuffered<protos::pbzero::HeapprofdConfig> heapprofd_config;
    heapprofd_config->set_sampling_interval_bytes(1);
    heapprofd_config->add_pid(static_cast<uint64_t>(pid));
    ds_config->set_heapprofd_config_raw(heapprofd_config.SerializeAsString());

    // Wait for child to have been scheduled at least once.
    char buf[1] = {};
    ASSERT_EQ(PERFETTO_EINTR(read(*start_pipe.rd, buf, sizeof(buf))), 0);
    start_pipe.rd.reset();

    // Trace until child exits.
    helper->StartTracing(trace_config);

    siginfo_t siginfo = {};
    int wait_ret = PERFETTO_EINTR(
        waitid(P_PID, static_cast<id_t>(pid), &siginfo, WEXITED));
    ASSERT_FALSE(wait_ret) << "Failed to waitid.";

    // Assert that the child exited successfully.
    EXPECT_EQ(siginfo.si_code, CLD_EXITED) << "Child did not exit by itself.";
    EXPECT_EQ(siginfo.si_status, 0) << "Child's exit status not successful.";

    // Assert that we did profile the process.
    helper->FlushAndWait(2000);
    helper->DisableTracing();
    helper->WaitForTracingDisabled(10000);
    helper->ReadData();
    helper->WaitForReadData();

    const auto& packets = helper->trace();
    ASSERT_GT(packets.size(), 0u);
    size_t profile_packets = 0;
    size_t samples = 0;
    uint64_t total_allocated = 0;
    for (const protos::TracePacket& packet : packets) {
      if (packet.has_profile_packet() &&
          packet.profile_packet().process_dumps().size() > 0) {
        const auto& dumps = packet.profile_packet().process_dumps();
        ASSERT_EQ(dumps.size(), 1);
        const protos::ProfilePacket_ProcessHeapSamples& dump = dumps.Get(0);
        EXPECT_EQ(dump.pid(), pid);
        profile_packets++;
        for (const auto& sample : dump.samples()) {
          samples++;
          total_allocated += sample.self_allocated();
        }
      }
    }
    EXPECT_EQ(profile_packets, 1);
    EXPECT_GT(samples, 0);
    EXPECT_GT(total_allocated, 0);
  }
};

// TODO(b/118428762): look into unwinding issues on x86.
#if defined(__i386__) || defined(__x86_64__)
#define MAYBE_SKIP(x) DISABLED_##x
#else
#define MAYBE_SKIP(x) x
#endif

TEST_F(HeapprofdEndToEnd, MAYBE_SKIP(Smoke_Central)) {
  auto prop = DisableFork();
  ASSERT_EQ(ReadProperty(kHeapprofdModeProperty, ""), "");
  Smoke();
}

TEST_F(HeapprofdEndToEnd, MAYBE_SKIP(TwoProcesses_Fork)) {
  // RAII handle that resets to central mode when out of scope.
  auto prop = EnableFork();
  ASSERT_EQ(ReadProperty(kHeapprofdModeProperty, ""), "fork");
  TwoProcesses();
}

TEST_F(HeapprofdEndToEnd, MAYBE_SKIP(TwoProcesses_Central)) {
  auto prop = DisableFork();
  ASSERT_EQ(ReadProperty(kHeapprofdModeProperty, ""), "");
  TwoProcesses();
}

TEST_F(HeapprofdEndToEnd, MAYBE_SKIP(Smoke_Fork)) {
  // RAII handle that resets to central mode when out of scope.
  auto prop = EnableFork();
  ASSERT_EQ(ReadProperty(kHeapprofdModeProperty, ""), "fork");
  Smoke();
}

TEST_F(HeapprofdEndToEnd, MAYBE_SKIP(FinalFlush_Central)) {
  auto prop = DisableFork();
  ASSERT_EQ(ReadProperty(kHeapprofdModeProperty, ""), "");
  FinalFlush();
}

TEST_F(HeapprofdEndToEnd, MAYBE_SKIP(FinalFlush_Fork)) {
  // RAII handle that resets to central mode when out of scope.
  auto prop = EnableFork();
  ASSERT_EQ(ReadProperty(kHeapprofdModeProperty, ""), "fork");
  FinalFlush();
}

TEST_F(HeapprofdEndToEnd, MAYBE_SKIP(NativeStartup_Central)) {
  auto prop = DisableFork();
  ASSERT_EQ(ReadProperty(kHeapprofdModeProperty, ""), "");
  NativeStartup();
}

TEST_F(HeapprofdEndToEnd, MAYBE_SKIP(NativeStartup_Fork)) {
  // RAII handle that resets to central mode when out of scope.
  auto prop = EnableFork();
  ASSERT_EQ(ReadProperty(kHeapprofdModeProperty, ""), "fork");
  NativeStartup();
}

TEST_F(HeapprofdEndToEnd,
       MAYBE_SKIP(NativeStartupDenormalizedCmdline_Central)) {
  auto prop = DisableFork();
  ASSERT_EQ(ReadProperty(kHeapprofdModeProperty, ""), "");
  NativeStartupDenormalizedCmdline();
}

TEST_F(HeapprofdEndToEnd, MAYBE_SKIP(NativeStartupDenormalizedCmdline_Fork)) {
  // RAII handle that resets to central mode when out of scope.
  auto prop = EnableFork();
  ASSERT_EQ(ReadProperty(kHeapprofdModeProperty, ""), "fork");
  NativeStartupDenormalizedCmdline();
}

TEST_F(HeapprofdEndToEnd, MAYBE_SKIP(DiscoverByName_Central)) {
  auto prop = DisableFork();
  ASSERT_EQ(ReadProperty(kHeapprofdModeProperty, ""), "");
  DiscoverByName();
}

TEST_F(HeapprofdEndToEnd, MAYBE_SKIP(DiscoverByName_Fork)) {
  // RAII handle that resets to central mode when out of scope.
  auto prop = EnableFork();
  ASSERT_EQ(ReadProperty(kHeapprofdModeProperty, ""), "fork");
  DiscoverByName();
}

TEST_F(HeapprofdEndToEnd,
       MAYBE_SKIP(DiscoverByNameDenormalizedCmdline_Central)) {
  auto prop = DisableFork();
  ASSERT_EQ(ReadProperty(kHeapprofdModeProperty, ""), "");
  DiscoverByNameDenormalizedCmdline();
}

TEST_F(HeapprofdEndToEnd, MAYBE_SKIP(DiscoverByNameDenormalizedCmdline_Fork)) {
  auto prop = DisableFork();
  ASSERT_EQ(ReadProperty(kHeapprofdModeProperty, ""), "");
  DiscoverByNameDenormalizedCmdline();
}

TEST_F(HeapprofdEndToEnd, MAYBE_SKIP(ReInit_Central)) {
  auto prop = DisableFork();
  ASSERT_EQ(ReadProperty(kHeapprofdModeProperty, ""), "");
  ReInit();
}

TEST_F(HeapprofdEndToEnd, MAYBE_SKIP(ReInit_Fork)) {
  // RAII handle that resets to central mode when out of scope.
  auto prop = EnableFork();
  ASSERT_EQ(ReadProperty(kHeapprofdModeProperty, ""), "fork");
  ReInit();
}

TEST_F(HeapprofdEndToEnd, MAYBE_SKIP(ConcurrentSession_Central)) {
  auto prop = DisableFork();
  ASSERT_EQ(ReadProperty(kHeapprofdModeProperty, ""), "");
  ConcurrentSession();
}

TEST_F(HeapprofdEndToEnd, MAYBE_SKIP(ConcurrentSession_Fork)) {
  // RAII handle that resets to central mode when out of scope.
  auto prop = EnableFork();
  ASSERT_EQ(ReadProperty(kHeapprofdModeProperty, ""), "fork");
  ConcurrentSession();
}

TEST_F(HeapprofdEndToEnd,
       MAYBE_SKIP(NativeProfilingActiveAtProcessExit_Central)) {
  auto prop = DisableFork();
  ASSERT_EQ(ReadProperty(kHeapprofdModeProperty, ""), "");
  NativeProfilingActiveAtProcessExit();
}

TEST_F(HeapprofdEndToEnd, MAYBE_SKIP(NativeProfilingActiveAtProcessExit_Fork)) {
  // RAII handle that resets to central mode when out of scope.
  auto prop = EnableFork();
  ASSERT_EQ(ReadProperty(kHeapprofdModeProperty, ""), "fork");
  NativeProfilingActiveAtProcessExit();
}

}  // namespace
}  // namespace profiling
}  // namespace perfetto
