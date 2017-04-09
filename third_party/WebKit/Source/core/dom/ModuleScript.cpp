// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ModuleScript.h"

namespace blink {

void ModuleScript::SetInstantiationError(v8::Isolate* isolate,
                                         v8::Local<v8::Value> error) {
  DCHECK_EQ(instantiation_state_, ModuleInstantiationState::kUninstantiated);
  instantiation_state_ = ModuleInstantiationState::kErrored;

  DCHECK(!error.IsEmpty());
  instantiation_error_.Set(isolate, error);
}

void ModuleScript::SetInstantiationSuccess() {
  DCHECK_EQ(instantiation_state_, ModuleInstantiationState::kUninstantiated);
  instantiation_state_ = ModuleInstantiationState::kInstantiated;
}

DEFINE_TRACE(ModuleScript) {}
DEFINE_TRACE_WRAPPERS(ModuleScript) {
  visitor->TraceWrappers(instantiation_error_);
}

}  // namespace blink
