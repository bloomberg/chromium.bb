// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/wake_lock/ScreenWakeLock.h"

#include "core/dom/DOMImplementation.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentInit.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/WebPageVisibilityState.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/FrameTestHelpers.h"

#include <memory>

namespace {

using blink::ScreenWakeLock;
using device::mojom::blink::WakeLockService;
using device::mojom::blink::WakeLockServiceRequest;

// This class allows binding interface requests to a MockWakeLockService.
class MockInterfaceProvider : public blink::InterfaceProvider {
 public:
  MockInterfaceProvider() : m_wakeLockStatus(false) {}
  ~MockInterfaceProvider() {}

  void getInterface(const char* name, mojo::ScopedMessagePipeHandle) override;

  bool wakeLockStatus() const { return m_wakeLockStatus; }
  void setWakeLockStatus(bool status) { m_wakeLockStatus = status; }

 private:
  // A mock WakeLockService used to intercept calls to the mojo methods.
  class MockWakeLockService : public WakeLockService {
   public:
    MockWakeLockService(MockInterfaceProvider* registry,
                        WakeLockServiceRequest request)
        : m_binding(this, std::move(request)), m_registry(registry) {}
    ~MockWakeLockService() {}

   private:
    // mojom::WakeLockService
    void RequestWakeLock() override { m_registry->setWakeLockStatus(true); }
    void CancelWakeLock() override { m_registry->setWakeLockStatus(false); }

    mojo::Binding<WakeLockService> m_binding;
    MockInterfaceProvider* const m_registry;
  };
  std::unique_ptr<MockWakeLockService> m_mockWakeLockService;

  bool m_wakeLockStatus;
};

void MockInterfaceProvider::getInterface(const char* name,
                                         mojo::ScopedMessagePipeHandle handle) {
  m_mockWakeLockService.reset(new MockWakeLockService(
      this, mojo::MakeRequest<WakeLockService>(std::move(handle))));
}

// A TestWebFrameClient to allow overriding the interfaceProvider() with a mock.
class TestWebFrameClient : public blink::FrameTestHelpers::TestWebFrameClient {
 public:
  ~TestWebFrameClient() override = default;
  blink::InterfaceProvider* interfaceProvider() override {
    return &m_interfaceProvider;
  }

 private:
  MockInterfaceProvider m_interfaceProvider;
};

class ScreenWakeLockTest : public testing::Test {
 protected:
  void SetUp() override {
    m_webViewHelper.initialize(true, &m_testWebFrameClient);
    blink::URLTestHelpers::registerMockedURLLoadFromBase(
        blink::WebString::fromUTF8("http://example.com/"),
        blink::testing::webTestDataPath(),
        blink::WebString::fromUTF8("foo.html"));
    loadFrame();
  }

  void TearDown() override {
    blink::Platform::current()
        ->getURLLoaderMockFactory()
        ->unregisterAllURLsAndClearMemoryCache();
    blink::testing::runPendingTasks();
  }

  void loadFrame() {
    blink::FrameTestHelpers::loadFrame(m_webViewHelper.webView()->mainFrame(),
                                       "http://example.com/foo.html");
    m_webViewHelper.webView()->updateAllLifecyclePhases();
  }

  blink::LocalFrame* frame() {
    DCHECK(m_webViewHelper.webView());
    DCHECK(m_webViewHelper.webView()->mainFrameImpl());
    return m_webViewHelper.webView()->mainFrameImpl()->frame();
  }

  blink::Screen* screen() {
    DCHECK(frame());
    DCHECK(frame()->domWindow());
    return frame()->domWindow()->screen();
  }

  bool screenKeepAwake() {
    DCHECK(screen());
    return ScreenWakeLock::keepAwake(*screen());
  }

  bool clientKeepScreenAwake() {
    return static_cast<MockInterfaceProvider*>(
               m_testWebFrameClient.interfaceProvider())
        ->wakeLockStatus();
  }

  void setKeepAwake(bool keepAwake) {
    DCHECK(screen());
    ScreenWakeLock::setKeepAwake(*screen(), keepAwake);
    // Let the notification sink through the mojo pipes.
    blink::testing::runPendingTasks();
  }

  void show() {
    DCHECK(m_webViewHelper.webView());
    m_webViewHelper.webView()->setVisibilityState(
        blink::WebPageVisibilityStateVisible, false);
    // Let the notification sink through the mojo pipes.
    blink::testing::runPendingTasks();
  }

  void hide() {
    DCHECK(m_webViewHelper.webView());
    m_webViewHelper.webView()->setVisibilityState(
        blink::WebPageVisibilityStateHidden, false);
    // Let the notification sink through the mojo pipes.
    blink::testing::runPendingTasks();
  }

  // Order of these members is important as we need to make sure that
  // m_testWebFrameClient outlives m_webViewHelper (destruction order)
  TestWebFrameClient m_testWebFrameClient;
  blink::FrameTestHelpers::WebViewHelper m_webViewHelper;
};

TEST_F(ScreenWakeLockTest, setAndReset) {
  ASSERT_FALSE(screenKeepAwake());
  ASSERT_FALSE(clientKeepScreenAwake());

  setKeepAwake(true);
  EXPECT_TRUE(screenKeepAwake());
  EXPECT_TRUE(clientKeepScreenAwake());

  setKeepAwake(false);
  EXPECT_FALSE(screenKeepAwake());
  EXPECT_FALSE(clientKeepScreenAwake());
}

TEST_F(ScreenWakeLockTest, hideWhenSet) {
  ASSERT_FALSE(screenKeepAwake());
  ASSERT_FALSE(clientKeepScreenAwake());

  setKeepAwake(true);
  hide();

  EXPECT_TRUE(screenKeepAwake());
  EXPECT_FALSE(clientKeepScreenAwake());
}

TEST_F(ScreenWakeLockTest, setWhenHidden) {
  ASSERT_FALSE(screenKeepAwake());
  ASSERT_FALSE(clientKeepScreenAwake());

  hide();
  setKeepAwake(true);

  EXPECT_TRUE(screenKeepAwake());
  EXPECT_FALSE(clientKeepScreenAwake());
}

TEST_F(ScreenWakeLockTest, showWhenSet) {
  ASSERT_FALSE(screenKeepAwake());
  ASSERT_FALSE(clientKeepScreenAwake());

  hide();
  setKeepAwake(true);
  show();

  EXPECT_TRUE(screenKeepAwake());
  EXPECT_TRUE(clientKeepScreenAwake());
}

TEST_F(ScreenWakeLockTest, navigate) {
  ASSERT_FALSE(screenKeepAwake());
  ASSERT_FALSE(clientKeepScreenAwake());

  setKeepAwake(true);
  loadFrame();

  EXPECT_FALSE(screenKeepAwake());
  EXPECT_FALSE(clientKeepScreenAwake());
}

}  // namespace
