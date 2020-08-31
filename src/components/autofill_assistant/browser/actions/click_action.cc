// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/click_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

ClickAction::ClickAction(ActionDelegate* delegate, const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_click());
  click_type_ = proto.click().click_type();
  if (click_type_ == ClickType::NOT_SET) {
    // default: TAP
    click_type_ = ClickType::TAP;
  }
}

ClickAction::~ClickAction() {}

void ClickAction::InternalProcessAction(ProcessActionCallback callback) {
  Selector selector =
      Selector(proto_.click().element_to_click()).MustBeVisible();
  if (selector.empty()) {
    VLOG(1) << __func__ << ": empty selector";
    UpdateProcessedAction(INVALID_SELECTOR);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }
  delegate_->ShortWaitForElement(selector,
                                 base::BindOnce(&ClickAction::OnWaitForElement,
                                                weak_ptr_factory_.GetWeakPtr(),
                                                std::move(callback), selector));
}

void ClickAction::OnWaitForElement(ProcessActionCallback callback,
                                   const Selector& selector,
                                   const ClientStatus& element_status) {
  if (!element_status.ok()) {
    UpdateProcessedAction(element_status.proto_status());
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  delegate_->ClickOrTapElement(
      selector, click_type_,
      base::BindOnce(&::autofill_assistant::ClickAction::OnClick,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ClickAction::OnClick(ProcessActionCallback callback,
                          const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
