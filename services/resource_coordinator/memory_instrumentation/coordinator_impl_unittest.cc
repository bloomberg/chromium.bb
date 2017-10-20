// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/coordinator_impl.h"

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/trace_event_analyzer.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/trace_buffer.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/resource_coordinator/memory_instrumentation/process_map.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

ACTION_P(RunClosure, closure) {
  closure.Run();
}

using ::testing::_;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Ne;
using ::testing::NotNull;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Contains;
using ::testing::Property;
using ::testing::Pointee;
using ::testing::Field;
using ::testing::Eq;
using ::testing::AllOf;

using RequestGlobalMemoryDumpCallback =
    memory_instrumentation::CoordinatorImpl::RequestGlobalMemoryDumpCallback;
using RequestGlobalMemoryDumpAndAppendToTraceCallback = memory_instrumentation::
    CoordinatorImpl::RequestGlobalMemoryDumpAndAppendToTraceCallback;
using GetVmRegionsForHeapProfilerCallback = memory_instrumentation::
    CoordinatorImpl::GetVmRegionsForHeapProfilerCallback;
using memory_instrumentation::mojom::GlobalMemoryDumpPtr;
using memory_instrumentation::mojom::GlobalMemoryDump;
using base::trace_event::MemoryAllocatorDump;
using base::trace_event::MemoryDumpArgs;
using base::trace_event::MemoryDumpLevelOfDetail;
using base::trace_event::MemoryDumpManager;
using base::trace_event::MemoryDumpRequestArgs;
using base::trace_event::MemoryDumpType;
using base::trace_event::ProcessMemoryDump;
using base::trace_event::TraceConfig;
using base::trace_event::TraceLog;
using base::trace_event::TraceResultBuffer;

namespace memory_instrumentation {

namespace {

std::unique_ptr<trace_analyzer::TraceAnalyzer> GetDeserializedTrace() {
  // Flush the trace into JSON.
  TraceResultBuffer buffer;
  TraceResultBuffer::SimpleOutput trace_output;
  buffer.SetOutputCallback(trace_output.GetCallback());
  base::RunLoop run_loop;
  buffer.Start();
  auto on_trace_data_collected =
      [](base::Closure quit_closure, TraceResultBuffer* buffer,
         const scoped_refptr<base::RefCountedString>& json,
         bool has_more_events) {
        buffer->AddFragment(json->data());
        if (!has_more_events)
          quit_closure.Run();
      };

  TraceLog::GetInstance()->Flush(Bind(on_trace_data_collected,
                                      run_loop.QuitClosure(),
                                      base::Unretained(&buffer)));
  run_loop.Run();
  buffer.Finish();

  // Analyze the JSON.
  return base::WrapUnique(
      trace_analyzer::TraceAnalyzer::Create(trace_output.json_output));
}

}  // namespace

class FakeCoordinatorImpl : public CoordinatorImpl {
 public:
  FakeCoordinatorImpl() : CoordinatorImpl(nullptr) {}
  ~FakeCoordinatorImpl() override {}

  MOCK_CONST_METHOD0(GetClientIdentityForCurrentRequest,
                     service_manager::Identity());
  MOCK_CONST_METHOD1(GetProcessIdForClientIdentity,
                     base::ProcessId(service_manager::Identity));
};

class CoordinatorImplTest : public testing::Test {
 public:
  CoordinatorImplTest() {}
  void SetUp() override {
    coordinator_.reset(new NiceMock<FakeCoordinatorImpl>);
  }

  void TearDown() override { coordinator_.reset(); }

  void RegisterClientProcess(mojom::ClientProcessPtr client_process,
                             base::ProcessId pid,
                             mojom::ProcessType process_type) {
    auto identity = service_manager::Identity(std::to_string(pid));

    ON_CALL(*coordinator_, GetClientIdentityForCurrentRequest())
        .WillByDefault(Return(identity));

    ON_CALL(*coordinator_, GetProcessIdForClientIdentity(identity))
        .WillByDefault(Return(pid));

    coordinator_->RegisterClientProcess(std::move(client_process),
                                        process_type);
  }

  void RequestGlobalMemoryDump(MemoryDumpRequestArgs args,
                               RequestGlobalMemoryDumpCallback callback) {
    coordinator_->RequestGlobalMemoryDump(args, callback);
  }

  void RequestGlobalMemoryDumpAndAppendToTrace(
      MemoryDumpRequestArgs args,
      RequestGlobalMemoryDumpAndAppendToTraceCallback callback) {
    coordinator_->RequestGlobalMemoryDumpAndAppendToTrace(args, callback);
  }

  void GetVmRegionsForHeapProfiler(
      GetVmRegionsForHeapProfilerCallback callback) {
    coordinator_->GetVmRegionsForHeapProfiler(callback);
  }

 private:
  std::unique_ptr<NiceMock<FakeCoordinatorImpl>> coordinator_;
  base::MessageLoop message_loop_;
};

class MockClientProcess : public mojom::ClientProcess {
 public:
  MockClientProcess(CoordinatorImplTest* test_coordinator)
      : MockClientProcess(test_coordinator,
                          base::GetCurrentProcId(),
                          mojom::ProcessType::OTHER) {}

  MockClientProcess(CoordinatorImplTest* test_coordinator,
                    base::ProcessId pid,
                    mojom::ProcessType process_type)
      : binding_(this) {
    // Register to the coordinator.
    mojom::ClientProcessPtr client_process;
    binding_.Bind(mojo::MakeRequest(&client_process));
    test_coordinator->RegisterClientProcess(std::move(client_process), pid,
                                            process_type);

    ON_CALL(*this, RequestChromeMemoryDump(_, _))
        .WillByDefault(
            Invoke([](const MemoryDumpRequestArgs& args,
                      const RequestChromeMemoryDumpCallback& callback) {
              MemoryDumpArgs dump_args{MemoryDumpLevelOfDetail::DETAILED};
              auto pmd =
                  std::make_unique<ProcessMemoryDump>(nullptr, dump_args);
              auto* mad = pmd->CreateAllocatorDump("malloc");
              mad->AddScalar(MemoryAllocatorDump::kNameSize,
                             MemoryAllocatorDump::kUnitsBytes, 1024);

              callback.Run(true, args.dump_guid, std::move(pmd));
            }));

    ON_CALL(*this, RequestOSMemoryDump(_, _, _))
        .WillByDefault(Invoke([](bool want_mmaps,
                                 const std::vector<base::ProcessId> pids,
                                 const RequestOSMemoryDumpCallback& callback) {
          std::unordered_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
          callback.Run(true, std::move(results));
        }));
  }

  ~MockClientProcess() override {}

  MOCK_METHOD2(RequestChromeMemoryDump,
               void(const MemoryDumpRequestArgs& args,
                    const RequestChromeMemoryDumpCallback& callback));

  MOCK_METHOD3(RequestOSMemoryDump,
               void(bool want_mmaps,
                    const std::vector<base::ProcessId>& args,
                    const RequestOSMemoryDumpCallback& callback));

  MOCK_METHOD2(EnableHeapProfiling,
               void(base::trace_event::HeapProfilingMode mode,
                    const EnableHeapProfilingCallback& callback));

 private:
  mojo::Binding<mojom::ClientProcess> binding_;
};

class MockGlobalMemoryDumpCallback {
 public:
  MockGlobalMemoryDumpCallback() = default;
  MOCK_METHOD2(OnCall, void(bool, GlobalMemoryDump*));

  void Run(bool success, GlobalMemoryDumpPtr ptr) {
    OnCall(success, ptr.get());
  }

  RequestGlobalMemoryDumpCallback Get() {
    return base::Bind(&MockGlobalMemoryDumpCallback::Run,
                      base::Unretained(this));
  }
};

class MockGlobalMemoryDumpAndAppendToTraceCallback {
 public:
  MockGlobalMemoryDumpAndAppendToTraceCallback() = default;
  MOCK_METHOD2(OnCall, void(bool, uint64_t));

  void Run(bool success, uint64_t dump_guid) { OnCall(success, dump_guid); }

  RequestGlobalMemoryDumpAndAppendToTraceCallback Get() {
    return base::Bind(&MockGlobalMemoryDumpAndAppendToTraceCallback::Run,
                      base::Unretained(this));
  }
};

class MockGetVmRegionsForHeapProfilerCallback {
 public:
  MockGetVmRegionsForHeapProfilerCallback() = default;
  MOCK_METHOD2(OnCall, void(bool, GlobalMemoryDump*));

  void Run(bool success, GlobalMemoryDumpPtr ptr) {
    OnCall(success, ptr.get());
  }

  GetVmRegionsForHeapProfilerCallback Get() {
    return base::Bind(&MockGetVmRegionsForHeapProfilerCallback::Run,
                      base::Unretained(this));
  }
};

uint64_t GetFakeAddrForVmRegion(int pid, int region_index) {
  return 0x100000ul * pid * (region_index + 1);
}

uint64_t GetFakeSizeForVmRegion(int pid, int region_index) {
  return 4096 * pid * (region_index + 1);
}

mojom::RawOSMemDumpPtr FillRawOSDump(int pid) {
  mojom::RawOSMemDumpPtr raw_os_dump = mojom::RawOSMemDump::New();
  raw_os_dump->platform_private_footprint =
      mojom::PlatformPrivateFootprint::New();
  raw_os_dump->resident_set_kb = pid;
  for (int i = 0; i < 3; i++) {
    mojom::VmRegionPtr vm_region = mojom::VmRegion::New();
    vm_region->start_address = GetFakeAddrForVmRegion(pid, i);
    vm_region->size_in_bytes = GetFakeSizeForVmRegion(pid, i);
    raw_os_dump->memory_maps.push_back(std::move(vm_region));
  }
  return raw_os_dump;
};

// Tests that the global dump is acked even in absence of clients.
TEST_F(CoordinatorImplTest, NoClients) {
  base::trace_event::MemoryDumpRequestArgs args = {
      0, base::trace_event::MemoryDumpType::SUMMARY_ONLY,
      MemoryDumpLevelOfDetail::DETAILED};

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(callback, OnCall(true, NotNull()));
  RequestGlobalMemoryDump(args, callback.Get());
}

// Nominal behavior: several clients contributing to the global dump.
TEST_F(CoordinatorImplTest, SeveralClients) {
  base::RunLoop run_loop;

  NiceMock<MockClientProcess> client_process_1(this, 1,
                                               mojom::ProcessType::BROWSER);
  NiceMock<MockClientProcess> client_process_2(this);

  EXPECT_CALL(client_process_1, RequestChromeMemoryDump(_, _)).Times(1);
  EXPECT_CALL(client_process_2, RequestChromeMemoryDump(_, _)).Times(1);

  base::trace_event::MemoryDumpRequestArgs args = {
      0, base::trace_event::MemoryDumpType::SUMMARY_ONLY,
      MemoryDumpLevelOfDetail::DETAILED};

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(callback, OnCall(true, NotNull()))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  RequestGlobalMemoryDump(args, callback.Get());
  run_loop.Run();
}

TEST_F(CoordinatorImplTest, MissingChromeDump) {
  base::RunLoop run_loop;

  base::trace_event::MemoryDumpRequestArgs args = {
      0, base::trace_event::MemoryDumpType::SUMMARY_ONLY,
      MemoryDumpLevelOfDetail::DETAILED};

  NiceMock<MockClientProcess> client_process(this, 1,
                                             mojom::ProcessType::BROWSER);

  EXPECT_CALL(client_process, RequestChromeMemoryDump(_, _))
      .WillOnce(
          Invoke([](const MemoryDumpRequestArgs& args,
                    const MockClientProcess::RequestChromeMemoryDumpCallback&
                        callback) {
            MemoryDumpArgs dump_args{MemoryDumpLevelOfDetail::DETAILED};
            auto pmd = std::make_unique<ProcessMemoryDump>(nullptr, dump_args);
            callback.Run(true, args.dump_guid, std::move(pmd));
          }));

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(
      callback,
      OnCall(true, Pointee(Field(&mojom::GlobalMemoryDump::process_dumps,
                                 IsEmpty()))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  RequestGlobalMemoryDump(args, callback.Get());
  run_loop.Run();
}

TEST_F(CoordinatorImplTest, MissingOsDump) {
  base::RunLoop run_loop;

  base::trace_event::MemoryDumpRequestArgs args = {
      0, base::trace_event::MemoryDumpType::SUMMARY_ONLY,
      MemoryDumpLevelOfDetail::DETAILED};

  NiceMock<MockClientProcess> client_process(this, 1,
                                             mojom::ProcessType::BROWSER);

  EXPECT_CALL(client_process, RequestOSMemoryDump(_, _, _))
      .WillOnce(Invoke(
          [](bool want_mmaps, const std::vector<base::ProcessId>& pids,
             const MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            std::unordered_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            callback.Run(true, std::move(results));
          }));

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(
      callback,
      OnCall(true, Pointee(Field(&mojom::GlobalMemoryDump::process_dumps,
                                 IsEmpty()))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  RequestGlobalMemoryDump(args, callback.Get());
  run_loop.Run();
}

// Tests that a global dump is completed even if a client disconnects (e.g. due
// to a crash) while a global dump is happening.
TEST_F(CoordinatorImplTest, ClientCrashDuringGlobalDump) {
  base::RunLoop run_loop;

  base::trace_event::MemoryDumpRequestArgs args = {
      0, base::trace_event::MemoryDumpType::EXPLICITLY_TRIGGERED,
      MemoryDumpLevelOfDetail::DETAILED};

  auto client_process_1 = base::MakeUnique<NiceMock<MockClientProcess>>(
      this, 1, mojom::ProcessType::BROWSER);
  auto client_process_2 = base::MakeUnique<NiceMock<MockClientProcess>>(this);

  // One of the client processes dies after a global dump is requested and
  // before it receives the corresponding process dump request. The coordinator
  // should detect that one of its clients is disconnected and claim the global
  // dump attempt has failed.

  // Whichever client is called first destroys the other client.
  ON_CALL(*client_process_1, RequestChromeMemoryDump(_, _))
      .WillByDefault(
          Invoke([&client_process_2](
                     const MemoryDumpRequestArgs& args,
                     const MockClientProcess::RequestChromeMemoryDumpCallback&
                         callback) {
            client_process_2.reset();
            callback.Run(true, args.dump_guid, nullptr);
          }));
  ON_CALL(*client_process_2, RequestChromeMemoryDump(_, _))
      .WillByDefault(
          Invoke([&client_process_1](
                     const MemoryDumpRequestArgs& args,
                     const MockClientProcess::RequestChromeMemoryDumpCallback&
                         callback) {
            client_process_1.reset();
            callback.Run(true, args.dump_guid, nullptr);
          }));

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(callback, OnCall(false, NotNull()))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  RequestGlobalMemoryDump(args, callback.Get());
  run_loop.Run();
}

// Like ClientCrashDuringGlobalDump but covers the case of having only one
// client. Regression testing for crbug.com/742265.
TEST_F(CoordinatorImplTest, SingleClientCrashDuringGlobalDump) {
  base::RunLoop run_loop;

  base::trace_event::MemoryDumpRequestArgs args = {
      0, base::trace_event::MemoryDumpType::EXPLICITLY_TRIGGERED,
      MemoryDumpLevelOfDetail::DETAILED};

  auto client_process = base::MakeUnique<NiceMock<MockClientProcess>>(
      this, 1, mojom::ProcessType::BROWSER);

  ON_CALL(*client_process, RequestChromeMemoryDump(_, _))
      .WillByDefault(
          Invoke([&client_process](
                     const MemoryDumpRequestArgs& args,
                     const MockClientProcess::RequestChromeMemoryDumpCallback&
                         callback) {
            // The dtor here will cause mojo to post an UnregisterClient call to
            // the coordinator.
            client_process.reset();
            callback.Run(true, args.dump_guid, nullptr);
          }));

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(callback, OnCall(false, NotNull()))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  RequestGlobalMemoryDump(args, callback.Get());
  run_loop.Run();
}

TEST_F(CoordinatorImplTest, GlobalMemoryDumpStruct) {
  base::RunLoop run_loop;

  MockClientProcess browser_client(this, 1, mojom::ProcessType::BROWSER);
  MockClientProcess renderer_client(this, 2, mojom::ProcessType::RENDERER);

  EXPECT_CALL(browser_client, RequestChromeMemoryDump(_, _))
      .WillOnce(Invoke([](const MemoryDumpRequestArgs& args,
                          const MockClientProcess::
                              RequestChromeMemoryDumpCallback& callback) {
        MemoryDumpArgs dump_args{MemoryDumpLevelOfDetail::DETAILED};
        auto pmd = std::make_unique<ProcessMemoryDump>(nullptr, dump_args);
        auto* size = MemoryAllocatorDump::kNameSize;
        auto* bytes = MemoryAllocatorDump::kUnitsBytes;
        const uint32_t kB = 1024;

        pmd->CreateAllocatorDump("malloc")->AddScalar(size, bytes, 1 * kB);
        pmd->CreateAllocatorDump("malloc/ignored")
            ->AddScalar(size, bytes, 99 * kB);

        pmd->CreateAllocatorDump("blink_gc")->AddScalar(size, bytes, 2 * kB);
        pmd->CreateAllocatorDump("blink_gc/ignored")
            ->AddScalar(size, bytes, 99 * kB);

        pmd->CreateAllocatorDump("v8/foo")->AddScalar(size, bytes, 1 * kB);
        pmd->CreateAllocatorDump("v8/bar")->AddScalar(size, bytes, 2 * kB);
        pmd->CreateAllocatorDump("v8")->AddScalar(size, bytes, 99 * kB);

        // All the 99 KB values here are expected to be ignored.
        pmd->CreateAllocatorDump("partition_alloc")
            ->AddScalar(size, bytes, 99 * kB);
        pmd->CreateAllocatorDump("partition_alloc/allocated_objects")
            ->AddScalar(size, bytes, 99 * kB);
        pmd->CreateAllocatorDump("partition_alloc/allocated_objects/ignored")
            ->AddScalar(size, bytes, 99 * kB);
        pmd->CreateAllocatorDump("partition_alloc/partitions")
            ->AddScalar(size, bytes, 99 * kB);
        pmd->CreateAllocatorDump("partition_alloc/partitions/not_ignored_1")
            ->AddScalar(size, bytes, 2 * kB);
        pmd->CreateAllocatorDump("partition_alloc/partitions/not_ignored_2")
            ->AddScalar(size, bytes, 2 * kB);

        callback.Run(true, args.dump_guid, std::move(pmd));
      }));
  EXPECT_CALL(renderer_client, RequestChromeMemoryDump(_, _))
      .WillOnce(
          Invoke([](const MemoryDumpRequestArgs& args,
                    const MockClientProcess::RequestChromeMemoryDumpCallback&
                        callback) {
            MemoryDumpArgs dump_args{MemoryDumpLevelOfDetail::DETAILED};
            auto pmd = std::make_unique<ProcessMemoryDump>(nullptr, dump_args);
            auto* mad = pmd->CreateAllocatorDump("malloc");
            mad->AddScalar(MemoryAllocatorDump::kNameSize,
                           MemoryAllocatorDump::kUnitsBytes, 1024 * 2);
            callback.Run(true, args.dump_guid, std::move(pmd));
          }));
#if defined(OS_LINUX)
  EXPECT_CALL(browser_client,
              RequestOSMemoryDump(_, AllOf(Contains(1), Contains(2)), _))
      .WillOnce(Invoke(
          [](bool want_mmaps, const std::vector<base::ProcessId>& pids,
             const MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            std::unordered_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[1] = mojom::RawOSMemDump::New();
            results[1]->resident_set_kb = 1;
            results[1]->platform_private_footprint =
                mojom::PlatformPrivateFootprint::New();
            results[2] = mojom::RawOSMemDump::New();
            results[2]->platform_private_footprint =
                mojom::PlatformPrivateFootprint::New();
            results[2]->resident_set_kb = 2;
            callback.Run(true, std::move(results));
          }));
  EXPECT_CALL(renderer_client, RequestOSMemoryDump(_, _, _)).Times(0);
#else
  EXPECT_CALL(browser_client, RequestOSMemoryDump(_, Contains(0), _))
      .WillOnce(Invoke(
          [](bool want_mmaps, const std::vector<base::ProcessId>& pids,
             const MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            std::unordered_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[0] = mojom::RawOSMemDump::New();
            results[0]->platform_private_footprint =
                mojom::PlatformPrivateFootprint::New();
            results[0]->resident_set_kb = 1;
            callback.Run(true, std::move(results));
          }));
  EXPECT_CALL(renderer_client, RequestOSMemoryDump(_, Contains(0), _))
      .WillOnce(Invoke(
          [](bool want_mmaps, const std::vector<base::ProcessId>& pids,
             const MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            std::unordered_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[0] = mojom::RawOSMemDump::New();
            results[0]->platform_private_footprint =
                mojom::PlatformPrivateFootprint::New();
            results[0]->resident_set_kb = 2;
            callback.Run(true, std::move(results));
          }));
#endif  // defined(OS_LINUX)

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(callback, OnCall(true, NotNull()))
      .WillOnce(Invoke([&run_loop](bool success,
                                   GlobalMemoryDump* global_dump) {
        EXPECT_TRUE(success);
        EXPECT_EQ(2U, global_dump->process_dumps.size());
        mojom::ProcessMemoryDumpPtr browser_dump = nullptr;
        mojom::ProcessMemoryDumpPtr renderer_dump = nullptr;
        for (mojom::ProcessMemoryDumpPtr& dump : global_dump->process_dumps) {
          if (dump->process_type == mojom::ProcessType::BROWSER) {
            browser_dump = std::move(dump);
          } else if (dump->process_type == mojom::ProcessType::RENDERER) {
            renderer_dump = std::move(dump);
          }
        }
        // For malloc we only count the root "malloc" not children "malloc/*".
        EXPECT_EQ(1u, browser_dump->chrome_dump->malloc_total_kb);

        // For blink_gc we only count the root "blink_gc" not children
        // "blink_gc/*".
        EXPECT_EQ(2u, browser_dump->chrome_dump->blink_gc_total_kb);

        // For v8 we count the children ("v8/*") as the root total is not given.
        EXPECT_EQ(3u, browser_dump->chrome_dump->v8_total_kb);

        // partition_alloc has partition_alloc/allocated_objects/* which is a
        // subset of partition_alloc/partitions/* so we only count the latter.
        EXPECT_EQ(4u, browser_dump->chrome_dump->partition_alloc_total_kb);

        EXPECT_EQ(browser_dump->os_dump->resident_set_kb, 1u);
        EXPECT_EQ(renderer_dump->os_dump->resident_set_kb, 2u);
        EXPECT_EQ(browser_dump->chrome_dump->malloc_total_kb, 1u);
        EXPECT_EQ(renderer_dump->chrome_dump->malloc_total_kb, 2u);
        run_loop.Quit();
      }));

  base::trace_event::MemoryDumpRequestArgs args = {
      0, base::trace_event::MemoryDumpType::SUMMARY_ONLY,
      MemoryDumpLevelOfDetail::BACKGROUND};
  RequestGlobalMemoryDump(args, callback.Get());
  run_loop.Run();
}

TEST_F(CoordinatorImplTest, VmRegionsForHeapProfiler) {
  base::RunLoop run_loop;
  // Not using a constexpr base::ProcessId because std:unordered_map<>
  // and friends makes it too easy to accidentally odr-use this variable
  // causing all sorts of compiler-toolchain divergent fun when trying
  // to decide of the lambda capture is necessary.
  enum {
    kBrowserPid = 1,
    kRendererPid = 2,
  };
  MockClientProcess browser_client(this, kBrowserPid,
                                   mojom::ProcessType::BROWSER);
  MockClientProcess renderer_client(this, kRendererPid,
                                    mojom::ProcessType::RENDERER);

// This ifdef is here to match the sandboxing behavior of the client.
// On Linux, all memory dumps come from the browser client. On all other
// platforms, they are expected to come from each individual client.
#if defined(OS_LINUX)
  EXPECT_CALL(
      browser_client,
      RequestOSMemoryDump(
          true, AllOf(Contains(kBrowserPid), Contains(kRendererPid)), _))
      .WillOnce(Invoke(
          [](bool want_mmaps, const std::vector<base::ProcessId>& pids,
             const MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            std::unordered_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[kBrowserPid] = FillRawOSDump(kBrowserPid);
            results[kRendererPid] = FillRawOSDump(kRendererPid);
            callback.Run(true, std::move(results));
          }));
  EXPECT_CALL(renderer_client, RequestOSMemoryDump(_, _, _)).Times(0);
#else
  EXPECT_CALL(browser_client, RequestOSMemoryDump(_, Contains(0), _))
      .WillOnce(Invoke(
          [](bool want_mmaps, const std::vector<base::ProcessId>& pids,
             const MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            std::unordered_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[0] = FillRawOSDump(kBrowserPid);
            callback.Run(true, std::move(results));
          }));
  EXPECT_CALL(renderer_client, RequestOSMemoryDump(_, Contains(0), _))
      .WillOnce(Invoke(
          [](bool want_mmaps, const std::vector<base::ProcessId>& pids,
             const MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            std::unordered_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
            results[0] = FillRawOSDump(kRendererPid);
            callback.Run(true, std::move(results));
          }));
#endif  // defined(OS_LINUX)

  MockGetVmRegionsForHeapProfilerCallback callback;
  EXPECT_CALL(callback, OnCall(true, NotNull()))
      .WillOnce(Invoke([&run_loop](bool success,
                                   GlobalMemoryDump* global_dump) {
        ASSERT_EQ(2U, global_dump->process_dumps.size());
        mojom::ProcessMemoryDumpPtr browser_dump = nullptr;
        mojom::ProcessMemoryDumpPtr renderer_dump = nullptr;
        for (mojom::ProcessMemoryDumpPtr& dump : global_dump->process_dumps) {
          if (dump->process_type == mojom::ProcessType::BROWSER) {
            browser_dump = std::move(dump);
            ASSERT_EQ(kBrowserPid, browser_dump->pid);
          } else if (dump->process_type == mojom::ProcessType::RENDERER) {
            renderer_dump = std::move(dump);
            ASSERT_EQ(kRendererPid, renderer_dump->pid);
          }
        }
        const std::vector<mojom::VmRegionPtr>& browser_mmaps =
            browser_dump->os_dump->memory_maps_for_heap_profiler;
        ASSERT_EQ(3u, browser_mmaps.size());
        for (int i = 0; i < 3; i++) {
          EXPECT_EQ(GetFakeAddrForVmRegion(browser_dump->pid, i),
                    browser_mmaps[i]->start_address);
        }

        const std::vector<mojom::VmRegionPtr>& renderer_mmaps =
            renderer_dump->os_dump->memory_maps_for_heap_profiler;
        ASSERT_EQ(3u, renderer_mmaps.size());
        for (int i = 0; i < 3; i++) {
          EXPECT_EQ(GetFakeAddrForVmRegion(renderer_dump->pid, i),
                    renderer_mmaps[i]->start_address);
        }
        run_loop.Quit();
      }));

  GetVmRegionsForHeapProfiler(callback.Get());
  run_loop.Run();
}

// RequestGlobalMemoryDump, as opposite to RequestGlobalMemoryDumpAndAddToTrace,
// shouldn't add anything into the trace
TEST_F(CoordinatorImplTest, DumpsArentAddedToTraceUnlessRequested) {
  using trace_analyzer::Query;

  base::RunLoop run_loop;

  base::trace_event::MemoryDumpRequestArgs args = {
      0, base::trace_event::MemoryDumpType::EXPLICITLY_TRIGGERED,
      MemoryDumpLevelOfDetail::DETAILED};

  NiceMock<MockClientProcess> client_process(this, 1,
                                             mojom::ProcessType::BROWSER);

  EXPECT_CALL(client_process, RequestChromeMemoryDump(_, _))
      .WillOnce(
          Invoke([](const MemoryDumpRequestArgs& args,
                    const MockClientProcess::RequestChromeMemoryDumpCallback&
                        callback) {
            MemoryDumpArgs dump_args{MemoryDumpLevelOfDetail::DETAILED};
            auto pmd = std::make_unique<ProcessMemoryDump>(nullptr, dump_args);
            callback.Run(true, args.dump_guid, std::move(pmd));
          }));

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(
      callback,
      OnCall(true, Pointee(Field(&mojom::GlobalMemoryDump::process_dumps,
                                 IsEmpty()))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  TraceLog::GetInstance()->SetEnabled(
      TraceConfig(MemoryDumpManager::kTraceCategory, ""),
      TraceLog::RECORDING_MODE);
  RequestGlobalMemoryDump(args, callback.Get());
  run_loop.Run();
  TraceLog::GetInstance()->SetDisabled();

  std::unique_ptr<trace_analyzer::TraceAnalyzer> analyzer =
      GetDeserializedTrace();
  trace_analyzer::TraceEventVector events;
  analyzer->FindEvents(Query::EventPhaseIs(TRACE_EVENT_PHASE_MEMORY_DUMP),
                       &events);

  ASSERT_EQ(0u, events.size());
}

TEST_F(CoordinatorImplTest, DumpsAreAddedToTraceWhenRequested) {
  using trace_analyzer::Query;

  base::RunLoop run_loop;

  base::trace_event::MemoryDumpRequestArgs args = {
      0, base::trace_event::MemoryDumpType::EXPLICITLY_TRIGGERED,
      MemoryDumpLevelOfDetail::DETAILED};

  NiceMock<MockClientProcess> client_process(this, 1,
                                             mojom::ProcessType::BROWSER);

  EXPECT_CALL(client_process, RequestChromeMemoryDump(_, _))
      .WillOnce(
          Invoke([](const MemoryDumpRequestArgs& args,
                    const MockClientProcess::RequestChromeMemoryDumpCallback&
                        callback) {
            MemoryDumpArgs dump_args{MemoryDumpLevelOfDetail::DETAILED};
            auto pmd = std::make_unique<ProcessMemoryDump>(nullptr, dump_args);
            callback.Run(true, args.dump_guid, std::move(pmd));
          }));

  MockGlobalMemoryDumpAndAppendToTraceCallback callback;
  EXPECT_CALL(callback, OnCall(true, Ne(0ul)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  TraceLog::GetInstance()->SetEnabled(
      TraceConfig(MemoryDumpManager::kTraceCategory, ""),
      TraceLog::RECORDING_MODE);
  RequestGlobalMemoryDumpAndAppendToTrace(args, callback.Get());
  run_loop.Run();
  TraceLog::GetInstance()->SetDisabled();

  std::unique_ptr<trace_analyzer::TraceAnalyzer> analyzer =
      GetDeserializedTrace();
  trace_analyzer::TraceEventVector events;
  analyzer->FindEvents(Query::EventPhaseIs(TRACE_EVENT_PHASE_MEMORY_DUMP),
                       &events);

  ASSERT_EQ(1u, events.size());
  ASSERT_TRUE(trace_analyzer::CountMatches(
      events, Query::EventNameIs(MemoryDumpTypeToString(
                  MemoryDumpType::EXPLICITLY_TRIGGERED))));
}

}  // namespace memory_instrumentation
