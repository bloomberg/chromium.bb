// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/wake_lock/ScreenWakeLock.h"

#include "core/dom/DOMImplementation.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentInit.h"
#include "core/frame/FrameTestHelpers.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebPageVisibilityState.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <memory>

namespace blink {
namespace {

using device::mojom::blink::WakeLock;
using device::mojom::blink::WakeLockRequest;

// A mock WakeLock used to intercept calls to the mojo methods.
class MockWakeLock : public WakeLock {
 public:
  MockWakeLock() : binding_(this) {}
  ~MockWakeLock() = default;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    binding_.Bind(WakeLockRequest(std::move(handle)));
  }

  bool WakeLockStatus() { return wake_lock_status_; }

 private:
  // mojom::WakeLock
  void RequestWakeLock() override { wake_lock_status_ = true; }
  void CancelWakeLock() override { wake_lock_status_ = false; }
  void AddClient(device::mojom::blink::WakeLockRequest wake_lock) override {}
  void ChangeType(device::mojom::WakeLockType type,
                  ChangeTypeCallback callback) override {}
  void HasWakeLockForTests(HasWakeLockForTestsCallback callback) override {}

  mojo::Binding<WakeLock> binding_;
  bool wake_lock_status_ = false;
};

class ScreenWakeLockTest : public ::testing::Test {
 protected:
  void SetUp() override {
    service_manager::InterfaceProvider::TestApi test_api(
        test_web_frame_client_.GetInterfaceProvider());
    test_api.SetBinderForName(
        WakeLock::Name_,
        ConvertToBaseCallback(
            WTF::Bind(&MockWakeLock::Bind, WTF::Unretained(&mock_wake_lock_))));

    web_view_helper_.Initialize(&test_web_frame_client_);
    URLTestHelpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8("http://example.com/"), testing::CoreTestDataPath(),
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
    DCHECK(web_view_helper_.WebView()->MainFrameImpl());
    return web_view_helper_.WebView()->MainFrameImpl()->GetFrame();
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

  bool ClientKeepScreenAwake() { return mock_wake_lock_.WakeLockStatus(); }

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
  FrameTestHelpers::TestWebFrameClient test_web_frame_client_;
  FrameTestHelpers::WebViewHelper web_view_helper_;

  MockWakeLock mock_wake_lock_;
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
