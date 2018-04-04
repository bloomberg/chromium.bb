// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/forms/FormDataEvent.h"

#include "core/html/forms/FormData.h"

namespace blink {

FormDataEvent::FormDataEvent(FormData& form_data)
    : Event(EventTypeNames::formdata, Bubbles::kYes, Cancelable::kNo),
      form_data_(form_data) {}

FormDataEvent* FormDataEvent::Create(FormData& form_data) {
  return new FormDataEvent(form_data);
}

void FormDataEvent::Trace(Visitor* visitor) {
  visitor->Trace(form_data_);
  Event::Trace(visitor);
}

const AtomicString& FormDataEvent::InterfaceName() const {
  return EventNames::FormDataEvent;
}

}  // namespace blink
