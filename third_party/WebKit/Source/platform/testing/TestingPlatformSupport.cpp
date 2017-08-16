/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/testing/TestingPlatformSupport.h"

#include <memory>
#include "base/command_line.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/test/icu_test_util.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "cc/blink/web_compositor_support_impl.h"
#include "components/viz/test/ordered_simple_task_runner.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "platform/FontFamilyNames.h"
#include "platform/HTTPNames.h"
#include "platform/Language.h"
#include "platform/heap/Heap.h"
#include "platform/instrumentation/resource_coordinator/BlinkResourceCoordinatorBase.h"
#include "platform/instrumentation/resource_coordinator/RendererResourceCoordinator.h"
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "platform/network/mime/MockMimeRegistry.h"
#include "platform/scheduler/base/real_time_domain.h"
#include "platform/scheduler/base/task_queue_manager.h"
#include "platform/scheduler/base/test_time_source.h"
#include "platform/scheduler/child/scheduler_tqm_delegate_for_test.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/wtf/CryptographicallyRandomNumber.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/WTF.h"
#include "platform/wtf/allocator/Partitions.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/WebContentLayer.h"
#include "public/platform/WebExternalTextureLayer.h"
#include "public/platform/WebImageLayer.h"
#include "public/platform/WebScrollbarLayer.h"

namespace blink {

class TestingPlatformSupport::TestingInterfaceProvider
    : public blink::InterfaceProvider {
 public:
  TestingInterfaceProvider() = default;
  virtual ~TestingInterfaceProvider() = default;

  void GetInterface(const char* name,
                    mojo::ScopedMessagePipeHandle handle) override {
    if (std::string(name) == mojom::blink::MimeRegistry::Name_) {
      mojo::MakeStrongBinding(
          WTF::MakeUnique<MockMimeRegistry>(),
          mojom::blink::MimeRegistryRequest(std::move(handle)));
      return;
    }
  }
};

namespace {

double DummyCurrentTime() {
  return 0.0;
}

class DummyThread final : public blink::WebThread {
 public:
  bool IsCurrentThread() const override { return true; }
  blink::WebScheduler* Scheduler() const override { return nullptr; }
};

}  // namespace

std::unique_ptr<WebLayer> TestingCompositorSupport::CreateLayer() {
  return nullptr;
}

std::unique_ptr<WebLayer> TestingCompositorSupport::CreateLayerFromCCLayer(
    cc::Layer*) {
  return nullptr;
}

std::unique_ptr<WebContentLayer> TestingCompositorSupport::CreateContentLayer(
    WebContentLayerClient*) {
  return nullptr;
}
std::unique_ptr<WebExternalTextureLayer>
TestingCompositorSupport::CreateExternalTextureLayer(cc::TextureLayerClient*) {
  return nullptr;
}

std::unique_ptr<WebImageLayer> TestingCompositorSupport::CreateImageLayer() {
  return nullptr;
}

std::unique_ptr<WebScrollbarLayer>
TestingCompositorSupport::CreateScrollbarLayer(
    std::unique_ptr<WebScrollbar>,
    WebScrollbarThemePainter,
    std::unique_ptr<WebScrollbarThemeGeometry>) {
  return nullptr;
}

std::unique_ptr<WebScrollbarLayer>
TestingCompositorSupport::CreateOverlayScrollbarLayer(
    std::unique_ptr<WebScrollbar>,
    WebScrollbarThemePainter,
    std::unique_ptr<WebScrollbarThemeGeometry>) {
  return nullptr;
}

std::unique_ptr<WebScrollbarLayer>
TestingCompositorSupport::CreateSolidColorScrollbarLayer(
    WebScrollbar::Orientation,
    int thumb_thickness,
    int track_start,
    bool is_left_side_vertical_scrollbar) {
  return nullptr;
}

TestingPlatformSupport::TestingPlatformSupport()
    : TestingPlatformSupport(TestingPlatformSupport::Config()) {}

TestingPlatformSupport::TestingPlatformSupport(const Config& config)
    : config_(config),
      old_platform_(Platform::Current()),
      interface_provider_(new TestingInterfaceProvider) {
  DCHECK(old_platform_);
}

TestingPlatformSupport::~TestingPlatformSupport() {
  DCHECK_EQ(this, Platform::Current());
}

WebString TestingPlatformSupport::DefaultLocale() {
  return WebString::FromUTF8("en-US");
}

WebCompositorSupport* TestingPlatformSupport::CompositorSupport() {
  if (config_.compositor_support)
    return config_.compositor_support;

  return old_platform_ ? old_platform_->CompositorSupport() : nullptr;
}

WebThread* TestingPlatformSupport::CurrentThread() {
  return old_platform_ ? old_platform_->CurrentThread() : nullptr;
}

WebBlobRegistry* TestingPlatformSupport::GetBlobRegistry() {
  return old_platform_ ? old_platform_->GetBlobRegistry() : nullptr;
}

WebClipboard* TestingPlatformSupport::Clipboard() {
  return old_platform_ ? old_platform_->Clipboard() : nullptr;
}

WebFileUtilities* TestingPlatformSupport::GetFileUtilities() {
  return old_platform_ ? old_platform_->GetFileUtilities() : nullptr;
}

WebIDBFactory* TestingPlatformSupport::IdbFactory() {
  return old_platform_ ? old_platform_->IdbFactory() : nullptr;
}

WebURLLoaderMockFactory* TestingPlatformSupport::GetURLLoaderMockFactory() {
  return old_platform_ ? old_platform_->GetURLLoaderMockFactory() : nullptr;
}

std::unique_ptr<WebURLLoader> TestingPlatformSupport::CreateURLLoader(
    const WebURLRequest& request,
    base::SingleThreadTaskRunner* runner) {
  return old_platform_ ? old_platform_->CreateURLLoader(request, runner)
                       : nullptr;
}

WebData TestingPlatformSupport::GetDataResource(const char* name) {
  return old_platform_ ? old_platform_->GetDataResource(name) : WebData();
}

InterfaceProvider* TestingPlatformSupport::GetInterfaceProvider() {
  return interface_provider_.get();
}

void TestingPlatformSupport::RunUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

// TestingPlatformSupportWithMockScheduler definition:

TestingPlatformSupportWithMockScheduler::
    TestingPlatformSupportWithMockScheduler()
    : TestingPlatformSupportWithMockScheduler(
          TestingPlatformSupport::Config()) {}

TestingPlatformSupportWithMockScheduler::
    TestingPlatformSupportWithMockScheduler(const Config& config)
    : TestingPlatformSupport(config),
      clock_(new base::SimpleTestTickClock()),
      mock_task_runner_(new cc::OrderedSimpleTaskRunner(clock_.get(), true)),
      scheduler_(new scheduler::RendererSchedulerImpl(
          scheduler::SchedulerTqmDelegateForTest::Create(
              mock_task_runner_,
              base::WrapUnique(new scheduler::TestTimeSource(clock_.get()))))),
      thread_(scheduler_->CreateMainThread()) {
  // Set the work batch size to one so RunPendingTasks behaves as expected.
  scheduler_->GetSchedulerHelperForTesting()->SetWorkBatchSizeForTesting(1);

  WTF::SetTimeFunctionsForTesting(GetTestTime);
}

TestingPlatformSupportWithMockScheduler::
    ~TestingPlatformSupportWithMockScheduler() {
  WTF::SetTimeFunctionsForTesting(nullptr);
  scheduler_->Shutdown();
}

WebThread* TestingPlatformSupportWithMockScheduler::CurrentThread() {
  if (thread_->IsCurrentThread())
    return thread_.get();
  return TestingPlatformSupport::CurrentThread();
}

void TestingPlatformSupportWithMockScheduler::RunSingleTask() {
  mock_task_runner_->SetRunTaskLimit(1);
  mock_task_runner_->RunPendingTasks();
  mock_task_runner_->ClearRunTaskLimit();
}

void TestingPlatformSupportWithMockScheduler::RunUntilIdle() {
  mock_task_runner_->RunUntilIdle();
}

void TestingPlatformSupportWithMockScheduler::RunForPeriodSeconds(
    double seconds) {
  const base::TimeTicks deadline =
      clock_->NowTicks() + base::TimeDelta::FromSecondsD(seconds);

  scheduler::TaskQueueManager* task_queue_manager =
      scheduler_->GetSchedulerHelperForTesting()
          ->GetTaskQueueManagerForTesting();

  for (;;) {
    // If we've run out of immediate work then fast forward to the next delayed
    // task, but don't pass |deadline|.
    if (!task_queue_manager->HasImmediateWorkForTesting()) {
      base::TimeTicks next_delayed_task;
      if (!task_queue_manager->real_time_domain()->NextScheduledRunTime(
              &next_delayed_task) ||
          next_delayed_task > deadline) {
        break;
      }

      clock_->SetNowTicks(next_delayed_task);
    }

    if (clock_->NowTicks() > deadline)
      break;

    mock_task_runner_->RunPendingTasks();
  }

  clock_->SetNowTicks(deadline);
}

void TestingPlatformSupportWithMockScheduler::AdvanceClockSeconds(
    double seconds) {
  clock_->Advance(base::TimeDelta::FromSecondsD(seconds));
}

void TestingPlatformSupportWithMockScheduler::SetAutoAdvanceNowToPendingTasks(
    bool auto_advance) {
  mock_task_runner_->SetAutoAdvanceNowToPendingTasks(auto_advance);
}

scheduler::RendererSchedulerImpl*
TestingPlatformSupportWithMockScheduler::GetRendererScheduler() const {
  return scheduler_.get();
}

// static
double TestingPlatformSupportWithMockScheduler::GetTestTime() {
  TestingPlatformSupportWithMockScheduler* platform =
      static_cast<TestingPlatformSupportWithMockScheduler*>(
          Platform::Current());
  return (platform->clock_->NowTicks() - base::TimeTicks()).InSecondsF();
}

class ScopedUnittestsEnvironmentSetup::DummyPlatform final
    : public blink::Platform {
 public:
  DummyPlatform() {}

  blink::WebThread* CurrentThread() override {
    static DummyThread dummy_thread;
    return &dummy_thread;
  };
};

class ScopedUnittestsEnvironmentSetup::DummyRendererResourceCoordinator final
    : public blink::RendererResourceCoordinator {};

ScopedUnittestsEnvironmentSetup::ScopedUnittestsEnvironmentSetup(int argc,
                                                                 char** argv) {
  base::CommandLine::Init(argc, argv);

  base::test::InitializeICUForTesting();

  discardable_memory_allocator_ =
      WTF::WrapUnique(new base::TestDiscardableMemoryAllocator);
  base::DiscardableMemoryAllocator::SetInstance(
      discardable_memory_allocator_.get());
  base::StatisticsRecorder::Initialize();

  dummy_platform_ = WTF::WrapUnique(new DummyPlatform);
  Platform::SetCurrentPlatformForTesting(dummy_platform_.get());

  WTF::Partitions::Initialize(nullptr);
  WTF::SetTimeFunctionsForTesting(DummyCurrentTime);
  WTF::Initialize(nullptr);

  compositor_support_ = WTF::WrapUnique(new cc_blink::WebCompositorSupportImpl);
  testing_platform_config_.compositor_support = compositor_support_.get();
  testing_platform_support_ =
      WTF::WrapUnique(new TestingPlatformSupport(testing_platform_config_));
  Platform::SetCurrentPlatformForTesting(testing_platform_support_.get());

  if (BlinkResourceCoordinatorBase::IsEnabled()) {
    dummy_renderer_resource_coordinator_ =
        WTF::WrapUnique(new DummyRendererResourceCoordinator);
    RendererResourceCoordinator::
        SetCurrentRendererResourceCoordinatorForTesting(
            dummy_renderer_resource_coordinator_.get());
  }

  ProcessHeap::Init();
  ThreadState::AttachMainThread();
  ThreadState::Current()->RegisterTraceDOMWrappers(nullptr, nullptr, nullptr,
                                                   nullptr);
  HTTPNames::init();
  FetchInitiatorTypeNames::init();

  InitializePlatformLanguage();
  FontFamilyNames::init();
}

ScopedUnittestsEnvironmentSetup::~ScopedUnittestsEnvironmentSetup() {}

}  // namespace blink
