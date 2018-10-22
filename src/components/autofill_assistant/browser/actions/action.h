// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_H_

#include "base/callback_forward.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

class ActionDelegate;

// An action that performs a single step of a script on the website.
class Action {
 public:
  virtual ~Action() = default;

  // Callback returns whether process action is succeed or not.
  // Delegate should outlive this object.
  using ProcessActionCallback = base::OnceCallback<void(bool)>;
  virtual void ProcessAction(ActionDelegate* delegate,
                             ProcessActionCallback callback) = 0;

  const ActionProto& proto() const { return proto_; }

 protected:
  explicit Action(const ActionProto& proto);

  const ActionProto proto_;
};

}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_H_
