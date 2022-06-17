// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_AGGREGATOR_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_AGGREGATOR_H_

#include "base/callback_forward.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"

namespace device_signals {

struct UserContext;

class SignalsAggregator : public KeyedService {
 public:
  using GetSignalsCallback = base::OnceCallback<void(base::Value)>;

  ~SignalsAggregator() override = default;

  // Will asynchronously collect signals as defined in the `parameters`
  // dictionary, where keys represent the names of the signals to be collected
  // and values represent their collection parameters. Invokes `callback` with
  // the collected signals stored in a dictionary, where the keys are the
  // signal names and values are the collected values. If signal collection
  // failed, the value returned in the callback may be solely an error string.
  // Will use `user_context` to validate that the user has permissions to the
  // device's signals.
  // Currently only supports the collection of one signal (only one entry in
  // `parameter`).
  virtual void GetSignals(const UserContext& user_context,
                          base::Value::Dict parameters,
                          GetSignalsCallback callback) = 0;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_AGGREGATOR_H_
