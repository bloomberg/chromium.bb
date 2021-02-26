// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_MOCK_ASSISTANT_H_
#define CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_MOCK_ASSISTANT_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/mojom/ax_assistant_structure.mojom.h"

namespace chromeos {
namespace assistant {

class MockAssistant : public Assistant {
 public:
  MockAssistant();
  ~MockAssistant() override;

  MOCK_METHOD1(StartEditReminderInteraction, void(const std::string&));

  MOCK_METHOD(void,
              StartScreenContextInteraction,
              (ax::mojom::AssistantStructurePtr, const std::vector<uint8_t>&));

  MOCK_METHOD(void,
              StartTextInteraction,
              (const std::string&, AssistantQuerySource, bool));

  MOCK_METHOD0(StartVoiceInteraction, void());

  MOCK_METHOD1(StopActiveInteraction, void(bool));

  MOCK_METHOD1(AddAssistantInteractionSubscriber,
               void(AssistantInteractionSubscriber*));

  MOCK_METHOD1(RemoveAssistantInteractionSubscriber,
               void(AssistantInteractionSubscriber*));

  MOCK_METHOD2(RetrieveNotification, void(const AssistantNotification&, int));

  MOCK_METHOD1(DismissNotification, void(const AssistantNotification&));

  MOCK_METHOD1(OnAccessibilityStatusChanged, void(bool));

  MOCK_METHOD1(SendAssistantFeedback, void(const AssistantFeedback&));

  MOCK_METHOD1(NotifyEntryIntoAssistantUi, void(AssistantEntryPoint));

  MOCK_METHOD0(StopAlarmTimerRinging, void());
  MOCK_METHOD1(CreateTimer, void(base::TimeDelta));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAssistant);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_MOCK_ASSISTANT_H_
