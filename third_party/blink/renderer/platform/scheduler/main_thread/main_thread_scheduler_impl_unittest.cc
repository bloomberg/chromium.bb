// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/sequence_manager/test/fake_task.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/task/task_executor.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "third_party/blink/public/common/page/launching_process_state.h"
#include "third_party/blink/public/platform/scheduler/web_widget_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/auto_advancing_virtual_time_domain.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/find_in_page_budget_pool_controller.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_task_queue_controller.h"
#include "third_party/blink/renderer/platform/scheduler/test/recording_task_time_observer.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "v8/include/v8.h"

using base::sequence_manager::TaskQueue;

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace main_thread_scheduler_impl_unittest {

using testing::InSequence;
using testing::Mock;
using testing::NiceMock;
using testing::NotNull;
using testing::Return;
using InputEventState = WebThreadScheduler::InputEventState;
using base::sequence_manager::FakeTask;
using base::sequence_manager::FakeTaskTiming;

class FakeInputEvent : public blink::WebInputEvent {
 public:
  explicit FakeInputEvent(blink::WebInputEvent::Type event_type,
                          int modifiers = WebInputEvent::kNoModifiers)
      : WebInputEvent(event_type,
                      modifiers,
                      WebInputEvent::GetStaticTimeStampForTests()) {}

  std::unique_ptr<WebInputEvent> Clone() const override {
    return std::make_unique<FakeInputEvent>(*this);
  }

  bool CanCoalesce(const blink::WebInputEvent& event) const override {
    return false;
  }

  void Coalesce(const WebInputEvent& event) override { NOTREACHED(); }
};

class FakeTouchEvent : public blink::WebTouchEvent {
 public:
  explicit FakeTouchEvent(blink::WebInputEvent::Type event_type,
                          DispatchType dispatch_type =
                              blink::WebInputEvent::DispatchType::kBlocking)
      : WebTouchEvent(event_type,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests()) {
    this->dispatch_type = dispatch_type;
  }
};

class FakeMouseWheelEvent : public blink::WebMouseWheelEvent {
 public:
  explicit FakeMouseWheelEvent(
      blink::WebInputEvent::Type event_type,
      DispatchType dispatch_type =
          blink::WebInputEvent::DispatchType::kBlocking)
      : WebMouseWheelEvent(event_type,
                           WebInputEvent::kNoModifiers,
                           WebInputEvent::GetStaticTimeStampForTests()) {
    this->dispatch_type = dispatch_type;
  }
};

void AppendToVectorTestTask(Vector<String>* vector, String value) {
  vector->push_back(value);
}

void AppendToVectorIdleTestTask(Vector<String>* vector,
                                String value,
                                base::TimeTicks deadline) {
  AppendToVectorTestTask(vector, value);
}

void NullTask() {}

void AppendToVectorReentrantTask(base::SingleThreadTaskRunner* task_runner,
                                 Vector<int>* vector,
                                 int* reentrant_count,
                                 int max_reentrant_count) {
  vector->push_back((*reentrant_count)++);
  if (*reentrant_count < max_reentrant_count) {
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(AppendToVectorReentrantTask,
                                         base::Unretained(task_runner), vector,
                                         reentrant_count, max_reentrant_count));
  }
}

void IdleTestTask(int* run_count,
                  base::TimeTicks* deadline_out,
                  base::TimeTicks deadline) {
  (*run_count)++;
  *deadline_out = deadline;
}

int g_max_idle_task_reposts = 2;

void RepostingIdleTestTask(SingleThreadIdleTaskRunner* idle_task_runner,
                           int* run_count,
                           base::TimeTicks deadline) {
  if ((*run_count + 1) < g_max_idle_task_reposts) {
    idle_task_runner->PostIdleTask(
        FROM_HERE,
        base::BindOnce(&RepostingIdleTestTask,
                       base::Unretained(idle_task_runner), run_count));
  }
  (*run_count)++;
}

void RepostingUpdateClockIdleTestTask(
    SingleThreadIdleTaskRunner* idle_task_runner,
    int* run_count,
    scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner,
    base::TimeDelta advance_time,
    Vector<base::TimeTicks>* deadlines,
    base::TimeTicks deadline) {
  if ((*run_count + 1) < g_max_idle_task_reposts) {
    idle_task_runner->PostIdleTask(
        FROM_HERE, base::BindOnce(&RepostingUpdateClockIdleTestTask,
                                  base::Unretained(idle_task_runner), run_count,
                                  test_task_runner, advance_time, deadlines));
  }
  deadlines->push_back(deadline);
  (*run_count)++;
  test_task_runner->AdvanceMockTickClock(advance_time);
}

void WillBeginFrameIdleTask(WebThreadScheduler* scheduler,
                            uint64_t sequence_number,
                            const base::TickClock* clock,
                            base::TimeTicks deadline) {
  scheduler->WillBeginFrame(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, sequence_number, clock->NowTicks(),
      base::TimeTicks(), base::TimeDelta::FromMilliseconds(1000),
      viz::BeginFrameArgs::NORMAL));
}

void UpdateClockToDeadlineIdleTestTask(
    scoped_refptr<base::TestMockTimeTaskRunner> task_runner,
    int* run_count,
    base::TimeTicks deadline) {
  task_runner->AdvanceMockTickClock(
      deadline - task_runner->GetMockTickClock()->NowTicks());
  (*run_count)++;
}

void PostingYieldingTestTask(MainThreadSchedulerImpl* scheduler,
                             base::SingleThreadTaskRunner* task_runner,
                             bool simulate_input,
                             bool* should_yield_before,
                             bool* should_yield_after) {
  *should_yield_before = scheduler->ShouldYieldForHighPriorityWork();
  task_runner->PostTask(FROM_HERE, base::BindOnce(NullTask));
  if (simulate_input) {
    scheduler->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  }
  *should_yield_after = scheduler->ShouldYieldForHighPriorityWork();
}

enum class SimulateInputType {
  kNone,
  kTouchStart,
  kTouchEnd,
  kGestureScrollBegin,
  kGestureScrollEnd
};

void AnticipationTestTask(MainThreadSchedulerImpl* scheduler,
                          SimulateInputType simulate_input,
                          bool* is_anticipated_before,
                          bool* is_anticipated_after) {
  *is_anticipated_before = scheduler->IsHighPriorityWorkAnticipated();
  switch (simulate_input) {
    case SimulateInputType::kNone:
      break;

    case SimulateInputType::kTouchStart:
      scheduler->DidHandleInputEventOnCompositorThread(
          FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
          InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
      break;

    case SimulateInputType::kTouchEnd:
      scheduler->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::Type::kTouchEnd),
          InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
      break;

    case SimulateInputType::kGestureScrollBegin:
      scheduler->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollBegin),
          InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
      break;

    case SimulateInputType::kGestureScrollEnd:
      scheduler->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollEnd),
          InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
      break;
  }
  *is_anticipated_after = scheduler->IsHighPriorityWorkAnticipated();
}

class MockPageSchedulerImpl : public PageSchedulerImpl {
 public:
  explicit MockPageSchedulerImpl(MainThreadSchedulerImpl* scheduler)
      : PageSchedulerImpl(nullptr, scheduler) {
    ON_CALL(*this, IsWaitingForMainFrameContentfulPaint)
        .WillByDefault(Return(true));
    ON_CALL(*this, IsWaitingForMainFrameMeaningfulPaint)
        .WillByDefault(Return(true));
    ON_CALL(*this, IsMainFrameLocal).WillByDefault(Return(true));
  }
  ~MockPageSchedulerImpl() override = default;

  MockPageSchedulerImpl(const MockPageSchedulerImpl&) = delete;
  MockPageSchedulerImpl& operator=(const MockPageSchedulerImpl&) = delete;

  MOCK_METHOD(bool, RequestBeginMainFrameNotExpected, (bool));
  MOCK_METHOD(bool, IsWaitingForMainFrameContentfulPaint, (), (const));
  MOCK_METHOD(bool, IsWaitingForMainFrameMeaningfulPaint, (), (const));
  MOCK_METHOD(bool, IsMainFrameLocal, (), (const));
};

class MainThreadSchedulerImplForTest : public MainThreadSchedulerImpl {
 public:
  using MainThreadSchedulerImpl::CompositorTaskQueue;
  using MainThreadSchedulerImpl::ControlTaskQueue;
  using MainThreadSchedulerImpl::DefaultTaskQueue;
  using MainThreadSchedulerImpl::EstimateLongestJankFreeTaskDuration;
  using MainThreadSchedulerImpl::OnIdlePeriodEnded;
  using MainThreadSchedulerImpl::OnIdlePeriodStarted;
  using MainThreadSchedulerImpl::OnPendingTasksChanged;
  using MainThreadSchedulerImpl::SetHaveSeenABlockingGestureForTesting;
  using MainThreadSchedulerImpl::V8TaskQueue;
  using MainThreadSchedulerImpl::VirtualTimeControlTaskQueue;

  MainThreadSchedulerImplForTest(
      std::unique_ptr<base::sequence_manager::SequenceManager> manager,
      base::Optional<base::Time> initial_virtual_time)
      : MainThreadSchedulerImpl(std::move(manager), initial_virtual_time),
        update_policy_count_(0) {}

  void UpdatePolicyLocked(UpdateType update_type) override {
    update_policy_count_++;
    MainThreadSchedulerImpl::UpdatePolicyLocked(update_type);

    String use_case = MainThreadSchedulerImpl::UseCaseToString(
        main_thread_only().current_use_case);
    if (main_thread_only().blocking_input_expected_soon) {
      use_cases_.push_back(use_case + " blocking input expected");
    } else {
      use_cases_.push_back(use_case);
    }
  }

  void OnQueueingTimeForWindowEstimated(base::TimeDelta queueing_time,
                                        bool is_disjoint_window) override {
    MainThreadSchedulerImpl::OnQueueingTimeForWindowEstimated(
        queueing_time, is_disjoint_window);
    expected_queueing_times_.push_back(queueing_time);
  }

  void EnsureUrgentPolicyUpdatePostedOnMainThread() {
    base::AutoLock lock(any_thread_lock_);
    MainThreadSchedulerImpl::EnsureUrgentPolicyUpdatePostedOnMainThread(
        FROM_HERE);
  }

  void ScheduleDelayedPolicyUpdate(base::TimeTicks now, base::TimeDelta delay) {
    delayed_update_policy_runner_.SetDeadline(FROM_HERE, delay, now);
  }

  bool BeginMainFrameOnCriticalPath() {
    base::AutoLock lock(any_thread_lock_);
    return any_thread().begin_main_frame_on_critical_path;
  }

  VirtualTimePolicy virtual_time_policy() const {
    return main_thread_only().virtual_time_policy;
  }

  const Vector<base::TimeDelta>& expected_queueing_times() const {
    return expected_queueing_times_;
  }

  void PerformMicrotaskCheckpoint() override {
    if (on_microtask_checkpoint_)
      std::move(on_microtask_checkpoint_).Run();
  }

  int update_policy_count_;
  Vector<String> use_cases_;
  Vector<base::TimeDelta> expected_queueing_times_;
  base::OnceClosure on_microtask_checkpoint_;
};

// Lets gtest print human readable Policy values.
::std::ostream& operator<<(::std::ostream& os, const UseCase& use_case) {
  return os << MainThreadSchedulerImpl::UseCaseToString(use_case);
}

class MainThreadSchedulerImplTest : public testing::Test {
 public:
  MainThreadSchedulerImplTest(std::vector<base::Feature> features_to_enable,
                              std::vector<base::Feature> features_to_disable) {
    feature_list_.InitWithFeatures(features_to_enable, features_to_disable);
  }

  MainThreadSchedulerImplTest() : MainThreadSchedulerImplTest({}, {}) {}

  ~MainThreadSchedulerImplTest() override = default;

  void SetUp() override {
    CreateTestTaskRunner();
    Initialize(std::make_unique<MainThreadSchedulerImplForTest>(
        base::sequence_manager::SequenceManagerForTest::Create(
            nullptr, test_task_runner_, test_task_runner_->GetMockTickClock(),
            base::sequence_manager::SequenceManager::Settings::Builder()
                .SetRandomisedSamplingEnabled(true)
                .Build()),
        base::nullopt));
    if (initially_ensure_usecase_none_)
      EnsureUseCaseNone();
  }

  void CreateTestTaskRunner() {
    test_task_runner_ = base::WrapRefCounted(new base::TestMockTimeTaskRunner(
        base::TestMockTimeTaskRunner::Type::kBoundToThread));
    // A null clock triggers some assertions.
    test_task_runner_->AdvanceMockTickClock(
        base::TimeDelta::FromMilliseconds(5));
  }

  void Initialize(std::unique_ptr<MainThreadSchedulerImplForTest> scheduler) {
    scheduler_ = std::move(scheduler);

    if (kLaunchingProcessIsBackgrounded) {
      scheduler_->SetRendererBackgrounded(false);
      // Reset the policy count as foregrounding would force an initial update.
      scheduler_->update_policy_count_ = 0;
      scheduler_->use_cases_.clear();
    }

    default_task_runner_ = scheduler_->DefaultTaskQueue()->task_runner();
    compositor_task_runner_ = scheduler_->CompositorTaskQueue()->task_runner();
    idle_task_runner_ = scheduler_->IdleTaskRunner();
    v8_task_runner_ = scheduler_->V8TaskQueue()->task_runner();

    page_scheduler_ =
        std::make_unique<NiceMock<MockPageSchedulerImpl>>(scheduler_.get());
    main_frame_scheduler_ =
        FrameSchedulerImpl::Create(page_scheduler_.get(), nullptr, nullptr,
                                   FrameScheduler::FrameType::kMainFrame);

    widget_scheduler_ = scheduler_->CreateWidgetScheduler();
    input_task_runner_ = widget_scheduler_->InputTaskRunner();

    loading_control_task_runner_ =
        main_frame_scheduler_->FrameTaskQueueControllerForTest()
            ->GetTaskQueue(
                main_frame_scheduler_->LoadingControlTaskQueueTraits())
            ->task_runner();
    timer_task_runner_ = timer_task_queue()->task_runner();
    find_in_page_task_runner_ = main_frame_scheduler_->GetTaskRunner(
        blink::TaskType::kInternalFindInPage);
  }

  TaskQueue* loading_task_queue() {
    auto queue_traits = FrameSchedulerImpl::LoadingTaskQueueTraits();
    return main_frame_scheduler_->FrameTaskQueueControllerForTest()
        ->GetTaskQueue(queue_traits)
        .get();
  }

  TaskQueue* timer_task_queue() {
    auto* frame_task_queue_controller =
        main_frame_scheduler_->FrameTaskQueueControllerForTest();
    return frame_task_queue_controller
        ->GetTaskQueue(main_frame_scheduler_->ThrottleableTaskQueueTraits())
        .get();
  }

  MainThreadTaskQueue* find_in_page_task_queue() {
    auto* frame_task_queue_controller =
        main_frame_scheduler_->FrameTaskQueueControllerForTest();

    return frame_task_queue_controller
        ->GetTaskQueue(main_frame_scheduler_->FindInPageTaskQueueTraits())
        .get();
  }

  void TearDown() override {
    widget_scheduler_.reset();
    main_frame_scheduler_.reset();
    page_scheduler_.reset();
    scheduler_->Shutdown();
    base::RunLoop().RunUntilIdle();
    scheduler_.reset();
  }

  virtual base::TimeTicks Now() {
    CHECK(test_task_runner_);
    return test_task_runner_->GetMockTickClock()->NowTicks();
  }

  void AdvanceMockTickClockTo(base::TimeTicks time) {
    CHECK(test_task_runner_);
    CHECK_LE(Now(), time);
    test_task_runner_->AdvanceMockTickClock(time - Now());
  }

  void AdvanceMockTickClockBy(base::TimeDelta delta) {
    CHECK(test_task_runner_);
    CHECK_LE(base::TimeDelta(), delta);
    test_task_runner_->AdvanceMockTickClock(delta);
  }

  void DoMainFrame() {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
        viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = false;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidCommitFrameToCompositor();
  }

  void DoMainFrameOnCriticalPath() {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
        viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
  }

  void ForceBlockingInputToBeExpectedSoon() {
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollEnd),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    test_task_runner_->AdvanceMockTickClock(
        priority_escalation_after_input_duration() * 2);
    scheduler_->ForceUpdatePolicy();
  }

  void SimulateExpensiveTasks(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
    // Simulate a bunch of expensive tasks.
    for (int i = 0; i < 10; i++) {
      task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(&base::TestMockTimeTaskRunner::AdvanceMockTickClock,
                         test_task_runner_,
                         base::TimeDelta::FromMilliseconds(500)));
    }
    test_task_runner_->FastForwardUntilNoTasksRemain();
  }

  enum class TouchEventPolicy {
    kSendTouchStart,
    kDontSendTouchStart,
  };

  void SimulateCompositorGestureStart(TouchEventPolicy touch_event_policy) {
    if (touch_event_policy == TouchEventPolicy::kSendTouchStart) {
      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
          InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
          InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
          InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    }
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollBegin),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  }

  // Simulate a gesture where there is an active compositor scroll, but no
  // scroll updates are generated. Instead, the main thread handles
  // non-canceleable touch events, making this an effectively main thread
  // driven gesture.
  void SimulateMainThreadGestureWithoutScrollUpdates() {
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollBegin),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  }

  // Simulate a gesture where the main thread handles touch events but does not
  // preventDefault(), allowing the gesture to turn into a compositor driven
  // gesture. This function also verifies the necessary policy updates are
  // scheduled.
  void SimulateMainThreadGestureWithoutPreventDefault() {
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);

    // Touchstart policy update.
    EXPECT_TRUE(scheduler_->PolicyNeedsUpdateForTesting());
    EXPECT_EQ(UseCase::kTouchstart, ForceUpdatePolicyAndGetCurrentUseCase());
    EXPECT_FALSE(scheduler_->PolicyNeedsUpdateForTesting());

    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureTapCancel),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollBegin),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

    // Main thread gesture policy update.
    EXPECT_TRUE(scheduler_->PolicyNeedsUpdateForTesting());
    EXPECT_EQ(UseCase::kMainThreadCustomInputHandling,
              ForceUpdatePolicyAndGetCurrentUseCase());
    EXPECT_FALSE(scheduler_->PolicyNeedsUpdateForTesting());

    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kTouchScrollStarted),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);

    // Compositor thread gesture policy update.
    EXPECT_TRUE(scheduler_->PolicyNeedsUpdateForTesting());
    EXPECT_EQ(UseCase::kCompositorGesture,
              ForceUpdatePolicyAndGetCurrentUseCase());
    EXPECT_FALSE(scheduler_->PolicyNeedsUpdateForTesting());
  }

  void SimulateMainThreadGestureStart(TouchEventPolicy touch_event_policy,
                                      blink::WebInputEvent::Type gesture_type) {
    if (touch_event_policy == TouchEventPolicy::kSendTouchStart) {
      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
          InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnMainThread(
          FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
          WebInputEventResult::kHandledSystem);

      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
          InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnMainThread(
          FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
          WebInputEventResult::kHandledSystem);

      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
          InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnMainThread(
          FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
          WebInputEventResult::kHandledSystem);
    }
    if (gesture_type != blink::WebInputEvent::Type::kUndefined) {
      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(gesture_type),
          InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnMainThread(
          FakeInputEvent(gesture_type), WebInputEventResult::kHandledSystem);
    }
  }

  void SimulateMainThreadInputHandlingCompositorTask(
      base::TimeDelta begin_main_frame_duration) {
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    test_task_runner_->AdvanceMockTickClock(begin_main_frame_duration);
    scheduler_->DidHandleInputEventOnMainThread(
        FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
        WebInputEventResult::kHandledApplication);
    scheduler_->DidCommitFrameToCompositor();
  }

  void SimulateMainThreadCompositorTask(
      base::TimeDelta begin_main_frame_duration) {
    test_task_runner_->AdvanceMockTickClock(begin_main_frame_duration);
    scheduler_->DidCommitFrameToCompositor();
  }

  void SimulateMainThreadCompositorAndQuitRunLoopTask(
      base::TimeDelta begin_main_frame_duration) {
    SimulateMainThreadCompositorTask(begin_main_frame_duration);
    base::RunLoop().Quit();
  }

  void SimulateTimerTask(base::TimeDelta duration) {
    test_task_runner_->AdvanceMockTickClock(duration);
    simulate_timer_task_ran_ = true;
  }

  void EnableIdleTasks() { DoMainFrame(); }

  UseCase CurrentUseCase() {
    return scheduler_->main_thread_only().current_use_case;
  }

  UseCase ForceUpdatePolicyAndGetCurrentUseCase() {
    scheduler_->ForceUpdatePolicy();
    return scheduler_->main_thread_only().current_use_case;
  }

  RAILMode GetRAILMode() {
    return scheduler_->main_thread_only().current_policy.rail_mode();
  }

  bool BlockingInputExpectedSoon() {
    return scheduler_->main_thread_only().blocking_input_expected_soon;
  }

  base::TimeTicks EstimatedNextFrameBegin() {
    return scheduler_->main_thread_only().estimated_next_frame_begin;
  }

  bool HaveSeenABlockingGesture() {
    base::AutoLock lock(scheduler_->any_thread_lock_);
    return scheduler_->any_thread().have_seen_a_blocking_gesture;
  }

  void AdvanceTimeWithTask(double duration_seconds) {
    base::TimeDelta duration = base::TimeDelta::FromSecondsD(duration_seconds);
    RunTask(base::BindOnce(
        [](scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner,
           base::TimeDelta duration) {
          test_task_runner->AdvanceMockTickClock(duration);
        },
        test_task_runner_, duration));
  }

  void RunTask(base::OnceClosure task) {
    scoped_refptr<MainThreadTaskQueue> fake_queue =
        scheduler_->NewLoadingTaskQueue(
            MainThreadTaskQueue::QueueType::kFrameLoading, nullptr);

    base::TimeTicks start = Now();
    FakeTask fake_task;
    fake_task.set_enqueue_order(
        base::sequence_manager::EnqueueOrder::FromIntForTesting(42));
    scheduler_->OnTaskStarted(fake_queue.get(), fake_task,
                              FakeTaskTiming(start, base::TimeTicks()));
    std::move(task).Run();
    base::TimeTicks end = Now();
    FakeTaskTiming task_timing(start, end);
    scheduler_->OnTaskCompleted(fake_queue->weak_ptr_factory_.GetWeakPtr(),
                                fake_task, &task_timing, nullptr);
  }

  void RunSlowCompositorTask() {
    // Run a long compositor task so that compositor tasks appear to be running
    // slow and thus compositor tasks will not be prioritized.
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &MainThreadSchedulerImplTest::SimulateMainThreadCompositorTask,
            base::Unretained(this), base::TimeDelta::FromMilliseconds(1000)));
    base::RunLoop().RunUntilIdle();
  }

  // Helper for posting several tasks of specific types. |task_descriptor| is a
  // string with space delimited task identifiers. The first letter of each
  // task identifier specifies the task type:
  // - 'D': Default task
  // - 'C': Compositor task
  // - 'P': Input task
  // - 'L': Loading task
  // - 'M': Loading Control task
  // - 'I': Idle task
  // - 'T': Timer task
  // - 'V': kV8 task
  void PostTestTasks(Vector<String>* run_order, const String& task_descriptor) {
    std::istringstream stream(task_descriptor.Utf8());
    while (!stream.eof()) {
      std::string task;
      stream >> task;
      switch (task[0]) {
        case 'D':
          default_task_runner_->PostTask(
              FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        case 'C':
          compositor_task_runner_->PostTask(
              FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        case 'P':
          input_task_runner_->PostTask(
              FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        case 'L':
          loading_task_queue()->task_runner()->PostTask(
              FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        case 'M':
          loading_control_task_runner_->PostTask(
              FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        case 'I':
          idle_task_runner_->PostIdleTask(
              FROM_HERE, base::BindOnce(&AppendToVectorIdleTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        case 'T':
          timer_task_runner_->PostTask(
              FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        case 'V':
          v8_task_runner_->PostTask(
              FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        case 'F':
          find_in_page_task_runner_->PostTask(
              FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        default:
          NOTREACHED();
      }
    }
  }

  void EnsureUseCaseNone() {
    // Make sure we're not in UseCase::kLoading.
    ON_CALL(*page_scheduler_, IsWaitingForMainFrameContentfulPaint)
        .WillByDefault(Return(false));
    ON_CALL(*page_scheduler_, IsWaitingForMainFrameMeaningfulPaint)
        .WillByDefault(Return(false));

    EXPECT_EQ(ForceUpdatePolicyAndGetCurrentUseCase(), UseCase::kNone);

    // Don't count the above policy change.
    scheduler_->update_policy_count_ = 0;
    scheduler_->use_cases_.clear();
  }

 protected:
  static base::TimeDelta priority_escalation_after_input_duration() {
    return base::TimeDelta::FromMilliseconds(
        UserModel::kGestureEstimationLimitMillis);
  }

  static base::TimeDelta subsequent_input_expected_after_input_duration() {
    return base::TimeDelta::FromMilliseconds(
        UserModel::kExpectSubsequentGestureMillis);
  }

  static base::TimeDelta maximum_idle_period_duration() {
    return base::TimeDelta::FromMilliseconds(
        IdleHelper::kMaximumIdlePeriodMillis);
  }

  static base::TimeDelta end_idle_when_hidden_delay() {
    return base::TimeDelta::FromMilliseconds(
        MainThreadSchedulerImpl::kEndIdleWhenHiddenDelayMillis);
  }

  static base::TimeDelta rails_response_time() {
    return base::TimeDelta::FromMilliseconds(
        MainThreadSchedulerImpl::kRailsResponseTimeMillis);
  }

  template <typename E>
  static void CallForEachEnumValue(E first,
                                   E last,
                                   const char* (*function)(E)) {
    for (E val = first; val < last;
         val = static_cast<E>(static_cast<int>(val) + 1)) {
      (*function)(val);
    }
  }

  static void CheckAllUseCaseToString() {
    CallForEachEnumValue<UseCase>(UseCase::kFirstUseCase, UseCase::kCount,
                                  &MainThreadSchedulerImpl::UseCaseToString);
  }

  static scoped_refptr<TaskQueue> ThrottleableTaskQueue(
      FrameSchedulerImpl* scheduler) {
    auto* frame_task_queue_controller =
        scheduler->FrameTaskQueueControllerForTest();
    auto queue_traits = FrameSchedulerImpl::ThrottleableTaskQueueTraits();
    return frame_task_queue_controller->GetTaskQueue(queue_traits);
  }

  QueueingTimeEstimator* queueing_time_estimator() {
    return &scheduler_->queueing_time_estimator_;
  }

  base::test::ScopedFeatureList feature_list_;

  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;

  std::unique_ptr<MainThreadSchedulerImplForTest> scheduler_;
  std::unique_ptr<MockPageSchedulerImpl> page_scheduler_;
  std::unique_ptr<FrameSchedulerImpl> main_frame_scheduler_;
  std::unique_ptr<WebWidgetScheduler> widget_scheduler_;

  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> loading_control_task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> timer_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> v8_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> find_in_page_task_runner_;
  bool simulate_timer_task_ran_;
  bool initially_ensure_usecase_none_ = true;
  uint64_t next_begin_frame_number_ = viz::BeginFrameArgs::kStartingFrameNumber;

  DISALLOW_COPY_AND_ASSIGN(MainThreadSchedulerImplTest);
};

TEST_F(MainThreadSchedulerImplTest, TestPostDefaultTask) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 D2 D3 D4");

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "D2", "D3", "D4"));
}

TEST_F(MainThreadSchedulerImplTest, TestPostDefaultAndCompositor) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 C1 P1");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::Contains("D1"));
  EXPECT_THAT(run_order, testing::Contains("C1"));
  EXPECT_THAT(run_order, testing::Contains("P1"));
}

TEST_F(MainThreadSchedulerImplTest, TestRentrantTask) {
  int count = 0;
  Vector<int> run_order;
  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(AppendToVectorReentrantTask,
                                base::RetainedRef(default_task_runner_),
                                &run_order, &count, 5));
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, testing::ElementsAre(0, 1, 2, 3, 4));
}

TEST_F(MainThreadSchedulerImplTest, TestPostIdleTask) {
  int run_count = 0;
  base::TimeTicks expected_deadline =
      Now() + base::TimeDelta::FromMilliseconds(2300);
  base::TimeTicks deadline_in_task;

  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(100));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Shouldn't run yet as no WillBeginFrame.

  scheduler_->WillBeginFrame(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::TimeDelta::FromMilliseconds(1000),
      viz::BeginFrameArgs::NORMAL));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Shouldn't run as no DidCommitFrameToCompositor.

  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(1200));
  scheduler_->DidCommitFrameToCompositor();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, run_count);  // We missed the deadline.

  scheduler_->WillBeginFrame(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::TimeDelta::FromMilliseconds(1000),
      viz::BeginFrameArgs::NORMAL));
  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(800));
  scheduler_->DidCommitFrameToCompositor();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, run_count);
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(MainThreadSchedulerImplTest, TestRepostingIdleTask) {
  int run_count = 0;

  g_max_idle_task_reposts = 2;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&RepostingIdleTestTask,
                     base::RetainedRef(idle_task_runner_), &run_count));
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, run_count);

  // Reposted tasks shouldn't run until next idle period.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, run_count);

  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, run_count);
}

TEST_F(MainThreadSchedulerImplTest, TestIdleTaskExceedsDeadline) {
  int run_count = 0;

  // Post two UpdateClockToDeadlineIdleTestTask tasks.
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&UpdateClockToDeadlineIdleTestTask,
                                test_task_runner_, &run_count));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&UpdateClockToDeadlineIdleTestTask,
                                test_task_runner_, &run_count));

  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  // Only the first idle task should execute since it's used up the deadline.
  EXPECT_EQ(1, run_count);

  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  // Second task should be run on the next idle period.
  EXPECT_EQ(2, run_count);
}

TEST_F(MainThreadSchedulerImplTest, TestDelayedEndIdlePeriodCanceled) {
  int run_count = 0;

  base::TimeTicks deadline_in_task;
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));

  // Trigger the beginning of an idle period for 1000ms.
  scheduler_->WillBeginFrame(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::TimeDelta::FromMilliseconds(1000),
      viz::BeginFrameArgs::NORMAL));
  DoMainFrame();

  // End the idle period early (after 500ms), and send a WillBeginFrame which
  // specifies that the next idle period should end 1000ms from now.
  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(500));
  scheduler_->WillBeginFrame(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::TimeDelta::FromMilliseconds(1000),
      viz::BeginFrameArgs::NORMAL));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Not currently in an idle period.

  // Trigger the start of the idle period before the task to end the previous
  // idle period has been triggered.
  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(400));
  scheduler_->DidCommitFrameToCompositor();

  // Post a task which simulates running until after the previous end idle
  // period delayed task was scheduled for
  scheduler_->DefaultTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(NullTask));
  test_task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(300));
  EXPECT_EQ(1, run_count);  // We should still be in the new idle period.
}

TEST_F(MainThreadSchedulerImplTest, TestDefaultPolicy) {
  EnsureUseCaseNone();

  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 P1 C1 D2 P2 C2");

  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  // High-priority input is enabled and input tasks are processed first.
  // One compositing event is prioritized after an input event but still
  // has lower priority than input event.
  EXPECT_THAT(run_order, testing::ElementsAre("P1", "P2", "L1", "D1", "C1",
                                              "D2", "C2", "I1"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest, TestDefaultPolicyWithSlowCompositor) {
  RunSlowCompositorTask();

  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 P1 D2 C2");

  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  // Even with slow compositor input tasks are handled first.
  EXPECT_THAT(run_order,
              testing::ElementsAre("P1", "L1", "D1", "C1", "D2", "C2", "I1"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_CompositorHandlesInput_WithTouchHandler) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EnableIdleTasks();
  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("L1", "D1", "D2", "C1", "C2", "I1"));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_MainThreadHandlesInput_WithoutScrollUpdates) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EnableIdleTasks();
  SimulateMainThreadGestureWithoutScrollUpdates();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("C1", "C2", "L1", "D1", "D2", "I1"));
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_MainThreadHandlesInput_WithoutPreventDefault) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EnableIdleTasks();
  SimulateMainThreadGestureWithoutPreventDefault();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("L1", "D1", "D2", "C1", "C2", "I1"));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_CompositorHandlesInput_LongGestureDuration) {
  EnableIdleTasks();
  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);

  base::TimeTicks loop_end_time =
      Now() + base::TimeDelta::FromMilliseconds(
                  UserModel::kMedianGestureDurationMillis * 2);

  // The UseCase::kCompositorGesture usecase initially deprioritizes
  // compositor tasks (see
  // TestCompositorPolicy_CompositorHandlesInput_WithTouchHandler) but if the
  // gesture is long enough, compositor tasks get prioritized again.
  while (Now() < loop_end_time) {
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    test_task_runner_->AdvanceMockTickClock(
        base::TimeDelta::FromMilliseconds(16));
    base::RunLoop().RunUntilIdle();
  }

  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "L1", "D1", "D2"));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_CompositorHandlesInput_WithoutTouchHandler) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  EnableIdleTasks();
  SimulateCompositorGestureStart(TouchEventPolicy::kDontSendTouchStart);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("L1", "D1", "D2", "C1", "C2", "I1"));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_MainThreadHandlesInput_WithTouchHandler) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EnableIdleTasks();
  SimulateMainThreadGestureStart(
      TouchEventPolicy::kSendTouchStart,
      blink::WebInputEvent::Type::kGestureScrollBegin);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("C1", "C2", "L1", "D1", "D2", "I1"));
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureFlingStart),
      WebInputEventResult::kHandledSystem);
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_MainThreadHandlesInput_WithoutTouchHandler) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  EnableIdleTasks();
  SimulateMainThreadGestureStart(
      TouchEventPolicy::kDontSendTouchStart,
      blink::WebInputEvent::Type::kGestureScrollBegin);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("C1", "C2", "L1", "D1", "D2", "I1"));
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureFlingStart),
      WebInputEventResult::kHandledSystem);
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_MainThreadHandlesInput_SingleEvent_PreventDefault) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      WebInputEventResult::kHandledApplication);
  base::RunLoop().RunUntilIdle();
  // Because the main thread is performing custom input handling, we let all
  // tasks run. However compositing tasks are still given priority.
  EXPECT_THAT(run_order,
              testing::ElementsAre("C1", "C2", "L1", "D1", "D2", "I1"));
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());
}

TEST_F(
    MainThreadSchedulerImplTest,
    TestCompositorPolicy_MainThreadHandlesInput_SingleEvent_NoPreventDefault) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      WebInputEventResult::kHandledSystem);
  base::RunLoop().RunUntilIdle();
  // Because we are still waiting for the touchstart to be processed,
  // non-essential tasks like loading tasks are blocked.
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1", "D2", "I1"));
  EXPECT_EQ(UseCase::kTouchstart, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest, TestCompositorPolicy_DidAnimateForInput) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  scheduler_->DidAnimateForInputOnCompositorThread();
  // Note DidAnimateForInputOnCompositorThread does not by itself trigger a
  // policy update.
  EXPECT_EQ(UseCase::kCompositorGesture,
            ForceUpdatePolicyAndGetCurrentUseCase());
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "D2", "C1", "C2", "I1"));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest, Navigation_ResetsTaskCostEstimations) {
  Vector<String> run_order;

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  SimulateExpensiveTasks(timer_task_runner_);
  DoMainFrame();
  // A navigation occurs which creates a new Document thus resetting the task
  // cost estimations.
  scheduler_->DidStartProvisionalLoad(true);
  SimulateMainThreadGestureStart(
      TouchEventPolicy::kSendTouchStart,
      blink::WebInputEvent::Type::kGestureScrollUpdate);

  PostTestTasks(&run_order, "C1 T1");

  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, testing::ElementsAre("C1", "T1"));
}

TEST_F(MainThreadSchedulerImplTest, TestTouchstartPolicy_Compositor) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 D1 C1 D2 C2 T1 T2");

  // Observation of touchstart should defer execution of timer, idle and loading
  // tasks.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1", "D2"));

  // Animation or meta events like TapDown/FlingCancel shouldn't affect the
  // priority.
  run_order.clear();
  scheduler_->DidAnimateForInputOnCompositorThread();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureFlingCancel),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureTapDown),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());

  // Action events like ScrollBegin will kick us back into compositor priority,
  // allowing service of the timer, loading and idle queues.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollBegin),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, testing::ElementsAre("L1", "T1", "T2"));
}

TEST_F(MainThreadSchedulerImplTest, TestTouchstartPolicy_MainThread) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 D1 C1 D2 C2 T1 T2");

  // Observation of touchstart should defer execution of timer, idle and loading
  // tasks.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      WebInputEventResult::kHandledSystem);
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1", "D2"));

  // Meta events like TapDown/FlingCancel shouldn't affect the priority.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureFlingCancel),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureFlingCancel),
      WebInputEventResult::kHandledSystem);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureTapDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureTapDown),
      WebInputEventResult::kHandledSystem);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());

  // Action events like ScrollBegin will kick us back into compositor priority,
  // allowing service of the timer, loading and idle queues.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollBegin),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollBegin),
      WebInputEventResult::kHandledSystem);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, testing::ElementsAre("L1", "T1", "T2"));
}

class DefaultUseCaseTest : public MainThreadSchedulerImplTest {
 public:
  DefaultUseCaseTest() { initially_ensure_usecase_none_ = false; }
};

TEST_F(DefaultUseCaseTest, InitiallyInEarlyLoadingUseCase) {
  scheduler_->OnMainFramePaint();

  // Should be early loading by default.
  EXPECT_EQ(UseCase::kEarlyLoading, ForceUpdatePolicyAndGetCurrentUseCase());

  ON_CALL(*page_scheduler_, IsWaitingForMainFrameContentfulPaint)
      .WillByDefault(Return(false));
  scheduler_->OnMainFramePaint();
  EXPECT_EQ(UseCase::kLoading, CurrentUseCase());

  ON_CALL(*page_scheduler_, IsWaitingForMainFrameMeaningfulPaint)
      .WillByDefault(Return(false));
  scheduler_->OnMainFramePaint();
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

class PrioritizeCompositingAndLoadingInUseCaseLoadingTest
    : public MainThreadSchedulerImplTest {
 public:
  PrioritizeCompositingAndLoadingInUseCaseLoadingTest()
      : MainThreadSchedulerImplTest(
            {kPrioritizeCompositingAndLoadingDuringEarlyLoading},
            {}) {}
};

TEST_F(PrioritizeCompositingAndLoadingInUseCaseLoadingTest, LoadingUseCase) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 T1 L1 D2 C2 T2 L2");

  ON_CALL(*page_scheduler_, IsWaitingForMainFrameContentfulPaint)
      .WillByDefault(Return(true));
  ON_CALL(*page_scheduler_, IsWaitingForMainFrameMeaningfulPaint)
      .WillByDefault(Return(true));
  scheduler_->DidStartProvisionalLoad(true);
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();

  // In early loading policy, loading and composting tasks are prioritized over
  // other tasks.
  String early_loading_policy_expected[] = {"C1", "L1", "C2", "L2", "D1",
                                            "T1", "D2", "T2", "I1"};
  EXPECT_THAT(run_order,
              testing::ElementsAreArray(early_loading_policy_expected));
  EXPECT_EQ(UseCase::kEarlyLoading, ForceUpdatePolicyAndGetCurrentUseCase());

  // After OnMainFrameFirstContentfulPaint we should transition to
  // UseCase::kLoading.
  ON_CALL(*page_scheduler_, IsWaitingForMainFrameContentfulPaint)
      .WillByDefault(Return(false));
  scheduler_->OnMainFramePaint();

  run_order.clear();
  PostTestTasks(&run_order, "I1 D1 C1 T1 L1 D2 C2 T2 L2");
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();

  String default_order_expected[] = {"D1", "C1", "T1", "L1", "D2",
                                     "C2", "T2", "L2", "I1"};
  EXPECT_THAT(run_order, testing::ElementsAreArray(default_order_expected));
  EXPECT_EQ(UseCase::kLoading, CurrentUseCase());

  // Advance 15s and try again, the loading policy should have ended and the
  // task order should return to the NONE use case where loading tasks are no
  // longer prioritized.
  ON_CALL(*page_scheduler_, IsWaitingForMainFrameMeaningfulPaint)
      .WillByDefault(Return(false));
  scheduler_->OnMainFramePaint();
  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(150000));
  run_order.clear();
  PostTestTasks(&run_order, "I1 D1 C1 T1 L1 D2 C2 T2 L2");
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, testing::ElementsAreArray(default_order_expected));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       EventConsumedOnCompositorThread_IgnoresMouseMove_WhenMouseUp) {
  RunSlowCompositorTask();

  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kMouseMove),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "C1", "D2", "C2", "I1"));
}

TEST_F(MainThreadSchedulerImplTest,
       EventForwardedToMainThread_IgnoresMouseMove_WhenMouseUp) {
  RunSlowCompositorTask();

  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kMouseMove),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "C1", "D2", "C2", "I1"));
}

TEST_F(MainThreadSchedulerImplTest,
       EventConsumedOnCompositorThread_MouseMove_WhenMouseDown) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  // Note that currently the compositor will never consume mouse move events,
  // but this test reflects what should happen if that was the case.
  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kMouseMove,
                     blink::WebInputEvent::kLeftButtonDown),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks deprioritized.
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "D2", "C1", "C2", "I1"));
}

TEST_F(MainThreadSchedulerImplTest,
       EventForwardedToMainThread_MouseMove_WhenMouseDown) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kMouseMove,
                     blink::WebInputEvent::kLeftButtonDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are prioritized.
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1", "D2", "I1"));
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kMouseMove,
                     blink::WebInputEvent::kLeftButtonDown),
      WebInputEventResult::kHandledSystem);
}

TEST_F(MainThreadSchedulerImplTest,
       EventForwardedToMainThread_MouseMove_WhenMouseDown_AfterMouseWheel) {
  // Simulate a main thread driven mouse wheel scroll gesture.
  SimulateMainThreadGestureStart(
      TouchEventPolicy::kSendTouchStart,
      blink::WebInputEvent::Type::kGestureScrollUpdate);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(BlockingInputExpectedSoon());
  EXPECT_EQ(UseCase::kMainThreadGesture, CurrentUseCase());

  // Now start a main thread mouse touch gesture. It should be detected as main
  // thread custom input handling.
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");
  EnableIdleTasks();

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kMouseDown,
                     blink::WebInputEvent::kLeftButtonDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kMouseMove,
                     blink::WebInputEvent::kLeftButtonDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());

  // Note compositor tasks are prioritized.
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1", "D2", "I1"));
}

TEST_F(MainThreadSchedulerImplTest, EventForwardedToMainThread_MouseClick) {
  // A mouse click should be detected as main thread input handling, which means
  // we won't try to defer expensive tasks because of one. We can, however,
  // prioritize compositing/input handling.
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");
  EnableIdleTasks();

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kMouseDown,
                     blink::WebInputEvent::kLeftButtonDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kMouseUp,
                     blink::WebInputEvent::kLeftButtonDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());

  // Note compositor tasks are prioritized.
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1", "D2", "I1"));
}

TEST_F(MainThreadSchedulerImplTest,
       EventConsumedOnCompositorThread_MouseWheel) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeMouseWheelEvent(blink::WebInputEvent::Type::kMouseWheel),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "D2", "C1", "C2", "I1"));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       EventForwardedToMainThread_MouseWheel_PreventDefault) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeMouseWheelEvent(blink::WebInputEvent::Type::kMouseWheel),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are prioritized (since they are fast).
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1", "D2", "I1"));
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       EventForwardedToMainThread_NoPreventDefault) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeMouseWheelEvent(blink::WebInputEvent::Type::kMouseWheel),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollBegin),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are prioritized.
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1", "D2", "I1"));
  EXPECT_EQ(UseCase::kMainThreadGesture, CurrentUseCase());
}

TEST_F(
    MainThreadSchedulerImplTest,
    EventForwardedToMainThreadAndBackToCompositor_MouseWheel_NoPreventDefault) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeMouseWheelEvent(blink::WebInputEvent::Type::kMouseWheel),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollBegin),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "D2", "C1", "C2", "I1"));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       EventConsumedOnCompositorThread_IgnoresKeyboardEvents) {
  RunSlowCompositorTask();

  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kKeyDown),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "C1", "D2", "C2", "I1"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       EventForwardedToMainThread_IgnoresKeyboardEvents) {
  RunSlowCompositorTask();

  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kKeyDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "C1", "D2", "C2", "I1"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
  // Note compositor tasks are not prioritized.
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kKeyDown),
      WebInputEventResult::kHandledSystem);
}

TEST_F(MainThreadSchedulerImplTest,
       TestMainthreadScrollingUseCaseDoesNotStarveDefaultTasks) {
  SimulateMainThreadGestureStart(
      TouchEventPolicy::kDontSendTouchStart,
      blink::WebInputEvent::Type::kGestureScrollBegin);
  EnableIdleTasks();

  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 C1");

  for (int i = 0; i < 20; i++) {
    compositor_task_runner_->PostTask(FROM_HERE, base::BindOnce(&NullTask));
  }
  PostTestTasks(&run_order, "C2");

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureFlingStart),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1"));
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicyEnds_CompositorHandlesInput) {
  SimulateCompositorGestureStart(TouchEventPolicy::kDontSendTouchStart);
  EXPECT_EQ(UseCase::kCompositorGesture,
            ForceUpdatePolicyAndGetCurrentUseCase());

  test_task_runner_->AdvanceMockTickClock(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicyEnds_MainThreadHandlesInput) {
  SimulateMainThreadGestureStart(
      TouchEventPolicy::kDontSendTouchStart,
      blink::WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling,
            ForceUpdatePolicyAndGetCurrentUseCase());

  test_task_runner_->AdvanceMockTickClock(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest, TestTouchstartPolicyEndsAfterTimeout) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 D1 C1 D2 C2");

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1", "D2"));

  run_order.clear();
  test_task_runner_->AdvanceMockTickClock(base::TimeDelta::FromSeconds(1));

  // Don't post any compositor tasks to simulate a very long running event
  // handler.
  PostTestTasks(&run_order, "D1 D2");

  // Touchstart policy mode should have ended now that the clock has advanced.
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("L1", "D1", "D2"));
}

TEST_F(MainThreadSchedulerImplTest,
       TestTouchstartPolicyEndsAfterConsecutiveTouchmoves) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 D1 C1 D2 C2");

  // Observation of touchstart should defer execution of idle and loading tasks.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1", "D2"));

  // Receiving the first touchmove will not affect scheduler priority.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());

  // Receiving the second touchmove will kick us back into compositor priority.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("L1"));
}

TEST_F(MainThreadSchedulerImplTest, TestIsHighPriorityWorkAnticipated) {
  bool is_anticipated_before = false;
  bool is_anticipated_after = false;

  scheduler_->SetHaveSeenABlockingGestureForTesting(true);
  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AnticipationTestTask, scheduler_.get(),
                                SimulateInputType::kNone,
                                &is_anticipated_before, &is_anticipated_after));
  base::RunLoop().RunUntilIdle();
  // In its default state, without input receipt, the scheduler should indicate
  // that no high-priority is anticipated.
  EXPECT_FALSE(is_anticipated_before);
  EXPECT_FALSE(is_anticipated_after);

  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AnticipationTestTask, scheduler_.get(),
                                SimulateInputType::kTouchStart,
                                &is_anticipated_before, &is_anticipated_after));
  bool dummy;
  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AnticipationTestTask, scheduler_.get(),
                                SimulateInputType::kTouchEnd, &dummy, &dummy));
  default_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AnticipationTestTask, scheduler_.get(),
                     SimulateInputType::kGestureScrollBegin, &dummy, &dummy));
  default_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AnticipationTestTask, scheduler_.get(),
                     SimulateInputType::kGestureScrollEnd, &dummy, &dummy));

  base::RunLoop().RunUntilIdle();
  // When input is received, the scheduler should indicate that high-priority
  // work is anticipated.
  EXPECT_FALSE(is_anticipated_before);
  EXPECT_TRUE(is_anticipated_after);

  test_task_runner_->AdvanceMockTickClock(
      priority_escalation_after_input_duration() * 2);
  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AnticipationTestTask, scheduler_.get(),
                                SimulateInputType::kNone,
                                &is_anticipated_before, &is_anticipated_after));
  base::RunLoop().RunUntilIdle();
  // Without additional input, the scheduler should go into NONE
  // use case but with scrolling expected where high-priority work is still
  // anticipated.
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
  EXPECT_TRUE(BlockingInputExpectedSoon());
  EXPECT_TRUE(is_anticipated_before);
  EXPECT_TRUE(is_anticipated_after);

  test_task_runner_->AdvanceMockTickClock(
      subsequent_input_expected_after_input_duration() * 2);
  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AnticipationTestTask, scheduler_.get(),
                                SimulateInputType::kNone,
                                &is_anticipated_before, &is_anticipated_after));
  base::RunLoop().RunUntilIdle();
  // Eventually the scheduler should go into the default use case where
  // high-priority work is no longer anticipated.
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
  EXPECT_FALSE(BlockingInputExpectedSoon());
  EXPECT_FALSE(is_anticipated_before);
  EXPECT_FALSE(is_anticipated_after);
}

TEST_F(MainThreadSchedulerImplTest, TestShouldYield) {
  bool should_yield_before = false;
  bool should_yield_after = false;

  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PostingYieldingTestTask, scheduler_.get(),
                                base::RetainedRef(default_task_runner_), false,
                                &should_yield_before, &should_yield_after));
  base::RunLoop().RunUntilIdle();
  // Posting to default runner shouldn't cause yielding.
  EXPECT_FALSE(should_yield_before);
  EXPECT_FALSE(should_yield_after);

  default_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PostingYieldingTestTask, scheduler_.get(),
                     base::RetainedRef(compositor_task_runner_), false,
                     &should_yield_before, &should_yield_after));
  base::RunLoop().RunUntilIdle();
  // Posting while not mainthread scrolling shouldn't cause yielding.
  EXPECT_FALSE(should_yield_before);
  EXPECT_FALSE(should_yield_after);

  default_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PostingYieldingTestTask, scheduler_.get(),
                     base::RetainedRef(compositor_task_runner_), true,
                     &should_yield_before, &should_yield_after));
  base::RunLoop().RunUntilIdle();
  // We should be able to switch to compositor priority mid-task.
  EXPECT_FALSE(should_yield_before);
  EXPECT_TRUE(should_yield_after);
}

TEST_F(MainThreadSchedulerImplTest, TestShouldYield_TouchStart) {
  // Receiving a touchstart should immediately trigger yielding, even if
  // there's no immediately pending work in the compositor queue.
  EXPECT_FALSE(scheduler_->ShouldYieldForHighPriorityWork());
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EXPECT_TRUE(scheduler_->ShouldYieldForHighPriorityWork());
  base::RunLoop().RunUntilIdle();
}

TEST_F(MainThreadSchedulerImplTest, SlowMainThreadInputEvent) {
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());

  // An input event should bump us into input priority.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureFlingStart),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());

  // Simulate the input event being queued for a very long time. The compositor
  // task we post here represents the enqueued input task.
  test_task_runner_->AdvanceMockTickClock(
      priority_escalation_after_input_duration() * 2);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureFlingStart),
      WebInputEventResult::kHandledSystem);
  base::RunLoop().RunUntilIdle();

  // Even though we exceeded the input priority escalation period, we should
  // still be in main thread gesture since the input remains queued.
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());

  // After the escalation period ends we should go back into normal mode.
  test_task_runner_->FastForwardBy(priority_escalation_after_input_duration() *
                                   2);
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest, OnlyOnePendingUrgentPolicyUpdate) {
  for (int i = 0; i < 4; i++) {
    scheduler_->EnsureUrgentPolicyUpdatePostedOnMainThread();
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, scheduler_->update_policy_count_);
}

TEST_F(MainThreadSchedulerImplTest, OnePendingDelayedAndOneUrgentUpdatePolicy) {
  scheduler_->ScheduleDelayedPolicyUpdate(Now(),
                                          base::TimeDelta::FromMilliseconds(1));
  scheduler_->EnsureUrgentPolicyUpdatePostedOnMainThread();

  test_task_runner_->FastForwardUntilNoTasksRemain();
  // We expect both the urgent and the delayed updates to run.
  EXPECT_EQ(2, scheduler_->update_policy_count_);
}

TEST_F(MainThreadSchedulerImplTest, OneUrgentAndOnePendingDelayedUpdatePolicy) {
  scheduler_->EnsureUrgentPolicyUpdatePostedOnMainThread();
  scheduler_->ScheduleDelayedPolicyUpdate(Now(),
                                          base::TimeDelta::FromMilliseconds(1));

  test_task_runner_->FastForwardUntilNoTasksRemain();
  // We expect both the urgent and the delayed updates to run.
  EXPECT_EQ(2, scheduler_->update_policy_count_);
}

TEST_F(MainThreadSchedulerImplTest, UpdatePolicyCountTriggeredByOneInputEvent) {
  // We expect DidHandleInputEventOnCompositorThread to post an urgent policy
  // update.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(0, scheduler_->update_policy_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      WebInputEventResult::kHandledSystem);
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  test_task_runner_->AdvanceMockTickClock(base::TimeDelta::FromSeconds(1));
  base::RunLoop().RunUntilIdle();
  // We finally expect a delayed policy update 100ms later.
  EXPECT_EQ(2, scheduler_->update_policy_count_);
}

TEST_F(MainThreadSchedulerImplTest,
       UpdatePolicyCountTriggeredByThreeInputEvents) {
  // We expect DidHandleInputEventOnCompositorThread to post
  // an urgent policy update.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart,
                     blink::WebInputEvent::DispatchType::kEventNonBlocking),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(0, scheduler_->update_policy_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      WebInputEventResult::kHandledSystem);
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  // The second call to DidHandleInputEventOnCompositorThread should not post
  // a policy update because we are already in compositor priority.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  // We expect DidHandleInputEvent to trigger a policy update.
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
      WebInputEventResult::kHandledSystem);
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  // The third call to DidHandleInputEventOnCompositorThread should post a
  // policy update because the awaiting_touch_start_response_ flag changed.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(1, scheduler_->update_policy_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, scheduler_->update_policy_count_);

  // We expect DidHandleInputEvent to trigger a policy update.
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
      WebInputEventResult::kHandledSystem);
  EXPECT_EQ(2, scheduler_->update_policy_count_);
  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  // We finally expect a delayed policy update.
  EXPECT_EQ(3, scheduler_->update_policy_count_);
}

TEST_F(MainThreadSchedulerImplTest,
       UpdatePolicyCountTriggeredByTwoInputEventsWithALongSeparatingDelay) {
  // We expect DidHandleInputEventOnCompositorThread to post an urgent policy
  // update.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart,
                     blink::WebInputEvent::DispatchType::kEventNonBlocking),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(0, scheduler_->update_policy_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      WebInputEventResult::kHandledSystem);
  EXPECT_EQ(1, scheduler_->update_policy_count_);
  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  // We expect a delayed policy update.
  EXPECT_EQ(2, scheduler_->update_policy_count_);

  // We expect the second call to DidHandleInputEventOnCompositorThread to post
  // an urgent policy update because we are no longer in compositor priority.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(2, scheduler_->update_policy_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, scheduler_->update_policy_count_);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
      WebInputEventResult::kHandledSystem);
  EXPECT_EQ(3, scheduler_->update_policy_count_);
  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  // We finally expect a delayed policy update.
  EXPECT_EQ(4, scheduler_->update_policy_count_);
}

TEST_F(MainThreadSchedulerImplTest, EnsureUpdatePolicyNotTriggeredTooOften) {
  EXPECT_EQ(0, scheduler_->update_policy_count_);
  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);

  // We expect the first call to IsHighPriorityWorkAnticipated to be called
  // after receiving an input event (but before the UpdateTask was processed) to
  // call UpdatePolicy.
  EXPECT_EQ(1, scheduler_->update_policy_count_);
  scheduler_->IsHighPriorityWorkAnticipated();
  EXPECT_EQ(2, scheduler_->update_policy_count_);
  // Subsequent calls should not call UpdatePolicy.
  scheduler_->IsHighPriorityWorkAnticipated();
  scheduler_->IsHighPriorityWorkAnticipated();
  scheduler_->IsHighPriorityWorkAnticipated();
  scheduler_->ShouldYieldForHighPriorityWork();
  scheduler_->ShouldYieldForHighPriorityWork();
  scheduler_->ShouldYieldForHighPriorityWork();
  scheduler_->ShouldYieldForHighPriorityWork();

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollEnd),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchEnd),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      WebInputEventResult::kHandledSystem);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
      WebInputEventResult::kHandledSystem);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
      WebInputEventResult::kHandledSystem);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchEnd),
      WebInputEventResult::kHandledSystem);

  EXPECT_EQ(2, scheduler_->update_policy_count_);

  // We expect both the urgent and the delayed updates to run in addition to the
  // earlier updated cause by IsHighPriorityWorkAnticipated, a final update
  // transitions from 'not_scrolling touchstart expected' to 'not_scrolling'.
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_THAT(scheduler_->use_cases_,
              testing::ElementsAre("none", "compositor_gesture",
                                   "compositor_gesture blocking input expected",
                                   "none blocking input expected", "none"));
}

TEST_F(MainThreadSchedulerImplTest,
       BlockingInputExpectedSoonWhenBlockInputEventSeen) {
  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);
  EXPECT_TRUE(HaveSeenABlockingGesture());
  ForceBlockingInputToBeExpectedSoon();
  EXPECT_TRUE(BlockingInputExpectedSoon());
}

TEST_F(MainThreadSchedulerImplTest,
       BlockingInputNotExpectedSoonWhenNoBlockInputEventSeen) {
  SimulateCompositorGestureStart(TouchEventPolicy::kDontSendTouchStart);
  EXPECT_FALSE(HaveSeenABlockingGesture());
  ForceBlockingInputToBeExpectedSoon();
  EXPECT_FALSE(BlockingInputExpectedSoon());
}

TEST_F(MainThreadSchedulerImplTest,
       GetTaskExecutorForCurrentThreadInPostedTask) {
  base::TaskExecutor* task_executor = base::GetTaskExecutorForCurrentThread();
  EXPECT_THAT(task_executor, NotNull());

  base::RunLoop run_loop;

  default_task_runner_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        EXPECT_EQ(base::GetTaskExecutorForCurrentThread(), task_executor);
        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(MainThreadSchedulerImplTest, TestBeginMainFrameNotExpectedUntil) {
  base::TimeDelta ten_millis(base::TimeDelta::FromMilliseconds(10));
  base::TimeTicks expected_deadline = Now() + ten_millis;
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Shouldn't run yet as no idle period.

  base::TimeTicks now = Now();
  base::TimeTicks frame_time = now + ten_millis;
  // No main frame is expected until frame_time, so short idle work can be
  // scheduled in the mean time.
  scheduler_->BeginMainFrameNotExpectedUntil(frame_time);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, run_count);  // Should have run in a long idle time.
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(MainThreadSchedulerImplTest, TestLongIdlePeriod) {
  base::TimeTicks expected_deadline = Now() + maximum_idle_period_duration();
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Shouldn't run yet as no idle period.

  scheduler_->BeginFrameNotExpectedSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, run_count);  // Should have run in a long idle time.
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(MainThreadSchedulerImplTest, TestLongIdlePeriodWithPendingDelayedTask) {
  base::TimeDelta pending_task_delay = base::TimeDelta::FromMilliseconds(30);
  base::TimeTicks expected_deadline = Now() + pending_task_delay;
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));
  default_task_runner_->PostDelayedTask(FROM_HERE, base::BindOnce(&NullTask),
                                        pending_task_delay);

  scheduler_->BeginFrameNotExpectedSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, run_count);  // Should have run in a long idle time.
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(MainThreadSchedulerImplTest,
       TestLongIdlePeriodWithLatePendingDelayedTask) {
  base::TimeDelta pending_task_delay = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  default_task_runner_->PostDelayedTask(FROM_HERE, base::BindOnce(&NullTask),
                                        pending_task_delay);

  // Advance clock until after delayed task was meant to be run.
  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(20));

  // Post an idle task and BeginFrameNotExpectedSoon to initiate a long idle
  // period. Since there is a late pending delayed task this shouldn't actually
  // start an idle period.
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));
  scheduler_->BeginFrameNotExpectedSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, run_count);

  // After the delayed task has been run we should trigger an idle period.
  test_task_runner_->FastForwardBy(maximum_idle_period_duration());
  EXPECT_EQ(1, run_count);
}

TEST_F(MainThreadSchedulerImplTest, TestLongIdlePeriodRepeating) {
  Vector<base::TimeTicks> actual_deadlines;
  int run_count = 0;

  g_max_idle_task_reposts = 3;
  base::TimeTicks clock_before = Now();
  base::TimeDelta idle_task_runtime(base::TimeDelta::FromMilliseconds(10));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&RepostingUpdateClockIdleTestTask,
                     base::RetainedRef(idle_task_runner_), &run_count,
                     test_task_runner_, idle_task_runtime, &actual_deadlines));
  scheduler_->BeginFrameNotExpectedSoon();
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(3, run_count);
  EXPECT_THAT(
      actual_deadlines,
      testing::ElementsAre(clock_before + maximum_idle_period_duration(),
                           clock_before + 2 * maximum_idle_period_duration(),
                           clock_before + 3 * maximum_idle_period_duration()));

  // Check that idle tasks don't run after the idle period ends with a
  // new BeginMainFrame.
  g_max_idle_task_reposts = 5;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&RepostingUpdateClockIdleTestTask,
                     base::RetainedRef(idle_task_runner_), &run_count,
                     test_task_runner_, idle_task_runtime, &actual_deadlines));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&WillBeginFrameIdleTask,
                     base::Unretained(scheduler_.get()),
                     next_begin_frame_number_++,
                     base::Unretained(test_task_runner_->GetMockTickClock())));
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(4, run_count);
}

TEST_F(MainThreadSchedulerImplTest, TestLongIdlePeriodInTouchStartPolicy) {
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));

  // Observation of touchstart should defer the start of the long idle period.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->BeginFrameNotExpectedSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, run_count);

  // The long idle period should start after the touchstart policy has finished.
  test_task_runner_->FastForwardBy(priority_escalation_after_input_duration());
  EXPECT_EQ(1, run_count);
}

void TestCanExceedIdleDeadlineIfRequiredTask(ThreadScheduler* scheduler,
                                             bool* can_exceed_idle_deadline_out,
                                             int* run_count,
                                             base::TimeTicks deadline) {
  *can_exceed_idle_deadline_out = scheduler->CanExceedIdleDeadlineIfRequired();
  (*run_count)++;
}

TEST_F(MainThreadSchedulerImplTest, CanExceedIdleDeadlineIfRequired) {
  int run_count = 0;
  bool can_exceed_idle_deadline = false;

  // Should return false if not in an idle period.
  EXPECT_FALSE(scheduler_->CanExceedIdleDeadlineIfRequired());

  // Should return false for short idle periods.
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&TestCanExceedIdleDeadlineIfRequiredTask, scheduler_.get(),
                     &can_exceed_idle_deadline, &run_count));
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, run_count);
  EXPECT_FALSE(can_exceed_idle_deadline);

  // Should return false for a long idle period which is shortened due to a
  // pending delayed task.
  default_task_runner_->PostDelayedTask(FROM_HERE, base::BindOnce(&NullTask),
                                        base::TimeDelta::FromMilliseconds(10));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&TestCanExceedIdleDeadlineIfRequiredTask, scheduler_.get(),
                     &can_exceed_idle_deadline, &run_count));
  scheduler_->BeginFrameNotExpectedSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, run_count);
  EXPECT_FALSE(can_exceed_idle_deadline);

  // Next long idle period will be for the maximum time, so
  // CanExceedIdleDeadlineIfRequired should return true.
  test_task_runner_->AdvanceMockTickClock(maximum_idle_period_duration());
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&TestCanExceedIdleDeadlineIfRequiredTask, scheduler_.get(),
                     &can_exceed_idle_deadline, &run_count));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, run_count);
  EXPECT_TRUE(can_exceed_idle_deadline);

  // Next long idle period will be for the maximum time, so
  // CanExceedIdleDeadlineIfRequired should return true.
  scheduler_->WillBeginFrame(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::TimeDelta::FromMilliseconds(1000),
      viz::BeginFrameArgs::NORMAL));
  EXPECT_FALSE(scheduler_->CanExceedIdleDeadlineIfRequired());
}

TEST_F(MainThreadSchedulerImplTest, TestRendererHiddenIdlePeriod) {
  int run_count = 0;

  g_max_idle_task_reposts = 2;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&RepostingIdleTestTask,
                     base::RetainedRef(idle_task_runner_), &run_count));

  // Renderer should start in visible state.
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(0, run_count);

  // When we hide the renderer it should start a max deadline idle period, which
  // will run an idle task and then immediately start a new idle period, which
  // runs the second idle task.
  scheduler_->SetAllRenderWidgetsHidden(true);
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(2, run_count);

  // Advance time by amount of time by the maximum amount of time we execute
  // idle tasks when hidden (plus some slack) - idle period should have ended.
  g_max_idle_task_reposts = 3;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&RepostingIdleTestTask,
                     base::RetainedRef(idle_task_runner_), &run_count));
  test_task_runner_->FastForwardBy(end_idle_when_hidden_delay() +
                                   base::TimeDelta::FromMilliseconds(10));
  EXPECT_EQ(2, run_count);
}

TEST_F(MainThreadSchedulerImplTest, TimerQueueEnabledByDefault) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "T1 T2");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("T1", "T2"));
}

TEST_F(MainThreadSchedulerImplTest, StopAndResumeRenderer) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "T1 T2");

  auto pause_handle = scheduler_->PauseRenderer();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());

  pause_handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("T1", "T2"));
}

TEST_F(MainThreadSchedulerImplTest, StopAndThrottleTimerQueue) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "T1 T2");

  auto pause_handle = scheduler_->PauseRenderer();
  base::RunLoop().RunUntilIdle();
  scheduler_->task_queue_throttler()->IncreaseThrottleRefCount(
      timer_task_queue());
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());
}

TEST_F(MainThreadSchedulerImplTest, ThrottleAndPauseRenderer) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "T1 T2");

  scheduler_->task_queue_throttler()->IncreaseThrottleRefCount(
      timer_task_queue());
  base::RunLoop().RunUntilIdle();
  auto pause_handle = scheduler_->PauseRenderer();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());
}

TEST_F(MainThreadSchedulerImplTest, MultipleStopsNeedMultipleResumes) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "T1 T2");

  auto pause_handle1 = scheduler_->PauseRenderer();
  auto pause_handle2 = scheduler_->PauseRenderer();
  auto pause_handle3 = scheduler_->PauseRenderer();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());

  pause_handle1.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());

  pause_handle2.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());

  pause_handle3.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("T1", "T2"));
}

TEST_F(MainThreadSchedulerImplTest, PauseRenderer) {
  // Tasks in some queues don't fire when the renderer is paused.
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 C1 L1 I1 T1");
  auto pause_handle = scheduler_->PauseRenderer();
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "C1", "I1"));

  // Tasks are executed when renderer is resumed.
  run_order.clear();
  pause_handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("L1", "T1"));
}

TEST_F(MainThreadSchedulerImplTest, UseCaseToString) {
  CheckAllUseCaseToString();
}

TEST_F(MainThreadSchedulerImplTest, MismatchedDidHandleInputEventOnMainThread) {
  // This should not DCHECK because there was no corresponding compositor side
  // call to DidHandleInputEventOnCompositorThread with
  // blink::mojom::InputEventResultState::kNotConsumed. There are legitimate
  // reasons for the compositor to not be there and we don't want to make
  // debugging impossible.
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureFlingStart),
      WebInputEventResult::kHandledSystem);
}

TEST_F(MainThreadSchedulerImplTest, BeginMainFrameOnCriticalPath) {
  ASSERT_FALSE(scheduler_->BeginMainFrameOnCriticalPath());
  viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::TimeDelta::FromMilliseconds(1000),
      viz::BeginFrameArgs::NORMAL);
  scheduler_->WillBeginFrame(begin_frame_args);
  ASSERT_TRUE(scheduler_->BeginMainFrameOnCriticalPath());

  begin_frame_args.on_critical_path = false;
  scheduler_->WillBeginFrame(begin_frame_args);
  ASSERT_FALSE(scheduler_->BeginMainFrameOnCriticalPath());
}

TEST_F(MainThreadSchedulerImplTest, ShutdownPreventsPostingOfNewTasks) {
  main_frame_scheduler_.reset();
  page_scheduler_.reset();
  scheduler_->Shutdown();
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 C1");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());
}

TEST_F(MainThreadSchedulerImplTest,
       EstimateLongestJankFreeTaskDuration_UseCase_NONE) {
  EnsureUseCaseNone();
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
  EXPECT_EQ(rails_response_time(),
            scheduler_->EstimateLongestJankFreeTaskDuration());
}

TEST_F(MainThreadSchedulerImplTest,
       EstimateLongestJankFreeTaskDuration_UseCase_kCompositorGesture) {
  SimulateCompositorGestureStart(TouchEventPolicy::kDontSendTouchStart);
  EXPECT_EQ(UseCase::kCompositorGesture,
            ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(rails_response_time(),
            scheduler_->EstimateLongestJankFreeTaskDuration());
}

TEST_F(MainThreadSchedulerImplTest,
       EstimateLongestJankFreeTaskDuration_UseCase_EarlyLoading) {
  ON_CALL(*page_scheduler_, IsWaitingForMainFrameContentfulPaint)
      .WillByDefault(Return(true));
  ON_CALL(*page_scheduler_, IsWaitingForMainFrameMeaningfulPaint)
      .WillByDefault(Return(true));
  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_EQ(UseCase::kEarlyLoading, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(rails_response_time(),
            scheduler_->EstimateLongestJankFreeTaskDuration());
}

TEST_F(MainThreadSchedulerImplTest,
       EstimateLongestJankFreeTaskDuration_UseCase_Loading) {
  ON_CALL(*page_scheduler_, IsWaitingForMainFrameContentfulPaint)
      .WillByDefault(Return(false));
  ON_CALL(*page_scheduler_, IsWaitingForMainFrameMeaningfulPaint)
      .WillByDefault(Return(true));
  scheduler_->OnMainFramePaint();
  EXPECT_EQ(UseCase::kLoading, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(rails_response_time(),
            scheduler_->EstimateLongestJankFreeTaskDuration());
}

TEST_F(MainThreadSchedulerImplTest,
       EstimateLongestJankFreeTaskDuration_UseCase_MAIN_THREAD_GESTURE) {
  SimulateMainThreadGestureStart(
      TouchEventPolicy::kSendTouchStart,
      blink::WebInputEvent::Type::kGestureScrollUpdate);
  viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
      viz::BeginFrameArgs::NORMAL);
  begin_frame_args.on_critical_path = false;
  scheduler_->WillBeginFrame(begin_frame_args);

  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MainThreadSchedulerImplTest::
                         SimulateMainThreadInputHandlingCompositorTask,
                     base::Unretained(this),
                     base::TimeDelta::FromMilliseconds(5)));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(UseCase::kMainThreadGesture, CurrentUseCase());

  // 16ms frame - 5ms compositor work = 11ms for other stuff.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(11),
            scheduler_->EstimateLongestJankFreeTaskDuration());
}

TEST_F(
    MainThreadSchedulerImplTest,
    EstimateLongestJankFreeTaskDuration_UseCase_MAIN_THREAD_CUSTOM_INPUT_HANDLING) {
  viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
      viz::BeginFrameArgs::NORMAL);
  begin_frame_args.on_critical_path = false;
  scheduler_->WillBeginFrame(begin_frame_args);

  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MainThreadSchedulerImplTest::
                         SimulateMainThreadInputHandlingCompositorTask,
                     base::Unretained(this),
                     base::TimeDelta::FromMilliseconds(5)));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());

  // 16ms frame - 5ms compositor work = 11ms for other stuff.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(11),
            scheduler_->EstimateLongestJankFreeTaskDuration());
}

TEST_F(MainThreadSchedulerImplTest,
       EstimateLongestJankFreeTaskDuration_UseCase_SYNCHRONIZED_GESTURE) {
  SimulateCompositorGestureStart(TouchEventPolicy::kDontSendTouchStart);

  viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
      viz::BeginFrameArgs::NORMAL);
  begin_frame_args.on_critical_path = true;
  scheduler_->WillBeginFrame(begin_frame_args);

  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MainThreadSchedulerImplTest::SimulateMainThreadCompositorTask,
          base::Unretained(this), base::TimeDelta::FromMilliseconds(5)));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(UseCase::kSynchronizedGesture, CurrentUseCase());

  // 16ms frame - 5ms compositor work = 11ms for other stuff.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(11),
            scheduler_->EstimateLongestJankFreeTaskDuration());
}

namespace {
void SlowCountingTask(size_t* count,
                      scoped_refptr<base::TestMockTimeTaskRunner> task_runner,
                      int task_duration,
                      scoped_refptr<base::SingleThreadTaskRunner> timer_queue) {
  task_runner->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(task_duration));
  if (++(*count) < 500) {
    timer_queue->PostTask(FROM_HERE,
                          base::BindOnce(SlowCountingTask, count, task_runner,
                                         task_duration, timer_queue));
  }
}
}  // namespace

TEST_F(MainThreadSchedulerImplTest,
       SYNCHRONIZED_GESTURE_TimerTaskThrottling_TimersStopped) {
  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);

  base::TimeTicks first_throttled_run_time =
      TaskQueueThrottler::AlignedThrottledRunTime(Now());

  size_t count = 0;
  // With the compositor task taking 10ms, there is not enough time to run this
  // 7ms timer task in the 16ms frame.
  timer_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(SlowCountingTask, &count, test_task_runner_, 7,
                                timer_task_runner_));

  std::unique_ptr<WebThreadScheduler::RendererPauseHandle> paused;
  for (int i = 0; i < 1000; i++) {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
        viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::
                           SimulateMainThreadCompositorAndQuitRunLoopTask,
                       base::Unretained(this),
                       base::TimeDelta::FromMilliseconds(10)));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(UseCase::kSynchronizedGesture, CurrentUseCase()) << "i = " << i;

    // Before the policy is updated the queue will be enabled. Subsequently it
    // will be disabled until the throttled queue is pumped.
    bool expect_queue_enabled = (i == 0) || (Now() > first_throttled_run_time);
    if (paused)
      expect_queue_enabled = false;
    EXPECT_EQ(expect_queue_enabled, timer_task_queue()->IsQueueEnabled())
        << "i = " << i;

    // After we've run any expensive tasks suspend the queue.  The throttling
    // helper should /not/ re-enable this queue under any circumstances while
    // timers are paused.
    if (count > 0 && !paused) {
      EXPECT_EQ(2u, count);
      paused = scheduler_->PauseRenderer();
    }
  }

  // Make sure the timer queue stayed paused!
  EXPECT_EQ(2u, count);
}

TEST_F(MainThreadSchedulerImplTest,
       SYNCHRONIZED_GESTURE_TimerTaskThrottling_task_not_expensive) {
  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);

  size_t count = 0;
  // With the compositor task taking 10ms, there is enough time to run this 6ms
  // timer task in the 16ms frame.
  timer_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(SlowCountingTask, &count, test_task_runner_, 6,
                                timer_task_runner_));

  for (int i = 0; i < 1000; i++) {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
        viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::
                           SimulateMainThreadCompositorAndQuitRunLoopTask,
                       base::Unretained(this),
                       base::TimeDelta::FromMilliseconds(10)));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(UseCase::kSynchronizedGesture, CurrentUseCase()) << "i = " << i;
    EXPECT_TRUE(timer_task_queue()->IsQueueEnabled()) << "i = " << i;
  }

  // Task is not throttled.
  EXPECT_EQ(500u, count);
}

TEST_F(MainThreadSchedulerImplTest, DenyLongIdleDuringTouchStart) {
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EXPECT_EQ(UseCase::kTouchstart, ForceUpdatePolicyAndGetCurrentUseCase());

  // First check that long idle is denied during the TOUCHSTART use case.
  IdleHelper::Delegate* idle_delegate = scheduler_.get();
  base::TimeTicks now;
  base::TimeDelta next_time_to_check;
  EXPECT_FALSE(idle_delegate->CanEnterLongIdlePeriod(now, &next_time_to_check));
  EXPECT_GE(next_time_to_check, base::TimeDelta());

  // Check again at a time past the TOUCHSTART expiration. We should still get a
  // non-negative delay to when to check again.
  now += base::TimeDelta::FromMilliseconds(500);
  EXPECT_FALSE(idle_delegate->CanEnterLongIdlePeriod(now, &next_time_to_check));
  EXPECT_GE(next_time_to_check, base::TimeDelta());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_TouchStartDuringFling) {
  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  scheduler_->DidAnimateForInputOnCompositorThread();
  // Note DidAnimateForInputOnCompositorThread does not by itself trigger a
  // policy update.
  EXPECT_EQ(UseCase::kCompositorGesture,
            ForceUpdatePolicyAndGetCurrentUseCase());

  // Make sure TouchStart causes a policy change.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(UseCase::kTouchstart, ForceUpdatePolicyAndGetCurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest, SYNCHRONIZED_GESTURE_CompositingExpensive) {
  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);

  // With the compositor task taking 20ms, there is not enough time to run
  // other tasks in the same 16ms frame. To avoid starvation, compositing tasks
  // should therefore not get prioritized.
  Vector<String> run_order;
  for (int i = 0; i < 1000; i++)
    PostTestTasks(&run_order, "T1");

  for (int i = 0; i < 100; i++) {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
        viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::
                           SimulateMainThreadCompositorAndQuitRunLoopTask,
                       base::Unretained(this),
                       base::TimeDelta::FromMilliseconds(20)));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(UseCase::kSynchronizedGesture, CurrentUseCase()) << "i = " << i;
  }

  // Timer tasks should not have been starved by the expensive compositor
  // tasks.
  EXPECT_EQ(TaskQueue::kNormalPriority,
            scheduler_->CompositorTaskQueue()->GetQueuePriority());
  EXPECT_EQ(1000u, run_order.size());
}

TEST_F(MainThreadSchedulerImplTest, MAIN_THREAD_CUSTOM_INPUT_HANDLING) {
  SimulateMainThreadGestureStart(
      TouchEventPolicy::kSendTouchStart,
      blink::WebInputEvent::Type::kGestureScrollBegin);

  // With the compositor task taking 20ms, there is not enough time to run
  // other tasks in the same 16ms frame. To avoid starvation, compositing tasks
  // should therefore not get prioritized.
  Vector<String> run_order;
  for (int i = 0; i < 1000; i++)
    PostTestTasks(&run_order, "T1");

  for (int i = 0; i < 100; i++) {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
        viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::
                           SimulateMainThreadCompositorAndQuitRunLoopTask,
                       base::Unretained(this),
                       base::TimeDelta::FromMilliseconds(20)));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase())
        << "i = " << i;
  }

  // Timer tasks should not have been starved by the expensive compositor
  // tasks.
  EXPECT_EQ(TaskQueue::kNormalPriority,
            scheduler_->CompositorTaskQueue()->GetQueuePriority());
  EXPECT_EQ(1000u, run_order.size());
}

TEST_F(MainThreadSchedulerImplTest, MAIN_THREAD_GESTURE) {
  SimulateMainThreadGestureStart(
      TouchEventPolicy::kDontSendTouchStart,
      blink::WebInputEvent::Type::kGestureScrollBegin);

  // With the compositor task taking 20ms, there is not enough time to run
  // other tasks in the same 16ms frame. However because this is a main thread
  // gesture instead of custom main thread input handling, we allow the timer
  // tasks to be starved.
  Vector<String> run_order;
  for (int i = 0; i < 1000; i++)
    PostTestTasks(&run_order, "T1");

  for (int i = 0; i < 100; i++) {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
        viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::
                           SimulateMainThreadCompositorAndQuitRunLoopTask,
                       base::Unretained(this),
                       base::TimeDelta::FromMilliseconds(20)));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(UseCase::kMainThreadGesture, CurrentUseCase()) << "i = " << i;
  }

  EXPECT_EQ(TaskQueue::kHighestPriority,
            scheduler_->CompositorTaskQueue()->GetQueuePriority());
  EXPECT_EQ(279u, run_order.size());
}

class MockRAILModeObserver : public RAILModeObserver {
 public:
  MOCK_METHOD(void, OnRAILModeChanged, (RAILMode rail_mode));
};

TEST_F(MainThreadSchedulerImplTest, TestResponseRAILMode) {
  MockRAILModeObserver observer;
  scheduler_->AddRAILModeObserver(&observer);
  EXPECT_CALL(observer, OnRAILModeChanged(RAILMode::kResponse));

  scheduler_->SetHaveSeenABlockingGestureForTesting(true);
  ForceBlockingInputToBeExpectedSoon();
  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(RAILMode::kResponse, GetRAILMode());
  scheduler_->RemoveRAILModeObserver(&observer);
}

TEST_F(MainThreadSchedulerImplTest, TestAnimateRAILMode) {
  MockRAILModeObserver observer;
  scheduler_->AddRAILModeObserver(&observer);
  EXPECT_CALL(observer, OnRAILModeChanged(RAILMode::kAnimation)).Times(0);

  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(RAILMode::kAnimation, GetRAILMode());
  scheduler_->RemoveRAILModeObserver(&observer);
}

TEST_F(MainThreadSchedulerImplTest, TestIdleRAILMode) {
  MockRAILModeObserver observer;
  scheduler_->AddRAILModeObserver(&observer);
  EXPECT_CALL(observer, OnRAILModeChanged(RAILMode::kAnimation));
  EXPECT_CALL(observer, OnRAILModeChanged(RAILMode::kIdle));

  scheduler_->SetAllRenderWidgetsHidden(true);
  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(RAILMode::kIdle, GetRAILMode());
  scheduler_->SetAllRenderWidgetsHidden(false);
  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(RAILMode::kAnimation, GetRAILMode());
  scheduler_->RemoveRAILModeObserver(&observer);
}

TEST_F(MainThreadSchedulerImplTest, TestLoadRAILMode) {
  InSequence s;
  MockRAILModeObserver observer;
  EXPECT_CALL(observer, OnRAILModeChanged(RAILMode::kAnimation));
  EXPECT_CALL(observer, OnRAILModeChanged(RAILMode::kLoad));
  EXPECT_CALL(observer, OnRAILModeChanged(RAILMode::kAnimation));
  scheduler_->AddRAILModeObserver(&observer);

  ON_CALL(*page_scheduler_, IsWaitingForMainFrameContentfulPaint)
      .WillByDefault(Return(true));
  ON_CALL(*page_scheduler_, IsWaitingForMainFrameMeaningfulPaint)
      .WillByDefault(Return(true));
  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_EQ(RAILMode::kLoad, GetRAILMode());
  EXPECT_EQ(UseCase::kEarlyLoading, ForceUpdatePolicyAndGetCurrentUseCase());
  ON_CALL(*page_scheduler_, IsWaitingForMainFrameContentfulPaint)
      .WillByDefault(Return(false));
  scheduler_->OnMainFramePaint();
  EXPECT_EQ(UseCase::kLoading, ForceUpdatePolicyAndGetCurrentUseCase());
  ON_CALL(*page_scheduler_, IsWaitingForMainFrameMeaningfulPaint)
      .WillByDefault(Return(false));
  scheduler_->OnMainFramePaint();
  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(RAILMode::kAnimation, GetRAILMode());
  scheduler_->RemoveRAILModeObserver(&observer);
}

TEST_F(MainThreadSchedulerImplTest, InputTerminatesLoadRAILMode) {
  MockRAILModeObserver observer;
  scheduler_->AddRAILModeObserver(&observer);
  EXPECT_CALL(observer, OnRAILModeChanged(RAILMode::kAnimation));
  EXPECT_CALL(observer, OnRAILModeChanged(RAILMode::kLoad));

  ON_CALL(*page_scheduler_, IsWaitingForMainFrameContentfulPaint)
      .WillByDefault(Return(true));
  ON_CALL(*page_scheduler_, IsWaitingForMainFrameMeaningfulPaint)
      .WillByDefault(Return(true));
  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_EQ(RAILMode::kLoad, GetRAILMode());
  EXPECT_EQ(UseCase::kEarlyLoading, ForceUpdatePolicyAndGetCurrentUseCase());
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollBegin),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EXPECT_EQ(UseCase::kCompositorGesture,
            ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(RAILMode::kAnimation, GetRAILMode());
  scheduler_->RemoveRAILModeObserver(&observer);
}

TEST_F(MainThreadSchedulerImplTest, UnthrottledTaskRunner) {
  // Ensure neither suspension nor timer task throttling affects an unthrottled
  // task runner.
  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);
  scoped_refptr<TaskQueue> unthrottled_task_queue =
      scheduler_->NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
          MainThreadTaskQueue::QueueType::kUnthrottled));

  size_t timer_count = 0;
  size_t unthrottled_count = 0;
  timer_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(SlowCountingTask, &timer_count,
                                test_task_runner_, 7, timer_task_runner_));
  unthrottled_task_queue->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(SlowCountingTask, &unthrottled_count, test_task_runner_, 7,
                     unthrottled_task_queue->task_runner()));
  auto handle = scheduler_->PauseRenderer();

  for (int i = 0; i < 1000; i++) {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
        viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::
                           SimulateMainThreadCompositorAndQuitRunLoopTask,
                       base::Unretained(this),
                       base::TimeDelta::FromMilliseconds(10)));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(UseCase::kSynchronizedGesture, CurrentUseCase()) << "i = " << i;
  }

  EXPECT_EQ(0u, timer_count);
  EXPECT_EQ(500u, unthrottled_count);
}

TEST_F(MainThreadSchedulerImplTest,
       VirtualTimePolicyDoesNotAffectNewTimerTaskQueueIfVirtualTimeNotEnabled) {
  scheduler_->SetVirtualTimePolicy(
      PageSchedulerImpl::VirtualTimePolicy::kPause);
  scoped_refptr<MainThreadTaskQueue> timer_tq = scheduler_->NewTimerTaskQueue(
      MainThreadTaskQueue::QueueType::kFrameThrottleable, nullptr);
  EXPECT_FALSE(timer_tq->HasActiveFence());
}

TEST_F(MainThreadSchedulerImplTest, EnableVirtualTime) {
  EXPECT_FALSE(scheduler_->IsVirtualTimeEnabled());
  scheduler_->EnableVirtualTime(
      MainThreadSchedulerImpl::BaseTimeOverridePolicy::DO_NOT_OVERRIDE);
  EXPECT_TRUE(scheduler_->IsVirtualTimeEnabled());
  scoped_refptr<MainThreadTaskQueue> loading_tq =
      scheduler_->NewLoadingTaskQueue(
          MainThreadTaskQueue::QueueType::kFrameLoading, nullptr);
  scoped_refptr<TaskQueue> loading_control_tq = scheduler_->NewLoadingTaskQueue(
      MainThreadTaskQueue::QueueType::kFrameLoadingControl, nullptr);
  scoped_refptr<MainThreadTaskQueue> timer_tq = scheduler_->NewTimerTaskQueue(
      MainThreadTaskQueue::QueueType::kFrameThrottleable, nullptr);
  scoped_refptr<MainThreadTaskQueue> unthrottled_tq =
      scheduler_->NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
          MainThreadTaskQueue::QueueType::kUnthrottled));

  EXPECT_EQ(scheduler_->DefaultTaskQueue()->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());
  EXPECT_EQ(scheduler_->CompositorTaskQueue()->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());
  EXPECT_EQ(loading_task_queue()->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());
  EXPECT_EQ(timer_task_queue()->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());
  EXPECT_EQ(scheduler_->VirtualTimeControlTaskQueue()->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());
  EXPECT_EQ(scheduler_->V8TaskQueue()->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());

  // The main control task queue remains in the real time domain.
  EXPECT_EQ(scheduler_->ControlTaskQueue()->GetTimeDomain(),
            scheduler_->real_time_domain());

  EXPECT_EQ(loading_tq->GetTimeDomain(), scheduler_->GetVirtualTimeDomain());
  EXPECT_EQ(loading_control_tq->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());
  EXPECT_EQ(timer_tq->GetTimeDomain(), scheduler_->GetVirtualTimeDomain());
  EXPECT_EQ(unthrottled_tq->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());

  EXPECT_EQ(scheduler_
                ->NewLoadingTaskQueue(
                    MainThreadTaskQueue::QueueType::kFrameLoading, nullptr)
                ->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());
  EXPECT_EQ(scheduler_
                ->NewTimerTaskQueue(
                    MainThreadTaskQueue::QueueType::kFrameThrottleable, nullptr)
                ->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());
  EXPECT_EQ(scheduler_
                ->NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
                    MainThreadTaskQueue::QueueType::kUnthrottled))
                ->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());
  EXPECT_EQ(scheduler_
                ->NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
                    MainThreadTaskQueue::QueueType::kTest))
                ->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());
}

TEST_F(MainThreadSchedulerImplTest, EnableVirtualTimeAfterThrottling) {
  std::unique_ptr<PageSchedulerImpl> page_scheduler =
      base::WrapUnique(new PageSchedulerImpl(nullptr, scheduler_.get()));
  scheduler_->AddPageScheduler(page_scheduler.get());

  std::unique_ptr<FrameSchedulerImpl> frame_scheduler =
      FrameSchedulerImpl::Create(page_scheduler.get(), nullptr, nullptr,
                                 FrameScheduler::FrameType::kSubframe);

  TaskQueue* timer_tq = ThrottleableTaskQueue(frame_scheduler.get()).get();

  frame_scheduler->SetCrossOriginToMainFrame(true);
  frame_scheduler->SetFrameVisible(false);
  EXPECT_TRUE(scheduler_->task_queue_throttler()->IsThrottled(timer_tq));

  scheduler_->EnableVirtualTime(
      MainThreadSchedulerImpl::BaseTimeOverridePolicy::DO_NOT_OVERRIDE);
  EXPECT_EQ(timer_tq->GetTimeDomain(), scheduler_->GetVirtualTimeDomain());
  EXPECT_FALSE(scheduler_->task_queue_throttler()->IsThrottled(timer_tq));
}

TEST_F(MainThreadSchedulerImplTest, DisableVirtualTimeForTesting) {
  scheduler_->EnableVirtualTime(
      MainThreadSchedulerImpl::BaseTimeOverridePolicy::DO_NOT_OVERRIDE);

  scoped_refptr<MainThreadTaskQueue> timer_tq = scheduler_->NewTimerTaskQueue(
      MainThreadTaskQueue::QueueType::kFrameThrottleable, nullptr);
  scoped_refptr<MainThreadTaskQueue> unthrottled_tq =
      scheduler_->NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
          MainThreadTaskQueue::QueueType::kUnthrottled));

  scheduler_->DisableVirtualTimeForTesting();
  EXPECT_EQ(scheduler_->DefaultTaskQueue()->GetTimeDomain(),
            scheduler_->real_time_domain());
  EXPECT_EQ(scheduler_->CompositorTaskQueue()->GetTimeDomain(),
            scheduler_->real_time_domain());
  EXPECT_EQ(loading_task_queue()->GetTimeDomain(),
            scheduler_->real_time_domain());
  EXPECT_EQ(timer_task_queue()->GetTimeDomain(),
            scheduler_->real_time_domain());
  EXPECT_EQ(scheduler_->ControlTaskQueue()->GetTimeDomain(),
            scheduler_->real_time_domain());
  EXPECT_EQ(scheduler_->V8TaskQueue()->GetTimeDomain(),
            scheduler_->real_time_domain());
  EXPECT_FALSE(scheduler_->VirtualTimeControlTaskQueue());
}

TEST_F(MainThreadSchedulerImplTest, VirtualTimePauser) {
  scheduler_->EnableVirtualTime(
      MainThreadSchedulerImpl::BaseTimeOverridePolicy::DO_NOT_OVERRIDE);
  scheduler_->SetVirtualTimePolicy(
      PageSchedulerImpl::VirtualTimePolicy::kDeterministicLoading);

  WebScopedVirtualTimePauser pauser(
      scheduler_.get(),
      WebScopedVirtualTimePauser::VirtualTaskDuration::kInstant, "test");

  base::TimeTicks before = scheduler_->GetVirtualTimeDomain()->Now();
  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());
  pauser.PauseVirtualTime();
  EXPECT_FALSE(scheduler_->VirtualTimeAllowedToAdvance());

  pauser.UnpauseVirtualTime();
  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());
  base::TimeTicks after = scheduler_->GetVirtualTimeDomain()->Now();
  EXPECT_EQ(after, before);
}

TEST_F(MainThreadSchedulerImplTest, VirtualTimePauserNonInstantTask) {
  scheduler_->EnableVirtualTime(
      MainThreadSchedulerImpl::BaseTimeOverridePolicy::DO_NOT_OVERRIDE);
  scheduler_->SetVirtualTimePolicy(
      PageSchedulerImpl::VirtualTimePolicy::kDeterministicLoading);

  WebScopedVirtualTimePauser pauser(
      scheduler_.get(),
      WebScopedVirtualTimePauser::VirtualTaskDuration::kNonInstant, "test");

  base::TimeTicks before = scheduler_->GetVirtualTimeDomain()->Now();
  pauser.PauseVirtualTime();
  pauser.UnpauseVirtualTime();
  base::TimeTicks after = scheduler_->GetVirtualTimeDomain()->Now();
  EXPECT_GT(after, before);
}

TEST_F(MainThreadSchedulerImplTest, VirtualTimeWithOneQueueWithoutVirtualTime) {
  // This test ensures that we do not do anything strange like stopping
  // processing task queues after we encountered one task queue with
  // DoNotUseVirtualTime trait.
  scheduler_->EnableVirtualTime(
      MainThreadSchedulerImpl::BaseTimeOverridePolicy::DO_NOT_OVERRIDE);
  scheduler_->SetVirtualTimePolicy(
      PageSchedulerImpl::VirtualTimePolicy::kDeterministicLoading);

  WebScopedVirtualTimePauser pauser(
      scheduler_.get(),
      WebScopedVirtualTimePauser::VirtualTaskDuration::kNonInstant, "test");

  // Test will pass if the queue without virtual is the last one in the
  // iteration order. Create 100 of them and ensure that it is created in the
  // middle.
  std::vector<scoped_refptr<MainThreadTaskQueue>> task_queues;
  constexpr int kTaskQueueCount = 100;

  for (size_t i = 0; i < kTaskQueueCount; ++i) {
    task_queues.push_back(scheduler_->NewTaskQueue(
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::kFrameThrottleable)
            .SetCanRunWhenVirtualTimePaused(i == 42)));
  }

  // This should install a fence on all queues with virtual time.
  pauser.PauseVirtualTime();

  int counter = 0;

  for (const auto& task_queue : task_queues) {
    task_queue->task_runner()->PostTask(
        FROM_HERE, base::BindOnce([](int* counter) { ++*counter; }, &counter));
  }

  // Only the queue without virtual time should run, all others should be
  // blocked by their fences.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(counter, 1);

  pauser.UnpauseVirtualTime();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(counter, kTaskQueueCount);
}

TEST_F(MainThreadSchedulerImplTest, Tracing) {
  // This test sets renderer scheduler to some non-trivial state
  // (by posting tasks, creating child schedulers, etc) and converts it into a
  // traced value. This test checks that no internal checks fire during this.

  std::unique_ptr<PageSchedulerImpl> page_scheduler1 =
      base::WrapUnique(new PageSchedulerImpl(nullptr, scheduler_.get()));
  scheduler_->AddPageScheduler(page_scheduler1.get());

  std::unique_ptr<FrameSchedulerImpl> frame_scheduler =
      FrameSchedulerImpl::Create(page_scheduler1.get(), nullptr, nullptr,
                                 FrameScheduler::FrameType::kSubframe);

  std::unique_ptr<PageSchedulerImpl> page_scheduler2 =
      base::WrapUnique(new PageSchedulerImpl(nullptr, scheduler_.get()));
  scheduler_->AddPageScheduler(page_scheduler2.get());

  CPUTimeBudgetPool* time_budget_pool =
      scheduler_->task_queue_throttler()->CreateCPUTimeBudgetPool("test");

  time_budget_pool->AddQueue(base::TimeTicks(), timer_task_queue());

  timer_task_runner_->PostTask(FROM_HERE, base::BindOnce(NullTask));

  loading_task_queue()->task_runner()->PostDelayedTask(
      FROM_HERE, base::BindOnce(NullTask),
      base::TimeDelta::FromMilliseconds(10));

  EXPECT_FALSE(scheduler_->ToString().empty());
}

void RecordingTimeTestTask(
    Vector<base::TimeTicks>* run_times,
    scoped_refptr<base::TestMockTimeTaskRunner> task_runner) {
  run_times->push_back(task_runner->GetMockTickClock()->NowTicks());
}

//                  Nav Start     Nav Start            assert
//                     |             |                   |
//                     v             v                   v
//    ------------------------------------------------------------>
//     |---long task---|---1s task---|-----long task ----|
//
//                     (---MaxEQT1---)
//                                   (---MaxEQT2---)
//
// --- EQT untracked---|             |---EQT unflushed-----
//
// MaxEQT1 = 500ms is recorded and observed in histogram.
// MaxEQT2 is recorded but not yet in histogram for not being flushed.
TEST_F(MainThreadSchedulerImplTest,
       MaxQueueingTimeMetricRecordedOnlyDuringNavigation) {
  base::HistogramTester tester;
  // Start with a long task whose queueing time will be ignored.
  AdvanceTimeWithTask(10);
  // Navigation start.
  scheduler_->DidCommitProvisionalLoad(false, false, false);
  // The max queueing time of the following task will be recorded.
  AdvanceTimeWithTask(1);
  // The smaller queuing time will be ignored.
  AdvanceTimeWithTask(0.5);
  scheduler_->DidCommitProvisionalLoad(false, false, false);
  // Add another long task after navigation start but without navigation end.
  // This value won't be recorded as there is not navigation.
  AdvanceTimeWithTask(10);
  // The expected queueing time of 1s task in 1s window is 500ms.
  tester.ExpectUniqueSample("RendererScheduler.MaxQueueingTime", 500, 1);
}

// Only the max of all the queueing times is recorded.
TEST_F(MainThreadSchedulerImplTest, MaxQueueingTimeMetricRecordTheMax) {
  base::HistogramTester tester;
  scheduler_->DidCommitProvisionalLoad(false, false, false);
  // The smaller queuing time will be ignored.
  AdvanceTimeWithTask(0.5);
  // The max queueing time of the following task will be recorded.
  AdvanceTimeWithTask(1);
  // The smaller queuing time will be ignored.
  AdvanceTimeWithTask(0.5);
  scheduler_->DidCommitProvisionalLoad(false, false, false);
  tester.ExpectUniqueSample("RendererScheduler.MaxQueueingTime", 500, 1);
}

TEST_F(MainThreadSchedulerImplTest, LoadingControlTasks) {
  // Expect control loading tasks (M) to jump ahead of any regular loading
  // tasks (L).
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 L2 M1 L3 L4 M2 L5 L6");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("M1", "M2", "L1", "L2", "L3",
                                              "L4", "L5", "L6"));
}

TEST_F(MainThreadSchedulerImplTest, RequestBeginMainFrameNotExpected) {
  scheduler_->OnPendingTasksChanged(true);
  EXPECT_CALL(*page_scheduler_, RequestBeginMainFrameNotExpected(true))
      .Times(1)
      .WillRepeatedly(testing::Return(true));
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(page_scheduler_.get());

  scheduler_->OnPendingTasksChanged(false);
  EXPECT_CALL(*page_scheduler_, RequestBeginMainFrameNotExpected(false))
      .Times(1)
      .WillRepeatedly(testing::Return(true));
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(page_scheduler_.get());
}

TEST_F(MainThreadSchedulerImplTest,
       RequestBeginMainFrameNotExpected_MultipleCalls) {
  scheduler_->OnPendingTasksChanged(true);
  scheduler_->OnPendingTasksChanged(true);
  // Multiple calls should result in only one call.
  EXPECT_CALL(*page_scheduler_, RequestBeginMainFrameNotExpected(true))
      .Times(1)
      .WillRepeatedly(testing::Return(true));
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(page_scheduler_.get());
}

#if defined(OS_ANDROID)
TEST_F(MainThreadSchedulerImplTest, PauseTimersForAndroidWebView) {
  // Tasks in some queues don't fire when the timers are paused.
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 C1 L1 I1 T1");
  scheduler_->PauseTimersForAndroidWebView();
  EnableIdleTasks();
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "C1", "L1", "I1"));
  // The rest queued tasks fire when the timers are resumed.
  run_order.clear();
  scheduler_->ResumeTimersForAndroidWebView();
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_THAT(run_order, testing::ElementsAre("T1"));
}
#endif  // defined(OS_ANDROID)

class MainThreadSchedulerImplWithInitalVirtualTimeTest
    : public MainThreadSchedulerImplTest {
 public:
  void SetUp() override {
    CreateTestTaskRunner();
    Initialize(std::make_unique<MainThreadSchedulerImplForTest>(
        base::sequence_manager::SequenceManagerForTest::Create(
            nullptr, test_task_runner_, test_task_runner_->GetMockTickClock(),
            base::sequence_manager::SequenceManager::Settings::Builder()
                .SetRandomisedSamplingEnabled(true)
                .Build()),
        base::Time::FromJsTime(1000000.0)));
  }
};

TEST_F(MainThreadSchedulerImplWithInitalVirtualTimeTest, VirtualTimeOverride) {
  EXPECT_TRUE(scheduler_->IsVirtualTimeEnabled());
  EXPECT_EQ(PageSchedulerImpl::VirtualTimePolicy::kPause,
            scheduler_->virtual_time_policy());
  EXPECT_EQ(base::Time::Now(), base::Time::FromJsTime(1000000.0));
}

TEST_F(MainThreadSchedulerImplTest, CompositingAfterInput) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "P1 T1 C1");
  base::RunLoop().RunUntilIdle();
  // Without an explicit signal nothing should be reordered.
  EXPECT_THAT(run_order, testing::ElementsAre("P1", "T1", "C1"));
  run_order.clear();

  scheduler_->OnMainFrameRequestedForInput();

  PostTestTasks(&run_order, "T2 C2 C3");
  base::RunLoop().RunUntilIdle();
  // When a signal is present, compositing tasks should be prioritized until
  // WillBeginMainFrame is received.
  EXPECT_THAT(run_order, testing::ElementsAre("C2", "C3", "T2"));
  run_order.clear();

  scheduler_->WillBeginFrame(viz::BeginFrameArgs());

  PostTestTasks(&run_order, "T3 C4 C5");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("T3", "C4", "C5"));
  run_order.clear();
}

TEST_F(MainThreadSchedulerImplTest, EQTWithNestedLoop) {
  AdvanceMockTickClockBy(base::TimeDelta::FromMilliseconds(100));

  RunTask(base::BindLambdaForTesting([&] {
    // After running a task for 10ms, start running a nested loop.
    // This contributes to the first step EQT by 1ms ((10ms)^2 / 2 / 50ms), and
    // the window EQT by 50us (1ms / 20).
    AdvanceMockTickClockBy(base::TimeDelta::FromMilliseconds(10));
    scheduler_->OnBeginNestedRunLoop();

    // Leave the loop idle for 20ms.
    AdvanceMockTickClockBy(base::TimeDelta::FromMilliseconds(20));

    RunTask(base::BindLambdaForTesting([&] {
      // Run a 30ms task in the nested loop.
      // This contributes to the first step EQT by 8ms ((30ms + 10ms) * 20ms / 2
      // / 50ms), and the window EQT by 400us (8ms / 20). Also, contributes to
      // the second step EQT by 1ms ((10ms)^2 / 2 / 50ms), and the window EQT by
      // 50us (1ms / 20).
      AdvanceMockTickClockBy(base::TimeDelta::FromMilliseconds(30));
    }));

    // After 40ms idle duration, exit the nested loop.
    AdvanceMockTickClockBy(base::TimeDelta::FromMilliseconds(40));
    scheduler_->OnExitNestedRunLoop();

    // The outer task ends after extra 50ms work.
    // This contributes to the third step EQT by 25ms ((50ms)^2 / 2 / 50ms), and
    // the window EQT by 1250us (25ms / 20).
    AdvanceMockTickClockBy(base::TimeDelta::FromMilliseconds(50));
  }));

  EXPECT_THAT(scheduler_->expected_queueing_times(),
              testing::ElementsAre(
                  base::TimeDelta::FromMicroseconds(400 + 50),
                  base::TimeDelta::FromMicroseconds(400 + 50 + 50),
                  base::TimeDelta::FromMicroseconds(400 + 50 + 50 + 1250)));
}

TEST_F(MainThreadSchedulerImplTest, TaskQueueReferenceClearedOnShutdown) {
  // Ensure that the scheduler clears its references to a task queue after
  // |shutdown| and doesn't try to update its policies.
  scoped_refptr<MainThreadTaskQueue> queue1 = scheduler_->NewTimerTaskQueue(
      MainThreadTaskQueue::QueueType::kFrameThrottleable, nullptr);
  scoped_refptr<MainThreadTaskQueue> queue2 = scheduler_->NewTimerTaskQueue(
      MainThreadTaskQueue::QueueType::kFrameThrottleable, nullptr);

  EXPECT_EQ(queue1->GetTimeDomain(), scheduler_->real_time_domain());
  EXPECT_EQ(queue2->GetTimeDomain(), scheduler_->real_time_domain());

  scheduler_->OnShutdownTaskQueue(queue1);

  scheduler_->EnableVirtualTime(
      MainThreadSchedulerImpl::BaseTimeOverridePolicy::DO_NOT_OVERRIDE);

  // Virtual time should be enabled for queue2, as it is a regular queue and
  // nothing should change for queue1 because it was shut down.
  EXPECT_EQ(queue1->GetTimeDomain(), scheduler_->real_time_domain());
  EXPECT_EQ(queue2->GetTimeDomain(), scheduler_->GetVirtualTimeDomain());
}

TEST_F(MainThreadSchedulerImplTest, MicrotaskCheckpointTiming) {
  base::RunLoop().RunUntilIdle();

  base::TimeTicks start_time = Now();
  RecordingTaskTimeObserver observer;

  const base::TimeDelta kTaskTime = base::TimeDelta::FromMilliseconds(100);
  const base::TimeDelta kMicrotaskTime = base::TimeDelta::FromMilliseconds(200);
  default_task_runner_->PostTask(
      FROM_HERE, WTF::Bind(&MainThreadSchedulerImplTest::AdvanceMockTickClockBy,
                           base::Unretained(this), kTaskTime));
  scheduler_->on_microtask_checkpoint_ =
      WTF::Bind(&MainThreadSchedulerImplTest::AdvanceMockTickClockBy,
                base::Unretained(this), kMicrotaskTime);

  scheduler_->AddTaskTimeObserver(&observer);
  base::RunLoop().RunUntilIdle();
  scheduler_->RemoveTaskTimeObserver(&observer);

  // Expect that the duration of microtask is counted as a part of the preceding
  // task.
  ASSERT_EQ(1u, observer.result().size());
  EXPECT_EQ(start_time, observer.result().front().first);
  EXPECT_EQ(start_time + kTaskTime + kMicrotaskTime,
            observer.result().front().second);
}

TEST_F(MainThreadSchedulerImplTest, IsBeginMainFrameScheduled) {
  EXPECT_FALSE(scheduler_->IsBeginMainFrameScheduled());
  scheduler_->DidScheduleBeginMainFrame();
  EXPECT_TRUE(scheduler_->IsBeginMainFrameScheduled());
  scheduler_->DidRunBeginMainFrame();
  EXPECT_FALSE(scheduler_->IsBeginMainFrameScheduled());
  scheduler_->DidScheduleBeginMainFrame();
  scheduler_->DidScheduleBeginMainFrame();
  EXPECT_TRUE(scheduler_->IsBeginMainFrameScheduled());
  scheduler_->DidRunBeginMainFrame();
  EXPECT_TRUE(scheduler_->IsBeginMainFrameScheduled());
  scheduler_->DidRunBeginMainFrame();
  EXPECT_FALSE(scheduler_->IsBeginMainFrameScheduled());
}

TEST_F(MainThreadSchedulerImplTest, NonWakingTaskQueue) {
  std::vector<std::pair<std::string, base::TimeTicks>> log;
  base::TimeTicks start = scheduler_->GetTickClock()->NowTicks();

  scheduler_->DefaultTaskQueue()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::vector<std::pair<std::string, base::TimeTicks>>* log,
             const base::TickClock* clock) {
            log->emplace_back("regular (immediate)", clock->NowTicks());
          },
          &log, scheduler_->GetTickClock()));
  scheduler_->NonWakingTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](std::vector<std::pair<std::string, base::TimeTicks>>* log,
             const base::TickClock* clock) {
            log->emplace_back("non-waking", clock->NowTicks());
          },
          &log, scheduler_->GetTickClock()),
      base::TimeDelta::FromSeconds(3));
  scheduler_->DefaultTaskQueue()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](std::vector<std::pair<std::string, base::TimeTicks>>* log,
             const base::TickClock* clock) {
            log->emplace_back("regular (delayed)", clock->NowTicks());
          },
          &log, scheduler_->GetTickClock()),
      base::TimeDelta::FromSeconds(5));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  // Check that the non-waking task runner didn't generate an unnecessary
  // wake-up.
  // Note: the exact order of these tasks is not fixed and depends on the time
  // domain iteration order.
  EXPECT_THAT(
      log,
      testing::UnorderedElementsAre(
          std::make_pair("regular (immediate)", start),
          std::make_pair("non-waking", start + base::TimeDelta::FromSeconds(5)),
          std::make_pair("regular (delayed)",
                         start + base::TimeDelta::FromSeconds(5))));
}

class BestEffortPriorityForFindInPageExperimentTest
    : public MainThreadSchedulerImplTest {
 public:
  BestEffortPriorityForFindInPageExperimentTest()
      : MainThreadSchedulerImplTest({kBestEffortPriorityForFindInPage}, {}) {}
};

TEST_F(BestEffortPriorityForFindInPageExperimentTest,
       FindInPageTasksAreBestEffortPriorityUnderExperiment) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "F1 D1 F2 D2 F3 D3");
  EnableIdleTasks();
  EXPECT_EQ(scheduler_->find_in_page_priority(),
            QueuePriority::kBestEffortPriority);
  base::RunLoop().RunUntilIdle();
  // Find-in-page tasks have "best-effort" priority, so they will be done after
  // the default tasks (which have normal priority).
  EXPECT_THAT(run_order,
              testing::ElementsAre("D1", "D2", "D3", "F1", "F2", "F3"));
}

TEST_F(MainThreadSchedulerImplTest, FindInPageTasksAreVeryHighPriority) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 D2 D3 F1 F2 F3");
  EnableIdleTasks();
  EXPECT_EQ(
      scheduler_->find_in_page_priority(),
      FindInPageBudgetPoolController::kFindInPageBudgetNotExhaustedPriority);
  base::RunLoop().RunUntilIdle();
  // Find-in-page tasks have very high task priority, so we will do them before
  // the default tasks.
  EXPECT_THAT(run_order,
              testing::ElementsAre("F1", "F2", "F3", "D1", "D2", "D3"));
}

TEST_F(MainThreadSchedulerImplTest, FindInPageTasksChangeToNormalPriority) {
  EXPECT_EQ(
      scheduler_->find_in_page_priority(),
      FindInPageBudgetPoolController::kFindInPageBudgetNotExhaustedPriority);
  EnableIdleTasks();
  // Simulate a really long find-in-page task that takes 30% of CPU time
  // (300ms out of 1000 ms).
  base::TimeTicks task_start_time = Now();
  base::TimeTicks task_end_time =
      task_start_time + base::TimeDelta::FromMilliseconds(300);
  FakeTask fake_task;
  fake_task.set_enqueue_order(
      base::sequence_manager::EnqueueOrder::FromIntForTesting(42));
  FakeTaskTiming task_timing(task_start_time, task_end_time);
  scheduler_->OnTaskStarted(find_in_page_task_queue(), fake_task, task_timing);
  AdvanceMockTickClockTo(task_start_time +
                         base::TimeDelta::FromMilliseconds(1000));
  scheduler_->OnTaskCompleted(find_in_page_task_queue()->AsWeakPtr(), fake_task,
                              &task_timing, nullptr);

  // Now the find-in-page tasks have normal priority (same priority as default
  // tasks, so we will do them in order).
  EXPECT_EQ(scheduler_->find_in_page_priority(),
            FindInPageBudgetPoolController::kFindInPageBudgetExhaustedPriority);
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 D2 F1 F2 D3 F3");

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("D1", "D2", "F1", "F2", "D3", "F3"));
}

class VeryHighPriorityForCompositingAlwaysExperimentTest
    : public MainThreadSchedulerImplTest {
 public:
  VeryHighPriorityForCompositingAlwaysExperimentTest()
      : MainThreadSchedulerImplTest({kVeryHighPriorityForCompositingAlways},
                                    {}) {}
};

TEST_F(VeryHighPriorityForCompositingAlwaysExperimentTest,
       TestCompositorPolicy) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2 P1");

  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("P1", "C1", "C2", "D1", "D2", "I1"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

class VeryHighPriorityForCompositingWhenFastExperimentTest
    : public MainThreadSchedulerImplTest {
 public:
  VeryHighPriorityForCompositingWhenFastExperimentTest()
      : MainThreadSchedulerImplTest({kVeryHighPriorityForCompositingWhenFast},
                                    {}) {}
};

TEST_F(VeryHighPriorityForCompositingWhenFastExperimentTest,
       TestCompositorPolicy_FastCompositing) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2 P1");

  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("P1", "C1", "C2", "D1", "D2", "I1"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(VeryHighPriorityForCompositingWhenFastExperimentTest,
       TestCompositorPolicy_SlowCompositing) {
  RunSlowCompositorTask();
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2 P1");

  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("P1", "D1", "C1", "D2", "C2", "I1"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(VeryHighPriorityForCompositingWhenFastExperimentTest,
       TestCompositorPolicy_CompositingStaysAtHighest) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 P1 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EnableIdleTasks();
  SimulateMainThreadGestureStart(
      TouchEventPolicy::kSendTouchStart,
      blink::WebInputEvent::Type::kGestureScrollBegin);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("C1", "P1", "C2", "L1", "D1", "D2", "I1"));
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());
}

class VeryHighPriorityForCompositingAlternatingExperimentTest
    : public MainThreadSchedulerImplTest {
 public:
  VeryHighPriorityForCompositingAlternatingExperimentTest()
      : MainThreadSchedulerImplTest(
            {kVeryHighPriorityForCompositingAlternating},
            {}) {}
};

TEST_F(VeryHighPriorityForCompositingAlternatingExperimentTest,
       TestCompositorPolicy_AlternatingCompositorTasks) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 D2 D3 C1 C2 C3");

  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("C1", "D1", "C2", "D2", "C3", "D3"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(VeryHighPriorityForCompositingAlternatingExperimentTest,
       TestCompositorPolicy_AlternatingCompositorStaysAtHighest) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 D2 D3 C1 C2 C3");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EnableIdleTasks();
  SimulateMainThreadGestureStart(
      TouchEventPolicy::kSendTouchStart,
      blink::WebInputEvent::Type::kGestureScrollBegin);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("C1", "C2", "C3", "D1", "D2", "D3"));
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());
}

class VeryHighPriorityForCompositingAfterDelayExperimentTest
    : public MainThreadSchedulerImplTest {
 public:
  VeryHighPriorityForCompositingAfterDelayExperimentTest()
      : MainThreadSchedulerImplTest({kVeryHighPriorityForCompositingAfterDelay},
                                    {}) {}
};

TEST_F(VeryHighPriorityForCompositingAfterDelayExperimentTest,
       TestCompositorPolicy_CompositorStaysAtNormalPriority) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2 P1");

  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("P1", "D1", "C1", "D2", "C2", "I1"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(VeryHighPriorityForCompositingAfterDelayExperimentTest,
       TestCompositorPolicy_FirstCompositorTaskSetToVeryHighPriority) {
  // 150ms task to complete the countdown and prioritze compositing.
  AdvanceTimeWithTask(0.15);

  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2 P1");

  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("P1", "C1", "D1", "D2", "C2", "I1"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(VeryHighPriorityForCompositingAfterDelayExperimentTest,
       TestCompositorPolicy_FirstCompositorTaskStaysAtNormalPriority) {
  // 0.5ms task should not prioritize compositing.
  AdvanceTimeWithTask(0.05);

  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2 P1");

  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("P1", "D1", "C1", "D2", "C2", "I1"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

class VeryHighPriorityForCompositingBudgetExperimentTest
    : public MainThreadSchedulerImplTest {
 public:
  VeryHighPriorityForCompositingBudgetExperimentTest()
      : MainThreadSchedulerImplTest({kVeryHighPriorityForCompositingBudget},
                                    {}) {}
};

TEST_F(VeryHighPriorityForCompositingBudgetExperimentTest,
       TestCompositorPolicy_CompositorPriorityVeryHighToNormal) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2 P1");
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();

  // Compositor is at kVeryHighPriority Initially.
  EXPECT_THAT(run_order,
              testing::ElementsAre("P1", "C1", "C2", "D1", "D2", "I1"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());

  // 1000ms compositor task will exhaust the budget.
  RunSlowCompositorTask();

  run_order.clear();
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2 P1");
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();

  // Compositor is now at kVeryHighPriority, compositing tasks will run
  // before default tasks.
  EXPECT_THAT(run_order,
              testing::ElementsAre("P1", "D1", "C1", "D2", "C2", "I1"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(VeryHighPriorityForCompositingBudgetExperimentTest,
       TestCompositorPolicy_CompositorPriorityNormalToVeryHigh) {
  // 1000ms compositor task will exhaust the budget.
  RunSlowCompositorTask();

  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2 P1");
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();

  // Compositor is at kNormalPriority, it will inteleave with default tasks.
  EXPECT_THAT(run_order,
              testing::ElementsAre("P1", "D1", "C1", "D2", "C2", "I1"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());

  // Recover the budget.
  AdvanceTimeWithTask(12);

  run_order.clear();
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2 P1");
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();

  // Compositor is now at kVeryHighPriority, compositing tasks will run
  // before default tasks.
  EXPECT_THAT(run_order,
              testing::ElementsAre("P1", "C1", "C2", "D1", "D2", "I1"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

class VeryHighPriorityForCompositingAlternatingBeginMainFrameExperimentTest
    : public MainThreadSchedulerImplTest {
 public:
  VeryHighPriorityForCompositingAlternatingBeginMainFrameExperimentTest()
      : MainThreadSchedulerImplTest({kVeryHighPriorityForCompositingAlternating,
                                     kPrioritizeCompositingUntilBeginMainFrame},
                                    {}) {}
};

TEST_F(VeryHighPriorityForCompositingAlternatingBeginMainFrameExperimentTest,
       TestCompositorPolicy_AlternatingCompositorTasks) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "C1 D1 C2 D2");

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1", "D2"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());

  // Next compositor task is the BeginMainFrame. Compositor priority is set
  // to normal for a single task before being prioritized again.
  DoMainFrame();

  run_order.clear();
  PostTestTasks(&run_order, "C1 D1 D2 C2");

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "D1", "C2", "D2"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

class VeryHighPriorityForCompositingAfterDelayUntilBeginMainFrameExperimentTest
    : public MainThreadSchedulerImplTest {
 public:
  VeryHighPriorityForCompositingAfterDelayUntilBeginMainFrameExperimentTest()
      : MainThreadSchedulerImplTest({kVeryHighPriorityForCompositingAfterDelay,
                                     kPrioritizeCompositingUntilBeginMainFrame},
                                    {}) {}
};

TEST_F(
    VeryHighPriorityForCompositingAfterDelayUntilBeginMainFrameExperimentTest,
    TestCompositorPolicy_FirstCompositorTaskSetToVeryHighPriority) {
  // 150ms task to complete the countdown and prioritze compositing.
  AdvanceTimeWithTask(0.15);

  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 C1 D2 C2 P1");

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("P1", "C1", "C2", "D1", "D2"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());

  // Next compositor task is the BeginMainFrame.
  DoMainFrame();
  run_order.clear();
  PostTestTasks(&run_order, "C1 D1 D2 C2 P1");

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("P1", "C1", "D1", "D2", "C2"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

class VeryHighPriorityForCompositingBudgetBeginMainFrameExperimentTest
    : public MainThreadSchedulerImplTest {
 public:
  VeryHighPriorityForCompositingBudgetBeginMainFrameExperimentTest()
      : MainThreadSchedulerImplTest({kVeryHighPriorityForCompositingBudget,
                                     kPrioritizeCompositingUntilBeginMainFrame},
                                    {}) {}
};

TEST_F(
    VeryHighPriorityForCompositingBudgetBeginMainFrameExperimentTest,
    TestCompositorPolicy_CompositorPriorityNonBeginMainFrameDoesntExhaustBudget) {
  // 1000ms compositor task will not exhaust the budget.
  RunSlowCompositorTask();

  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 C1 D2 C2 P1");
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, testing::ElementsAre("P1", "C1", "C2", "D1", "D2"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(VeryHighPriorityForCompositingBudgetBeginMainFrameExperimentTest,
       TestCompositorPolicy_CompositorPriorityBeginMainFrameExhaustsBudget) {
  // 1000ms BeginMainFrame will exhaust the budget.
  DoMainFrame();
  RunSlowCompositorTask();

  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 C1 D2 C2 P1");
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, testing::ElementsAre("P1", "D1", "C1", "D2", "C2"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

}  // namespace main_thread_scheduler_impl_unittest
}  // namespace scheduler
}  // namespace blink
