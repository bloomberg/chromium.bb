// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/tell_action.h"

#include <utility>

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

TellAction::TellAction(const ActionProto& proto) : Action(proto) {
  DCHECK(proto_.has_tell());
}

TellAction::~TellAction() {}

void TellAction::ProcessAction(ActionDelegate* delegate,
                               ProcessActionCallback callback) {
  // tell.message in the proto is localized.
  delegate->ShowStatusMessage(proto_.tell().message());
  std::move(callback).Run(true);
}

}  // namespace autofill_assistant.
