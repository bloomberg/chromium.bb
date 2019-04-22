// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_input_source_event.h"

namespace blink {

XRInputSourceEvent::XRInputSourceEvent() {}

XRInputSourceEvent::XRInputSourceEvent(const AtomicString& type,
                                       XRFrame* frame,
                                       XRInputSource* input_source)
    : Event(type, Bubbles::kYes, Cancelable::kNo),
      frame_(frame),
      input_source_(input_source) {}

XRInputSourceEvent::XRInputSourceEvent(
    const AtomicString& type,
    const XRInputSourceEventInit* initializer)
    : Event(type, initializer) {
  if (initializer->hasFrame())
    frame_ = initializer->frame();
  if (initializer->hasInputSource())
    input_source_ = initializer->inputSource();
  if (initializer->hasButtonIndex())
    button_index_ = initializer->buttonIndex();
}

XRInputSourceEvent::~XRInputSourceEvent() {}

const AtomicString& XRInputSourceEvent::InterfaceName() const {
  return event_interface_names::kXRInputSourceEvent;
}

int32_t XRInputSourceEvent::buttonIndex(bool& is_null) const {
  is_null = !button_index_;
  return button_index_ ? *button_index_ : 0;
}

void XRInputSourceEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(input_source_);
  Event::Trace(visitor);
}

}  // namespace blink
