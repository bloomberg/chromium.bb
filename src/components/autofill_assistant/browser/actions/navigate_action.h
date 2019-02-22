// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_NAVIGATE_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_NAVIGATE_ACTION_H_

#include <string>

#include "base/macros.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {
// An action to navigate to a given web page.
class NavigateAction : public Action {
 public:
  explicit NavigateAction(const ActionProto& proto);
  ~NavigateAction() override;

  // Overrides Action:
  void ProcessAction(ActionDelegate* delegate,
                     ProcessActionCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NavigateAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_NAVIGATE_ACTION_H_
