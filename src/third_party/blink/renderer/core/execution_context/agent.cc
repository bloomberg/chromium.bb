// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/agent.h"

#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"

namespace blink {

Agent::Agent(v8::Isolate* isolate)
    : event_loop_(base::AdoptRef(new scheduler::EventLoop(isolate))) {}

Agent::~Agent() = default;

void Agent::Trace(Visitor* visitor) {}

}  // namespace blink
