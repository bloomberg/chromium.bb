// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_JS_FLOW_EXECUTOR_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_JS_FLOW_EXECUTOR_H_

#include <memory>
#include <string>
#include "base/callback_forward.h"
#include "base/values.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

// Executes a JS flow. The flow may request additional native actions to be
// performed by its delegate.
class JsFlowExecutor {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Asks the delegate to run |native_action| and call |finished_callback|
    // when done. The contents of |native_action| depend on the JS script, but
    // will typically be a serialized protobuffer.
    virtual void RunNativeAction(
        std::unique_ptr<base::Value> native_action,
        base::OnceCallback<void(const ClientStatus& result_status,
                                std::unique_ptr<base::Value> result_value)>
            finished_callback) = 0;
  };

  virtual ~JsFlowExecutor() = default;

  // Runs the specified JS flow. Refer to the specific implementation for more
  // details.
  virtual void Start(const std::string& js_flow,
                     base::OnceCallback<void(const ClientStatus&,
                                             std::unique_ptr<base::Value>)>
                         result_callback) = 0;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_JS_FLOW_EXECUTOR_H_
