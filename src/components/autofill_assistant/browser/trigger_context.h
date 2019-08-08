// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_CONTEXT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_CONTEXT_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"

namespace autofill_assistant {

// A trigger context for data provided by callers.
struct TriggerContext {
  // Script parameters provided by the caller.
  std::map<std::string, std::string> script_parameters;

  // Experiment ids to be passed to the backend in requests. They may also be
  // used to change client behavior.
  std::string experiment_ids;

  TriggerContext();
  TriggerContext(std::map<std::string, std::string> params, std::string exp);
  TriggerContext(const TriggerContext& context);
  ~TriggerContext();
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_CONTEXT_H_
