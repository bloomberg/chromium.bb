// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOM_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOM_ACTION_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {
// An action to ask Chrome to wait for a DOM element to process next action.
class WaitForDomAction : public Action {
 public:
  explicit WaitForDomAction(const ActionProto& proto);
  ~WaitForDomAction() override;

  // Overrides Action:
  void ProcessAction(ActionDelegate* delegate,
                     ProcessActionCallback callback) override;

 private:
  void CheckElementExists(ActionDelegate* delegate,
                          int rounds,
                          ProcessActionCallback callback);
  void OnCheckElementExists(ActionDelegate* delegate,
                            int rounds,
                            ProcessActionCallback callback,
                            bool result);

  base::WeakPtrFactory<WaitForDomAction> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(WaitForDomAction);
};

}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOM_ACTION_H_
