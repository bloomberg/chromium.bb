// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/wait_for_dom_action.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace {
static constexpr base::TimeDelta kDefaultCheckDuration =
    base::TimeDelta::FromSeconds(15);
}  // namespace

namespace autofill_assistant {

WaitForDomAction::WaitForDomAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {}

WaitForDomAction::~WaitForDomAction() {}

void WaitForDomAction::InternalProcessAction(ActionDelegate* delegate,
                                             ProcessActionCallback callback) {
  DCHECK_GT(proto_.wait_for_dom().selectors_size(), 0);
  Selector a_selector;
  for (const auto& selector : proto_.wait_for_dom().selectors()) {
    a_selector.selectors.emplace_back(selector);
  }

  base::TimeDelta max_wait_time = kDefaultCheckDuration;
  int timeout_ms = proto_.wait_for_dom().timeout_ms();
  if (timeout_ms > 0)
    max_wait_time = base::TimeDelta::FromMilliseconds(timeout_ms);

  delegate->WaitForElementVisible(
      max_wait_time, proto_.wait_for_dom().allow_interrupt(), a_selector,
      base::BindOnce(&WaitForDomAction::OnCheckDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WaitForDomAction::OnCheckDone(ProcessActionCallback callback,
                                   bool element_found) {
  UpdateProcessedAction(element_found ? ACTION_APPLIED
                                      : ELEMENT_RESOLUTION_FAILED);
  std::move(callback).Run(std::move(processed_action_proto_));
}
}  // namespace autofill_assistant
