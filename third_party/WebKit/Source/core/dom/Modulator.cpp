// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Modulator.h"

#include "bindings/core/v8/ScriptState.h"

namespace blink {

Modulator* Modulator::from(LocalFrame* frame) {
  ScriptState* scriptState = ScriptState::forMainWorld(frame);
  if (!scriptState)
    return nullptr;
  // TODO(kouhei): setModulator in V8PerContextData when we land ModulatorImpl.
  return scriptState->perContextData()->modulator();
}

}  // namespace blink
