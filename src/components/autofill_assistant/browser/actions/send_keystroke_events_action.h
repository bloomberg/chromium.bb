// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SEND_KEYSTROKE_EVENTS_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SEND_KEYSTROKE_EVENTS_ACTION_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/web/element_finder.h"

namespace autofill_assistant {

// An action performs keystroke events based on a Text value delivered by the
// backend who's content may only be known to the client.
class SendKeystrokeEventsAction : public Action {
 public:
  SendKeystrokeEventsAction(ActionDelegate* delegate, const ActionProto& proto);
  ~SendKeystrokeEventsAction() override;

  SendKeystrokeEventsAction(const SendKeystrokeEventsAction&) = delete;
  SendKeystrokeEventsAction& operator=(const SendKeystrokeEventsAction&) =
      delete;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void OnGetPasswordLastTimeUsed(
      const absl::optional<base::Time> last_time_used);

  void ResolveTextValue();
  void OnResolveTextValue(const ClientStatus& status, const std::string& value);

  void EndAction(const ClientStatus& status);

  ElementFinder::Result element_;
  ProcessActionCallback callback_;

  base::WeakPtrFactory<SendKeystrokeEventsAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SEND_KEYSTROKE_EVENTS_ACTION_H_
