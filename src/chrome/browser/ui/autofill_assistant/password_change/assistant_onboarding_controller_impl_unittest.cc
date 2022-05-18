// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller_impl.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_assistant_onboarding_prompt.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrictMock;

class AssistantOnboardingControllerImplTest : public ::testing::Test {
 public:
  AssistantOnboardingControllerImplTest() {
    controller_ =
        AssistantOnboardingController::Create(AssistantOnboardingInformation());
  }
  ~AssistantOnboardingControllerImplTest() override = default;

 protected:
  // The controller to test.
  std::unique_ptr<AssistantOnboardingController> controller_;
};

TEST_F(AssistantOnboardingControllerImplTest, ShowPromptAndAccept) {
  StrictMock<MockAssistantOnboardingPrompt> prompt;
  base::MockCallback<AssistantOnboardingController::Callback> callback;

  EXPECT_CALL(prompt, Show);
  controller_->Show(&prompt, callback.Get());

  // Simulate click on accept.
  EXPECT_CALL(callback, Run(true));
  controller_->OnAccept();
}

TEST_F(AssistantOnboardingControllerImplTest, ShowPromptAndCancel) {
  StrictMock<MockAssistantOnboardingPrompt> prompt;
  base::MockCallback<AssistantOnboardingController::Callback> callback;

  EXPECT_CALL(prompt, Show);
  controller_->Show(&prompt, callback.Get());

  // Simulate click on cancel.
  EXPECT_CALL(callback, Run(false));
  controller_->OnCancel();
}

TEST_F(AssistantOnboardingControllerImplTest, ShowPromptAndClose) {
  StrictMock<MockAssistantOnboardingPrompt> prompt;
  base::MockCallback<AssistantOnboardingController::Callback> callback;

  EXPECT_CALL(prompt, Show);
  controller_->Show(&prompt, callback.Get());

  // Simulate click on cancel.
  EXPECT_CALL(callback, Run(false));
  controller_->OnClose();

  // A second call does not do anything.
  controller_->OnClose();
}

TEST_F(AssistantOnboardingControllerImplTest, ShowTwoPromptsAndAcceptSecond) {
  StrictMock<MockAssistantOnboardingPrompt> first_prompt;
  base::MockCallback<AssistantOnboardingController::Callback> first_callback;

  EXPECT_CALL(first_prompt, Show);
  controller_->Show(&first_prompt, first_callback.Get());

  StrictMock<MockAssistantOnboardingPrompt> second_prompt;
  base::MockCallback<AssistantOnboardingController::Callback> second_callback;

  // The second prompt closes the first.
  EXPECT_CALL(first_prompt, OnControllerGone);
  EXPECT_CALL(first_callback, Run(false));
  EXPECT_CALL(second_prompt, Show);
  controller_->Show(&second_prompt, second_callback.Get());

  // Simulate click on accept.
  EXPECT_CALL(second_callback, Run(true));
  controller_->OnAccept();
}
