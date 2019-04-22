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
  base::TimeDelta max_wait_time = kDefaultCheckDuration;
  int timeout_ms = proto_.wait_for_dom().timeout_ms();
  if (timeout_ms > 0)
    max_wait_time = base::TimeDelta::FromMilliseconds(timeout_ms);

  Selector wait_until = Selector(proto_.wait_for_dom().wait_until());
  Selector wait_while = Selector(proto_.wait_for_dom().wait_while());
  ActionDelegate::SelectorPredicate selector_predicate;
  Selector selector;
  if (!wait_until.empty()) {
    // wait until the selector matches something
    selector_predicate = ActionDelegate::SelectorPredicate::kMatches;
    selector = wait_until;
  } else if (!wait_while.empty()) {
    // wait as long as the selector matches something
    selector_predicate = ActionDelegate::SelectorPredicate::kDoesntMatch;
    selector = wait_while;
  } else {
    DVLOG(1) << __func__ << ": no selector specified for WaitForDom";
    OnCheckDone(std::move(callback), INVALID_SELECTOR);
    return;
  }

  delegate->WaitForDom(
      max_wait_time, proto_.wait_for_dom().allow_interrupt(),
      selector_predicate, selector,
      base::BindOnce(&WaitForDomAction::OnCheckDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WaitForDomAction::OnCheckDone(ProcessActionCallback callback,
                                   ProcessedActionStatusProto status) {
  UpdateProcessedAction(status);
  std::move(callback).Run(std::move(processed_action_proto_));
}
}  // namespace autofill_assistant
