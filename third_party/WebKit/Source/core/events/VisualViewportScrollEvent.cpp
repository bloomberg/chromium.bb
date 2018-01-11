// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/VisualViewportScrollEvent.h"

#include "core/frame/UseCounter.h"

namespace blink {

VisualViewportScrollEvent::~VisualViewportScrollEvent() = default;

VisualViewportScrollEvent::VisualViewportScrollEvent()
    : Event(EventTypeNames::scroll,
            false,
            false)  // non-bubbling non-cancellable
{}

void VisualViewportScrollEvent::DoneDispatchingEventAtCurrentTarget() {
  UseCounter::Count(currentTarget()->GetExecutionContext(),
                    WebFeature::kVisualViewportScrollFired);
}

}  // namespace blink
