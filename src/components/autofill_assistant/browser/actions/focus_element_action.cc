// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/focus_element_action.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

FocusElementAction::FocusElementAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {
  DCHECK(proto_.has_focus_element());
}

FocusElementAction::~FocusElementAction() {}

void FocusElementAction::InternalProcessAction(ActionDelegate* delegate,
                                               ProcessActionCallback callback) {
  const FocusElementProto& focus_element = proto_.focus_element();
  DCHECK_GT(focus_element.element().selectors_size(), 0);

  if (!focus_element.title().empty()) {
    delegate->SetStatusMessage(focus_element.title());
  }
  delegate->ShortWaitForElementExist(
      Selector(focus_element.element()),
      base::BindOnce(&FocusElementAction::OnWaitForElement,
                     weak_ptr_factory_.GetWeakPtr(), base::Unretained(delegate),
                     std::move(callback)));
}

void FocusElementAction::OnWaitForElement(ActionDelegate* delegate,
                                          ProcessActionCallback callback,
                                          bool element_found) {
  if (!element_found) {
    UpdateProcessedAction(ELEMENT_RESOLUTION_FAILED);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  delegate->FocusElement(
      Selector(proto_.focus_element().element()),
      base::BindOnce(&FocusElementAction::OnFocusElement,
                     weak_ptr_factory_.GetWeakPtr(), base::Unretained(delegate),
                     std::move(callback)));
}

void FocusElementAction::OnFocusElement(ActionDelegate* delegate,
                                        ProcessActionCallback callback,
                                        bool status) {
  delegate->SetTouchableElementArea(
      proto().focus_element().touchable_element_area());
  UpdateProcessedAction(status ? ACTION_APPLIED : OTHER_ACTION_STATUS);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
