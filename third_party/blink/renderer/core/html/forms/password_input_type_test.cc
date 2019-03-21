// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/html/forms/password_input_type.h"

#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/insecure_input/insecure_input_service.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class MockInsecureInputService : public mojom::blink::InsecureInputService {
 public:
  explicit MockInsecureInputService(LocalFrame& frame) {
    service_manager::InterfaceProvider::TestApi test_api(
        &frame.GetInterfaceProvider());
    test_api.SetBinderForName(
        mojom::blink::InsecureInputService::Name_,
        WTF::BindRepeating(&MockInsecureInputService::BindRequest,
                           WTF::Unretained(this)));
  }

  ~MockInsecureInputService() override = default;

  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    binding_set_.AddBinding(
        this, mojom::blink::InsecureInputServiceRequest(std::move(handle)));
  }

  unsigned DidEditFieldCalls() const { return num_did_edit_field_calls_; }

 private:
  void DidEditFieldInInsecureContext() override { ++num_did_edit_field_calls_; }

  mojo::BindingSet<InsecureInputService> binding_set_;

  unsigned num_did_edit_field_calls_ = 0;
};

// Tests that a Mojo message is sent when a password field is edited
// on the page.
TEST(PasswordInputTypeTest, DidEditFieldEvent) {
  auto page_holder = std::make_unique<DummyPageHolder>(IntSize(2000, 2000));
  MockInsecureInputService mock_service(page_holder->GetFrame());
  page_holder->GetDocument().body()->SetInnerHTMLFromString(
      "<input type='password'>");
  page_holder->GetDocument().View()->UpdateAllLifecyclePhases(
      DocumentLifecycle::LifecycleUpdateReason::kTest);
  blink::test::RunPendingTasks();
  EXPECT_EQ(0u, mock_service.DidEditFieldCalls());
  // Simulate a text field edit.
  page_holder->GetDocument().MaybeQueueSendDidEditFieldInInsecureContext();
  blink::test::RunPendingTasks();
  EXPECT_EQ(1u, mock_service.DidEditFieldCalls());
  // Ensure additional edits do not trigger additional notifications.
  page_holder->GetDocument().MaybeQueueSendDidEditFieldInInsecureContext();
  blink::test::RunPendingTasks();
  EXPECT_EQ(1u, mock_service.DidEditFieldCalls());
}

// Tests that a Mojo message is not sent when a password field is edited
// in a secure context.
TEST(PasswordInputTypeTest, DidEditFieldEventNotSentFromSecureContext) {
  auto page_holder = std::make_unique<DummyPageHolder>(IntSize(2000, 2000));
  MockInsecureInputService mock_service(page_holder->GetFrame());
  page_holder->GetDocument().SetURL(KURL("https://example.test"));
  page_holder->GetDocument().SetSecurityOrigin(
      SecurityOrigin::Create(KURL("https://example.test")));
  page_holder->GetDocument().SetSecureContextStateForTesting(
      SecureContextState::kSecure);
  page_holder->GetDocument().body()->SetInnerHTMLFromString(
      "<input type='password'>");
  page_holder->GetDocument().View()->UpdateAllLifecyclePhases(
      DocumentLifecycle::LifecycleUpdateReason::kTest);
  // Simulate a text field edit.
  page_holder->GetDocument().MaybeQueueSendDidEditFieldInInsecureContext();
  // No message should have been sent from a secure context.
  blink::test::RunPendingTasks();
  EXPECT_EQ(0u, mock_service.DidEditFieldCalls());
}

}  // namespace blink
