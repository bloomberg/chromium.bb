// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/conversation_controller.h"

#include <memory>

#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/thread_annotations.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/buildflag.h"
#include "chromeos/assistant/internal/buildflags.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/internal_options.pb.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/libassistant/grpc/assistant_client.h"
#include "chromeos/services/libassistant/public/mojom/conversation_controller.mojom.h"
#include "chromeos/services/libassistant/util.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace libassistant {

using assistant::AssistantInteractionMetadata;
using assistant::AssistantInteractionType;
using assistant::AssistantQuerySource;

namespace {

constexpr base::TimeDelta kStopInteractionDelayTime = base::Milliseconds(500);

// A macro which ensures we are running on the main thread.
#define ENSURE_MOJOM_THREAD(method, ...)                                    \
  DVLOG(3) << __func__;                                                     \
  if (!mojom_task_runner_->RunsTasksInCurrentSequence()) {                  \
    mojom_task_runner_->PostTask(                                           \
        FROM_HERE,                                                          \
        base::BindOnce(method, weak_factory_.GetWeakPtr(), ##__VA_ARGS__)); \
    return;                                                                 \
  }

// Helper function to convert |action::Suggestion| to |AssistantSuggestion|.
std::vector<assistant::AssistantSuggestion> ToAssistantSuggestion(
    const std::vector<assistant::action::Suggestion>& suggestions) {
  std::vector<assistant::AssistantSuggestion> result;
  for (const auto& suggestion : suggestions) {
    assistant::AssistantSuggestion assistant_suggestion;
    assistant_suggestion.id = base::UnguessableToken::Create();
    assistant_suggestion.text = suggestion.text;
    assistant_suggestion.icon_url = GURL(suggestion.icon_url);
    assistant_suggestion.action_url = GURL(suggestion.action_url);
    result.push_back(std::move(assistant_suggestion));
  }

  return result;
}

// Helper function to convert |action::Notification| to |AssistantNotification|.
chromeos::assistant::AssistantNotification ToAssistantNotification(
    const assistant::action::Notification& notification) {
  chromeos::assistant::AssistantNotification assistant_notification;
  assistant_notification.title = notification.title;
  assistant_notification.message = notification.text;
  assistant_notification.action_url = GURL(notification.action_url);
  assistant_notification.client_id = notification.notification_id;
  assistant_notification.server_id = notification.notification_id;
  assistant_notification.consistency_token = notification.consistency_token;
  assistant_notification.opaque_token = notification.opaque_token;
  assistant_notification.grouping_key = notification.grouping_key;
  assistant_notification.obfuscated_gaia_id = notification.obfuscated_gaia_id;
  assistant_notification.from_server = true;

  if (notification.expiry_timestamp_ms) {
    assistant_notification.expiry_time =
        base::Time::FromJavaTime(notification.expiry_timestamp_ms);
  }

  // The server sometimes sends an empty |notification_id|, but our client
  // requires a non-empty |client_id| for notifications. Known instances in
  // which the server sends an empty |notification_id| are for Reminders.
  if (assistant_notification.client_id.empty()) {
    assistant_notification.client_id =
        base::UnguessableToken::Create().ToString();
  }

  for (const auto& button : notification.buttons) {
    assistant_notification.buttons.push_back(
        {button.label, GURL(button.action_url),
         /*remove_notification_on_click=*/true});
  }
  return assistant_notification;
}
}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AssistantManagerDelegateImpl
////////////////////////////////////////////////////////////////////////////////

// Implementation of |AssistantManagerDelegate| that will forward all calls
// to the correct observers.
// It also keeps track of the last text query that was started, so we can
// pass its metadata to |OnConversationTurnStarted|.
class ConversationController::AssistantManagerDelegateImpl
    : public assistant_client::AssistantManagerDelegate {
 public:
  explicit AssistantManagerDelegateImpl(ConversationController* parent)
      : parent_(*parent),
        mojom_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}
  AssistantManagerDelegateImpl(const AssistantManagerDelegateImpl&) = delete;
  AssistantManagerDelegateImpl& operator=(const AssistantManagerDelegateImpl&) =
      delete;
  ~AssistantManagerDelegateImpl() override = default;

  std::string AddPendingTextInteraction(const std::string& query,
                                        AssistantQuerySource source) {
    return NewPendingInteraction(AssistantInteractionType::kText, source,
                                 query);
  }

  std::string NewPendingInteraction(AssistantInteractionType interaction_type,
                                    AssistantQuerySource source,
                                    const std::string& query) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto id = base::NumberToString(next_interaction_id_++);
    pending_interactions_.emplace(
        id, AssistantInteractionMetadata(interaction_type, source, query));
    return id;
  }

  // assistant_client::AssistantManagerDelegate overrides:
  void OnConversationTurnStartedInternal(
      const assistant_client::ConversationTurnMetadata& metadata) override {
    ENSURE_MOJOM_THREAD(
        &AssistantManagerDelegateImpl::OnConversationTurnStartedInternal,
        metadata);
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Retrieve the cached interaction metadata associated with this
    // conversation turn or construct a new instance if there's no match in the
    // cache.
    AssistantInteractionMetadata interaction_metadata;
    auto it = pending_interactions_.find(metadata.id);
    if (it != pending_interactions_.end()) {
      interaction_metadata = it->second;
      pending_interactions_.erase(it);
    } else {
      interaction_metadata.type = metadata.is_mic_open
                                      ? AssistantInteractionType::kVoice
                                      : AssistantInteractionType::kText;
      interaction_metadata.source =
          AssistantQuerySource::kLibAssistantInitiated;
    }

    for (auto& observer : parent_.observers_)
      observer->OnInteractionStarted(interaction_metadata);
  }

  void OnNotificationRemoved(const std::string& grouping_key) override {
    ENSURE_MOJOM_THREAD(&AssistantManagerDelegateImpl::OnNotificationRemoved,
                        grouping_key);

    if (grouping_key.empty())
      RemoveAllNotifications();
    else
      RemoveNotification(grouping_key);
  }

  void OnCommunicationError(int error_code) override {
    ENSURE_MOJOM_THREAD(&AssistantManagerDelegateImpl::OnCommunicationError,
                        error_code);
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (assistant::IsAuthError(error_code)) {
      for (auto& observer : parent_.authentication_state_observers_)
        observer->OnAuthenticationError();
    }
  }

 private:
  void RemoveAllNotifications() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    parent_.notification_delegate_->RemoveAllNotifications(
        /*from_server=*/true);
  }

  void RemoveNotification(const std::string& id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    parent_.notification_delegate_->RemoveNotificationByGroupingKey(
        id, /*from_server=*/true);
  }

  SEQUENCE_CHECKER(sequence_checker_);

  int next_interaction_id_ GUARDED_BY_CONTEXT(sequence_checker_) = 1;
  std::map<std::string, AssistantInteractionMetadata> pending_interactions_
      GUARDED_BY_CONTEXT(sequence_checker_);

  ConversationController& parent_ GUARDED_BY_CONTEXT(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> mojom_task_runner_;
  base::WeakPtrFactory<AssistantManagerDelegateImpl> weak_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
// ConversationController
////////////////////////////////////////////////////////////////////////////////

ConversationController::ConversationController()
    : receiver_(this),
      assistant_manager_delegate_(
          std::make_unique<AssistantManagerDelegateImpl>(this)),
      action_module_(std::make_unique<assistant::action::CrosActionModule>(
          assistant::features::IsAppSupportEnabled(),
          assistant::features::IsWaitSchedulingEnabled())),
      mojom_task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  action_module_->AddObserver(this);
}

ConversationController::~ConversationController() = default;

void ConversationController::Bind(
    mojo::PendingReceiver<mojom::ConversationController> receiver,
    mojo::PendingRemote<mojom::NotificationDelegate> notification_delegate) {
  // Cannot bind the receiver twice.
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));

  // Binds remote notification delegate.
  notification_delegate_.Bind(std::move(notification_delegate));
}

void ConversationController::AddActionObserver(
    chromeos::assistant::action::AssistantActionObserver* observer) {
  action_module_->AddObserver(observer);
}

void ConversationController::AddAuthenticationStateObserver(
    mojo::PendingRemote<
        chromeos::libassistant::mojom::AuthenticationStateObserver> observer) {
  authentication_state_observers_.Add(std::move(observer));
}

void ConversationController::OnAssistantClientCreated(
    AssistantClient* assistant_client) {
  if (!chromeos::assistant::features::IsLibAssistantV2Enabled()) {
    // Registers ActionModule when AssistantClient has been created but not yet
    // started.
    assistant_client->RegisterActionModule(action_module_.get());
  }

// TODO(b/196011844): Migrate `AssistantManagerDelegate` to V2.
#if !BUILDFLAG(IS_PREBUILT_LIBASSISTANT)
  assistant_client->assistant_manager_internal()->SetAssistantManagerDelegate(
      assistant_manager_delegate_.get());
#endif  // !BUILDFLAG(IS_PREBUILT_LIBASSISTANT)
}

void ConversationController::OnAssistantClientRunning(
    AssistantClient* assistant_client) {
  // Only when Libassistant is running we can start sending queries.
  assistant_client_ = assistant_client;
  requests_are_allowed_ = true;

  if (chromeos::assistant::features::IsLibAssistantV2Enabled()) {
    // Register the action module when all libassistant services are ready.
    // `action_module_` outlives gRPC services.
    assistant_client->RegisterActionModule(action_module_.get());
  }
}

void ConversationController::OnDestroyingAssistantClient(
    AssistantClient* assistant_client) {
  assistant_client_ = nullptr;
}

void ConversationController::SendTextQuery(const std::string& query,
                                           AssistantQuerySource source,
                                           bool allow_tts) {
  DVLOG(1) << __func__;

  DCHECK(requests_are_allowed_)
      << "Should not receive requests before Libassistant is running";
  if (!assistant_client_)
    return;

  MaybeStopPreviousInteraction();

  // Configs |VoicelessOptions|.
  ::assistant::api::VoicelessOptions options;
  options.set_is_user_initiated(true);
  if (!allow_tts) {
    options.set_modality(::assistant::api::VoicelessOptions::TYPING_MODALITY);
  }
  // Remember the interaction metadata, and pass the generated conversation id
  // to LibAssistant.
  options.set_conversation_turn_id(
      assistant_manager_delegate_->AddPendingTextInteraction(query, source));

  // Builds text interaction.
  auto interaction = CreateTextQueryInteraction(query);

  assistant_client_->SendVoicelessInteraction(
      interaction, /*description=*/"text_query", options, base::DoNothing());
}

void ConversationController::StartVoiceInteraction() {
  DVLOG(1) << __func__;

  DCHECK(requests_are_allowed_)
      << "Should not receive requests before Libassistant is running";
  if (!assistant_client_) {
    VLOG(1) << "Starting voice interaction without assistant manager.";
    return;
  }

  MaybeStopPreviousInteraction();

  assistant_client_->StartVoiceInteraction();
}

void ConversationController::StartEditReminderInteraction(
    const std::string& client_id) {
  DCHECK(requests_are_allowed_)
      << "Should not receive requests before Libassistant is running";
  if (!assistant_client_)
    return;

  // Cancels any ongoing StopInteraction posted by StopActiveInteraction()
  // before we move forward to start an EditReminderInteraction. Failing to
  // do this could expose a race condition and potentially result in the
  // following EditReminderInteraction getting barged in and cancelled.
  // See b/182948180.
  MaybeStopPreviousInteraction();

  ::assistant::api::VoicelessOptions options;
  options.set_is_user_initiated(true);

  assistant_client_->SendVoicelessInteraction(
      CreateEditReminderInteraction(client_id),
      /*description=*/std::string(), options, base::DoNothing());
}

void ConversationController::StartScreenContextInteraction(
    ax::mojom::AssistantStructurePtr assistant_structure,
    const std::vector<uint8_t>& screenshot) {
  DCHECK(requests_are_allowed_)
      << "Should not receive requests before Libassistant is running";
  if (!assistant_client_)
    return;

  MaybeStopPreviousInteraction();

  std::vector<std::string> context_protos;
  // Screen context can have the |assistant_structure|, or |assistant_extra| and
  // |assistant_tree| set to nullptr. This happens in the case where the screen
  // context is coming from the metalayer or there is no active window. For this
  // scenario, we don't create a context proto for the AssistantBundle that
  // consists of the |assistant_extra| and |assistant_tree|.
  if (assistant_structure && assistant_structure->assistant_extra &&
      assistant_structure->assistant_tree) {
    // Note: the value of |is_first_query| for screen context query is a no-op
    // because it is not used for metalayer and "What's on my screen" queries.
    context_protos.emplace_back(chromeos::assistant::CreateContextProto(
        chromeos::assistant::AssistantBundle{
            assistant_structure->assistant_extra.get(),
            assistant_structure->assistant_tree.get()},
        /*is_first_query=*/true));
  }

  // Note: the value of |is_first_query| for screen context query is a no-op.
  context_protos.emplace_back(
      chromeos::assistant::CreateContextProto(screenshot,
                                              /*is_first_query=*/true));
  assistant_client_->SendScreenContextRequest(context_protos);
}

void ConversationController::StopActiveInteraction(bool cancel_conversation) {
  if (!assistant_client_) {
    VLOG(1) << "Stopping interaction without assistant manager.";
    return;
  }

  // We do not stop the interaction immediately, but instead we give
  // Libassistant a bit of time to stop on its own accord. This improves
  // stability as Libassistant might misbehave when it's forcefully stopped.
  auto stop_callback = [](base::WeakPtr<ConversationController> weak_this,
                          bool cancel_conversation) {
    if (!weak_this || !weak_this->assistant_client_) {
      return;
    }
    VLOG(1) << "Stopping Assistant interaction.";
    weak_this->assistant_client_->StopAssistantInteraction(cancel_conversation);
  };

  stop_interaction_closure_ =
      std::make_unique<base::CancelableOnceClosure>(base::BindOnce(
          stop_callback, weak_factory_.GetWeakPtr(), cancel_conversation));

  mojom_task_runner_->PostDelayedTask(FROM_HERE,
                                      stop_interaction_closure_->callback(),
                                      kStopInteractionDelayTime);
}

void ConversationController::RetrieveNotification(
    AssistantNotification notification,
    int32_t action_index) {
  DCHECK(requests_are_allowed_)
      << "Should not receive requests before Libassistant is running";
  if (!assistant_client_)
    return;

  auto request_interaction = CreateNotificationRequestInteraction(
      notification.server_id, notification.consistency_token,
      notification.opaque_token, action_index);

  ::assistant::api::VoicelessOptions options;
  options.set_is_user_initiated(true);

  assistant_client_->SendVoicelessInteraction(
      request_interaction,
      /*description=*/"RequestNotification", options, base::DoNothing());
}

void ConversationController::DismissNotification(
    AssistantNotification notification) {
  DCHECK(requests_are_allowed_)
      << "Should not receive requests before Libassistant is running";
  if (!assistant_client_)
    return;

  auto dismissed_interaction = CreateNotificationDismissedInteraction(
      notification.server_id, notification.consistency_token,
      notification.opaque_token, {notification.grouping_key});

  ::assistant::api::VoicelessOptions options;
  options.set_obfuscated_gaia_id(notification.obfuscated_gaia_id);

  assistant_client_->SendVoicelessInteraction(
      dismissed_interaction, /*description=*/"DismissNotification", options,
      base::DoNothing());
}

void ConversationController::SendAssistantFeedback(
    const AssistantFeedback& feedback) {
  DCHECK(requests_are_allowed_)
      << "Should not receive requests before Libassistant is running";
  if (!assistant_client_)
    return;

  std::string raw_image_data(feedback.screenshot_png.begin(),
                             feedback.screenshot_png.end());
  auto interaction =
      CreateSendFeedbackInteraction(feedback.assistant_debug_info_allowed,
                                    feedback.description, raw_image_data);

  ::assistant::api::VoicelessOptions options;
  options.set_is_user_initiated(false);

  assistant_client_->SendVoicelessInteraction(
      interaction, /*description=*/"send feedback with details", options,
      base::DoNothing());
}

void ConversationController::AddRemoteObserver(
    mojo::PendingRemote<mojom::ConversationObserver> observer) {
  observers_.Add(std::move(observer));
}

// Called from Libassistant thread.
void ConversationController::OnShowHtml(const std::string& html_content,
                                        const std::string& fallback) {
  ENSURE_MOJOM_THREAD(&ConversationController::OnShowHtml, html_content,
                      fallback);

  for (auto& observer : observers_)
    observer->OnHtmlResponse(html_content, fallback);
}

// Called from Libassistant thread.
void ConversationController::OnShowText(const std::string& text) {
  ENSURE_MOJOM_THREAD(&ConversationController::OnShowText, text);

  for (auto& observer : observers_)
    observer->OnTextResponse(text);
}

// Called from Libassistant thread.
// Note that we should deprecate this API when the server provides a fallback.
void ConversationController::OnShowContextualQueryFallback() {
  // Show fallback message.
  OnShowText(l10n_util::GetStringUTF8(
      IDS_ASSISTANT_SCREEN_CONTEXT_QUERY_FALLBACK_TEXT));
}

// Called from Libassistant thread.
void ConversationController::OnShowSuggestions(
    const std::vector<assistant::action::Suggestion>& suggestions) {
  ENSURE_MOJOM_THREAD(&ConversationController::OnShowSuggestions, suggestions);

  for (auto& observer : observers_)
    observer->OnSuggestionsResponse(ToAssistantSuggestion(suggestions));
}

// Called from Libassistant thread.
void ConversationController::OnOpenUrl(const std::string& url,
                                       bool in_background) {
  ENSURE_MOJOM_THREAD(&ConversationController::OnOpenUrl, url, in_background);

  for (auto& observer : observers_)
    observer->OnOpenUrlResponse(GURL(url), in_background);
}

// Called from Libassistant thread.
// Note that OnVerifyAndroidApp() will be handled by |DisplayController|
// directly since it stores an updated list of all installed Android Apps on the
// device.
void ConversationController::OnOpenAndroidApp(
    const chromeos::assistant::AndroidAppInfo& app_info,
    const chromeos::assistant::InteractionInfo& interaction) {
  ENSURE_MOJOM_THREAD(&ConversationController::OnOpenAndroidApp, app_info,
                      interaction);

  for (auto& observer : observers_)
    observer->OnOpenAppResponse(app_info);

  // Note that we will always set |provider_found| to true since the preceding
  // OnVerifyAndroidApp() should already confirm that the requested provider is
  // available on the device.
  auto interaction_proto = CreateOpenProviderResponseInteraction(
      interaction.interaction_id, /*provider_found=*/true);
  ::assistant::api::VoicelessOptions options;
  options.set_obfuscated_gaia_id(interaction.user_id);

  assistant_client_->SendVoicelessInteraction(
      interaction_proto, /*description=*/"open_provider_response", options,
      base::DoNothing());
}

// Called from Libassistant thread.
void ConversationController::OnScheduleWait(int id, int time_ms) {
  ENSURE_MOJOM_THREAD(&ConversationController::OnScheduleWait, id, time_ms);

  DCHECK(assistant::features::IsWaitSchedulingEnabled());

  // Schedule a wait for |time_ms|, notifying the CrosActionModule when the wait
  // has finished so that it can inform LibAssistant to resume execution.
  mojom_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](const base::WeakPtr<ConversationController>& weak_ptr, int id) {
            if (weak_ptr) {
              weak_ptr->action_module_->OnScheduledWaitDone(
                  id, /*cancelled=*/false);
            }
          },
          weak_factory_.GetWeakPtr(), id),
      base::Milliseconds(time_ms));

  // Notify subscribers that a wait has been started.
  for (auto& observer : observers_)
    observer->OnWaitStarted();
}

// Called from Libassistant thread.
void ConversationController::OnShowNotification(
    const assistant::action::Notification& notification) {
  ENSURE_MOJOM_THREAD(&ConversationController::OnShowNotification,
                      notification);

  notification_delegate_->AddOrUpdateNotification(
      ToAssistantNotification(notification));
}

void ConversationController::OnInteractionStarted(
    const chromeos::assistant::AssistantInteractionMetadata& metadata) {
  stop_interaction_closure_.reset();
}

void ConversationController::OnInteractionFinished(
    chromeos::assistant::AssistantInteractionResolution resolution) {
  stop_interaction_closure_.reset();
}

void ConversationController::MaybeStopPreviousInteraction() {
  DVLOG(1) << __func__;

  if (!stop_interaction_closure_ || stop_interaction_closure_->IsCancelled()) {
    return;
  }

  stop_interaction_closure_->callback().Run();
}

}  // namespace libassistant
}  // namespace chromeos
