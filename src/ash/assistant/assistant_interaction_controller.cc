// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_interaction_controller.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_screen_context_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/model/assistant_ui_element.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

AssistantInteractionController::AssistantInteractionController(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      assistant_interaction_subscriber_binding_(this) {
  AddModelObserver(this);
  assistant_controller_->AddObserver(this);
  Shell::Get()->highlighter_controller()->AddObserver(this);
}

AssistantInteractionController::~AssistantInteractionController() {
  Shell::Get()->highlighter_controller()->RemoveObserver(this);
  assistant_controller_->RemoveObserver(this);
  RemoveModelObserver(this);
}

void AssistantInteractionController::SetAssistant(
    chromeos::assistant::mojom::Assistant* assistant) {
  assistant_ = assistant;

  // Subscribe to Assistant interaction events.
  chromeos::assistant::mojom::AssistantInteractionSubscriberPtr ptr;
  assistant_interaction_subscriber_binding_.Bind(mojo::MakeRequest(&ptr));
  assistant_->AddAssistantInteractionSubscriber(std::move(ptr));
}

void AssistantInteractionController::AddModelObserver(
    AssistantInteractionModelObserver* observer) {
  assistant_interaction_model_.AddObserver(observer);
}

void AssistantInteractionController::RemoveModelObserver(
    AssistantInteractionModelObserver* observer) {
  assistant_interaction_model_.RemoveObserver(observer);
}

void AssistantInteractionController::OnAssistantControllerConstructed() {
  assistant_controller_->ui_controller()->AddModelObserver(this);
}

void AssistantInteractionController::OnAssistantControllerDestroying() {
  assistant_controller_->ui_controller()->RemoveModelObserver(this);
}

void AssistantInteractionController::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  using namespace assistant::util;

  if (type == DeepLinkType::kWhatsOnMyScreen) {
    StartScreenContextInteraction();
    return;
  }

  if (type != DeepLinkType::kQuery)
    return;

  const base::Optional<std::string>& query =
      GetDeepLinkParam(params, DeepLinkParam::kQuery);

  if (!query.has_value())
    return;

  // If we receive an empty query, that's a bug that needs to be fixed by the
  // deep link sender. Rather than getting ourselves into a bad state, we'll
  // ignore the deep link.
  if (query.value().empty()) {
    LOG(ERROR) << "Ignoring deep link containing empty query.";
    return;
  }

  StartTextInteraction(query.value());
}

void AssistantInteractionController::OnUiModeChanged(AssistantUiMode ui_mode) {
  if (ui_mode == AssistantUiMode::kMiniUi)
    return;

  // When the Assistant is not in mini state there should not be an active
  // metalayer session. If we were in mini state when the UI mode was changed,
  // we need to clean up the metalayer session and reset default input modality.
  if (assistant_interaction_model_.input_modality() == InputModality::kStylus) {
    Shell::Get()->highlighter_controller()->AbortSession();
    assistant_interaction_model_.SetInputModality(InputModality::kKeyboard);
  }
}

void AssistantInteractionController::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    AssistantSource source) {
  switch (new_visibility) {
    case AssistantVisibility::kClosed:
      // When the UI is closed we need to stop any active interaction. We also
      // reset the interaction state and restore the default input modality.
      StopActiveInteraction();
      assistant_interaction_model_.ClearInteraction();
      assistant_interaction_model_.SetInputModality(InputModality::kKeyboard);
      break;
    case AssistantVisibility::kHidden:
      // When the UI is hidden we stop any voice query in progress so that we
      // don't listen to the user while not visible. We also restore the default
      // input modality for the next launch.
      if (assistant_interaction_model_.pending_query().type() ==
          AssistantQueryType::kVoice) {
        StopActiveInteraction();
      }
      assistant_interaction_model_.SetInputModality(InputModality::kKeyboard);
      break;
    case AssistantVisibility::kVisible:
      if (source == AssistantSource::kLongPressLauncher) {
        StartVoiceInteraction();
      } else if (source == AssistantSource::kStylus) {
        assistant_interaction_model_.SetInputModality(InputModality::kStylus);
      }
      break;
  }
}

void AssistantInteractionController::OnHighlighterEnabledChanged(
    HighlighterEnabledState state) {
  switch (state) {
    case HighlighterEnabledState::kEnabled:
      assistant_interaction_model_.SetInputModality(InputModality::kStylus);
      break;
    case HighlighterEnabledState::kDisabledByUser:
      FALLTHROUGH;
    case HighlighterEnabledState::kDisabledBySessionComplete:
      assistant_interaction_model_.SetInputModality(InputModality::kKeyboard);
      break;
    case HighlighterEnabledState::kDisabledBySessionAbort:
      // When metalayer mode has been aborted, no action necessary. Abort occurs
      // as a result of an interaction starting, most likely due to hotword
      // detection. Setting the input modality in these cases would have the
      // unintended consequence of stopping the active interaction.
      break;
  }
}

void AssistantInteractionController::OnHighlighterSelectionRecognized(
    const gfx::Rect& rect) {
  StartMetalayerInteraction(/*region=*/rect);
}

void AssistantInteractionController::OnInteractionStateChanged(
    InteractionState interaction_state) {
  if (interaction_state != InteractionState::kActive)
    return;

  // Metalayer mode should not be sticky. Disable it on interaction start.
  Shell::Get()->highlighter_controller()->AbortSession();
}

void AssistantInteractionController::OnInputModalityChanged(
    InputModality input_modality) {
  if (assistant_controller_->ui_controller()->model()->visibility() !=
      AssistantVisibility::kVisible) {
    return;
  }

  if (input_modality == InputModality::kVoice)
    return;

  // When switching to a non-voice input modality we instruct the underlying
  // service to terminate any pending query. We do not do this when switching to
  // voice input modality because initiation of a voice interaction will
  // automatically interrupt any pre-existing activity. Stopping the active
  // interaction here for voice input modality would actually have the undesired
  // effect of stopping the voice interaction.
  StopActiveInteraction();
}

void AssistantInteractionController::OnInteractionStarted(
    bool is_voice_interaction) {
  assistant_interaction_model_.SetInteractionState(InteractionState::kActive);

  // In the case of a voice interaction, we assume that the mic is open and
  // transition to voice input modality.
  if (is_voice_interaction) {
    assistant_interaction_model_.SetInputModality(InputModality::kVoice);
    assistant_interaction_model_.SetMicState(MicState::kOpen);

    // When a voice interaction is initiated by hotword, we haven't yet set a
    // pending query so this is our earliest opportunity.
    if (assistant_interaction_model_.pending_query().type() ==
        AssistantQueryType::kNull) {
      assistant_interaction_model_.SetPendingQuery(
          std::make_unique<AssistantVoiceQuery>());
    }
  } else {
    // TODO(b/112000321): It should not be possible to reach this code without
    // having previously pended a query. It does currently happen, however, in
    // the case of notifications and device action queries which bypass the
    // AssistantInteractionController when beginning an interaction. To address
    // this, we temporarily pend an empty text query to commit until we can do
    // development to expose something more meaningful.
    if (assistant_interaction_model_.pending_query().type() ==
        AssistantQueryType::kNull) {
      assistant_interaction_model_.SetPendingQuery(
          std::make_unique<AssistantTextQuery>());
    }

    assistant_interaction_model_.CommitPendingQuery();
    assistant_interaction_model_.SetMicState(MicState::kClosed);
  }

  // Start caching a new Assistant response for the interaction.
  assistant_interaction_model_.SetPendingResponse(
      std::make_unique<AssistantResponse>());
}

void AssistantInteractionController::OnInteractionFinished(
    AssistantInteractionResolution resolution) {
  assistant_interaction_model_.SetInteractionState(InteractionState::kInactive);
  assistant_interaction_model_.SetMicState(MicState::kClosed);

  // If the interaction was finished due to mic timeout, we only want to clear
  // the pending query/response state for that interaction.
  if (resolution == AssistantInteractionResolution::kMicTimeout) {
    assistant_interaction_model_.ClearPendingQuery();
    assistant_interaction_model_.ClearPendingResponse();
    return;
  }

  // In normal interaction flows the pending query has already been committed.
  // In some irregular cases, however, it has not. This happens during multi-
  // device hotword loss, for example, but can also occur if the interaction
  // errors out. In these cases we still need to commit the pending query as
  // this is a prerequisite step to being able to finalize the pending response.
  if (assistant_interaction_model_.pending_query().type() !=
      AssistantQueryType::kNull) {
    assistant_interaction_model_.CommitPendingQuery();
  }

  // If the interaction was finished due to multi-device hotword loss, we want
  // to show an appropriate message to the user.
  if (resolution == AssistantInteractionResolution::kMultiDeviceHotwordLoss) {
    assistant_interaction_model_.pending_response()->AddUiElement(
        std::make_unique<AssistantTextElement>(l10n_util::GetStringUTF8(
            IDS_ASH_ASSISTANT_MULTI_DEVICE_HOTWORD_LOSS)));
  }

  // The interaction has finished, so we finalize the pending response if it
  // hasn't already been finalized.
  if (assistant_interaction_model_.pending_response())
    assistant_interaction_model_.FinalizePendingResponse();
}

void AssistantInteractionController::OnHtmlResponse(
    const std::string& response) {
  if (assistant_interaction_model_.interaction_state() !=
      InteractionState::kActive) {
    return;
  }

  // If this occurs, the server has broken our response ordering agreement. We
  // should not crash but we cannot handle the response so we ignore it.
  if (!assistant_interaction_model_.pending_response()) {
    NOTREACHED();
    return;
  }

  assistant_interaction_model_.pending_response()->AddUiElement(
      std::make_unique<AssistantCardElement>(response));
}

void AssistantInteractionController::OnSuggestionChipPressed(
    const AssistantSuggestion* suggestion) {
  // If the suggestion contains a non-empty action url, we will handle the
  // suggestion chip pressed event by launching the action url in the browser.
  if (!suggestion->action_url.is_empty()) {
    assistant_controller_->OpenUrl(suggestion->action_url);
    return;
  }

  // Otherwise, we will submit a simple text query using the suggestion text.
  StartTextInteraction(suggestion->text);
}

void AssistantInteractionController::OnSuggestionsResponse(
    std::vector<AssistantSuggestionPtr> response) {
  if (assistant_interaction_model_.interaction_state() !=
      InteractionState::kActive) {
    return;
  }

  // If this occurs, the server has broken our response ordering agreement. We
  // should not crash but we cannot handle the response so we ignore it.
  if (!assistant_interaction_model_.pending_response()) {
    NOTREACHED();
    return;
  }

  assistant_interaction_model_.pending_response()->AddSuggestions(
      std::move(response));
}

void AssistantInteractionController::OnTextResponse(
    const std::string& response) {
  if (assistant_interaction_model_.interaction_state() !=
      InteractionState::kActive) {
    return;
  }

  // If this occurs, the server has broken our response ordering agreement. We
  // should not crash but we cannot handle the response so we ignore it.
  if (!assistant_interaction_model_.pending_response()) {
    NOTREACHED();
    return;
  }

  assistant_interaction_model_.pending_response()->AddUiElement(
      std::make_unique<AssistantTextElement>(response));
}

void AssistantInteractionController::OnSpeechRecognitionStarted() {}

void AssistantInteractionController::OnSpeechRecognitionIntermediateResult(
    const std::string& high_confidence_text,
    const std::string& low_confidence_text) {
  assistant_interaction_model_.SetPendingQuery(
      std::make_unique<AssistantVoiceQuery>(high_confidence_text,
                                            low_confidence_text));
}

void AssistantInteractionController::OnSpeechRecognitionEndOfUtterance() {
  assistant_interaction_model_.SetMicState(MicState::kClosed);
}

void AssistantInteractionController::OnSpeechRecognitionFinalResult(
    const std::string& final_result) {
  assistant_interaction_model_.SetPendingQuery(
      std::make_unique<AssistantVoiceQuery>(final_result));
  assistant_interaction_model_.CommitPendingQuery();
}

void AssistantInteractionController::OnSpeechLevelUpdated(float speech_level) {
  assistant_interaction_model_.SetSpeechLevel(speech_level);
}

void AssistantInteractionController::OnTtsStarted(bool due_to_error) {
  if (assistant_interaction_model_.interaction_state() !=
      InteractionState::kActive) {
    return;
  }

  // Commit the pending query in whatever state it's in. In most cases the
  // pending query is already committed, but we must always commit the pending
  // query before finalizing a pending result.
  if (assistant_interaction_model_.pending_query().type() !=
      AssistantQueryType::kNull) {
    assistant_interaction_model_.CommitPendingQuery();
  }

  if (due_to_error) {
    // In the case of an error occurring during a voice interaction, this is our
    // earliest indication that the mic has closed.
    assistant_interaction_model_.SetMicState(MicState::kClosed);

    // Add an error message to the response.
    assistant_interaction_model_.pending_response()->AddUiElement(
        std::make_unique<AssistantTextElement>(
            l10n_util::GetStringUTF8(IDS_ASH_ASSISTANT_ERROR_GENERIC)));
  }

  // We have an agreement with the server that TTS will always be the last part
  // of an interaction to be processed. To be timely in updating UI, we use
  // this as an opportunity to finalize the Assistant response and update the
  // interaction model.
  assistant_interaction_model_.FinalizePendingResponse();
}

void AssistantInteractionController::OnOpenUrlResponse(const GURL& url) {
  if (assistant_interaction_model_.interaction_state() !=
      InteractionState::kActive) {
    return;
  }
  assistant_controller_->OpenUrl(url);
}

void AssistantInteractionController::OnDialogPlateButtonPressed(
    DialogPlateButtonId id) {
  if (id == DialogPlateButtonId::kKeyboardInputToggle) {
    assistant_interaction_model_.SetInputModality(InputModality::kKeyboard);
    return;
  }

  if (id != DialogPlateButtonId::kVoiceInputToggle)
    return;

  switch (assistant_interaction_model_.mic_state()) {
    case MicState::kClosed:
      StartVoiceInteraction();
      break;
    case MicState::kOpen:
      StopActiveInteraction();
      break;
  }
}

void AssistantInteractionController::OnDialogPlateContentsCommitted(
    const std::string& text) {
  DCHECK(!text.empty());
  StartTextInteraction(text);
}

void AssistantInteractionController::StartMetalayerInteraction(
    const gfx::Rect& region) {
  StopActiveInteraction();

  assistant_interaction_model_.SetPendingQuery(
      std::make_unique<AssistantTextQuery>(
          l10n_util::GetStringUTF8(IDS_ASH_ASSISTANT_CHIP_WHATS_ON_MY_SCREEN)));

  assistant_->StartMetalayerInteraction(region);
}

void AssistantInteractionController::StartScreenContextInteraction() {
  StopActiveInteraction();

  assistant_interaction_model_.SetPendingQuery(
      std::make_unique<AssistantTextQuery>(
          l10n_util::GetStringUTF8(IDS_ASH_ASSISTANT_CHIP_WHATS_ON_MY_SCREEN)));

  // Note that screen context was cached when the UI was launched.
  assistant_->StartCachedScreenContextInteraction();
}

void AssistantInteractionController::StartTextInteraction(
    const std::string text) {
  StopActiveInteraction();

  assistant_interaction_model_.SetPendingQuery(
      std::make_unique<AssistantTextQuery>(text));

  assistant_->SendTextQuery(text);
}

void AssistantInteractionController::StartVoiceInteraction() {
  StopActiveInteraction();

  assistant_interaction_model_.SetPendingQuery(
      std::make_unique<AssistantVoiceQuery>());

  assistant_->StartVoiceInteraction();
}

void AssistantInteractionController::StopActiveInteraction() {
  // Even though the interaction state will be asynchronously set to inactive
  // via a call to OnInteractionFinished(Resolution), we explicitly set it to
  // inactive here to prevent processing any additional UI related service
  // events belonging to the interaction being stopped.
  assistant_interaction_model_.SetInteractionState(InteractionState::kInactive);
  assistant_interaction_model_.ClearPendingQuery();

  assistant_->StopActiveInteraction();

  // Because we are stopping an interaction in progress, we discard any pending
  // response for it that is cached to prevent it from being finalized when the
  // interaction is finished.
  assistant_interaction_model_.ClearPendingResponse();
}

}  // namespace ash
