// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/focus_element_action.h"

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

FocusElementAction::FocusElementAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {
  DCHECK(proto_.has_focus_element());
}

FocusElementAction::~FocusElementAction() {}

void FocusElementAction::ProcessAction(ActionDelegate* delegate,
                                       ProcessActionCallback callback) {
  processed_action_proto_ = std::make_unique<ProcessedActionProto>();
  const FocusElementProto& focus_element = proto_.focus_element();

  std::vector<std::string> selectors;
  for (const auto& selector : focus_element.element().selectors()) {
    selectors.emplace_back(selector);
  }
  DCHECK(!selectors.empty());

  if (!focus_element.title().empty()) {
    delegate->ShowStatusMessage(focus_element.title());
  }
  delegate->FocusElement(
      selectors,
      base::BindOnce(&FocusElementAction::OnFocusElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FocusElementAction::OnFocusElement(ProcessActionCallback callback,
                                        bool status) {
  // TODO(crbug.com/806868): Distinguish element not found from other error and
  // report them as ELEMENT_RESOLUTION_FAILED.
  UpdateProcessedAction(status ? ACTION_APPLIED : OTHER_ACTION_STATUS);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
