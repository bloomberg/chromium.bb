// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_MOCK_EXTERNAL_SCRIPT_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_MOCK_EXTERNAL_SCRIPT_CONTROLLER_H_

#include "base/callback_helpers.h"
#include "components/autofill_assistant/browser/public/external_script_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockExternalScriptController : public ExternalScriptController {
 public:
  MockExternalScriptController();
  ~MockExternalScriptController() override;

  MOCK_METHOD(void,
              StartScript,
              ((const base::flat_map<std::string, std::string>&),
               (base::OnceCallback<void(ScriptResult)>)),
              (override));
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_MOCK_EXTERNAL_SCRIPT_CONTROLLER_H_
