// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_SCRIPT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_SCRIPT_H_

#include <string>

#include "components/autofill_assistant/browser/assistant_script_precondition.h"

namespace autofill_assistant {

// Assistant script represents a sequence of client actions.
struct AssistantScript {
  AssistantScript();
  ~AssistantScript();

  std::string name;
  std::string path;
  std::unique_ptr<AssistantScriptPrecondition> precondition;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_SCRIPT_H_