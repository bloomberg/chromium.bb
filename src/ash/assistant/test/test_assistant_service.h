// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_TEST_TEST_ASSISTANT_SERVICE_H_
#define ASH_ASSISTANT_TEST_TEST_ASSISTANT_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {

class CurrentInteractionSubscriber;
class SanityCheckSubscriber;

// A response issued when an Assistant interaction is started.
// Used both for text and voice interactions.  To build a response, simply
// chain calls to the provided |AddXYZ| methods.
//
// Example usage:
//     auto response = std::make_unique<InteractionResponse>();
//     response->AddTextResponse("The response text")
//             ->AddResolution(InteractionResponse::Resolution::kNormal);
//     assistant_service()->SetInteractionResponse(std::move(response));
class InteractionResponse {
 public:
  using Resolution = chromeos::assistant::mojom::AssistantInteractionResolution;
  class Response;

  InteractionResponse();
  ~InteractionResponse();

  // A simple textual response.
  InteractionResponse* AddTextResponse(const std::string& text);
  // A suggestion chip response.
  InteractionResponse* AddSuggestionChip(const std::string& text);
  // If used this will cause us to finish the interaction by passing the given
  // |resolution| to |AssistantInteractionSubscriber::OnInteractionFinished|.
  InteractionResponse* AddResolution(Resolution resolution);

  void SendTo(
      chromeos::assistant::mojom::AssistantInteractionSubscriber* receiver);

 private:
  void AddResponse(std::unique_ptr<Response> responses);

  std::vector<std::unique_ptr<Response>> responses_;

  DISALLOW_COPY_AND_ASSIGN(InteractionResponse);
};

// Dummy implementation of the Assistant service.
// It behaves as if the Assistant service is up-and-running,
// and will inform the |AssistantInteractionSubscriber| instances when
// interactions start/stop.
// Note it is up to the test developer to assure the interactions are valid.
// The contract with LibAssistant specifies there is only one
// "conversation turn" (what we call "interaction") at any given time, so you
// must:
//    - Finish a conversation before starting a new one.
//    - Not send any responses (text, card, ...) before starting or after
//      finishing an interaction.
class TestAssistantService : public chromeos::assistant::mojom::Assistant {
 public:
  TestAssistantService();
  ~TestAssistantService() override;

  mojo::PendingRemote<chromeos::assistant::mojom::Assistant>
  CreateRemoteAndBind();

  // Set the response that will be invoked when the next interaction starts.
  void SetInteractionResponse(std::unique_ptr<InteractionResponse> response);

  // Returns the current interaction.
  base::Optional<chromeos::assistant::mojom::AssistantInteractionMetadata>
  current_interaction();

  // mojom::Assistant overrides:
  void StartEditReminderInteraction(const std::string& client_id) override;
  void StartScreenContextInteraction(
      ax::mojom::AssistantStructurePtr assistant_structure,
      const std::vector<uint8_t>& assistant_screenshot) override;
  void StartTextInteraction(
      const std::string& query,
      chromeos::assistant::mojom::AssistantQuerySource source,
      bool allow_tts) override;
  void StartVoiceInteraction() override;
  void StartWarmerWelcomeInteraction(int num_warmer_welcome_triggered,
                                     bool allow_tts) override;
  void StopActiveInteraction(bool cancel_conversation) override;
  void AddAssistantInteractionSubscriber(
      mojo::PendingRemote<
          chromeos::assistant::mojom::AssistantInteractionSubscriber>
          subscriber) override;
  void RetrieveNotification(
      chromeos::assistant::mojom::AssistantNotificationPtr notification,
      int action_index) override;
  void DismissNotification(chromeos::assistant::mojom::AssistantNotificationPtr
                               notification) override;
  void OnAccessibilityStatusChanged(bool spoken_feedback_enabled) override;
  void SendAssistantFeedback(
      chromeos::assistant::mojom::AssistantFeedbackPtr feedback) override;
  void NotifyEntryIntoAssistantUi(
      chromeos::assistant::mojom::AssistantEntryPoint entry_point) override;
  void AddTimeToTimer(const std::string& id, base::TimeDelta duration) override;
  void PauseTimer(const std::string& id) override;
  void RemoveAlarmOrTimer(const std::string& id) override;
  void ResumeTimer(const std::string& id) override;

 private:
  void StartInteraction(
      chromeos::assistant::mojom::AssistantInteractionType type,
      chromeos::assistant::mojom::AssistantQuerySource source =
          chromeos::assistant::mojom::AssistantQuerySource::kUnspecified,
      const std::string& query = std::string());
  void SendInteractionResponse();

  mojo::Receiver<chromeos::assistant::mojom::Assistant> receiver_{this};
  mojo::RemoteSet<chromeos::assistant::mojom::AssistantInteractionSubscriber>
      interaction_subscribers_;
  std::unique_ptr<SanityCheckSubscriber> sanity_check_subscriber_;
  std::unique_ptr<CurrentInteractionSubscriber> current_interaction_subscriber_;
  std::unique_ptr<InteractionResponse> interaction_response_;

  bool running_active_interaction_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestAssistantService);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_TEST_TEST_ASSISTANT_SERVICE_H_
