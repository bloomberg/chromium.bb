// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_STARTER_PLATFORM_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_STARTER_PLATFORM_DELEGATE_H_

#include <memory>
#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill_assistant/browser/starter_platform_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class FakeStarterPlatformDelegate : public StarterPlatformDelegate {
 public:
  FakeStarterPlatformDelegate();
  ~FakeStarterPlatformDelegate() override;

  // Implements StarterPlatformDelegate:
  std::unique_ptr<TriggerScriptCoordinator::UiDelegate>
  CreateTriggerScriptUiDelegate() override;
  std::unique_ptr<ServiceRequestSender> GetTriggerScriptRequestSenderToInject()
      override;
  void StartRegularScript(
      GURL url,
      std::unique_ptr<TriggerContext> trigger_context,
      const absl::optional<TriggerScriptProto>& trigger_script) override;
  bool IsRegularScriptRunning() const override;
  bool IsRegularScriptVisible() const override;
  WebsiteLoginManager* GetWebsiteLoginManager() const override;
  version_info::Channel GetChannel() const override;
  bool GetFeatureModuleInstalled() const override;
  void InstallFeatureModule(
      bool show_ui,
      base::OnceCallback<void(Metrics::FeatureModuleInstallation result)>
          callback) override;
  bool GetIsFirstTimeUser() const override;
  void SetIsFirstTimeUser(bool first_time_user) override;
  bool GetOnboardingAccepted() const override;
  void SetOnboardingAccepted(bool accepted) override;
  void ShowOnboarding(
      bool use_dialog_onboarding,
      const TriggerContext& trigger_context,
      base::OnceCallback<void(bool shown, OnboardingResult result)> callback)
      override;
  void HideOnboarding() override;
  bool GetProactiveHelpSettingEnabled() const override;
  void SetProactiveHelpSettingEnabled(bool enabled) override;
  bool GetMakeSearchesAndBrowsingBetterEnabled() const override;
  bool GetIsCustomTab() const override;
  bool GetIsTabCreatedByGSA() const override;

  // Intentionally public to give tests direct access.
  std::unique_ptr<TriggerScriptCoordinator::UiDelegate>
      trigger_script_ui_delegate_;
  std::unique_ptr<ServiceRequestSender> trigger_script_request_sender_for_test_;
  raw_ptr<WebsiteLoginManager> website_login_manager_ = nullptr;
  version_info::Channel channel_ = version_info::Channel::UNKNOWN;
  bool feature_module_installed_ = true;
  Metrics::FeatureModuleInstallation feature_module_installation_result_ =
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED;
  bool is_first_time_user_ = false;
  bool onboarding_accepted_ = true;
  bool show_onboarding_result_shown_ = false;
  OnboardingResult show_onboarding_result_ = OnboardingResult::ACCEPTED;
  base::OnceCallback<void(
      base::OnceCallback<void(bool, OnboardingResult)> result_callback)>
      on_show_onboarding_callback_;
  bool proactive_help_enabled_ = true;
  bool msbb_enabled_ = true;
  bool is_custom_tab_ = true;
  bool is_tab_created_by_gsa_ = true;
  base::OnceCallback<void(
      GURL url,
      std::unique_ptr<TriggerContext> trigger_context,
      const absl::optional<TriggerScriptProto>& trigger_script)>
      start_regular_script_callback_;
  bool is_regular_script_running_ = false;
  bool is_regular_script_visible_ = false;

  int num_install_feature_module_called_ = 0;
  int num_show_onboarding_called_ = 0;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_STARTER_PLATFORM_DELEGATE_H_
