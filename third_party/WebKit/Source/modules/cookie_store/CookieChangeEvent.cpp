// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/cookie_store/CookieChangeEvent.h"

#include "modules/EventModules.h"
#include "modules/cookie_store/CookieChangeEventInit.h"
#include "modules/cookie_store/CookieListItem.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

CookieChangeEvent::~CookieChangeEvent() = default;

const AtomicString& CookieChangeEvent::InterfaceName() const {
  return EventNames::CookieChangeEvent;
}

void CookieChangeEvent::Trace(blink::Visitor* visitor) {
  Event::Trace(visitor);
  visitor->Trace(changed_);
  visitor->Trace(deleted_);
}

CookieChangeEvent::CookieChangeEvent() = default;

CookieChangeEvent::CookieChangeEvent(const AtomicString& type,
                                     HeapVector<CookieListItem> changed,
                                     HeapVector<CookieListItem> deleted)
    : Event(type, Bubbles::kNo, Cancelable::kNo),
      changed_(std::move(changed)),
      deleted_(std::move(deleted)) {}

CookieChangeEvent::CookieChangeEvent(const AtomicString& type,
                                     const CookieChangeEventInit& initializer)
    : Event(type, initializer) {
  if (initializer.hasChanged())
    changed_ = initializer.changed();
  if (initializer.hasDeleted())
    deleted_ = initializer.deleted();
}

}  // namespace blink
