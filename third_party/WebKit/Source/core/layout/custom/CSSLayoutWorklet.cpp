// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/custom/CSSLayoutWorklet.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/layout/custom/LayoutWorklet.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

// static
Worklet* CSSLayoutWorklet::layoutWorklet(ScriptState* script_state) {
  return LayoutWorklet::From(*ToLocalDOMWindow(script_state->GetContext()));
}

}  // namespace blink
