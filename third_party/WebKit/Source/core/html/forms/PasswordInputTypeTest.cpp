// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/forms/PasswordInputType.h"

#include "core/frame/FrameView.h"
#include "core/html/HTMLInputElement.h"
#include "core/testing/DummyPageHolder.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/modules/sensitive_input_visibility/sensitive_input_visibility_service.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MockInterfaceProvider : public blink::InterfaceProvider {
 public:
  MockInterfaceProvider() : m_passwordFieldVisibleCalled(false) {}
  virtual ~MockInterfaceProvider() {}

  void getInterface(const char* name,
                    mojo::ScopedMessagePipeHandle handle) override {
    if (!m_mockSensitiveInputVisibilityService) {
      m_mockSensitiveInputVisibilityService.reset(
          new MockSensitiveInputVisibilityService(
              this,
              mojo::MakeRequest<mojom::blink::SensitiveInputVisibilityService>(
                  std::move(handle))));
    }
  }

  void setPasswordFieldVisibleCalled() { m_passwordFieldVisibleCalled = true; }

  bool passwordFieldVisibleCalled() const {
    return m_passwordFieldVisibleCalled;
  }

 private:
  class MockSensitiveInputVisibilityService
      : public mojom::blink::SensitiveInputVisibilityService {
   public:
    MockSensitiveInputVisibilityService(
        MockInterfaceProvider* registry,
        mojom::blink::SensitiveInputVisibilityServiceRequest request)
        : m_binding(this, std::move(request)), m_registry(registry) {}
    ~MockSensitiveInputVisibilityService() override {}

   private:
    // mojom::SensitiveInputVisibilityService
    void PasswordFieldVisibleInInsecureContext() override {
      m_registry->setPasswordFieldVisibleCalled();
    }

    mojo::Binding<SensitiveInputVisibilityService> m_binding;
    MockInterfaceProvider* const m_registry;
  };

  std::unique_ptr<MockSensitiveInputVisibilityService>
      m_mockSensitiveInputVisibilityService;
  bool m_passwordFieldVisibleCalled;
};

// Tests that a Mojo message is sent when a visible password field
// appears on the page.
TEST(PasswordInputTypeTest, PasswordVisibilityEvent) {
  MockInterfaceProvider interfaceProvider;
  std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create(
      IntSize(2000, 2000), nullptr, nullptr, nullptr, &interfaceProvider);
  pageHolder->document().body()->setInnerHTML("<input type='password'>",
                                              ASSERT_NO_EXCEPTION);
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  EXPECT_TRUE(interfaceProvider.passwordFieldVisibleCalled());
}

// Tests that a Mojo message is not sent when a visible password field
// appears in a secure context.
TEST(PasswordInputTypeTest, PasswordVisibilityEventInSecureContext) {
  MockInterfaceProvider interfaceProvider;
  std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create(
      IntSize(2000, 2000), nullptr, nullptr, nullptr, &interfaceProvider);
  pageHolder->document().setURL(KURL(KURL(), "https://example.test"));
  pageHolder->document().setSecurityOrigin(
      SecurityOrigin::create(KURL(KURL(), "https://example.test")));
  pageHolder->document().body()->setInnerHTML("<input type='password'>",
                                              ASSERT_NO_EXCEPTION);
  pageHolder->document().view()->updateAllLifecyclePhases();
  // No message should have been sent from a secure context.
  blink::testing::runPendingTasks();
  EXPECT_FALSE(interfaceProvider.passwordFieldVisibleCalled());
}

// Tests that a Mojo message is sent when a previously invisible password field
// becomes visible.
TEST(PasswordInputTypeTest, InvisiblePasswordFieldBecomesVisible) {
  MockInterfaceProvider interfaceProvider;
  std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create(
      IntSize(2000, 2000), nullptr, nullptr, nullptr, &interfaceProvider);
  pageHolder->document().body()->setInnerHTML(
      "<input type='password' style='display:none;'>", ASSERT_NO_EXCEPTION);
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  // The message should not be sent for a hidden password field.
  EXPECT_FALSE(interfaceProvider.passwordFieldVisibleCalled());

  // Now make the input visible.
  HTMLInputElement* input =
      toHTMLInputElement(pageHolder->document().body()->firstChild());
  input->setAttribute("style", "", ASSERT_NO_EXCEPTION);
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  EXPECT_TRUE(interfaceProvider.passwordFieldVisibleCalled());
}

// Tests that a Mojo message is sent when a previously non-password field
// becomes a password.
TEST(PasswordInputTypeTest, NonPasswordFieldBecomesPassword) {
  MockInterfaceProvider interfaceProvider;
  std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create(
      IntSize(2000, 2000), nullptr, nullptr, nullptr, &interfaceProvider);
  pageHolder->document().body()->setInnerHTML("<input type='text'>",
                                              ASSERT_NO_EXCEPTION);
  pageHolder->document().view()->updateAllLifecyclePhases();
  // The message should not be sent for a non-password field.
  blink::testing::runPendingTasks();
  EXPECT_FALSE(interfaceProvider.passwordFieldVisibleCalled());

  // Now make the input a password field.
  HTMLInputElement* input =
      toHTMLInputElement(pageHolder->document().body()->firstChild());
  input->setType("password");
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  EXPECT_TRUE(interfaceProvider.passwordFieldVisibleCalled());
}

// Tests that a Mojo message is *not* sent when a previously invisible password
// field becomes a visible non-password field.
TEST(PasswordInputTypeTest,
     InvisiblePasswordFieldBecomesVisibleNonPasswordField) {
  MockInterfaceProvider interfaceProvider;
  std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create(
      IntSize(2000, 2000), nullptr, nullptr, nullptr, &interfaceProvider);
  pageHolder->document().body()->setInnerHTML(
      "<input type='password' style='display:none;'>", ASSERT_NO_EXCEPTION);
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  // The message should not be sent for a hidden password field.
  EXPECT_FALSE(interfaceProvider.passwordFieldVisibleCalled());

  // Now make the input a visible non-password field.
  HTMLInputElement* input =
      toHTMLInputElement(pageHolder->document().body()->firstChild());
  input->setType("text");
  input->setAttribute("style", "", ASSERT_NO_EXCEPTION);
  pageHolder->document().view()->updateAllLifecyclePhases();
  blink::testing::runPendingTasks();
  EXPECT_FALSE(interfaceProvider.passwordFieldVisibleCalled());
}

}  // namespace blink
