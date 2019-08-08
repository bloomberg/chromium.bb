// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_H_

#include <memory>
#include <string>

#include "components/autofill_assistant/browser/script_precondition.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

// Minimal information about a script necessary to display and run it.
struct ScriptHandle {
  ScriptHandle();
  ScriptHandle(const ScriptHandle& orig);
  ~ScriptHandle();

  ChipProto chip;
  std::string path;
  std::string initial_prompt;

  // When set to true this script can be run in 'autostart mode'. Script won't
  // be shown.
  bool autostart;

  // If set, the script might be run during WaitForDom actions with
  // allow_interrupt=true.
  bool interrupt;
};

// Script represents a sequence of actions.
struct Script {
  Script();
  ~Script();

  ScriptHandle handle;

  // Display priority of the script. Lowest number has highest priority, which
  // means a script with priority 0 should be displayed before a script with
  // priority 1.
  int priority;

  std::unique_ptr<ScriptPrecondition> precondition;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_H_
