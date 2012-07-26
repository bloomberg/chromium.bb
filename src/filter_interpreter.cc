// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/filter_interpreter.h"

#include <base/values.h>

namespace gestures {

DictionaryValue* FilterInterpreter::EncodeCommonInfo() {
  DictionaryValue *root = Interpreter::EncodeCommonInfo();
  root->Set(ActivityLog::kKeyNext, next_->EncodeCommonInfo());
  return root;
}

void FilterInterpreter::Clear() {
  log_.Clear();
  next_->Clear();
}
}  // namespace gestures
