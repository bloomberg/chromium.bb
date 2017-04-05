// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ModuleScript.h"

namespace blink {

void ModuleScript::setInstantiationError(v8::Isolate* isolate,
                                         v8::Local<v8::Value> error) {
  DCHECK_EQ(m_instantiationState, ModuleInstantiationState::Uninstantiated);
  m_instantiationState = ModuleInstantiationState::Errored;

  DCHECK(!error.IsEmpty());
  m_instantiationError.set(isolate, error);
}

void ModuleScript::setInstantiationSuccess() {
  DCHECK_EQ(m_instantiationState, ModuleInstantiationState::Uninstantiated);
  m_instantiationState = ModuleInstantiationState::Instantiated;
}

DEFINE_TRACE(ModuleScript) {}
DEFINE_TRACE_WRAPPERS(ModuleScript) {
  visitor->traceWrappers(m_instantiationError);
}

}  // namespace blink
