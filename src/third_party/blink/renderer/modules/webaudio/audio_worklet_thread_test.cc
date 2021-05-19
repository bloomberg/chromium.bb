// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_worklet_messaging_proxy.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_worklet_thread.h"
#include "third_party/blink/renderer/modules/webaudio/realtime_audio_worklet_thread.h"
#include "third_party/blink/renderer/modules/webaudio/semi_realtime_audio_worklet_thread.h"

#include <memory>
#include <tuple>

#include "base/feature_list.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/module_record.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/script/js_module_script.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/testing/module_test_base.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

class AudioWorkletThreadTestBase : public PageTestBase,
                                   public ParametrizedModuleTestBase {
 public:
  explicit AudioWorkletThreadTestBase(bool use_top_level_await)
      : use_top_level_await_(use_top_level_await) {}

  void SetUp() override {
    ParametrizedModuleTestBase::SetUp(use_top_level_await_);
    PageTestBase::SetUp(IntSize());
    NavigateTo(KURL("https://example.com/"));
    reporting_proxy_ = std::make_unique<WorkerReportingProxy>();
  }

  void TearDown() override {
    OfflineAudioWorkletThread::ClearSharedBackingThread();
    RealtimeAudioWorkletThread::ClearSharedBackingThread();
    SemiRealtimeAudioWorkletThread::ClearSharedBackingThread();
    ParametrizedModuleTestBase::TearDown();
  }

  std::unique_ptr<WorkerThread> CreateAudioWorkletThread(
      bool has_realtime_constraint, bool is_top_level_frame) {
    std::unique_ptr<WorkerThread> thread =
        AudioWorkletMessagingProxy::CreateWorkletThreadWithConstraints(
            *reporting_proxy_,
            has_realtime_constraint,
            is_top_level_frame);
    StartBackingThread(thread.get());
    return thread;
  }

  // Attempts to run some simple script for |thread|.
  void CheckWorkletCanExecuteScript(WorkerThread* thread) {
    base::WaitableEvent wait_event;
    PostCrossThreadTask(
        *thread->GetWorkerBackingThread().BackingThread().GetTaskRunner(),
        FROM_HERE,
        CrossThreadBindOnce(&AudioWorkletThreadTestBase::ExecuteScriptInWorklet,
                            CrossThreadUnretained(this),
                            CrossThreadUnretained(thread),
                            CrossThreadUnretained(&wait_event)));
    wait_event.Wait();
  }

 private:
  void StartBackingThread(WorkerThread* thread) {
    LocalDOMWindow* window = GetFrame().DomWindow();
    thread->Start(
        std::make_unique<GlobalScopeCreationParams>(
            window->Url(), mojom::blink::ScriptType::kModule, "AudioWorklet",
            window->UserAgent(),
            window->GetFrame()->Loader().UserAgentMetadata(),
            nullptr /* web_worker_fetch_context */,
            Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
            window->GetReferrerPolicy(), window->GetSecurityOrigin(),
            window->IsSecureContext(), window->GetHttpsState(),
            nullptr /* worker_clients */, nullptr /* content_settings_client */,
            window->AddressSpace(), OriginTrialContext::GetTokens(window).get(),
            base::UnguessableToken::Create(), nullptr /* worker_settings */,
            mojom::blink::V8CacheOptions::kDefault,
            MakeGarbageCollected<WorkletModuleResponsesMap>(),
            mojo::NullRemote() /* browser_interface_broker */,
            BeginFrameProviderParams(), nullptr /* parent_permissions_policy */,
            window->GetAgentClusterID(), ukm::kInvalidSourceId,
            window->GetExecutionContextToken()),
        base::nullopt, std::make_unique<WorkerDevToolsParams>());
  }

  void ExecuteScriptInWorklet(WorkerThread* thread,
                              base::WaitableEvent* wait_event) {
    ScriptState* script_state =
        thread->GlobalScope()->ScriptController()->GetScriptState();
    EXPECT_TRUE(script_state);
    ScriptState::Scope scope(script_state);
    KURL js_url("https://example.com/worklet.js");
    v8::Local<v8::Module> module = ModuleTestBase::CompileModule(
        script_state->GetIsolate(), "var counter = 0; ++counter;", js_url);
    EXPECT_FALSE(module.IsEmpty());
    ScriptValue exception =
        ModuleRecord::Instantiate(script_state, module, js_url);
    EXPECT_TRUE(exception.IsEmpty());
    ScriptEvaluationResult result =
        JSModuleScript::CreateForTest(Modulator::From(script_state), module,
                                      js_url)
            ->RunScriptAndReturnValue();
    EXPECT_EQ(result.GetResultType(),
              ScriptEvaluationResult::ResultType::kSuccess);
    wait_event->Signal();
  }

  std::unique_ptr<WorkerReportingProxy> reporting_proxy_;
  bool use_top_level_await_;
};

class AudioWorkletThreadTest : public AudioWorkletThreadTestBase,
                               public testing::WithParamInterface<bool> {
 public:
  AudioWorkletThreadTest() : AudioWorkletThreadTestBase(GetParam()) {}
};

TEST_P(AudioWorkletThreadTest, Basic) {
  std::unique_ptr<WorkerThread> audio_worklet_thread =
      CreateAudioWorkletThread(true, true);
  CheckWorkletCanExecuteScript(audio_worklet_thread.get());
  audio_worklet_thread->Terminate();
  audio_worklet_thread->WaitForShutdownForTesting();
}

// Creates 2 different AudioWorkletThreads with different RT constraints.
// Checks if they are running on a different thread.
TEST_P(AudioWorkletThreadTest, CreateDifferentWorkletThreadsAndTerminate_1) {
  // Create RealtimeAudioWorkletThread.
  std::unique_ptr<WorkerThread> first_worklet_thread =
      CreateAudioWorkletThread(true, true);
  Thread* first_backing_thread =
      &first_worklet_thread->GetWorkerBackingThread().BackingThread();
  v8::Isolate* first_isolate = first_worklet_thread->GetIsolate();

  // Create OfflineAudioWorkletThread.
  std::unique_ptr<WorkerThread> second_worklet_thread =
      CreateAudioWorkletThread(false, true);
  Thread* second_backing_thread =
      &second_worklet_thread->GetWorkerBackingThread().BackingThread();
  v8::Isolate* second_isolate = second_worklet_thread->GetIsolate();

  // Check if they are two different threads, and two different v8::isolates.
  ASSERT_NE(first_backing_thread, second_backing_thread);
  ASSERT_NE(first_isolate, second_isolate);

  first_worklet_thread->Terminate();
  first_worklet_thread->WaitForShutdownForTesting();
  second_worklet_thread->Terminate();
  second_worklet_thread->WaitForShutdownForTesting();
}

// Creates 2 AudioWorkletThreads with RT constraint from 2 different
// originating frames. Checks if they are running on a different thread.
TEST_P(AudioWorkletThreadTest, CreateDifferentWorkletThreadsAndTerminate_2) {
  // Create an AudioWorkletThread from a main frame with RT constraint.
  std::unique_ptr<WorkerThread> first_worklet_thread =
      CreateAudioWorkletThread(true, true);
  Thread* first_backing_thread =
      &first_worklet_thread->GetWorkerBackingThread().BackingThread();
  v8::Isolate* first_isolate = first_worklet_thread->GetIsolate();

  // Create an AudioWorkletThread from a sub frame with RT constraint.
  std::unique_ptr<WorkerThread> second_worklet_thread =
      CreateAudioWorkletThread(true, false);
  Thread* second_backing_thread =
      &second_worklet_thread->GetWorkerBackingThread().BackingThread();
  v8::Isolate* second_isolate = second_worklet_thread->GetIsolate();

  // Check if they are two different threads, and two different v8::isolates.
  ASSERT_NE(first_backing_thread, second_backing_thread);
  ASSERT_NE(first_isolate, second_isolate);

  first_worklet_thread->Terminate();
  first_worklet_thread->WaitForShutdownForTesting();
  second_worklet_thread->Terminate();
  second_worklet_thread->WaitForShutdownForTesting();
}

// Instantiate tests once with TLA and once without:
INSTANTIATE_TEST_SUITE_P(AudioWorkletThreadTestGroup,
                         AudioWorkletThreadTest,
                         testing::Bool(),
                         ParametrizedModuleTestParamName());

class AudioWorkletThreadInteractionTest
    : public AudioWorkletThreadTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  AudioWorkletThreadInteractionTest()
      : AudioWorkletThreadTestBase(std::get<0>(GetParam())),
        has_realtime_constraint_(std::get<1>(GetParam())),
        is_top_level_frame_(std::get<2>(GetParam())) {}

 protected:
  const bool has_realtime_constraint_;
  const bool is_top_level_frame_;
};

TEST_P(AudioWorkletThreadInteractionTest, CreateSecondAndTerminateFirst) {
  // Create the first worklet and wait until it is initialized.
    std::unique_ptr<WorkerThread> first_worklet_thread =
        CreateAudioWorkletThread(has_realtime_constraint_, is_top_level_frame_);
    Thread* first_backing_thread =
        &first_worklet_thread->GetWorkerBackingThread().BackingThread();
    CheckWorkletCanExecuteScript(first_worklet_thread.get());
    v8::Isolate* first_isolate = first_worklet_thread->GetIsolate();
    ASSERT_TRUE(first_isolate);

    // Create the second worklet and immediately destroy the first worklet.
    std::unique_ptr<WorkerThread> second_worklet_thread =
        CreateAudioWorkletThread(has_realtime_constraint_, is_top_level_frame_);
    // We don't use terminateAndWait here to avoid forcible termination.
    first_worklet_thread->Terminate();
    first_worklet_thread->WaitForShutdownForTesting();

    // Wait until the second worklet is initialized. Verify that the second
    // worklet is using the same thread and Isolate as the first worklet.
    Thread* second_backing_thread =
        &second_worklet_thread->GetWorkerBackingThread().BackingThread();
    ASSERT_EQ(first_backing_thread, second_backing_thread);

    v8::Isolate* second_isolate = second_worklet_thread->GetIsolate();
    ASSERT_TRUE(second_isolate);
    EXPECT_EQ(first_isolate, second_isolate);

    // Verify that the worklet can still successfully execute script.
    CheckWorkletCanExecuteScript(second_worklet_thread.get());

    second_worklet_thread->Terminate();
    second_worklet_thread->WaitForShutdownForTesting();
}

TEST_P(AudioWorkletThreadInteractionTest, TerminateFirstAndCreateSecond) {
  // Create the first worklet, wait until it is initialized, and terminate it.
  std::unique_ptr<WorkerThread> worklet_thread =
      CreateAudioWorkletThread(has_realtime_constraint_, is_top_level_frame_);
  Thread* first_backing_thread =
      &worklet_thread->GetWorkerBackingThread().BackingThread();
  CheckWorkletCanExecuteScript(worklet_thread.get());

  // We don't use terminateAndWait here to avoid forcible termination.
  worklet_thread->Terminate();
  worklet_thread->WaitForShutdownForTesting();

  // Create the second worklet. The backing thread is same.
  worklet_thread =
      CreateAudioWorkletThread(has_realtime_constraint_, is_top_level_frame_);
  Thread* second_backing_thread =
      &worklet_thread->GetWorkerBackingThread().BackingThread();
  EXPECT_EQ(first_backing_thread, second_backing_thread);
  CheckWorkletCanExecuteScript(worklet_thread.get());

  worklet_thread->Terminate();
  worklet_thread->WaitForShutdownForTesting();
}

TEST_P(AudioWorkletThreadInteractionTest,
       CreatingSecondDuringTerminationOfFirst) {
  // Tests that v8::Isolate and WebThread are correctly set-up if a worklet is
  // created while another is terminating.
  std::unique_ptr<WorkerThread> first_worklet_thread =
      CreateAudioWorkletThread(has_realtime_constraint_, is_top_level_frame_);
  CheckWorkletCanExecuteScript(first_worklet_thread.get());
  v8::Isolate* first_isolate = first_worklet_thread->GetIsolate();
  ASSERT_TRUE(first_isolate);

  // Request termination of the first worklet and create the second worklet
  // as soon as possible. We don't wait for its termination.
  // Note: We rely on the assumption that the termination steps don't run
  // on the worklet thread so quickly. This could be a source of flakiness.
  first_worklet_thread->Terminate();

  std::unique_ptr<WorkerThread> second_worklet_thread =
      CreateAudioWorkletThread(has_realtime_constraint_, is_top_level_frame_);
  v8::Isolate* second_isolate = second_worklet_thread->GetIsolate();
  ASSERT_TRUE(second_isolate);

  ASSERT_EQ(first_isolate, second_isolate);

  // Verify that the isolate can run some scripts correctly in the second
  // worklet.
  CheckWorkletCanExecuteScript(second_worklet_thread.get());
  second_worklet_thread->Terminate();
  second_worklet_thread->WaitForShutdownForTesting();
}

INSTANTIATE_TEST_SUITE_P(AudioWorkletThreadInteractionTestGroup,
                         AudioWorkletThreadInteractionTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

struct ThreadPriorityTestParam {
  const bool use_top_level_await;
  const bool has_realtime_constraint;
  const bool is_top_level_frame;
  const bool is_enabled_by_finch;
  const bool is_enabled_by_flag;
  const base::ThreadPriority expected_priority;
};

constexpr ThreadPriorityTestParam kThreadPriorityTestParams[] = {
  // RT thread enabled by Finch.
  {true, true, true, true, true, base::ThreadPriority::REALTIME_AUDIO},
  {true, true, true, true, false, base::ThreadPriority::REALTIME_AUDIO},

  // RT thread disabled by Finch.
  {true, true, true, false, true, base::ThreadPriority::NORMAL},
  {true, true, true, false, false, base::ThreadPriority::NORMAL},

  // Non-main frame, RT thread enabled by Finch: depends the local flag.
  {true, true, false, true, true, base::ThreadPriority::REALTIME_AUDIO},
  {true, true, false, true, false, base::ThreadPriority::DISPLAY},

  // Non-main frame, RT thread disabled by Finch.
  {true, true, false, false, true, base::ThreadPriority::NORMAL},
  {true, true, false, false, false, base::ThreadPriority::NORMAL},

  // The OfflineAudioContext always uses a NORMAL priority thread.
  {true, false, true, true, true, base::ThreadPriority::NORMAL},
  {true, false, true, true, false, base::ThreadPriority::NORMAL},
  {true, false, true, false, true, base::ThreadPriority::NORMAL},
  {true, false, true, false, false, base::ThreadPriority::NORMAL},
  {true, false, false, true, true, base::ThreadPriority::NORMAL},
  {true, false, false, true, false, base::ThreadPriority::NORMAL},
  {true, false, false, false, true, base::ThreadPriority::NORMAL},
  {true, false, false, false, false, base::ThreadPriority::NORMAL},

  // Top-level await does not affect the test result.
  {false, true, true, true, true, base::ThreadPriority::REALTIME_AUDIO},
  {false, true, true, true, false, base::ThreadPriority::REALTIME_AUDIO},
  {false, true, true, false, true, base::ThreadPriority::NORMAL},
  {false, true, true, false, false, base::ThreadPriority::NORMAL},
  {false, true, false, true, true, base::ThreadPriority::REALTIME_AUDIO},
  {false, true, false, true, false, base::ThreadPriority::DISPLAY},
  {false, true, false, false, true, base::ThreadPriority::NORMAL},
  {false, true, false, false, false, base::ThreadPriority::NORMAL},
  {false, false, true, true, true, base::ThreadPriority::NORMAL},
  {false, false, true, true, false, base::ThreadPriority::NORMAL},
  {false, false, true, false, true, base::ThreadPriority::NORMAL},
  {false, false, true, false, false, base::ThreadPriority::NORMAL},
  {false, false, false, true, true, base::ThreadPriority::NORMAL},
  {false, false, false, true, false, base::ThreadPriority::NORMAL},
  {false, false, false, false, true, base::ThreadPriority::NORMAL},
  {false, false, false, false, false, base::ThreadPriority::NORMAL},
};

class AudioWorkletThreadPriorityTest
    : public AudioWorkletThreadTestBase,
      public testing::WithParamInterface<ThreadPriorityTestParam> {
 public:
  AudioWorkletThreadPriorityTest()
      : AudioWorkletThreadTestBase(GetParam().use_top_level_await) {}

  void InitWithRealtimePrioritySettings(bool is_enabled_by_finch,
                                        bool is_enabled_by_flag) {
    std::vector<base::Feature> enabled;
    std::vector<base::Feature> disabled;
    if (is_enabled_by_finch)
      enabled.push_back(features::kAudioWorkletThreadRealtimePriority);
    else
      disabled.push_back(features::kAudioWorkletThreadRealtimePriority);
    if (is_enabled_by_flag)
      enabled.push_back(features::kAudioWorkletRealtimeThread);
    else
      disabled.push_back(features::kAudioWorkletRealtimeThread);
    feature_list_.InitWithFeatures(enabled, disabled);
  }

  void CreateCheckThreadPriority(bool has_realtime_constraint,
                                 bool is_top_level_frame,
                                 base::ThreadPriority expected_priority) {
    std::unique_ptr<WorkerThread> audio_worklet_thread =
        CreateAudioWorkletThread(has_realtime_constraint, is_top_level_frame);
    WorkerThread* thread = audio_worklet_thread.get();
    base::WaitableEvent wait_event;
    PostCrossThreadTask(
        *thread->GetWorkerBackingThread().BackingThread().GetTaskRunner(),
        FROM_HERE,
        CrossThreadBindOnce(
            &AudioWorkletThreadPriorityTest::CheckThreadPriorityOnWorkerThread,
            CrossThreadUnretained(this),
            CrossThreadUnretained(thread),
            expected_priority,
            CrossThreadUnretained(&wait_event)));
    wait_event.Wait();
    audio_worklet_thread->Terminate();
    audio_worklet_thread->WaitForShutdownForTesting();
  }

 private:
  void CheckThreadPriorityOnWorkerThread(
      WorkerThread* thread,
      base::ThreadPriority expected_priority,
      base::WaitableEvent* wait_event) {
    ASSERT_TRUE(thread->IsCurrentThread());
    base::ThreadPriority actual_priority =
        base::PlatformThread::GetCurrentThreadPriority();

    // TODO(crbug.com/1022888): The worklet thread priority is always NORMAL
    // on OS_LINUX and OS_CHROMEOS regardless of the thread priority setting.
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
    if (expected_priority == base::ThreadPriority::REALTIME_AUDIO ||
        expected_priority == base::ThreadPriority::DISPLAY) {
      EXPECT_EQ(actual_priority, base::ThreadPriority::NORMAL);
    } else {
      EXPECT_EQ(actual_priority, expected_priority);
    }
#elif defined(OS_FUCHSIA)
    // The thread priority is no-op on Fuchsia. It's always NORMAL priority.
    // See crbug.com/1090245.
    EXPECT_EQ(actual_priority, base::ThreadPriority::NORMAL);
#else
    EXPECT_EQ(actual_priority, expected_priority);
#endif

    wait_event->Signal();
  }
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(AudioWorkletThreadPriorityTest, CheckThreadPriority) {
  const auto& test_param = GetParam();
  InitWithRealtimePrioritySettings(test_param.is_enabled_by_finch,
                                   test_param.is_enabled_by_flag);
  CreateCheckThreadPriority(test_param.has_realtime_constraint,
                            test_param.is_top_level_frame,
                            test_param.expected_priority);
}

INSTANTIATE_TEST_SUITE_P(AudioWorkletThreadPriorityTestGroup,
                         AudioWorkletThreadPriorityTest,
                         testing::ValuesIn(kThreadPriorityTestParams));

}  // namespace blink
