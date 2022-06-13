// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_SCRIPT_EXECUTOR_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_SCRIPT_EXECUTOR_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/trigger_context.h"

namespace autofill_assistant {

// Implementation of ScriptExecutorDelegate that's convenient to use in
// unittests.
class FakeScriptExecutorDelegate : public ScriptExecutorDelegate {
 public:
  FakeScriptExecutorDelegate();

  FakeScriptExecutorDelegate(const FakeScriptExecutorDelegate&) = delete;
  FakeScriptExecutorDelegate& operator=(const FakeScriptExecutorDelegate&) =
      delete;

  ~FakeScriptExecutorDelegate() override;

  const ClientSettings& GetSettings() override;
  const GURL& GetCurrentURL() override;
  const GURL& GetDeeplinkURL() override;
  const GURL& GetScriptURL() override;
  Service* GetService() override;
  WebController* GetWebController() override;
  TriggerContext* GetTriggerContext() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  WebsiteLoginManager* GetWebsiteLoginManager() override;
  content::WebContents* GetWebContents() override;
  std::string GetEmailAddressForAccessTokenAccount() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  bool EnterState(AutofillAssistantState state) override;
  AutofillAssistantState GetState() override;
  void SetTouchableElementArea(const ElementAreaProto& element) override;
  void SetStatusMessage(const std::string& message) override;
  std::string GetStatusMessage() const override;
  void SetBubbleMessage(const std::string& message) override;
  std::string GetBubbleMessage() const override;
  void SetTtsMessage(const std::string& message) override;
  std::string GetTtsMessage() const override;
  TtsButtonState GetTtsButtonState() const override;
  void MaybePlayTtsMessage() override;
  void SetDetails(std::unique_ptr<Details> details,
                  base::TimeDelta delay) override;
  void AppendDetails(std::unique_ptr<Details> details,
                     base::TimeDelta delay) override;
  void SetInfoBox(const InfoBox& info_box) override;
  void ClearInfoBox() override;
  bool SetProgressActiveStepIdentifier(
      const std::string& active_step_identifier) override;
  void SetProgressActiveStep(int active_step) override;
  void SetProgressVisible(bool visible) override;
  void SetProgressBarErrorState(bool error) override;
  void SetStepProgressBarConfiguration(
      const ShowProgressBarProto::StepProgressBarConfiguration& configuration)
      override;
  void SetUserActions(
      std::unique_ptr<std::vector<UserAction>> user_actions) override;
  void SetCollectUserDataOptions(CollectUserDataOptions* options) override;
  void SetLastSuccessfulUserDataOptions(std::unique_ptr<CollectUserDataOptions>
                                            collect_user_data_options) override;
  const CollectUserDataOptions* GetLastSuccessfulUserDataOptions()
      const override;
  void WriteUserData(
      base::OnceCallback<void(UserData*, UserData::FieldChange*)>) override;
  void SetViewportMode(ViewportMode mode) override;
  ViewportMode GetViewportMode() override;
  void SetPeekMode(ConfigureBottomSheetProto::PeekMode peek_mode) override;
  ConfigureBottomSheetProto::PeekMode GetPeekMode() override;
  void ExpandBottomSheet() override;
  void CollapseBottomSheet() override;
  void SetClientSettings(const ClientSettingsProto& client_settings) override;
  bool SetForm(
      std::unique_ptr<FormProto> form,
      base::RepeatingCallback<void(const FormProto::Result*)> changed_callback,
      base::OnceCallback<void(const ClientStatus&)> cancel_callback) override;
  UserModel* GetUserModel() override;
  EventHandler* GetEventHandler() override;
  void ExpectNavigation() override;
  bool HasNavigationError() override;
  bool IsNavigatingToNewDocument() override;
  void RequireUI() override;
  void AddNavigationListener(
      ScriptExecutorDelegate::NavigationListener* listener) override;
  void RemoveNavigationListener(
      ScriptExecutorDelegate::NavigationListener* listener) override;
  void AddListener(ScriptExecutorDelegate::Listener* listener) override;
  void RemoveListener(ScriptExecutorDelegate::Listener* listener) override;
  void SetExpandSheetForPromptAction(bool expand) override;
  void SetBrowseDomainsAllowlist(std::vector<std::string> domains) override;
  void SetGenericUi(
      std::unique_ptr<GenericUserInterfaceProto> generic_ui,
      base::OnceCallback<void(const ClientStatus&)> end_action_callback,
      base::OnceCallback<void(const ClientStatus&)>
          view_inflation_finished_callback) override;
  void SetPersistentGenericUi(
      std::unique_ptr<GenericUserInterfaceProto> generic_ui,
      base::OnceCallback<void(const ClientStatus&)>
          view_inflation_finished_callback) override;
  void ClearGenericUi() override;
  void ClearPersistentGenericUi() override;
  void SetOverlayBehavior(
      ConfigureUiStateProto::OverlayBehavior overlay_behavior) override;
  void SetBrowseModeInvisible(bool invisible) override;
  void SetShowFeedbackChip(bool show_feedback_chip) override;
  ProcessedActionStatusDetailsProto& GetLogInfo() override;

  bool ShouldShowWarning() override;

  ClientSettings* GetMutableSettings() { return &client_settings_; }

  void SetCurrentURL(const GURL& url) { current_url_ = url; }

  void SetService(Service* service) { service_ = service; }

  void SetWebController(WebController* web_controller) {
    web_controller_ = web_controller;
  }

  void SetTriggerContext(std::unique_ptr<TriggerContext> trigger_context) {
    trigger_context_ = std::move(trigger_context);
  }

  void SetUserModel(UserModel* user_model) { user_model_ = user_model; }
  std::vector<AutofillAssistantState> GetStateHistory() {
    return state_history_;
  }
  std::vector<ElementAreaProto> GetTouchableElementAreaHistory() {
    return touchable_element_area_history_;
  }

  const std::vector<Details>& GetDetails() { return details_; }

  const GenericUserInterfaceProto* GetPersistentGenericUi() {
    return persistent_generic_ui_.get();
  }

  InfoBox* GetInfoBox() { return info_box_.get(); }

  std::vector<UserAction>* GetUserActions() { return user_actions_.get(); }

  CollectUserDataOptions* GetOptions() { return payment_request_options_; }

  void UpdateNavigationState(bool navigating, bool error) {
    navigating_to_new_document_ = navigating;
    navigation_error_ = error;

    for (auto* listener : navigation_listeners_) {
      listener->OnNavigationStateChanged();
    }
  }

  bool HasNavigationListeners() { return !navigation_listeners_.empty(); }

  bool HasListeners() { return !listeners_.empty(); }

  bool IsUIRequired() { return require_ui_; }

 private:
  ClientSettings client_settings_;
  GURL current_url_;
  raw_ptr<Service> service_ = nullptr;
  raw_ptr<WebController> web_controller_ = nullptr;
  std::unique_ptr<TriggerContext> trigger_context_;
  std::vector<AutofillAssistantState> state_history_;
  std::vector<ElementAreaProto> touchable_element_area_history_;
  std::string status_message_;
  std::string tts_message_;
  std::string bubble_message_;
  std::vector<Details> details_;
  std::unique_ptr<InfoBox> info_box_;
  std::unique_ptr<std::vector<UserAction>> user_actions_;
  std::unique_ptr<CollectUserDataOptions> last_payment_request_options_;
  raw_ptr<CollectUserDataOptions> payment_request_options_;
  std::unique_ptr<UserData> payment_request_info_;
  bool navigating_to_new_document_ = false;
  bool navigation_error_ = false;
  base::flat_set<ScriptExecutorDelegate::NavigationListener*>
      navigation_listeners_;
  base::flat_set<ScriptExecutorDelegate::Listener*> listeners_;
  ViewportMode viewport_mode_ = ViewportMode::NO_RESIZE;
  ConfigureBottomSheetProto::PeekMode peek_mode_ =
      ConfigureBottomSheetProto::HANDLE;
  bool expand_or_collapse_updated_ = false;
  bool expand_or_collapse_value_ = false;
  bool expand_sheet_for_prompt_ = true;
  std::vector<std::string> browse_domains_;
  raw_ptr<UserModel> user_model_ = nullptr;
  std::unique_ptr<GenericUserInterfaceProto> persistent_generic_ui_;
  ProcessedActionStatusDetailsProto log_info_;

  bool require_ui_ = false;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_SCRIPT_EXECUTOR_DELEGATE_H_
