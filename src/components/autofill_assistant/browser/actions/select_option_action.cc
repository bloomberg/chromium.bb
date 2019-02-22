// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/select_option_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

SelectOptionAction::SelectOptionAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {
  DCHECK(proto_.has_select_option());
}

SelectOptionAction::~SelectOptionAction() {}

void SelectOptionAction::ProcessAction(ActionDelegate* delegate,
                                       ProcessActionCallback callback) {
  processed_action_proto_ = std::make_unique<ProcessedActionProto>();
  const SelectOptionProto& select_option = proto_.select_option();

  // A non prefilled |select_option| is not supported.
  DCHECK(select_option.has_selected_option());
  std::vector<std::string> selectors;
  for (const auto& selector : select_option.element().selectors()) {
    selectors.emplace_back(selector);
  }
  DCHECK(!selectors.empty());

  delegate->SelectOption(
      selectors, select_option.selected_option(),
      base::BindOnce(&::autofill_assistant::SelectOptionAction::OnSelectOption,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SelectOptionAction::OnSelectOption(ProcessActionCallback callback,
                                        bool status) {
  // TODO(crbug.com/806868): Distinguish element not found from other error and
  // report them as ELEMENT_RESOLUTION_FAILED.
  UpdateProcessedAction(status ? ACTION_APPLIED : OTHER_ACTION_STATUS);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
