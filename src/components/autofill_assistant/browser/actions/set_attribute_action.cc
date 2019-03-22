// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/set_attribute_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

SetAttributeAction::SetAttributeAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {
  DCHECK_GT(proto_.set_attribute().element().selectors_size(), 0);
  DCHECK_GT(proto_.set_attribute().attribute_size(), 0);
}

SetAttributeAction::~SetAttributeAction() {}

void SetAttributeAction::InternalProcessAction(ActionDelegate* delegate,
                                               ProcessActionCallback callback) {
  delegate->ShortWaitForElementExist(
      ExtractSelector(proto_.set_attribute().element()),
      base::BindOnce(&SetAttributeAction::OnWaitForElement,
                     weak_ptr_factory_.GetWeakPtr(), base::Unretained(delegate),
                     std::move(callback)));
}

void SetAttributeAction::OnWaitForElement(ActionDelegate* delegate,
                                          ProcessActionCallback callback,
                                          bool element_found) {
  if (!element_found) {
    UpdateProcessedAction(ELEMENT_RESOLUTION_FAILED);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  delegate->SetAttribute(
      ExtractSelector(proto_.set_attribute().element()),
      ExtractVector(proto_.set_attribute().attribute()),
      proto_.set_attribute().value(),
      base::BindOnce(&SetAttributeAction::OnSetAttribute,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SetAttributeAction::OnSetAttribute(ProcessActionCallback callback,
                                        bool status) {
  UpdateProcessedAction(status ? ACTION_APPLIED : OTHER_ACTION_STATUS);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
