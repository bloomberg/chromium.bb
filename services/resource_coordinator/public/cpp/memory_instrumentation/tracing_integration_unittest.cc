// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/client_process_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/trace_event_analyzer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_manager_test_utils.h"
#include "base/trace_event/memory_dump_scheduler.h"
#include "base/trace_event/memory_infra_background_whitelist.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_config_memory_test_util.h"
#include "base/trace_event/trace_log.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/coordinator.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::trace_event::MemoryAllocatorDump;
using base::trace_event::MemoryDumpArgs;
using base::trace_event::MemoryDumpCallbackResult;
using base::trace_event::MemoryDumpLevelOfDetail;
using base::trace_event::MemoryDumpManager;
using base::trace_event::MemoryDumpProvider;
using base::trace_event::MemoryDumpRequestArgs;
using base::trace_event::MemoryDumpScheduler;
using base::trace_event::MemoryDumpType;
using base::trace_event::ProcessMemoryDump;
using base::trace_event::TraceConfig;
using base::trace_event::TraceLog;
using base::trace_event::TraceResultBuffer;
using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::Return;

namespace memory_instrumentation {

namespace {

const char kMDPName[] = "TestDumpProvider";
const char* kWhitelistedMDPName = "WhitelistedTestDumpProvider";
const char* kBackgroundButNotSummaryWhitelistedMDPName =
    "BackgroundButNotSummaryWhitelistedTestDumpProvider";
const char* const kTestMDPWhitelist[] = {
    kWhitelistedMDPName, kBackgroundButNotSummaryWhitelistedMDPName, nullptr};
const char* const kTestMDPWhitelistForSummary[] = {kWhitelistedMDPName,
                                                   nullptr};
const uint64_t kTestGuid = 0;

// GTest matchers for MemoryDumpRequestArgs arguments.
MATCHER(IsDetailedDump, "") {
  return arg.level_of_detail == MemoryDumpLevelOfDetail::DETAILED;
}

MATCHER(IsLightDump, "") {
  return arg.level_of_detail == MemoryDumpLevelOfDetail::LIGHT;
}

MATCHER(IsBackgroundDump, "") {
  return arg.level_of_detail == MemoryDumpLevelOfDetail::BACKGROUND;
}

// TODO(ssid): This class is replicated in memory_dump_manager_unittest. Move
// this to memory_dump_manager_test_utils.h crbug.com/728199.
class MockMemoryDumpProvider : public MemoryDumpProvider {
 public:
  MOCK_METHOD0(Destructor, void());
  MOCK_METHOD2(OnMemoryDump,
               bool(const MemoryDumpArgs& args, ProcessMemoryDump* pmd));
  MOCK_METHOD1(PollFastMemoryTotal, void(uint64_t* memory_total));
  MOCK_METHOD0(SuspendFastMemoryPolling, void());

  MockMemoryDumpProvider() : enable_mock_destructor(false) {
    ON_CALL(*this, OnMemoryDump(_, _))
        .WillByDefault(
            Invoke([](const MemoryDumpArgs&, ProcessMemoryDump* pmd) -> bool {
              return true;
            }));

    ON_CALL(*this, PollFastMemoryTotal(_))
        .WillByDefault(
            Invoke([](uint64_t* memory_total) -> void { NOTREACHED(); }));
  }

  ~MockMemoryDumpProvider() override {
    if (enable_mock_destructor)
      Destructor();
  }

  bool enable_mock_destructor;
};

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

class MemoryTracingIntegrationTest;

class MockCoordinator : public Coordinator, public mojom::Coordinator {
 public:
  MockCoordinator(MemoryTracingIntegrationTest* client) : client_(client) {}

  void BindCoordinatorRequest(
      mojom::CoordinatorRequest request,
      const service_manager::BindSourceInfo& source_info) override {
    bindings_.AddBinding(this, std::move(request));
  }

  void RegisterClientProcess(mojom::ClientProcessPtr,
                             mojom::ProcessType) override {}

  void RequestGlobalMemoryDump(
      const MemoryDumpRequestArgs& args,
      const RequestGlobalMemoryDumpCallback& callback) override;

 private:
  mojo::BindingSet<mojom::Coordinator> bindings_;
  MemoryTracingIntegrationTest* client_;
};

class MemoryTracingIntegrationTest : public testing::Test {
 public:
  void SetUp() override {
    message_loop_ = base::MakeUnique<base::MessageLoop>();
    coordinator_ = base::MakeUnique<MockCoordinator>(this);
  }

  void InitializeClientProcess(mojom::ProcessType process_type) {
    mdm_ = MemoryDumpManager::CreateInstanceForTesting();
    mdm_->set_dumper_registrations_ignored_for_testing(true);
    const char* kServiceName = "TestServiceName";
    ClientProcessImpl::Config config(nullptr, kServiceName, process_type);
    config.coordinator_for_testing = coordinator_.get();
    client_process_.reset(new ClientProcessImpl(config));
  }

  void TearDown() override {
    TraceLog::GetInstance()->SetDisabled();
    mdm_.reset();
    client_process_.reset();
    coordinator_.reset();
    message_loop_.reset();
    TraceLog::DeleteForTesting();
  }

  // Blocks the current thread (spinning a nested message loop) until the
  // memory dump is complete. Returns:
  // - return value: the |success| from the RequestProcessMemoryDump() callback.
  bool RequestProcessDumpAndWait(
      MemoryDumpType dump_type,
      MemoryDumpLevelOfDetail level_of_detail,
      base::Optional<MemoryDumpCallbackResult>* result = nullptr) {
    base::RunLoop run_loop;
    bool success = false;
    MemoryDumpRequestArgs request_args{kTestGuid, dump_type, level_of_detail};
    ClientProcessImpl::RequestProcessMemoryDumpCallback callback = base::Bind(
        [](bool* curried_success, base::Closure curried_quit_closure,
           base::Optional<MemoryDumpCallbackResult>* curried_result,
           bool success, uint64_t dump_guid,
           mojom::RawProcessMemoryDumpPtr result) {
          EXPECT_EQ(kTestGuid, dump_guid);
          *curried_success = success;
          if (curried_result) {
            *curried_result = MemoryDumpCallbackResult();
            (*curried_result)->os_dump = result->os_dump;
            (*curried_result)->chrome_dump = result->chrome_dump;
            EXPECT_TRUE(result->extra_processes_dumps.empty());
          }
          curried_quit_closure.Run();
        },
        &success, run_loop.QuitClosure(), result);
    client_process_->RequestProcessMemoryDump(request_args, callback);
    run_loop.Run();
    return success;
  }

  void RequestProcessDump(MemoryDumpType dump_type,
                          MemoryDumpLevelOfDetail level_of_detail) {
    MemoryDumpRequestArgs request_args{kTestGuid, dump_type, level_of_detail};
    ClientProcessImpl::RequestProcessMemoryDumpCallback callback =
        base::Bind([](bool success, uint64_t dump_guid,
                      mojom::RawProcessMemoryDumpPtr result) {});
    client_process_->RequestProcessMemoryDump(request_args, callback);
  }

 protected:
  void EnableMemoryInfraTracing() {
    TraceLog::GetInstance()->SetEnabled(
        TraceConfig(MemoryDumpManager::kTraceCategory, ""),
        TraceLog::RECORDING_MODE);
  }

  void EnableMemoryInfraTracingWithTraceConfig(
      const std::string& trace_config) {
    TraceLog::GetInstance()->SetEnabled(TraceConfig(trace_config),
                                        TraceLog::RECORDING_MODE);
  }

  void DisableTracing() { TraceLog::GetInstance()->SetDisabled(); }

  void RegisterDumpProvider(
      MemoryDumpProvider* mdp,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const MemoryDumpProvider::Options& options,
      const char* name = kMDPName) {
    mdm_->set_dumper_registrations_ignored_for_testing(false);
    mdm_->RegisterDumpProvider(mdp, name, std::move(task_runner), options);
    mdm_->set_dumper_registrations_ignored_for_testing(true);
  }

  void RegisterDumpProvider(
      MemoryDumpProvider* mdp,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    RegisterDumpProvider(mdp, task_runner, MemoryDumpProvider::Options());
  }

  bool IsPeriodicDumpingEnabled() const {
    return MemoryDumpScheduler::GetInstance()->is_enabled_for_testing();
  }

  std::unique_ptr<MemoryDumpManager> mdm_;

 private:
  std::unique_ptr<base::MessageLoop> message_loop_;
  std::unique_ptr<MockCoordinator> coordinator_;
  std::unique_ptr<ClientProcessImpl> client_process_;
};

void MockCoordinator::RequestGlobalMemoryDump(
    const MemoryDumpRequestArgs& args,
    const RequestGlobalMemoryDumpCallback& callback) {
  client_->RequestProcessDump(args.dump_type, args.level_of_detail);
  callback.Run(args.dump_guid, true, mojom::GlobalMemoryDumpPtr());
}

// Tests that the MemoryDumpProvider(s) are invoked even if tracing has not
// been initialized.
TEST_F(MemoryTracingIntegrationTest, DumpWithoutInitializingTracing) {
  InitializeClientProcess(mojom::ProcessType::RENDERER);
  MockMemoryDumpProvider mdp;
  RegisterDumpProvider(&mdp, base::ThreadTaskRunnerHandle::Get(),
                       MemoryDumpProvider::Options());
  DisableTracing();
  EXPECT_CALL(mdp, OnMemoryDump(_, _)).Times(3);
  for (int i = 0; i < 3; ++i) {
    // A non-SUMMARY_ONLY dump was requested when tracing was not enabled. So,
    // the request should fail because dump was not added to the trace.
    EXPECT_FALSE(RequestProcessDumpAndWait(MemoryDumpType::EXPLICITLY_TRIGGERED,
                                           MemoryDumpLevelOfDetail::DETAILED));
  }
  mdm_->UnregisterDumpProvider(&mdp);
}

// Similar to DumpWithoutInitializingTracing. Tracing is initialized but not
// enabled.
TEST_F(MemoryTracingIntegrationTest,
       DumpWithMDMInitializedForTracingButDisabled) {
  InitializeClientProcess(mojom::ProcessType::RENDERER);
  MockMemoryDumpProvider mdp;
  RegisterDumpProvider(&mdp, base::ThreadTaskRunnerHandle::Get(),
                       MemoryDumpProvider::Options());

  DisableTracing();

  const TraceConfig& trace_config =
      TraceConfig(base::trace_event::TraceConfigMemoryTestUtil::
                      GetTraceConfig_NoTriggers());
  const TraceConfig::MemoryDumpConfig& memory_dump_config =
      trace_config.memory_dump_config();
  mdm_->SetupForTracing(memory_dump_config);

  EXPECT_CALL(mdp, OnMemoryDump(_, _)).Times(3);
  for (int i = 0; i < 3; ++i) {
    // Same as the above. Even if the MDP(s) are invoked, this will return false
    // while attempting to add the dump into the trace.
    EXPECT_FALSE(RequestProcessDumpAndWait(MemoryDumpType::EXPLICITLY_TRIGGERED,
                                           MemoryDumpLevelOfDetail::DETAILED));
  }
  mdm_->TeardownForTracing();
  mdm_->UnregisterDumpProvider(&mdp);
}

TEST_F(MemoryTracingIntegrationTest, SummaryOnlyDumpsArentAddedToTrace) {
  InitializeClientProcess(mojom::ProcessType::RENDERER);
  using trace_analyzer::Query;

  base::trace_event::SetDumpProviderSummaryWhitelistForTesting(
      kTestMDPWhitelistForSummary);
  base::trace_event::SetDumpProviderWhitelistForTesting(kTestMDPWhitelist);

  // Standard provider with default options (create dump for current process).
  MockMemoryDumpProvider mdp;
  RegisterDumpProvider(&mdp, nullptr, MemoryDumpProvider::Options(),
                       kWhitelistedMDPName);

  EnableMemoryInfraTracing();

  EXPECT_CALL(mdp, OnMemoryDump(_, _)).Times(2);
  EXPECT_TRUE(RequestProcessDumpAndWait(MemoryDumpType::EXPLICITLY_TRIGGERED,
                                        MemoryDumpLevelOfDetail::BACKGROUND));
  EXPECT_TRUE(RequestProcessDumpAndWait(MemoryDumpType::SUMMARY_ONLY,
                                        MemoryDumpLevelOfDetail::BACKGROUND));
  DisableTracing();

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

// Checks that is the ClientProcessImpl is initialized after tracing already
// began, it will still late-join the party (real use case: startup tracing).
TEST_F(MemoryTracingIntegrationTest, InitializedAfterStartOfTracing) {
  EnableMemoryInfraTracing();

  // TODO(ssid): Add tests for
  // MemoryInstrumentation::RequestGlobalDumpAndAppendToTrace to fail gracefully
  // before creating ClientProcessImpl.

  // Now late-initialize and check that the CreateProcessDump() completes
  // successfully.
  InitializeClientProcess(mojom::ProcessType::RENDERER);
  MockMemoryDumpProvider mdp;
  RegisterDumpProvider(&mdp, nullptr, MemoryDumpProvider::Options());
  EXPECT_CALL(mdp, OnMemoryDump(_, _)).Times(1);
  EXPECT_TRUE(RequestProcessDumpAndWait(MemoryDumpType::EXPLICITLY_TRIGGERED,
                                        MemoryDumpLevelOfDetail::DETAILED));
  DisableTracing();
}

// Configures periodic dumps with MemoryDumpLevelOfDetail::BACKGROUND triggers
// and tests that only BACKGROUND are added to the trace, but not LIGHT or
// DETAILED, even if requested explicitly.
TEST_F(MemoryTracingIntegrationTest, TestBackgroundTracingSetup) {
  InitializeClientProcess(mojom::ProcessType::BROWSER);
  base::trace_event::SetDumpProviderWhitelistForTesting(kTestMDPWhitelist);
  auto mdp = base::MakeUnique<MockMemoryDumpProvider>();
  RegisterDumpProvider(&*mdp, nullptr, MemoryDumpProvider::Options(),
                       kWhitelistedMDPName);

  base::RunLoop run_loop;
  auto test_task_runner = base::ThreadTaskRunnerHandle::Get();
  auto quit_closure = run_loop.QuitClosure();

  {
    testing::InSequence sequence;
    EXPECT_CALL(*mdp, OnMemoryDump(IsBackgroundDump(), _))
        .Times(3)
        .WillRepeatedly(Invoke(
            [](const MemoryDumpArgs&, ProcessMemoryDump*) { return true; }));
    EXPECT_CALL(*mdp, OnMemoryDump(IsBackgroundDump(), _))
        .WillOnce(Invoke([test_task_runner, quit_closure](const MemoryDumpArgs&,
                                                          ProcessMemoryDump*) {
          test_task_runner->PostTask(FROM_HERE, quit_closure);
          return true;
        }));
    EXPECT_CALL(*mdp, OnMemoryDump(IsBackgroundDump(), _)).Times(AnyNumber());
  }

  EnableMemoryInfraTracingWithTraceConfig(
      base::trace_event::TraceConfigMemoryTestUtil::
          GetTraceConfig_BackgroundTrigger(1 /* period_ms */));

  run_loop.Run();

  // When requesting non-BACKGROUND dumps the MDP will be invoked but the
  // data is expected to be dropped on the floor, hence the EXPECT_FALSE.
  EXPECT_CALL(*mdp, OnMemoryDump(IsLightDump(), _));
  EXPECT_FALSE(RequestProcessDumpAndWait(MemoryDumpType::EXPLICITLY_TRIGGERED,
                                         MemoryDumpLevelOfDetail::LIGHT));

  EXPECT_CALL(*mdp, OnMemoryDump(IsDetailedDump(), _));
  EXPECT_FALSE(RequestProcessDumpAndWait(MemoryDumpType::EXPLICITLY_TRIGGERED,
                                         MemoryDumpLevelOfDetail::DETAILED));

  ASSERT_TRUE(IsPeriodicDumpingEnabled());
  DisableTracing();
  mdm_->UnregisterAndDeleteDumpProviderSoon(std::move(mdp));
}

// This test (and the TraceConfigExpectationsWhenIsCoordinator below)
// crystallizes the expectations of the chrome://tracing UI and chrome telemetry
// w.r.t. periodic dumps in memory-infra, handling gracefully the transition
// between the legacy and the new-style (JSON-based) TraceConfig.
TEST_F(MemoryTracingIntegrationTest, TraceConfigExpectations) {
  InitializeClientProcess(mojom::ProcessType::RENDERER);

  // We don't need to create any dump in this test, only check whether the dumps
  // are requested or not.

  // Enabling memory-infra in a non-coordinator process should not trigger any
  // periodic dumps.
  EnableMemoryInfraTracing();
  EXPECT_FALSE(IsPeriodicDumpingEnabled());
  DisableTracing();

  // Enabling memory-infra with the new (JSON) TraceConfig in a non-coordinator
  // process with a fully defined trigger config should NOT enable any periodic
  // dumps.
  EnableMemoryInfraTracingWithTraceConfig(
      base::trace_event::TraceConfigMemoryTestUtil::
          GetTraceConfig_PeriodicTriggers(1, 5));
  EXPECT_FALSE(IsPeriodicDumpingEnabled());
  DisableTracing();
}

TEST_F(MemoryTracingIntegrationTest, TraceConfigExpectationsWhenIsCoordinator) {
  InitializeClientProcess(mojom::ProcessType::BROWSER);

  // Enabling memory-infra with the legacy TraceConfig (category filter) in
  // a coordinator process should not enable periodic dumps.
  EnableMemoryInfraTracing();
  EXPECT_FALSE(IsPeriodicDumpingEnabled());
  DisableTracing();

  // Enabling memory-infra with the new (JSON) TraceConfig in a coordinator
  // process while specifying a "memory_dump_config" section should enable
  // periodic dumps. This is to preserve the behavior chrome://tracing UI, that
  // is: ticking memory-infra should dump periodically with an explicit config.
  EnableMemoryInfraTracingWithTraceConfig(
      base::trace_event::TraceConfigMemoryTestUtil::
          GetTraceConfig_PeriodicTriggers(100, 5));

  EXPECT_TRUE(IsPeriodicDumpingEnabled());
  DisableTracing();

  // Enabling memory-infra with the new (JSON) TraceConfig in a coordinator
  // process with an empty "memory_dump_config" should NOT enable periodic
  // dumps. This is the way telemetry is supposed to use memory-infra with
  // only explicitly triggered dumps.
  EnableMemoryInfraTracingWithTraceConfig(
      base::trace_event::TraceConfigMemoryTestUtil::
          GetTraceConfig_EmptyTriggers());
  EXPECT_FALSE(IsPeriodicDumpingEnabled());
  DisableTracing();
}

TEST_F(MemoryTracingIntegrationTest, PeriodicDumpingWithMultipleModes) {
  InitializeClientProcess(mojom::ProcessType::BROWSER);

  // Enabling memory-infra with the new (JSON) TraceConfig in a coordinator
  // process with a fully defined trigger config should cause periodic dumps to
  // be performed in the correct order.
  base::RunLoop run_loop;
  auto test_task_runner = base::ThreadTaskRunnerHandle::Get();
  auto quit_closure = run_loop.QuitClosure();

  const int kHeavyDumpRate = 5;
  const int kLightDumpPeriodMs = 1;
  const int kHeavyDumpPeriodMs = kHeavyDumpRate * kLightDumpPeriodMs;

  // The expected sequence with light=1ms, heavy=5ms is H,L,L,L,L,H,...
  auto mdp = base::MakeUnique<MockMemoryDumpProvider>();
  RegisterDumpProvider(&*mdp, nullptr, MemoryDumpProvider::Options(),
                       kWhitelistedMDPName);

  testing::InSequence sequence;
  EXPECT_CALL(*mdp, OnMemoryDump(IsDetailedDump(), _));
  EXPECT_CALL(*mdp, OnMemoryDump(IsLightDump(), _)).Times(kHeavyDumpRate - 1);
  EXPECT_CALL(*mdp, OnMemoryDump(IsDetailedDump(), _));
  EXPECT_CALL(*mdp, OnMemoryDump(IsLightDump(), _)).Times(kHeavyDumpRate - 2);
  EXPECT_CALL(*mdp, OnMemoryDump(IsLightDump(), _))
      .WillOnce(Invoke([test_task_runner, quit_closure](const MemoryDumpArgs&,
                                                        ProcessMemoryDump*) {
        test_task_runner->PostTask(FROM_HERE, quit_closure);
        return true;
      }));

  // Swallow all the final spurious calls until tracing gets disabled.
  EXPECT_CALL(*mdp, OnMemoryDump(_, _)).Times(AnyNumber());

  EnableMemoryInfraTracingWithTraceConfig(
      base::trace_event::TraceConfigMemoryTestUtil::
          GetTraceConfig_PeriodicTriggers(kLightDumpPeriodMs,
                                          kHeavyDumpPeriodMs));
  run_loop.Run();
  DisableTracing();
  mdm_->UnregisterAndDeleteDumpProviderSoon(std::move(mdp));
}

TEST_F(MemoryTracingIntegrationTest, TestWhitelistingMDP) {
  InitializeClientProcess(mojom::ProcessType::RENDERER);
  base::trace_event::SetDumpProviderWhitelistForTesting(kTestMDPWhitelist);
  std::unique_ptr<MockMemoryDumpProvider> mdp1(new MockMemoryDumpProvider);
  RegisterDumpProvider(mdp1.get(), nullptr);
  std::unique_ptr<MockMemoryDumpProvider> mdp2(new MockMemoryDumpProvider);
  RegisterDumpProvider(mdp2.get(), nullptr, MemoryDumpProvider::Options(),
                       kWhitelistedMDPName);

  EXPECT_CALL(*mdp1, OnMemoryDump(_, _)).Times(0);
  EXPECT_CALL(*mdp2, OnMemoryDump(_, _)).Times(1);

  EnableMemoryInfraTracing();
  EXPECT_FALSE(IsPeriodicDumpingEnabled());
  EXPECT_TRUE(RequestProcessDumpAndWait(MemoryDumpType::EXPLICITLY_TRIGGERED,
                                        MemoryDumpLevelOfDetail::BACKGROUND));
  DisableTracing();
}

TEST_F(MemoryTracingIntegrationTest, TestSummaryComputation) {
  InitializeClientProcess(mojom::ProcessType::RENDERER);
  MockMemoryDumpProvider mdp;
  RegisterDumpProvider(&mdp, base::ThreadTaskRunnerHandle::Get(),
                       MemoryDumpProvider::Options());

  EXPECT_CALL(mdp, OnMemoryDump(_, _))
      .WillOnce(Invoke([](const MemoryDumpArgs&,
                          ProcessMemoryDump* pmd) -> bool {
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
        pmd->process_totals()->set_resident_set_bytes(5 * kB);
        pmd->set_has_process_totals();
        return true;
      }));

  EnableMemoryInfraTracing();
  base::Optional<MemoryDumpCallbackResult> result;
  EXPECT_TRUE(RequestProcessDumpAndWait(MemoryDumpType::EXPLICITLY_TRIGGERED,
                                        MemoryDumpLevelOfDetail::LIGHT,
                                        &result));
  DisableTracing();

  ASSERT_TRUE(result);

  // For malloc we only count the root "malloc" not children "malloc/*".
  EXPECT_EQ(1u, result->chrome_dump.malloc_total_kb);

  // For blink_gc we only count the root "blink_gc" not children "blink_gc/*".
  EXPECT_EQ(2u, result->chrome_dump.blink_gc_total_kb);

  // For v8 we count the children ("v8/*") as the root total is not given.
  EXPECT_EQ(3u, result->chrome_dump.v8_total_kb);

  // partition_alloc has partition_alloc/allocated_objects/* which is a subset
  // of partition_alloc/partitions/* so we only count the latter.
  EXPECT_EQ(4u, result->chrome_dump.partition_alloc_total_kb);

  // resident_set_kb should read from process_totals.
  EXPECT_EQ(5u, result->os_dump.resident_set_kb);
};

TEST_F(MemoryTracingIntegrationTest, DumpOnBehalfOfOtherProcess) {
  InitializeClientProcess(mojom::ProcessType::RENDERER);
  using trace_analyzer::Query;

  // Standard provider with default options (create dump for current process).
  MemoryDumpProvider::Options options;
  MockMemoryDumpProvider mdp1;
  RegisterDumpProvider(&mdp1, nullptr, options);

  // Provider with out-of-process dumping.
  MockMemoryDumpProvider mdp2;
  options.target_pid = 123;
  RegisterDumpProvider(&mdp2, nullptr, options);

  // Another provider with out-of-process dumping.
  MockMemoryDumpProvider mdp3;
  options.target_pid = 456;
  RegisterDumpProvider(&mdp3, nullptr, options);

  EnableMemoryInfraTracing();
  EXPECT_CALL(mdp1, OnMemoryDump(_, _)).Times(1);
  EXPECT_CALL(mdp2, OnMemoryDump(_, _)).Times(1);
  EXPECT_CALL(mdp3, OnMemoryDump(_, _)).Times(1);
  EXPECT_TRUE(RequestProcessDumpAndWait(MemoryDumpType::EXPLICITLY_TRIGGERED,
                                        MemoryDumpLevelOfDetail::DETAILED));
  DisableTracing();

  std::unique_ptr<trace_analyzer::TraceAnalyzer> analyzer =
      GetDeserializedTrace();
  trace_analyzer::TraceEventVector events;
  analyzer->FindEvents(Query::EventPhaseIs(TRACE_EVENT_PHASE_MEMORY_DUMP),
                       &events);

  ASSERT_EQ(3u, events.size());
  ASSERT_EQ(1u, trace_analyzer::CountMatches(events, Query::EventPidIs(123)));
  ASSERT_EQ(1u, trace_analyzer::CountMatches(events, Query::EventPidIs(456)));
  ASSERT_EQ(1u, trace_analyzer::CountMatches(
                    events, Query::EventPidIs(base::GetCurrentProcId())));
  ASSERT_EQ(events[0]->id, events[1]->id);
  ASSERT_EQ(events[0]->id, events[2]->id);
}

TEST_F(MemoryTracingIntegrationTest, TestPollingOnDumpThread) {
  InitializeClientProcess(mojom::ProcessType::RENDERER);
  std::unique_ptr<MockMemoryDumpProvider> mdp1(new MockMemoryDumpProvider());
  std::unique_ptr<MockMemoryDumpProvider> mdp2(new MockMemoryDumpProvider());
  mdp1->enable_mock_destructor = true;
  mdp2->enable_mock_destructor = true;
  EXPECT_CALL(*mdp1, Destructor());
  EXPECT_CALL(*mdp2, Destructor());

  MemoryDumpProvider::Options options;
  options.is_fast_polling_supported = true;
  RegisterDumpProvider(mdp1.get(), nullptr, options);

  base::RunLoop run_loop;
  auto test_task_runner = base::ThreadTaskRunnerHandle::Get();
  auto quit_closure = run_loop.QuitClosure();
  MemoryDumpManager* mdm = mdm_.get();

  EXPECT_CALL(*mdp1, PollFastMemoryTotal(_))
      .WillOnce(Invoke([&mdp2, options, this](uint64_t*) {
        RegisterDumpProvider(mdp2.get(), nullptr, options);
      }))
      .WillOnce(Return())
      .WillOnce(Invoke([mdm, &mdp2](uint64_t*) {
        mdm->UnregisterAndDeleteDumpProviderSoon(std::move(mdp2));
      }))
      .WillOnce(Invoke([test_task_runner, quit_closure](uint64_t*) {
        test_task_runner->PostTask(FROM_HERE, quit_closure);
      }))
      .WillRepeatedly(Return());

  // We expect a call to |mdp1| because it is still registered at the time the
  // Peak detector is Stop()-ed (upon OnTraceLogDisabled(). We do NOT expect
  // instead a call for |mdp2|, because that gets unregisterd before the Stop().
  EXPECT_CALL(*mdp1, SuspendFastMemoryPolling()).Times(1);
  EXPECT_CALL(*mdp2, SuspendFastMemoryPolling()).Times(0);

  // |mdp2| should invoke exactly twice:
  // - once after the registrarion, when |mdp1| hits the first Return()
  // - the 2nd time when |mdp1| unregisters |mdp1|. The unregistration is
  //   posted and will necessarily happen after the polling task.
  EXPECT_CALL(*mdp2, PollFastMemoryTotal(_)).Times(2).WillRepeatedly(Return());

  EnableMemoryInfraTracingWithTraceConfig(
      base::trace_event::TraceConfigMemoryTestUtil::
          GetTraceConfig_PeakDetectionTrigger(1));
  run_loop.Run();
  DisableTracing();
  mdm_->UnregisterAndDeleteDumpProviderSoon(std::move(mdp1));
}

}  // namespace memory_instrumentation
