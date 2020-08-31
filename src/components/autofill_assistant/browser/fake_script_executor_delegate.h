// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_SCRIPT_EXECUTOR_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_SCRIPT_EXECUTOR_DELEGATE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/trigger_context.h"

namespace autofill_assistant {

// Implementation of ScriptExecutorDelegate that's convenient to use in
// unittests.
class FakeScriptExecutorDelegate : public ScriptExecutorDelegate {
 public:
  FakeScriptExecutorDelegate();
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
  std::string GetLocale() override;
  bool EnterState(AutofillAssistantState state) override;
  void SetTouchableElementArea(const ElementAreaProto& element) override;
  void SetStatusMessage(const std::string& message) override;
  std::string GetStatusMessage() const override;
  void SetBubbleMessage(const std::string& message) override;
  std::string GetBubbleMessage() const override;
  void SetDetails(std::unique_ptr<Details> details) override;
  void SetInfoBox(const InfoBox& info_box) override;
  void ClearInfoBox() override;
  void SetProgress(int progress) override;
  void SetProgressVisible(bool visible) override;
  void SetUserActions(
      std::unique_ptr<std::vector<UserAction>> user_actions) override;
  void SetCollectUserDataOptions(CollectUserDataOptions* options) override;
  void WriteUserData(
      base::OnceCallback<void(UserData*, UserData::FieldChange*)>) override;
  void WriteUserModel(
      base::OnceCallback<void(UserModel*)> write_callback) override;
  void SetViewportMode(ViewportMode mode) override;
  ViewportMode GetViewportMode() override;
  void SetPeekMode(ConfigureBottomSheetProto::PeekMode peek_mode) override;
  ConfigureBottomSheetProto::PeekMode GetPeekMode() override;
  void ExpandBottomSheet() override;
  void CollapseBottomSheet() override;
  bool SetForm(
      std::unique_ptr<FormProto> form,
      base::RepeatingCallback<void(const FormProto::Result*)> changed_callback,
      base::OnceCallback<void(const ClientStatus&)> cancel_callback) override;
  UserModel* GetUserModel() override;
  EventHandler* GetEventHandler() override;
  bool HasNavigationError() override;
  bool IsNavigatingToNewDocument() override;
  void RequireUI() override;
  void AddListener(NavigationListener* listener) override;
  void RemoveListener(NavigationListener* listener) override;
  void SetExpandSheetForPromptAction(bool expand) override;
  void SetBrowseDomainsWhitelist(std::vector<std::string> domains) override;

  void SetGenericUi(
      std::unique_ptr<GenericUserInterfaceProto> generic_ui,
      base::OnceCallback<void(bool,
                              ProcessedActionStatusProto,
                              const UserModel*)> end_action_callback) override;
  void ClearGenericUi() override;

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
  AutofillAssistantState GetState() {
    return state_history_.empty() ? AutofillAssistantState::INACTIVE
                                  : state_history_.back();
  }

  Details* GetDetails() { return details_.get(); }

  InfoBox* GetInfoBox() { return info_box_.get(); }

  std::vector<UserAction>* GetUserActions() { return user_actions_.get(); }

  CollectUserDataOptions* GetOptions() { return payment_request_options_; }

  void UpdateNavigationState(bool navigating, bool error) {
    navigating_to_new_document_ = navigating;
    navigation_error_ = error;

    for (auto* listener : listeners_) {
      listener->OnNavigationStateChanged();
    }
  }

  bool HasListeners() { return !listeners_.empty(); }

  bool IsUIRequired() { return require_ui_; }

 private:
  ClientSettings client_settings_;
  GURL current_url_;
  Service* service_ = nullptr;
  WebController* web_controller_ = nullptr;
  std::unique_ptr<TriggerContext> trigger_context_;
  std::vector<AutofillAssistantState> state_history_;
  std::string status_message_;
  std::unique_ptr<Details> details_;
  std::unique_ptr<InfoBox> info_box_;
  std::unique_ptr<std::vector<UserAction>> user_actions_;
  CollectUserDataOptions* payment_request_options_;
  std::unique_ptr<UserData> payment_request_info_;
  bool navigating_to_new_document_ = false;
  bool navigation_error_ = false;
  std::set<ScriptExecutorDelegate::NavigationListener*> listeners_;
  ViewportMode viewport_mode_ = ViewportMode::NO_RESIZE;
  ConfigureBottomSheetProto::PeekMode peek_mode_ =
      ConfigureBottomSheetProto::HANDLE;
  bool expand_or_collapse_updated_ = false;
  bool expand_or_collapse_value_ = false;
  bool expand_sheet_for_prompt_ = true;
  std::vector<std::string> browse_domains_;
  UserModel* user_model_ = nullptr;

  bool require_ui_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeScriptExecutorDelegate);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_SCRIPT_EXECUTOR_DELEGATE_H_
