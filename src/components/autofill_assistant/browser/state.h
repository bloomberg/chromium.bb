// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STATE_H_

#include <ostream>

namespace autofill_assistant {

// High-level states the Autofill Assistant can be in.
enum class AutofillAssistantState {
  // Autofill assistant is not doing or showing anything.
  // Initial state.
  INACTIVE = 0,

  // Autofill assistant is waiting for an autostart script.
  //
  // Status message, progress and details are initialized to useful values.
  STARTING,

  // Autofill assistant is manipulating the website.
  //
  // Status message, progress and details kept up-to-date by the running
  // script.
  RUNNING,

  // Autofill assistant is waiting for the user to make a choice.
  //
  // Status message is initialized to a useful value. Chips are set and might be
  // empty. A touchable area must be configured. The user might be filling in
  // the data for a payment request.
  PROMPT,

  // Autofill assistant is waiting for the user to make the first choice.
  //
  // When autostartable scripts are expected, this is only triggered as a
  // fallback if there are non-autostartable scripts to choose from instead.
  AUTOSTART_FALLBACK_PROMPT,

  // Autofill assistant is expecting a modal dialog, such as the one asking for
  // CVC.
  MODAL_DIALOG,

  // Autofill assistant is stopped, but still visible to the user.
  //
  // Status message contains the final message.
  STOPPED
};

inline std::ostream& operator<<(std::ostream& out,
                                const AutofillAssistantState& state) {
#ifdef NDEBUG
  // Non-debugging builds write the enum number.
  out << static_cast<int>(state);
  return out;
#else
  // Debugging builds write a string representation of |state|.
  switch (state) {
    case AutofillAssistantState::INACTIVE:
      out << "INACTIVE";
      break;
    case AutofillAssistantState::STARTING:
      out << "STARTING";
      break;
    case AutofillAssistantState::RUNNING:
      out << "RUNNING";
      break;
    case AutofillAssistantState::PROMPT:
      out << "PROMPT";
      break;
    case AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT:
      out << "AUTOSTART_FALLBACK_PROMPT";
      break;
    case AutofillAssistantState::MODAL_DIALOG:
      out << "MODAL_DIALOG";
      break;
    case AutofillAssistantState::STOPPED:
      out << "STOPPED";
      break;
      // Intentionally no default case to make compilation fail if a new value
      // was added to the enum but not to this list.
  }
  return out;
#endif  // NDEBUG
}

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STATE_H_
