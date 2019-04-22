// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SET_FORM_FIELD_VALUE_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SET_FORM_FIELD_VALUE_ACTION_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {

// An action to set the value of a form input element.
class SetFormFieldValueAction : public Action {
 public:
  explicit SetFormFieldValueAction(const ActionProto& proto);
  ~SetFormFieldValueAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ActionDelegate* delegate,
                             ProcessActionCallback callback) override;

  void OnWaitForElement(ActionDelegate* delegate,
                        ProcessActionCallback callback,
                        const Selector& selector,
                        bool element_found);

  void OnGetFieldValue(ActionDelegate* delegate,
                       ProcessActionCallback callback,
                       const Selector& selector,
                       int next,
                       bool status,
                       const std::string& value);

  void OnSetFieldValue(ActionDelegate* delegate,
                       ProcessActionCallback callback,
                       const Selector& selector,
                       int next,
                       const ClientStatus& status);

  void OnSetFieldValueAndCheckFallback(ActionDelegate* delegate,
                                       ProcessActionCallback callback,
                                       const Selector& selector,
                                       int next,
                                       const ClientStatus& status);

  base::WeakPtrFactory<SetFormFieldValueAction> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SetFormFieldValueAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SET_FORM_FIELD_VALUE_ACTION_H_
