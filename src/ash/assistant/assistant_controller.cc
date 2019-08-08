// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_controller.h"

#include <algorithm>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/utility/screenshot_controller.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "components/prefs/pref_registry_simple.h"
#include "services/content/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace ash {

namespace {

// Scheme of the Android intent url.
constexpr char kAndroidIntentScheme[] = "intent";

}  // namespace

AssistantController::AssistantController()
    : assistant_volume_control_binding_(this),
      assistant_alarm_timer_controller_(this),
      assistant_cache_controller_(this),
      assistant_interaction_controller_(this),
      assistant_notification_controller_(this),
      assistant_screen_context_controller_(this),
      assistant_setup_controller_(this),
      assistant_ui_controller_(this),
      view_delegate_(this),
      weak_factory_(this) {
  Shell::Get()->voice_interaction_controller()->AddLocalObserver(this);
  chromeos::CrasAudioHandler::Get()->AddAudioObserver(this);
  AddObserver(this);

  NotifyConstructed();
}

AssistantController::~AssistantController() {
  NotifyDestroying();

  chromeos::CrasAudioHandler::Get()->RemoveAudioObserver(this);
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
  Shell::Get()->voice_interaction_controller()->RemoveLocalObserver(this);
  RemoveObserver(this);
}

// static
void AssistantController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kAssistantNumWarmerWelcomeTriggered, 0);
}

void AssistantController::BindRequest(
    mojom::AssistantControllerRequest request) {
  assistant_controller_bindings_.AddBinding(this, std::move(request));
}

void AssistantController::BindRequest(
    mojom::AssistantVolumeControlRequest request) {
  assistant_volume_control_binding_.Bind(std::move(request));
}

void AssistantController::AddObserver(AssistantControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void AssistantController::RemoveObserver(
    AssistantControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantController::SetAssistant(
    chromeos::assistant::mojom::AssistantPtr assistant) {
  assistant_ = std::move(assistant);

  // Provide reference to sub-controllers.
  assistant_alarm_timer_controller_.SetAssistant(assistant_.get());
  assistant_interaction_controller_.SetAssistant(assistant_.get());
  assistant_notification_controller_.SetAssistant(assistant_.get());
  assistant_screen_context_controller_.SetAssistant(assistant_.get());
  assistant_ui_controller_.SetAssistant(assistant_.get());

  // The Assistant service needs to have accessibility state synced with ash
  // and be notified of any accessibility status changes in the future to
  // provide an opportunity to turn on/off A11Y features.
  Shell::Get()->accessibility_controller()->AddObserver(this);
  OnAccessibilityStatusChanged();

  for (AssistantControllerObserver& observer : observers_)
    observer.OnAssistantReady();
}

void AssistantController::OpenAssistantSettings() {
  // Launch Assistant settings via deeplink.
  OpenUrl(assistant::util::CreateAssistantSettingsDeepLink());
}

void AssistantController::SendAssistantFeedback(
    bool assistant_debug_info_allowed,
    const std::string& feedback_description,
    const std::string& screenshot_png) {
  chromeos::assistant::mojom::AssistantFeedbackPtr assistant_feedback =
      chromeos::assistant::mojom::AssistantFeedback::New();
  assistant_feedback->assistant_debug_info_allowed =
      assistant_debug_info_allowed;
  assistant_feedback->description = feedback_description;
  assistant_feedback->screenshot_png = screenshot_png;
  assistant_->SendAssistantFeedback(std::move(assistant_feedback));
}

void AssistantController::SetDeviceActions(
    chromeos::assistant::mojom::DeviceActionsPtr device_actions) {
  device_actions_ = std::move(device_actions);
}

void AssistantController::StartSpeakerIdEnrollmentFlow() {
  mojom::ConsentStatus consent_status =
      Shell::Get()->voice_interaction_controller()->consent_status().value_or(
          mojom::ConsentStatus::kUnknown);
  if (consent_status == mojom::ConsentStatus::kActivityControlAccepted) {
    // If activity control has been accepted, launch the enrollment flow.
    setup_controller()->StartOnboarding(false /* relaunch */,
                                        FlowType::kSpeakerIdEnrollment);
  } else {
    // If activity control has not been accepted, launch the opt-in flow.
    setup_controller()->StartOnboarding(false /* relaunch */,
                                        FlowType::kConsentFlow);
  }
}

void AssistantController::DownloadImage(
    const GURL& url,
    AssistantImageDownloader::DownloadCallback callback) {
  const UserSession* user_session =
      Shell::Get()->session_controller()->GetUserSession(0);

  if (!user_session) {
    LOG(WARNING) << "Unable to retrieve active user session.";
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  AccountId account_id = user_session->user_info.account_id;
  AssistantImageDownloader::GetInstance()->Download(account_id, url,
                                                    std::move(callback));
}

void AssistantController::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  using assistant::util::DeepLinkParam;
  using assistant::util::DeepLinkType;

  switch (type) {
    case DeepLinkType::kChromeSettings: {
      // Chrome Settings deep links are opened in a new browser tab.
      OpenUrl(assistant::util::GetChromeSettingsUrl(
          assistant::util::GetDeepLinkParam(params, DeepLinkParam::kPage)));
      break;
    }
    case DeepLinkType::kFeedback:
      NewWindowDelegate::GetInstance()->OpenFeedbackPage(
          /*from_assistant=*/true);
      break;
    case DeepLinkType::kScreenshot:
      // We close the UI before taking the screenshot as it's probably not the
      // user's intention to include the Assistant in the picture.
      assistant_ui_controller_.CloseUi(AssistantExitPoint::kUnspecified);
      Shell::Get()->screenshot_controller()->TakeScreenshotForAllRootWindows();
      break;
    case DeepLinkType::kTaskManager:
      // Open task manager window.
      NewWindowDelegate::GetInstance()->ShowTaskManager();
      break;
    case DeepLinkType::kUnsupported:
    case DeepLinkType::kAlarmTimer:
    case DeepLinkType::kLists:
    case DeepLinkType::kNotes:
    case DeepLinkType::kOnboarding:
    case DeepLinkType::kQuery:
    case DeepLinkType::kReminders:
    case DeepLinkType::kSettings:
    case DeepLinkType::kWhatsOnMyScreen:
      // No action needed.
      break;
  }
}

void AssistantController::SetVolume(int volume, bool user_initiated) {
  volume = std::min(100, volume);
  volume = std::max(volume, 0);
  chromeos::CrasAudioHandler::Get()->SetOutputVolumePercent(volume);
}

void AssistantController::SetMuted(bool muted) {
  chromeos::CrasAudioHandler::Get()->SetOutputMute(muted);
}

void AssistantController::AddVolumeObserver(mojom::VolumeObserverPtr observer) {
  volume_observer_.AddPtr(std::move(observer));

  int output_volume =
      chromeos::CrasAudioHandler::Get()->GetOutputVolumePercent();
  bool mute = chromeos::CrasAudioHandler::Get()->IsOutputMuted();
  OnOutputMuteChanged(mute);
  OnOutputNodeVolumeChanged(0 /* node */, output_volume);
}

void AssistantController::OnOutputMuteChanged(bool mute_on) {
  volume_observer_.ForAllPtrs([mute_on](mojom::VolumeObserver* observer) {
    observer->OnMuteStateChanged(mute_on);
  });
}

void AssistantController::OnOutputNodeVolumeChanged(uint64_t node, int volume) {
  // |node| refers to the active volume device, which we don't care here.
  volume_observer_.ForAllPtrs([volume](mojom::VolumeObserver* observer) {
    observer->OnVolumeChanged(volume);
  });
}

void AssistantController::OnAccessibilityStatusChanged() {
  // The Assistant service needs to be informed of changes to accessibility
  // state so that it can turn on/off A11Y features appropriately.
  assistant_->OnAccessibilityStatusChanged(
      Shell::Get()->accessibility_controller()->spoken_feedback_enabled());
}

void AssistantController::OpenUrl(const GURL& url, bool from_server) {
  if (url.SchemeIs(kAndroidIntentScheme) && device_actions_) {
    device_actions_->LaunchAndroidIntent(url.spec());
    return;
  }

  if (assistant::util::IsDeepLinkUrl(url)) {
    NotifyDeepLinkReceived(url);
    return;
  }

  // Give observers an opportunity to perform any necessary handling before we
  // open the specified |url| in a new browser tab.
  NotifyOpeningUrl(url, from_server);

  // The new tab should be opened with a user activation since the user
  // interacted with the Assistant to open the url.
  NewWindowDelegate::GetInstance()->NewTabWithUrl(
      url, /*from_user_interaction=*/true);
  NotifyUrlOpened(url, from_server);
}

void AssistantController::GetNavigableContentsFactory(
    mojo::PendingReceiver<content::mojom::NavigableContentsFactory> receiver) {
  const UserSession* user_session =
      Shell::Get()->session_controller()->GetUserSession(0);

  if (!user_session) {
    LOG(WARNING) << "Unable to retrieve active user session.";
    return;
  }

  const base::Optional<base::Token>& service_instance_group =
      user_session->user_info.service_instance_group;
  if (!service_instance_group) {
    LOG(ERROR) << "Unable to retrieve service instance group.";
    return;
  }

  Shell::Get()->connector()->Connect(
      service_manager::ServiceFilter::ByNameInGroup(
          content::mojom::kServiceName, *service_instance_group),
      std::move(receiver));
}

bool AssistantController::IsAssistantReady() const {
  return !!assistant_;
}

void AssistantController::NotifyConstructed() {
  for (AssistantControllerObserver& observer : observers_)
    observer.OnAssistantControllerConstructed();
}

void AssistantController::NotifyDestroying() {
  for (AssistantControllerObserver& observer : observers_)
    observer.OnAssistantControllerDestroying();
}

void AssistantController::NotifyDeepLinkReceived(const GURL& deep_link) {
  using assistant::util::DeepLinkType;

  // Retrieve deep link type and parsed parameters.
  DeepLinkType type = assistant::util::GetDeepLinkType(deep_link);
  const std::map<std::string, std::string> params =
      assistant::util::GetDeepLinkParams(deep_link);

  // TODO(wutao): Remove AssistantControllerObserver::OnDeepLinkReceived.
  for (AssistantControllerObserver& observer : observers_)
    observer.OnDeepLinkReceived(type, params);

  view_delegate_.NotifyDeepLinkReceived(type, params);
}

void AssistantController::NotifyOpeningUrl(const GURL& url, bool from_server) {
  for (AssistantControllerObserver& observer : observers_)
    observer.OnOpeningUrl(url, from_server);
}

void AssistantController::NotifyUrlOpened(const GURL& url, bool from_server) {
  for (AssistantControllerObserver& observer : observers_)
    observer.OnUrlOpened(url, from_server);
}

void AssistantController::OnVoiceInteractionStatusChanged(
    mojom::VoiceInteractionState state) {
  if (state == mojom::VoiceInteractionState::NOT_READY)
    assistant_ui_controller_.CloseUi(AssistantExitPoint::kUnspecified);
}

void AssistantController::OnLockedFullScreenStateChanged(bool enabled) {
  if (enabled)
    assistant_ui_controller_.CloseUi(AssistantExitPoint::kUnspecified);
}

base::WeakPtr<AssistantController> AssistantController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace ash
