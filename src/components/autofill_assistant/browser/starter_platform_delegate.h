// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_PLATFORM_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_PLATFORM_DELEGATE_H_

#include "base/callback_forward.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/onboarding_result.h"
#include "components/autofill_assistant/browser/service/service_request_sender.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/trigger_scripts/trigger_script_coordinator.h"
#include "components/autofill_assistant/browser/website_login_manager.h"
#include "components/version_info/version_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {

// Interface for platform delegates that provide platform-dependent features
// and dependencies to the starter.
class StarterPlatformDelegate {
 public:
  StarterPlatformDelegate() = default;
  virtual ~StarterPlatformDelegate() = default;

  // Asks the platform delegate to return a UI delegate for trigger scripts.
  virtual std::unique_ptr<TriggerScriptCoordinator::UiDelegate>
  CreateTriggerScriptUiDelegate() = 0;
  // Allows integration tests to provide their own mocked trigger script request
  // senders. Returns null if the default request sender should be used.
  virtual std::unique_ptr<ServiceRequestSender>
  GetTriggerScriptRequestSenderToInject() = 0;

  // Requests the platform delegate to start the regular script.
  virtual void StartRegularScript(
      GURL url,
      std::unique_ptr<TriggerContext> trigger_context,
      const absl::optional<TriggerScriptProto>& trigger_script) = 0;
  // Returns whether a regular script is currently running.
  virtual bool IsRegularScriptRunning() const = 0;
  // Returns whether a regular script is currently showing UI to the user.
  virtual bool IsRegularScriptVisible() const = 0;

  // Access to the login manager.
  virtual WebsiteLoginManager* GetWebsiteLoginManager() const = 0;
  // Returns the channel for the installation (canary, dev, beta, stable).
  virtual version_info::Channel GetChannel() const = 0;

  // Returns whether the feature module is already installed.
  virtual bool GetFeatureModuleInstalled() const = 0;
  // Installs the feature module and runs |callback| with the result.
  virtual void InstallFeatureModule(
      bool show_ui,
      base::OnceCallback<void(Metrics::FeatureModuleInstallation result)>
          callback) = 0;

  // Returns whether the user has seen the UI before.
  virtual bool GetIsFirstTimeUser() const = 0;
  // Marks a user as a first-time or returning user.
  virtual void SetIsFirstTimeUser(bool first_time_user) = 0;

  // Returns whether the onboarding has already been accepted.
  virtual bool GetOnboardingAccepted() const = 0;
  // Changes whether the onboarding has already been accepted.
  virtual void SetOnboardingAccepted(bool accepted) = 0;
  // Show the onboarding screen and run |callback| with the result. Note: it is
  // illegal to invoke this method if a previous call has not yet run its
  // callback.
  virtual void ShowOnboarding(
      bool use_dialog_onboarding,
      const TriggerContext& trigger_context,
      base::OnceCallback<void(bool shown, OnboardingResult result)>
          callback) = 0;
  // Hide the onboarding, if currently shown. This may be invoked if the
  // conditions necessary to proceed with the startup are no longer satisfied.
  virtual void HideOnboarding() = 0;

  // Returns whether the proactive help setting is enabled.
  virtual bool GetProactiveHelpSettingEnabled() const = 0;
  // Changes whether the proactive help setting is enabled.
  virtual void SetProactiveHelpSettingEnabled(bool enabled) = 0;

  // TODO(arbesser): Move this out of the platform delegate.
  // Returns whether the MSBB seetting is enabled.
  virtual bool GetMakeSearchesAndBrowsingBetterEnabled() const = 0;
  // Returns whether this is a custom tab or not.
  virtual bool GetIsCustomTab() const = 0;
  // Returns whether the tab was created by GSA or not.
  virtual bool GetIsTabCreatedByGSA() const = 0;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_PLATFORM_DELEGATE_H_
