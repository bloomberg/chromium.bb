// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ASSISTANT_OPTIN_FLOW_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ASSISTANT_OPTIN_FLOW_SCREEN_HANDLER_H_

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/assistant/assistant_setup.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "base/containers/circular_deque.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chromeos/services/assistant/public/cpp/assistant_settings.h"
#include "components/sync/protocol/user_consent_types.pb.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
class AssistantOptInFlowScreen;
}

namespace chromeos {

// Interface for dependency injection between AssistantOptInFlowScreen
// and its WebUI representation.
class AssistantOptInFlowScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"assistant-optin-flow"};

  AssistantOptInFlowScreenView(const AssistantOptInFlowScreenView&) = delete;
  AssistantOptInFlowScreenView& operator=(const AssistantOptInFlowScreenView&) =
      delete;

  virtual ~AssistantOptInFlowScreenView() = default;

  virtual void Bind(ash::AssistantOptInFlowScreen* screen) = 0;
  virtual void Unbind() = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;

 protected:
  AssistantOptInFlowScreenView() = default;
};

class AssistantOptInFlowScreenHandler
    : public BaseScreenHandler,
      public AssistantOptInFlowScreenView,
      public ash::AssistantStateObserver,
      public chromeos::assistant::SpeakerIdEnrollmentClient {
 public:
  struct ConsentData {
    // Consent token used to complete the opt-in.
    std::string consent_token;

    // An opaque token for audit record.
    std::string ui_audit_key;

    // An enum denoting the Assistant activity control setting type.
    sync_pb::UserConsentTypes::AssistantActivityControlConsent::SettingType
        setting_type;
  };

  using TView = AssistantOptInFlowScreenView;

  explicit AssistantOptInFlowScreenHandler(
      JSCallsContainer* js_calls_container);

  AssistantOptInFlowScreenHandler(const AssistantOptInFlowScreenHandler&) =
      delete;
  AssistantOptInFlowScreenHandler& operator=(
      const AssistantOptInFlowScreenHandler&) = delete;

  ~AssistantOptInFlowScreenHandler() override;

  // Set an optional callback that will run when the screen has been
  // initialized.
  void set_on_initialized(base::OnceClosure on_initialized) {
    DCHECK(on_initialized_.is_null());
    on_initialized_ = std::move(on_initialized);
  }

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void RegisterMessages() override;
  void GetAdditionalParameters(base::DictionaryValue* dict) override;

  // AssistantOptInFlowScreenView:
  void Bind(ash::AssistantOptInFlowScreen* screen) override;
  void Unbind() override;
  void Show() override;
  void Hide() override;

  // assistant::SpeakerIdEnrollmentClient:
  void OnListeningHotword() override;
  void OnProcessingHotword() override;
  void OnSpeakerIdEnrollmentDone() override;
  void OnSpeakerIdEnrollmentFailure() override;

  // Setup Assistant settings manager connection.
  void SetupAssistantConnection();

  // Send messages to the page.
  void ShowNextScreen();

  // Handle user opt-in result.
  void OnActivityControlOptInResult(bool opted_in);
  void OnScreenContextOptInResult(bool opted_in);

  // Called when the UI dialog is closed.
  void OnDialogClosed();

 private:
  // BaseScreenHandler:
  void Initialize() override;

  // ash::AssistantStateObserver:
  void OnAssistantSettingsEnabled(bool enabled) override;
  void OnAssistantStatusChanged(
      chromeos::assistant::AssistantStatus status) override;

  // Send GetSettings request for the opt-in UI.
  void SendGetSettingsRequest();

  // Stops the current speaker ID enrollment flow.
  void StopSpeakerIdEnrollment();

  // Send message and consent data to the page.
  void ReloadContent(const base::Value& dict);
  void AddSettingZippy(const std::string& type, const base::Value& data);

  // Update value prop screen to show the next settings.
  void UpdateValuePropScreen();

  // Handle response from the settings manager.
  void OnGetSettingsResponse(const std::string& settings);
  void OnUpdateSettingsResponse(const std::string& settings);

  // Handler for JS WebUI message.
  void HandleValuePropScreenUserAction(const std::string& action);
  void HandleRelatedInfoScreenUserAction(const std::string& action);
  void HandleVoiceMatchScreenUserAction(const std::string& action);
  void HandleValuePropScreenShown();
  void HandleRelatedInfoScreenShown();
  void HandleVoiceMatchScreenShown();
  void HandleLoadingTimeout();
  void HandleFlowFinished();
  void HandleFlowInitialized(const int flow_type);

  // Power related
  bool DeviceHasBattery();

  ash::AssistantOptInFlowScreen* screen_ = nullptr;

  base::OnceClosure on_initialized_;

  // Whether the screen should be shown right after initialization.
  bool show_on_init_ = false;

  // Whether activity control is needed for user.
  bool activity_control_needed_ = true;

  // Whether the user has started voice match enrollment.
  bool voice_match_enrollment_started_ = false;

  // Whether the use has completed voice match enrollment.
  bool voice_match_enrollment_done_ = false;

  // Whether error occurs during voice match enrollment.
  bool voice_match_enrollment_error_ = false;

  // Assistant optin flow type.
  ash::FlowType flow_type_ = ash::FlowType::kConsentFlow;

  // Time that get settings request is sent.
  base::TimeTicks send_request_time_;

  // Counter for the number of loading timeout happens.
  int loading_timeout_counter_ = 0;

  // Whether the screen has been initialized.
  bool initialized_ = false;

  // Whether the user has opted in/out any activity control consent.
  bool has_opted_out_any_consent_ = false;
  bool has_opted_in_any_consent_ = false;

  // Used to record related information of activity control consents which are
  // pending for user action.
  base::circular_deque<ConsentData> pending_consent_data_;

  base::WeakPtrFactory<AssistantOptInFlowScreenHandler> weak_factory_{this};
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::AssistantOptInFlowScreenHandler;
using ::chromeos::AssistantOptInFlowScreenView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ASSISTANT_OPTIN_FLOW_SCREEN_HANDLER_H_
