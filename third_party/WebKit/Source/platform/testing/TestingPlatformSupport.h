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

#ifndef TestingPlatformSupport_h
#define TestingPlatformSupport_h

#include "platform/PlatformExport.h"
#include "platform/WebTaskRunner.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebThread.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include <memory>
#include <utility>

namespace base {
class SimpleTestTickClock;
class TestDiscardableMemoryAllocator;
}

namespace cc {
class OrderedSimpleTaskRunner;
}

namespace cc_blink {
class WebCompositorSupportImpl;
}  // namespace cc_blink

namespace blink {
namespace scheduler {
class RendererScheduler;
class RendererSchedulerImpl;
}
class WebCompositorSupport;
class WebThread;

class TestingCompositorSupport : public WebCompositorSupport {
  std::unique_ptr<WebLayer> createLayer() override;
  std::unique_ptr<WebLayer> createLayerFromCCLayer(cc::Layer*) override;
  std::unique_ptr<WebContentLayer> createContentLayer(
      WebContentLayerClient*) override;
  std::unique_ptr<WebExternalTextureLayer> createExternalTextureLayer(
      cc::TextureLayerClient*) override;
  std::unique_ptr<WebImageLayer> createImageLayer() override;
  std::unique_ptr<WebScrollbarLayer> createScrollbarLayer(
      std::unique_ptr<WebScrollbar>,
      WebScrollbarThemePainter,
      std::unique_ptr<WebScrollbarThemeGeometry>) override;
  std::unique_ptr<WebScrollbarLayer> createOverlayScrollbarLayer(
      std::unique_ptr<WebScrollbar>,
      WebScrollbarThemePainter,
      std::unique_ptr<WebScrollbarThemeGeometry>) override;
  std::unique_ptr<WebScrollbarLayer> createSolidColorScrollbarLayer(
      WebScrollbar::Orientation,
      int thumbThickness,
      int trackStart,
      bool isLeftSideVerticalScrollbar) override;
};

// A base class to override Platform methods for testing.  You can override the
// behavior by subclassing TestingPlatformSupport or using
// ScopedTestingPlatformSupport (see below).
class TestingPlatformSupport : public Platform {
  WTF_MAKE_NONCOPYABLE(TestingPlatformSupport);

 public:
  struct Config {
    WebCompositorSupport* compositorSupport = nullptr;
  };

  TestingPlatformSupport();
  explicit TestingPlatformSupport(const Config&);

  ~TestingPlatformSupport() override;

  // Platform:
  WebString defaultLocale() override;
  WebCompositorSupport* compositorSupport() override;
  WebThread* currentThread() override;
  WebBlobRegistry* getBlobRegistry() override;
  WebClipboard* clipboard() override;
  WebFileUtilities* fileUtilities() override;
  WebIDBFactory* idbFactory() override;
  WebURLLoaderMockFactory* getURLLoaderMockFactory() override;
  blink::WebURLLoader* createURLLoader() override;
  WebData loadResource(const char* name) override;
  WebURLError cancelledError(const WebURL&) const override;
  InterfaceProvider* interfaceProvider() override;

  virtual void runUntilIdle();

 protected:
  class TestingInterfaceProvider;

  const Config m_config;
  Platform* const m_oldPlatform;
  std::unique_ptr<TestingInterfaceProvider> m_interfaceProvider;
};

// This class adds mocked scheduler support to TestingPlatformSupport.  See also
// ScopedTestingPlatformSupport to use this class correctly.
class TestingPlatformSupportWithMockScheduler : public TestingPlatformSupport {
  WTF_MAKE_NONCOPYABLE(TestingPlatformSupportWithMockScheduler);

 public:
  TestingPlatformSupportWithMockScheduler();
  explicit TestingPlatformSupportWithMockScheduler(const Config&);
  ~TestingPlatformSupportWithMockScheduler() override;

  // Platform:
  WebThread* currentThread() override;

  // Runs a single task.
  void runSingleTask();

  // Runs all currently queued immediate tasks and delayed tasks whose delay has
  // expired plus any immediate tasks that are posted as a result of running
  // those tasks.
  //
  // This function ignores future delayed tasks when deciding if the system is
  // idle.  If you need to ensure delayed tasks run, try runForPeriodSeconds()
  // instead.
  void runUntilIdle() override;

  // Runs for |seconds| the testing clock is advanced by |seconds|.  Note real
  // time elapsed will typically much less than |seconds| because delays between
  // timers are fast forwarded.
  void runForPeriodSeconds(double seconds);

  // Advances |m_clock| by |seconds|.
  void advanceClockSeconds(double seconds);

  scheduler::RendererScheduler* rendererScheduler() const;

  // Controls the behavior of |m_mockTaskRunner| if true, then |m_clock| will
  // be advanced to the next timer when there's no more immediate work to do.
  void setAutoAdvanceNowToPendingTasks(bool);

 protected:
  static double getTestTime();

  std::unique_ptr<base::SimpleTestTickClock> m_clock;
  scoped_refptr<cc::OrderedSimpleTaskRunner> m_mockTaskRunner;
  std::unique_ptr<scheduler::RendererSchedulerImpl> m_scheduler;
  std::unique_ptr<WebThread> m_thread;
};

// ScopedTestingPlatformSupport<MyTestingPlatformSupport> can be used to
// override Platform::current() with MyTestingPlatformSupport, like this:
//
// #include "platform/testing/TestingPlatformSupport.h"
//
// TEST_F(SampleTest, sampleTest) {
//   ScopedTestingPlatformSupport<MyTestingPlatformSupport> platform;
//   ...
//   // You can call methods of MyTestingPlatformSupport.
//   EXPECT_TRUE(platform->myMethodIsCalled());
//
//   // Another instance can be nested.
//   {
//     // Constructor's arguments can be passed like this.
//     Arg arg;
//     ScopedTestingPlatformSupport<MyAnotherTestingPlatformSupport, const Arg&>
//         another_platform(args);
//     ...
//   }
//
//   // Here the original MyTestingPlatformSupport should be restored.
// }
template <class T, typename... Args>
class ScopedTestingPlatformSupport final {
  WTF_MAKE_NONCOPYABLE(ScopedTestingPlatformSupport);

 public:
  explicit ScopedTestingPlatformSupport(Args&&... args) {
    m_testingPlatformSupport = WTF::makeUnique<T>(std::forward<Args>(args)...);
    m_originalPlatform = Platform::current();
    DCHECK(m_originalPlatform);
    Platform::setCurrentPlatformForTesting(m_testingPlatformSupport.get());
  }
  ~ScopedTestingPlatformSupport() {
    DCHECK_EQ(m_testingPlatformSupport.get(), Platform::current());
    m_testingPlatformSupport.reset();
    Platform::setCurrentPlatformForTesting(m_originalPlatform);
  }

  const T* operator->() const { return m_testingPlatformSupport.get(); }
  T* operator->() { return m_testingPlatformSupport.get(); }

 private:
  std::unique_ptr<T> m_testingPlatformSupport;
  Platform* m_originalPlatform;
};

class ScopedUnittestsEnvironmentSetup final {
  WTF_MAKE_NONCOPYABLE(ScopedUnittestsEnvironmentSetup);

 public:
  ScopedUnittestsEnvironmentSetup(int argc, char** argv);
  ~ScopedUnittestsEnvironmentSetup();

 private:
  class DummyPlatform;
  std::unique_ptr<base::TestDiscardableMemoryAllocator>
      m_discardableMemoryAllocator;
  std::unique_ptr<DummyPlatform> m_dummyPlatform;
  std::unique_ptr<cc_blink::WebCompositorSupportImpl> m_compositorSupport;
  TestingPlatformSupport::Config m_testingPlatformConfig;
  std::unique_ptr<TestingPlatformSupport> m_testingPlatformSupport;
};

}  // namespace blink

#endif  // TestingPlatformSupport_h
