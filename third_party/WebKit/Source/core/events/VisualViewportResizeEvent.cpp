// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/VisualViewportResizeEvent.h"

#include "core/frame/UseCounter.h"

namespace blink {

VisualViewportResizeEvent::~VisualViewportResizeEvent() = default;

VisualViewportResizeEvent::VisualViewportResizeEvent()
    : Event(EventTypeNames::resize,
            false,
            false)  // non-bubbling non-cancellable
{}

void VisualViewportResizeEvent::DoneDispatchingEventAtCurrentTarget() {
  UseCounter::Count(currentTarget()->GetExecutionContext(),
                    WebFeature::kVisualViewportResizeFired);
}

}  // namespace blink
