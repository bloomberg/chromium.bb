// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/window_agent.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_stack.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"

namespace blink {

WindowAgent::WindowAgent(v8::Isolate* isolate) : Agent(isolate) {}

WindowAgent::~WindowAgent() = default;

void WindowAgent::Trace(Visitor* visitor) {
  Agent::Trace(visitor);
}

}  // namespace blink
