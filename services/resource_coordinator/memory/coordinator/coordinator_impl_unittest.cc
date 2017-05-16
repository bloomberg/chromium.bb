// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory/coordinator/coordinator_impl.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/resource_coordinator/public/interfaces/memory/memory_instrumentation.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_instrumentation {

class CoordinatorImplTest : public testing::Test {
 public:
  CoordinatorImplTest() {}
  void SetUp() override {
    dump_response_args_ = {0U, false};
    coordinator_.reset(new CoordinatorImpl(false));
  }

  void TearDown() override { coordinator_.reset(); }

  void RegisterProcessLocalDumpManager(
      mojom::ProcessLocalDumpManagerPtr process_manager) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&CoordinatorImpl::RegisterProcessLocalDumpManager,
                              base::Unretained(coordinator_.get()),
                              base::Passed(&process_manager)));
  }

  void RequestGlobalMemoryDump(base::trace_event::MemoryDumpRequestArgs args,
                               base::Closure closure) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&CoordinatorImpl::RequestGlobalMemoryDump,
                   base::Unretained(coordinator_.get()), args,
                   base::Bind(&CoordinatorImplTest::OnGlobalMemoryDumpResponse,
                              base::Unretained(this), closure)));
  }

  void OnGlobalMemoryDumpResponse(base::Closure closure,
                                  uint64_t dump_guid,
                                  bool success,
                                  mojom::GlobalMemoryDumpPtr) {
    dump_response_args_ = {dump_guid, success};
    closure.Run();
  }

 protected:
  struct DumpResponseArgs {
    uint64_t dump_guid;
    bool success;
  };

  DumpResponseArgs dump_response_args_;

 private:
  std::unique_ptr<CoordinatorImpl> coordinator_;
  base::MessageLoop message_loop_;
};

class MockDumpManager : public mojom::ProcessLocalDumpManager {
 public:
  MockDumpManager(CoordinatorImplTest* test_coordinator, int expected_calls)
      : binding_(this), expected_calls_(expected_calls) {
    // Register to the coordinator.
    mojom::ProcessLocalDumpManagerPtr process_manager;
    binding_.Bind(mojo::MakeRequest(&process_manager));
    test_coordinator->RegisterProcessLocalDumpManager(
        std::move(process_manager));
  }

  ~MockDumpManager() override { EXPECT_EQ(0, expected_calls_); }

  void RequestProcessMemoryDump(
      const base::trace_event::MemoryDumpRequestArgs& args,
      const RequestProcessMemoryDumpCallback& callback) override {
    expected_calls_--;
    callback.Run(args.dump_guid, true, mojom::ProcessMemoryDumpPtr());
  }

 private:
  mojo::Binding<mojom::ProcessLocalDumpManager> binding_;
  int expected_calls_;
};

TEST_F(CoordinatorImplTest, NoProcessLocalManagers) {
  base::RunLoop run_loop;
  base::trace_event::MemoryDumpRequestArgs args = {
      1234, base::trace_event::MemoryDumpType::EXPLICITLY_TRIGGERED,
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED};
  RequestGlobalMemoryDump(args, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(1234U, dump_response_args_.dump_guid);
  EXPECT_TRUE(dump_response_args_.success);
}

TEST_F(CoordinatorImplTest, SeveralProcessLocalManagers) {
  base::RunLoop run_loop;

  MockDumpManager dump_manager_1(this, 1);
  MockDumpManager dump_manager_2(this, 1);
  base::trace_event::MemoryDumpRequestArgs args = {
      2345, base::trace_event::MemoryDumpType::EXPLICITLY_TRIGGERED,
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED};
  RequestGlobalMemoryDump(args, run_loop.QuitClosure());

  run_loop.Run();

  EXPECT_EQ(2345U, dump_response_args_.dump_guid);
  EXPECT_TRUE(dump_response_args_.success);
}

TEST_F(CoordinatorImplTest, FaultyProcessLocalManager) {
  base::RunLoop run_loop;

  MockDumpManager dump_manager_1(this, 1);
  std::unique_ptr<MockDumpManager> dump_manager_2(new MockDumpManager(this, 0));
  base::trace_event::MemoryDumpRequestArgs args = {
      3456, base::trace_event::MemoryDumpType::EXPLICITLY_TRIGGERED,
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED};
  RequestGlobalMemoryDump(args, run_loop.QuitClosure());
  // One of the process-local managers dies after a global dump is requested and
  // before it receives the corresponding process dump request. The coordinator
  // should detect that one of its clients is disconnected and claim the global
  // dump attempt has failed.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind([](std::unique_ptr<MockDumpManager> dm) {},
                            base::Passed(&dump_manager_2)));

  run_loop.Run();

  EXPECT_EQ(3456U, dump_response_args_.dump_guid);
  EXPECT_FALSE(dump_response_args_.success);
}
}  // namespace memory_instrumentation
