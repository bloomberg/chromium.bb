// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/upload_dom_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

UploadDomAction::UploadDomAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {
  DCHECK(proto_.has_upload_dom());
}

UploadDomAction::~UploadDomAction() {}

void UploadDomAction::ProcessAction(ActionDelegate* delegate,
                                    ProcessActionCallback callback) {
  processed_action_proto_ = std::make_unique<ProcessedActionProto>();

  std::vector<std::string> selectors;
  for (const auto& selector : proto_.upload_dom().tree_root().selectors()) {
    selectors.emplace_back(selector);
  }
  DCHECK(!selectors.empty());
  delegate->GetOuterHtml(
      selectors,
      base::BindOnce(&UploadDomAction::OnGetOuterHtml,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UploadDomAction::OnGetOuterHtml(ProcessActionCallback callback,
                                     bool successful,
                                     const std::string& outer_html) {
  // TODO(crbug.com/806868): Distinguish element not found from other error and
  // report them as ELEMENT_RESOLUTION_FAILED.
  if (!successful) {
    UpdateProcessedAction(OTHER_ACTION_STATUS);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  processed_action_proto_->set_html_source(outer_html);
  UpdateProcessedAction(ACTION_APPLIED);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
