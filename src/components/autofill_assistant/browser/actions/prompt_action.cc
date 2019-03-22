// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/prompt_action.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "url/gurl.h"

namespace autofill_assistant {

PromptAction::PromptAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {
  DCHECK(proto_.has_prompt());
}

PromptAction::~PromptAction() {}

void PromptAction::InternalProcessAction(ActionDelegate* delegate,
                                         ProcessActionCallback callback) {
  delegate->ShowStatusMessage(proto_.prompt().message());

  callback_ = std::move(callback);
  std::vector<UiController::Choice> choices;
  DCHECK_GT(proto_.prompt().choices_size(), 0);
  for (const auto& choice_proto : proto_.prompt().choices()) {
    if (choice_proto.name().empty())
      continue;

    choices.emplace_back();
    auto& choice = choices.back();
    choice.name = choice_proto.name();
    choice.highlight = choice_proto.highlight();
    choice_proto.SerializeToString(&choice.server_payload);
  }
  delegate->Choose(choices, base::BindOnce(&PromptAction::OnSuggestionChosen,
                                           weak_ptr_factory_.GetWeakPtr()));

  batch_element_checker_ = delegate->CreateBatchElementChecker();
  for (const auto& choice_proto : proto_.prompt().choices()) {
    if (choice_proto.element_exists().selectors_size() == 0)
      continue;

    std::string payload;
    choice_proto.SerializeToString(&payload);
    batch_element_checker_->AddElementCheck(
        kExistenceCheck, ExtractSelector(choice_proto.element_exists()),
        base::BindOnce(&PromptAction::OnElementExist,
                       weak_ptr_factory_.GetWeakPtr(), payload));
  }
  // Wait as long as necessary for one of the elements to show up. This is
  // cancelled by OnSuggestionChosen()
  batch_element_checker_->Run(
      base::TimeDelta::Max(),
      /* try_done= */
      base::BindRepeating(&PromptAction::OnElementChecksDone,
                          weak_ptr_factory_.GetWeakPtr(),
                          base::Unretained(delegate)),
      /* all_done= */ base::DoNothing());
}

void PromptAction::OnElementExist(const std::string& payload, bool exists) {
  if (exists)
    forced_payload_ = payload;

  // Calling ForceChoose is delayed until try_done, as it indirectly deletes
  // batch_element_checker_, which isn't supported from an element check
  // callback.
}

void PromptAction::OnElementChecksDone(ActionDelegate* delegate) {
  if (!forced_payload_.empty())
    delegate->ForceChoose(forced_payload_);
}

void PromptAction::OnSuggestionChosen(const std::string& payload) {
  batch_element_checker_.reset();
  PromptProto::Choice choice;
  if (!choice.ParseFromString(payload)) {
    DLOG(ERROR) << "Invalid result.";
    UpdateProcessedAction(OTHER_ACTION_STATUS);
  } else {
    UpdateProcessedAction(ACTION_APPLIED);
    std::swap(*processed_action_proto_->mutable_prompt_choice(), choice);
  }
  std::move(callback_).Run(std::move(processed_action_proto_));
}
}  // namespace autofill_assistant
