// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/assistant_script_executor.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill_assistant/browser/assistant_protocol_utils.h"
#include "components/autofill_assistant/browser/assistant_service.h"
#include "components/autofill_assistant/browser/assistant_ui_controller.h"
#include "components/autofill_assistant/browser/assistant_web_controller.h"

namespace autofill_assistant {

AssistantScriptExecutor::AssistantScriptExecutor(
    AssistantScript* script,
    AssistantScriptExecutorDelegate* delegate)
    : script_(script), delegate_(delegate), weak_ptr_factory_(this) {
  DCHECK(script_);
  DCHECK(delegate_);
}
AssistantScriptExecutor::~AssistantScriptExecutor() {}

void AssistantScriptExecutor::Run(RunScriptCallback callback) {
  callback_ = std::move(callback);
  DCHECK(delegate_->GetAssistantService());

  delegate_->GetAssistantService()->GetAssistantActions(
      script_->path,
      base::BindOnce(&AssistantScriptExecutor::OnGetAssistantActions,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AssistantScriptExecutor::ShowStatusMessage(const std::string& message) {
  delegate_->GetAssistantUiController()->ShowStatusMessage(message);
}

void AssistantScriptExecutor::ClickElement(
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool)> callback) {
  delegate_->GetAssistantWebController()->ClickElement(selectors,
                                                       std::move(callback));
}

void AssistantScriptExecutor::ElementExists(
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool)> callback) {
  delegate_->GetAssistantWebController()->ElementExists(selectors,
                                                        std::move(callback));
}

void AssistantScriptExecutor::ChooseAddress(
    base::OnceCallback<void(const std::string&)> callback) {
  delegate_->GetAssistantUiController()->ChooseAddress(std::move(callback));
}

void AssistantScriptExecutor::FillAddressForm(
    const std::string& guid,
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool)> callback) {
  delegate_->GetAssistantWebController()->FillAddressForm(guid, selectors,
                                                          std::move(callback));
}

void AssistantScriptExecutor::ChooseCard(
    base::OnceCallback<void(const std::string&)> callback) {
  delegate_->GetAssistantUiController()->ChooseCard(std::move(callback));
}

void AssistantScriptExecutor::FillCardForm(
    const std::string& guid,
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool)> callback) {
  delegate_->GetAssistantWebController()->FillCardForm(guid, selectors,
                                                       std::move(callback));
}

ClientMemory* AssistantScriptExecutor::GetClientMemory() {
  return delegate_->GetClientMemory();
}

void AssistantScriptExecutor::OnGetAssistantActions(
    bool result,
    const std::string& response) {
  if (!result) {
    std::move(callback_).Run(false);
    return;
  }
  processed_actions_.clear();
  actions_.clear();

  bool parse_result = AssistantProtocolUtils::ParseActions(
      response, &last_server_payload_, &actions_);
  if (!parse_result) {
    std::move(callback_).Run(false);
    return;
  }

  if (actions_.empty()) {
    // Finished executing the script if there are no more actions.
    std::move(callback_).Run(true);
    return;
  }
  ProcessNextAction();
}

void AssistantScriptExecutor::ProcessNextAction() {
  if (actions_.empty()) {
    // Request more actions to execute.
    GetNextAssistantActions();
    return;
  }

  std::unique_ptr<Action> action = std::move(actions_.front());
  actions_.pop_front();
  int delay_ms = action->proto().action_delay_ms();
  if (delay_ms > 0) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AssistantScriptExecutor::ProcessAction,
                       weak_ptr_factory_.GetWeakPtr(), std::move(action)),
        base::TimeDelta::FromMilliseconds(delay_ms));
  } else {
    ProcessAction(std::move(action));
  }
}

void AssistantScriptExecutor::ProcessAction(std::unique_ptr<Action> action) {
  action->ProcessAction(
      this, base::BindOnce(&AssistantScriptExecutor::OnProcessedAction,
                           weak_ptr_factory_.GetWeakPtr(), std::move(action)));
}

void AssistantScriptExecutor::GetNextAssistantActions() {
  delegate_->GetAssistantService()->GetNextAssistantActions(
      last_server_payload_, processed_actions_,
      base::BindOnce(&AssistantScriptExecutor::OnGetAssistantActions,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AssistantScriptExecutor::OnProcessedAction(std::unique_ptr<Action> action,
                                                bool success) {
  processed_actions_.emplace_back();
  ProcessedActionProto* proto = &processed_actions_.back();
  proto->mutable_action()->MergeFrom(action->proto());
  proto->set_status(success ? ProcessedActionStatus::ACTION_APPLIED
                            : ProcessedActionStatus::OTHER_ACTION_STATUS);
  if (!success) {
    // Report error immediately, interrupting action processing.
    GetNextAssistantActions();
    return;
  }
  ProcessNextAction();
}

}  // namespace autofill_assistant
