// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/form_data_event.h"

#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"

namespace blink {

FormDataEvent::FormDataEvent(FormData& form_data)
    : Event(event_type_names::kFormdata, Bubbles::kYes, Cancelable::kNo),
      form_data_(form_data) {}

FormDataEvent* FormDataEvent::Create(FormData& form_data) {
  return MakeGarbageCollected<FormDataEvent>(form_data);
}

void FormDataEvent::Trace(Visitor* visitor) {
  visitor->Trace(form_data_);
  Event::Trace(visitor);
}

const AtomicString& FormDataEvent::InterfaceName() const {
  return event_interface_names::kFormDataEvent;
}

}  // namespace blink
