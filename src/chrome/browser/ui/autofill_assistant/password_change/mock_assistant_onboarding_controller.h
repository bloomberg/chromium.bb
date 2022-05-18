// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_ASSISTANT_ONBOARDING_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_ASSISTANT_ONBOARDING_CONTROLLER_H_

#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

// Mocked AssistantOnboardingController used in unit tests.
class MockAssistantOnboardingController : public AssistantOnboardingController {
 public:
  MockAssistantOnboardingController();
  ~MockAssistantOnboardingController() override;

  MOCK_METHOD(void,
              Show,
              (AssistantOnboardingPrompt*, Callback callback),
              (override));
  MOCK_METHOD(void, OnAccept, (), (override));
  MOCK_METHOD(void, OnCancel, (), (override));
  MOCK_METHOD(void, OnClose, (), (override));
  MOCK_METHOD(const AssistantOnboardingInformation&,
              GetOnboardingInformation,
              (),
              (override));
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_ASSISTANT_ONBOARDING_CONTROLLER_H_
