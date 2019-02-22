// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_STOP_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_STOP_ACTION_H_

#include <string>

#include "base/macros.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {
// An action to stop Autofill Assistant.
class StopAction : public Action {
 public:
  explicit StopAction(const ActionProto& proto);
  ~StopAction() override;

  // Overrides Action:
  void ProcessAction(ActionDelegate* delegate,
                     ProcessActionCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(StopAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_STOP_ACTION_H_
