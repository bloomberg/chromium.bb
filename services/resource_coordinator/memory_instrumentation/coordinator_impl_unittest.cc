// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/coordinator_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/trace_event/memory_dump_request_args.h"
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

using OSMemDump = base::trace_event::MemoryDumpCallbackResult::OSMemDump;
using RequestGlobalMemoryDumpCallback =
    memory_instrumentation::CoordinatorImpl::RequestGlobalMemoryDumpCallback;
using memory_instrumentation::mojom::GlobalMemoryDumpPtr;
using memory_instrumentation::mojom::GlobalMemoryDump;
using base::trace_event::MemoryDumpRequestArgs;

namespace memory_instrumentation {

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

    ON_CALL(*this, RequestProcessMemoryDump(_, _))
        .WillByDefault(
            Invoke([](const MemoryDumpRequestArgs& args,
                      const RequestProcessMemoryDumpCallback& callback) {
              callback.Run(true, args.dump_guid, nullptr);

            }));

    ON_CALL(*this, RequestOSMemoryDump(_, _))
        .WillByDefault(Invoke([](const std::vector<base::ProcessId> pids,
                                 const RequestOSMemoryDumpCallback& callback) {
          std::unordered_map<base::ProcessId, OSMemDump> results;
          callback.Run(true, results);
        }));
  }

  ~MockClientProcess() override {}

  MOCK_METHOD2(RequestProcessMemoryDump,
               void(const MemoryDumpRequestArgs& args,
                    const RequestProcessMemoryDumpCallback& callback));

  MOCK_METHOD2(RequestOSMemoryDump,
               void(const std::vector<base::ProcessId>& args,
                    const RequestOSMemoryDumpCallback& callback));

 private:
  mojo::Binding<mojom::ClientProcess> binding_;
};

class MockGlobalMemoryDumpCallback {
 public:
  MockGlobalMemoryDumpCallback() = default;
  MOCK_METHOD3(OnCall, void(bool, uint64_t, GlobalMemoryDump*));

  void Run(bool success, uint64_t dump_guid, GlobalMemoryDumpPtr ptr) {
    OnCall(success, dump_guid, ptr.get());
  }

  RequestGlobalMemoryDumpCallback Get() {
    return base::Bind(&MockGlobalMemoryDumpCallback::Run,
                      base::Unretained(this));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGlobalMemoryDumpCallback);
};

// Tests that the global dump is acked even in absence of clients.
TEST_F(CoordinatorImplTest, NoClients) {
  base::trace_event::MemoryDumpRequestArgs args = {
      0, base::trace_event::MemoryDumpType::EXPLICITLY_TRIGGERED,
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED};

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(callback, OnCall(true, Ne(0u), NotNull()));
  RequestGlobalMemoryDump(args, callback.Get());
}

// Nominal behavior: several clients contributing to the global dump.
TEST_F(CoordinatorImplTest, SeveralClients) {
  base::RunLoop run_loop;

  NiceMock<MockClientProcess> client_process_1(this, 1,
                                               mojom::ProcessType::BROWSER);
  NiceMock<MockClientProcess> client_process_2(this);
  EXPECT_CALL(client_process_1, RequestProcessMemoryDump(_, _)).Times(1);
  EXPECT_CALL(client_process_2, RequestProcessMemoryDump(_, _)).Times(1);

  base::trace_event::MemoryDumpRequestArgs args = {
      0, base::trace_event::MemoryDumpType::EXPLICITLY_TRIGGERED,
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED};

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(callback, OnCall(true, Ne(0u), NotNull()))
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
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED};

  auto client_process_1 = base::MakeUnique<NiceMock<MockClientProcess>>(
      this, 1, mojom::ProcessType::BROWSER);
  auto client_process_2 = base::MakeUnique<NiceMock<MockClientProcess>>(this);

  // One of the client processes dies after a global dump is requested and
  // before it receives the corresponding process dump request. The coordinator
  // should detect that one of its clients is disconnected and claim the global
  // dump attempt has failed.

  // Whichever client is called first destroys the other client.
  ON_CALL(*client_process_1, RequestProcessMemoryDump(_, _))
      .WillByDefault(
          Invoke([&client_process_2](
                     const MemoryDumpRequestArgs& args,
                     const MockClientProcess::RequestProcessMemoryDumpCallback&
                         callback) {
            client_process_2.reset();
            callback.Run(true, args.dump_guid, nullptr);
          }));
  ON_CALL(*client_process_2, RequestProcessMemoryDump(_, _))
      .WillByDefault(
          Invoke([&client_process_1](
                     const MemoryDumpRequestArgs& args,
                     const MockClientProcess::RequestProcessMemoryDumpCallback&
                         callback) {
            client_process_1.reset();
            callback.Run(true, args.dump_guid, nullptr);
          }));

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(callback, OnCall(false, Ne(0u), NotNull()))
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
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED};

  auto client_process = base::MakeUnique<NiceMock<MockClientProcess>>(
      this, 1, mojom::ProcessType::BROWSER);

  ON_CALL(*client_process, RequestProcessMemoryDump(_, _))
      .WillByDefault(
          Invoke([&client_process](
                     const MemoryDumpRequestArgs& args,
                     const MockClientProcess::RequestProcessMemoryDumpCallback&
                         callback) {
            // The dtor here will cause mojo to post an UnregisterClient call to
            // the coordinator.
            client_process.reset();
            callback.Run(true, args.dump_guid, nullptr);
          }));

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(callback, OnCall(false, Ne(0u), NotNull()))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  RequestGlobalMemoryDump(args, callback.Get());
  run_loop.Run();
}

TEST_F(CoordinatorImplTest, GlobalMemoryDumpStruct) {
  base::RunLoop run_loop;

  NiceMock<MockClientProcess> client_process_1(this, 1,
                                               mojom::ProcessType::BROWSER);
  NiceMock<MockClientProcess> client_process_2(this, 2,
                                               mojom::ProcessType::RENDERER);
  EXPECT_CALL(client_process_1, RequestProcessMemoryDump(_, _))
      .WillOnce(
          Invoke([](const MemoryDumpRequestArgs& args,
                    const MockClientProcess::RequestProcessMemoryDumpCallback&
                        callback) {
            auto dump = mojom::RawProcessMemoryDump::New();
            dump->chrome_dump.malloc_total_kb = 1;
            dump->os_dump.resident_set_kb = 1;
            callback.Run(true, args.dump_guid, std::move(dump));

          }));
  EXPECT_CALL(client_process_2, RequestProcessMemoryDump(_, _))
      .WillOnce(
          Invoke([](const MemoryDumpRequestArgs& args,
                    const MockClientProcess::RequestProcessMemoryDumpCallback&
                        callback) {
            auto dump = mojom::RawProcessMemoryDump::New();
            dump->chrome_dump.malloc_total_kb = 2;
            dump->os_dump.resident_set_kb = 2;
            callback.Run(true, args.dump_guid, std::move(dump));
          }));

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(callback, OnCall(true, Ne(0u), NotNull()))
      .WillOnce(Invoke([&run_loop](uint64_t dump_guid, bool success,
                                   GlobalMemoryDump* dump) {

        EXPECT_EQ(2U, dump->process_dumps.size());
        EXPECT_THAT(dump->process_dumps,
                    Contains(Property(
                        &mojom::ProcessMemoryDumpPtr::get,
                        Pointee(Field(&mojom::ProcessMemoryDump::process_type,
                                      Eq(mojom::ProcessType::BROWSER))))));

        EXPECT_THAT(dump->process_dumps,
                    Contains(Property(
                        &mojom::ProcessMemoryDumpPtr::get,
                        Pointee(Field(&mojom::ProcessMemoryDump::process_type,
                                      Eq(mojom::ProcessType::RENDERER))))));

        run_loop.Quit();
      }));

  base::trace_event::MemoryDumpRequestArgs args = {
      0, base::trace_event::MemoryDumpType::SUMMARY_ONLY,
      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND};
  RequestGlobalMemoryDump(args, callback.Get());
  run_loop.Run();
}

TEST_F(CoordinatorImplTest, OsDumps) {
  base::RunLoop run_loop;

  MockClientProcess browser_client(this, 1, mojom::ProcessType::BROWSER);
  MockClientProcess renderer_client(this, 2, mojom::ProcessType::RENDERER);

  EXPECT_CALL(browser_client, RequestProcessMemoryDump(_, _))
      .WillOnce(
          Invoke([](const MemoryDumpRequestArgs& args,
                    const MockClientProcess::RequestProcessMemoryDumpCallback&
                        callback) {
            auto dump = mojom::RawProcessMemoryDump::New();
            dump->chrome_dump.malloc_total_kb = 1;
            callback.Run(args.dump_guid, true, std::move(dump));
          }));
  EXPECT_CALL(renderer_client, RequestProcessMemoryDump(_, _))
      .WillOnce(
          Invoke([](const MemoryDumpRequestArgs& args,
                    const MockClientProcess::RequestProcessMemoryDumpCallback&
                        callback) {
            auto dump = mojom::RawProcessMemoryDump::New();
            dump->chrome_dump.malloc_total_kb = 2;
            callback.Run(args.dump_guid, true, std::move(dump));
          }));
#if defined(OS_LINUX)
  EXPECT_CALL(browser_client,
              RequestOSMemoryDump(AllOf(Contains(1), Contains(2)), _))
      .WillOnce(Invoke(
          [](const std::vector<base::ProcessId>& pids,
             const MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            std::unordered_map<base::ProcessId, OSMemDump> results;
            results[1].resident_set_kb = 1;
            results[2].resident_set_kb = 2;
            callback.Run(true, results);
          }));
  EXPECT_CALL(renderer_client, RequestOSMemoryDump(_, _)).Times(0);
#else
  EXPECT_CALL(browser_client, RequestOSMemoryDump(Contains(1), _))
      .WillOnce(Invoke(
          [](const std::vector<base::ProcessId>& pids,
             const MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            std::unordered_map<base::ProcessId, OSMemDump> results;
            results[1].resident_set_kb = 1;
            callback.Run(true, results);
          }));
  EXPECT_CALL(renderer_client, RequestOSMemoryDump(Contains(2), _))
      .WillOnce(Invoke(
          [](const std::vector<base::ProcessId>& pids,
             const MockClientProcess::RequestOSMemoryDumpCallback& callback) {
            std::unordered_map<base::ProcessId, OSMemDump> results;
            results[2].resident_set_kb = 2;
            callback.Run(true, results);
          }));
#endif  // defined(OS_LINUX)

  MockGlobalMemoryDumpCallback callback;
  EXPECT_CALL(callback, OnCall(true, Ne(0u), NotNull()))
      .WillOnce(Invoke([&run_loop](bool success, uint64_t dump_guid,
                                   GlobalMemoryDump* global_dump) {
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
        EXPECT_EQ(browser_dump->os_dump->resident_set_kb, 1u);
        EXPECT_EQ(renderer_dump->os_dump->resident_set_kb, 2u);
        run_loop.Quit();
      }));

  base::trace_event::MemoryDumpRequestArgs args = {
      0, base::trace_event::MemoryDumpType::SUMMARY_ONLY,
      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND};
  RequestGlobalMemoryDump(args, callback.Get());
  run_loop.Run();
}

}  // namespace memory_instrumentation
