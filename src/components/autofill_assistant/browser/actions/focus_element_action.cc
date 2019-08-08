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
  if (!focus_element.title().empty()) {
    delegate->SetStatusMessage(focus_element.title());
  }
  Selector selector = Selector(focus_element.element()).MustBeVisible();
  if (selector.empty()) {
    DVLOG(1) << __func__ << ": empty selector";
    UpdateProcessedAction(INVALID_SELECTOR);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  // Default value of 25%. This value should always be overriden
  // by backend.
  TopPadding top_padding{0.25, TopPadding::Unit::RATIO};
  switch (focus_element.top_padding().top_padding_case()) {
    case FocusElementProto::TopPadding::kPixels:
      top_padding = TopPadding(focus_element.top_padding().pixels(),
                               TopPadding::Unit::PIXELS);
      break;
    case FocusElementProto::TopPadding::kRatio:
      top_padding = TopPadding(focus_element.top_padding().ratio(),
                               TopPadding::Unit::RATIO);
      break;
    case FocusElementProto::TopPadding::TOP_PADDING_NOT_SET:
      // Default value set before switch.
      break;
  }

  delegate->ShortWaitForElement(
      selector,
      base::BindOnce(&FocusElementAction::OnWaitForElement,
                     weak_ptr_factory_.GetWeakPtr(), base::Unretained(delegate),
                     std::move(callback), selector, top_padding));
}

void FocusElementAction::OnWaitForElement(ActionDelegate* delegate,
                                          ProcessActionCallback callback,
                                          const Selector& selector,
                                          const TopPadding& top_padding,
                                          bool element_found) {
  if (!element_found) {
    UpdateProcessedAction(ELEMENT_RESOLUTION_FAILED);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  delegate->FocusElement(
      selector, top_padding,
      base::BindOnce(&FocusElementAction::OnFocusElement,
                     weak_ptr_factory_.GetWeakPtr(), base::Unretained(delegate),
                     std::move(callback)));
}

void FocusElementAction::OnFocusElement(ActionDelegate* delegate,
                                        ProcessActionCallback callback,
                                        const ClientStatus& status) {
  delegate->SetTouchableElementArea(
      proto().focus_element().touchable_element_area());
  UpdateProcessedAction(status);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
