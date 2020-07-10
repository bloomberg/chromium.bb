/*
 * Copyright (C) 2014 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/events/touch_event_context.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/touch_event.h"
#include "third_party/blink/renderer/core/input/touch_list.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

TouchEventContext* TouchEventContext::Create() {
  return MakeGarbageCollected<TouchEventContext>();
}

TouchEventContext::TouchEventContext()
    : touches_(MakeGarbageCollected<TouchList>()),
      target_touches_(MakeGarbageCollected<TouchList>()),
      changed_touches_(MakeGarbageCollected<TouchList>()) {}

void TouchEventContext::HandleLocalEvents(Event& event) const {
  DCHECK(event.IsTouchEvent());
  TouchEvent& touch_event = ToTouchEvent(event);
  touch_event.SetTouches(touches_);
  touch_event.SetTargetTouches(target_touches_);
  touch_event.SetChangedTouches(changed_touches_);
}

void TouchEventContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(touches_);
  visitor->Trace(target_touches_);
  visitor->Trace(changed_touches_);
}

}  // namespace blink
