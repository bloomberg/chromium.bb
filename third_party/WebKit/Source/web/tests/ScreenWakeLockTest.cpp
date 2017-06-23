// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/wake_lock/ScreenWakeLock.h"

#include "core/dom/DOMImplementation.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentInit.h"
#include "core/frame/FrameTestHelpers.h"
#include "core/frame/WebLocalFrameBase.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/WebPageVisibilityState.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <memory>

namespace blink {
namespace {

using device::mojom::blink::WakeLock;
using device::mojom::blink::WakeLockRequest;

// This class allows binding interface requests to a MockWakeLock.
class MockInterfaceProvider : public InterfaceProvider {
 public:
  MockInterfaceProvider() : wake_lock_status_(false) {}
  ~MockInterfaceProvider() {}

  void GetInterface(const char* name, mojo::ScopedMessagePipeHandle) override;

  bool WakeLockStatus() const { return wake_lock_status_; }
  void SetWakeLockStatus(bool status) { wake_lock_status_ = status; }

 private:
  // A mock WakeLock used to intercept calls to the mojo methods.
  class MockWakeLock : public WakeLock {
   public:
    MockWakeLock(MockInterfaceProvider* registry, WakeLockRequest request)
        : binding_(this, std::move(request)), registry_(registry) {}
    ~MockWakeLock() {}

   private:
    // mojom::WakeLock
    void RequestWakeLock() override { registry_->SetWakeLockStatus(true); }
    void CancelWakeLock() override { registry_->SetWakeLockStatus(false); }
    void AddClient(device::mojom::blink::WakeLockRequest wake_lock) override {}
    void ChangeType(device::mojom::WakeLockType type,
                    ChangeTypeCallback callback) override {}
    void HasWakeLockForTests(HasWakeLockForTestsCallback callback) override {}

    mojo::Binding<WakeLock> binding_;
    MockInterfaceProvider* const registry_;
  };
  std::unique_ptr<MockWakeLock> mock_wake_lock_;

  bool wake_lock_status_;
};

void MockInterfaceProvider::GetInterface(const char* name,
                                         mojo::ScopedMessagePipeHandle handle) {
  mock_wake_lock_.reset(
      new MockWakeLock(this, WakeLockRequest(std::move(handle))));
}

// A TestWebFrameClient to allow overriding the interfaceProvider() with a mock.
class TestWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
 public:
  ~TestWebFrameClient() override = default;
  InterfaceProvider* GetInterfaceProviderForTesting() override {
    return &interface_provider_;
  }

 private:
  MockInterfaceProvider interface_provider_;
};

class ScreenWakeLockTest : public ::testing::Test {
 protected:
  void SetUp() override {
    web_view_helper_.Initialize(&test_web_frame_client_);
    URLTestHelpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8("http://example.com/"), testing::WebTestDataPath(),
        WebString::FromUTF8("foo.html"));
    LoadFrame();
  }

  void TearDown() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
    testing::RunPendingTasks();
  }

  void LoadFrame() {
    FrameTestHelpers::LoadFrame(web_view_helper_.WebView()->MainFrameImpl(),
                                "http://example.com/foo.html");
    web_view_helper_.WebView()->UpdateAllLifecyclePhases();
  }

  LocalFrame* GetFrame() {
    DCHECK(web_view_helper_.WebView());
    DCHECK(web_view_helper_.LocalMainFrame());
    return web_view_helper_.LocalMainFrame()->GetFrame();
  }

  Screen* GetScreen() {
    DCHECK(GetFrame());
    DCHECK(GetFrame()->DomWindow());
    return GetFrame()->DomWindow()->screen();
  }

  bool ScreenKeepAwake() {
    DCHECK(GetScreen());
    return ScreenWakeLock::keepAwake(*GetScreen());
  }

  bool ClientKeepScreenAwake() {
    return static_cast<MockInterfaceProvider*>(
               test_web_frame_client_.GetInterfaceProviderForTesting())
        ->WakeLockStatus();
  }

  void SetKeepAwake(bool keepAwake) {
    DCHECK(GetScreen());
    ScreenWakeLock::setKeepAwake(*GetScreen(), keepAwake);
    // Let the notification sink through the mojo pipes.
    testing::RunPendingTasks();
  }

  void Show() {
    DCHECK(web_view_helper_.WebView());
    web_view_helper_.WebView()->SetVisibilityState(
        kWebPageVisibilityStateVisible, false);
    // Let the notification sink through the mojo pipes.
    testing::RunPendingTasks();
  }

  void Hide() {
    DCHECK(web_view_helper_.WebView());
    web_view_helper_.WebView()->SetVisibilityState(
        kWebPageVisibilityStateHidden, false);
    // Let the notification sink through the mojo pipes.
    testing::RunPendingTasks();
  }

  // Order of these members is important as we need to make sure that
  // test_web_frame_client_ outlives web_view_helper_ (destruction order)
  TestWebFrameClient test_web_frame_client_;
  FrameTestHelpers::WebViewHelper web_view_helper_;
};

TEST_F(ScreenWakeLockTest, setAndReset) {
  ASSERT_FALSE(ScreenKeepAwake());
  ASSERT_FALSE(ClientKeepScreenAwake());

  SetKeepAwake(true);
  EXPECT_TRUE(ScreenKeepAwake());
  EXPECT_TRUE(ClientKeepScreenAwake());

  SetKeepAwake(false);
  EXPECT_FALSE(ScreenKeepAwake());
  EXPECT_FALSE(ClientKeepScreenAwake());
}

TEST_F(ScreenWakeLockTest, hideWhenSet) {
  ASSERT_FALSE(ScreenKeepAwake());
  ASSERT_FALSE(ClientKeepScreenAwake());

  SetKeepAwake(true);
  Hide();

  EXPECT_TRUE(ScreenKeepAwake());
  EXPECT_FALSE(ClientKeepScreenAwake());
}

TEST_F(ScreenWakeLockTest, setWhenHidden) {
  ASSERT_FALSE(ScreenKeepAwake());
  ASSERT_FALSE(ClientKeepScreenAwake());

  Hide();
  SetKeepAwake(true);

  EXPECT_TRUE(ScreenKeepAwake());
  EXPECT_FALSE(ClientKeepScreenAwake());
}

TEST_F(ScreenWakeLockTest, showWhenSet) {
  ASSERT_FALSE(ScreenKeepAwake());
  ASSERT_FALSE(ClientKeepScreenAwake());

  Hide();
  SetKeepAwake(true);
  Show();

  EXPECT_TRUE(ScreenKeepAwake());
  EXPECT_TRUE(ClientKeepScreenAwake());
}

TEST_F(ScreenWakeLockTest, navigate) {
  ASSERT_FALSE(ScreenKeepAwake());
  ASSERT_FALSE(ClientKeepScreenAwake());

  SetKeepAwake(true);
  LoadFrame();

  EXPECT_FALSE(ScreenKeepAwake());
  EXPECT_FALSE(ClientKeepScreenAwake());
}

}  // namespace
}  // namespace blink
