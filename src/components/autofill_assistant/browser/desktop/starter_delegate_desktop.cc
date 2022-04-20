// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/desktop/starter_delegate_desktop.h"

#include "base/notreached.h"
#include "base/time/default_tick_clock.h"
#include "components/autofill_assistant/browser/public/runtime_manager_impl.h"
#include "components/autofill_assistant/browser/script_parameters.h"
#include "components/autofill_assistant/browser/website_login_manager_impl.h"
#include "components/version_info/channel.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

namespace autofill_assistant {

StarterDelegateDesktop::StarterDelegateDesktop(
    content::WebContents* web_contents)
    : content::WebContentsUserData<StarterDelegateDesktop>(*web_contents) {}

StarterDelegateDesktop::~StarterDelegateDesktop() = default;

std::unique_ptr<TriggerScriptCoordinator::UiDelegate>
StarterDelegateDesktop::CreateTriggerScriptUiDelegate() {
  return nullptr;
}

std::unique_ptr<ServiceRequestSender>
StarterDelegateDesktop::GetTriggerScriptRequestSenderToInject() {
  return nullptr;
}

WebsiteLoginManager* StarterDelegateDesktop::GetWebsiteLoginManager() const {
  return nullptr;
}

version_info::Channel StarterDelegateDesktop::GetChannel() const {
  // TODO(b/201964911): Inject on instantiation.
  return version_info::Channel::DEV;
}

bool StarterDelegateDesktop::GetFeatureModuleInstalled() const {
  return true;
}

void StarterDelegateDesktop::InstallFeatureModule(
    bool show_ui,
    base::OnceCallback<void(Metrics::FeatureModuleInstallation result)>
        callback) {
  NOTREACHED();
}

bool StarterDelegateDesktop::GetIsFirstTimeUser() const {
  NOTREACHED();
  return false;
}

void StarterDelegateDesktop::SetIsFirstTimeUser(bool first_time_user) {
  NOTREACHED();
}

bool StarterDelegateDesktop::GetOnboardingAccepted() const {
  // For headless runs, the consent is gated externally. At this point we assume
  // it has been accepted.
  return true;
}

void StarterDelegateDesktop::SetOnboardingAccepted(bool accepted) {
  // The user should not be able to accept the onboarding through headless.
}

void StarterDelegateDesktop::ShowOnboarding(
    bool use_dialog_onboarding,
    const TriggerContext& trigger_context,
    base::OnceCallback<void(bool shown, OnboardingResult result)> callback) {
  NOTREACHED();
}

void StarterDelegateDesktop::HideOnboarding() {
  NOTREACHED();
}

bool StarterDelegateDesktop::GetProactiveHelpSettingEnabled() const {
  // Only relevant for trigger scripts, which don't exist in headless.
  return false;
}

void StarterDelegateDesktop::SetProactiveHelpSettingEnabled(bool enabled) {
  NOTREACHED();
}

bool StarterDelegateDesktop::GetMakeSearchesAndBrowsingBetterEnabled() const {
  // Only relevant for trigger scripts, which don't exist in headless.
  return false;
}

bool StarterDelegateDesktop::GetIsLoggedIn() {
  // Only relevant for trigger scripts, which don't exist in headless.
  return false;
}

bool StarterDelegateDesktop::GetIsCustomTab() const {
  return false;
}

bool StarterDelegateDesktop::GetIsWebLayer() const {
  return false;
}

bool StarterDelegateDesktop::GetIsTabCreatedByGSA() const {
  return false;
}

std::unique_ptr<AssistantFieldTrialUtil>
StarterDelegateDesktop::CreateFieldTrialUtil() {
  // TODO(b/201964911): Create a field trial util.
  return nullptr;
}

void StarterDelegateDesktop::StartScriptDefaultUi(
    GURL url,
    std::unique_ptr<TriggerContext> trigger_context,
    const absl::optional<TriggerScriptProto>& unused_trigger_script) {
  // On desktop there is no default UI, so this is not supported.
  NOTREACHED();
}

bool StarterDelegateDesktop::IsRegularScriptRunning() const {
  // TODO(b/201964911): rework how we check for running scripts.
  return false;
}

bool StarterDelegateDesktop::IsRegularScriptVisible() const {
  return false;
}

bool StarterDelegateDesktop::IsAttached() {
  return true;
}

base::WeakPtr<StarterPlatformDelegate> StarterDelegateDesktop::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(StarterDelegateDesktop);

}  // namespace autofill_assistant
