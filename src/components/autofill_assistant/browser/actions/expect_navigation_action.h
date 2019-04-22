// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_EXPECT_NAVIGATION_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_EXPECT_NAVIGATION_ACTION_H_

#include <string>

#include "base/macros.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {

class ExpectNavigationAction : public Action {
 public:
  explicit ExpectNavigationAction(const ActionProto& proto);
  ~ExpectNavigationAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ActionDelegate* delegate,
                             ProcessActionCallback callback) override;

  DISALLOW_COPY_AND_ASSIGN(ExpectNavigationAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_EXPECT_NAVIGATION_ACTION_H_
