// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_setup_controller.h"

#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/assistant/util/i18n_util.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/cpp/features.h"

namespace {

constexpr char kGSuiteAdministratorInstructionsUrl[] =
    "https://support.google.com/a/answer/6304876";

}  // namespace

namespace ash {

AssistantSetupController::AssistantSetupController(
    AssistantControllerImpl* assistant_controller)
    : assistant_controller_(assistant_controller) {
  assistant_controller_observer_.Add(AssistantController::Get());
}

AssistantSetupController::~AssistantSetupController() = default;

void AssistantSetupController::OnAssistantControllerConstructed() {
  assistant_controller_->view_delegate()->AddObserver(this);
}

void AssistantSetupController::OnAssistantControllerDestroying() {
  assistant_controller_->view_delegate()->RemoveObserver(this);
}

void AssistantSetupController::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  if (type != assistant::util::DeepLinkType::kOnboarding)
    return;

  base::Optional<bool> relaunch = assistant::util::GetDeepLinkParamAsBool(
      params, assistant::util::DeepLinkParam::kRelaunch);

  StartOnboarding(relaunch.value_or(false));
}

void AssistantSetupController::OnOptInButtonPressed() {
  using chromeos::assistant::prefs::ConsentStatus;
  if (AssistantState::Get()->consent_status().value_or(
          ConsentStatus::kUnknown) == ConsentStatus::kUnauthorized) {
    AssistantController::Get()->OpenUrl(assistant::util::CreateLocalizedGURL(
        kGSuiteAdministratorInstructionsUrl));
  } else {
    StartOnboarding(/*relaunch=*/true);
  }
}

void AssistantSetupController::StartOnboarding(bool relaunch, FlowType type) {
  auto* assistant_setup = AssistantSetup::GetInstance();
  if (!assistant_setup)
    return;

  AssistantUiController::Get()->CloseUi(
      chromeos::assistant::mojom::AssistantExitPoint::kSetup);

  assistant_setup->StartAssistantOptInFlow(
      type, base::BindOnce(&AssistantSetupController::OnOptInFlowFinished,
                           weak_ptr_factory_.GetWeakPtr(), relaunch));
}

void AssistantSetupController::OnOptInFlowFinished(bool relaunch,
                                                   bool completed) {
  if (relaunch && completed) {
    AssistantUiController::Get()->ShowUi(
        chromeos::assistant::mojom::AssistantEntryPoint::kSetup);
  }
}

}  // namespace ash
