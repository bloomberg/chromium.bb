// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_TEST_MOCKED_ASSISTANT_INTERACTION_H_
#define ASH_ASSISTANT_TEST_MOCKED_ASSISTANT_INTERACTION_H_

#include <memory>
#include <string>
#include "ash/assistant/test/test_assistant_service.h"

namespace ash {

class AssistantTestApi;

// Builder for an Assistant interaction.
// It contains methods to set the query and the response.
// The interaction will be auto submitted in the destructor.
class MockedAssistantInteraction {
 public:
  using Resolution = InteractionResponse::Resolution;

  MockedAssistantInteraction(AssistantTestApi* test_api,
                             TestAssistantService* service);
  ~MockedAssistantInteraction();
  MockedAssistantInteraction(MockedAssistantInteraction&&);
  MockedAssistantInteraction& operator=(MockedAssistantInteraction&&);

  MockedAssistantInteraction& WithQuery(const std::string& text_query);
  MockedAssistantInteraction& WithTextResponse(
      const std::string& text_response);
  MockedAssistantInteraction& WithSuggestionChip(const std::string& text);
  MockedAssistantInteraction& WithResolution(Resolution);

 private:
  void Submit();

  AssistantTestApi* test_api_;
  TestAssistantService* service_;
  std::unique_ptr<InteractionResponse> response_;

  std::string query_ = "<dummy-query>";
  Resolution resolution_ = Resolution::kNormal;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_TEST_MOCKED_ASSISTANT_INTERACTION_H_
