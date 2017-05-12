// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory/process_local_dump_manager_impl.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_log.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/resource_coordinator/public/cpp/memory/coordinator.h"
#include "services/resource_coordinator/public/interfaces/memory/memory_instrumentation.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::trace_event::MemoryDumpLevelOfDetail;
using base::trace_event::MemoryDumpManager;
using base::trace_event::MemoryDumpType;

namespace memory_instrumentation {

class MockCoordinator : public Coordinator, public mojom::Coordinator {
 public:
  void BindCoordinatorRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::CoordinatorRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  void RegisterProcessLocalDumpManager(
      mojom::ProcessLocalDumpManagerPtr process_manager) override {}

  void RequestGlobalMemoryDump(
      const base::trace_event::MemoryDumpRequestArgs& args,
      const RequestGlobalMemoryDumpCallback& callback) override {
    callback.Run(args.dump_guid, true, mojom::GlobalMemoryDumpPtr());
  }

 private:
  mojo::BindingSet<mojom::Coordinator> bindings_;
};

class ProcessLocalDumpManagerImplTest : public testing::Test {
 public:
  void SetUp() override {
    message_loop_.reset(new base::MessageLoop());
    coordinator_.reset(new MockCoordinator());
    mdm_.reset(new MemoryDumpManager());
    MemoryDumpManager::SetInstanceForTesting(mdm_.get());
    auto process_type = mojom::ProcessType::OTHER;
    ProcessLocalDumpManagerImpl::Config config(coordinator_.get(),
                                               process_type);
    local_manager_impl_.reset(new ProcessLocalDumpManagerImpl(config));
    local_manager_impl_->SetAsNonCoordinatorForTesting();

    // Enable tracing.
    std::string category_filter = "-*,";
    category_filter += MemoryDumpManager::kTraceCategory;
    base::trace_event::TraceConfig trace_config(category_filter, "");
    base::trace_event::TraceLog::GetInstance()->SetEnabled(
        trace_config, base::trace_event::TraceLog::RECORDING_MODE);

    // Reset the counters.
    expected_callback_calls_ = 0;
    dump_requests_received_by_coordinator_ = 0;
    quit_closure_.Reset();
  }

  void TearDown() override {
    base::trace_event::TraceLog::GetInstance()->SetDisabled();
    MemoryDumpManager::SetInstanceForTesting(nullptr);
    mdm_.reset();
    local_manager_impl_.reset();
    coordinator_.reset();
    message_loop_.reset();
  }

  void OnGlobalMemoryDumpDone(int more_requests,
                              uint64_t dump_guid,
                              bool success) {
    EXPECT_GT(expected_callback_calls_, 0);
    EXPECT_FALSE(quit_closure_.is_null());

    dump_requests_received_by_coordinator_ += success ? 1 : 0;
    expected_callback_calls_--;
    if (expected_callback_calls_ == 0)
      quit_closure_.Run();

    if (more_requests > 0)
      SequentiallyRequestGlobalDumps(more_requests);
  }

  void SequentiallyRequestGlobalDumps(int num_requests) {
    MemoryDumpManager::GetInstance()->RequestGlobalDump(
        MemoryDumpType::EXPLICITLY_TRIGGERED, MemoryDumpLevelOfDetail::LIGHT,
        base::Bind(&ProcessLocalDumpManagerImplTest::OnGlobalMemoryDumpDone,
                   base::Unretained(this), num_requests - 1));
  }

  int expected_callback_calls_;
  int dump_requests_received_by_coordinator_;
  base::Closure quit_closure_;

 private:
  std::unique_ptr<base::MessageLoop> message_loop_;
  std::unique_ptr<MockCoordinator> coordinator_;
  std::unique_ptr<MemoryDumpManager> mdm_;
  std::unique_ptr<ProcessLocalDumpManagerImpl> local_manager_impl_;
};

// Makes several global dump requests each after receiving the ACK for the
// previous one. There should be no throttling and all requests should be
// forwarded to the coordinator.
TEST_F(ProcessLocalDumpManagerImplTest, NonOverlappingMemoryDumpRequests) {
  base::RunLoop run_loop;
  expected_callback_calls_ = 3;
  quit_closure_ = run_loop.QuitClosure();
  SequentiallyRequestGlobalDumps(3);
  run_loop.Run();
  EXPECT_EQ(3, dump_requests_received_by_coordinator_);
}

// Makes several global dump requests without waiting for previous requests to
// finish. Only the first request should make it to the coordinator. The rest
// should be cancelled.
TEST_F(ProcessLocalDumpManagerImplTest, OverlappingMemoryDumpRequests) {
  base::RunLoop run_loop;
  expected_callback_calls_ = 3;
  quit_closure_ = run_loop.QuitClosure();
  SequentiallyRequestGlobalDumps(1);
  SequentiallyRequestGlobalDumps(1);
  SequentiallyRequestGlobalDumps(1);
  run_loop.Run();
  EXPECT_EQ(1, dump_requests_received_by_coordinator_);
}

}  // namespace memory_instrumentation
