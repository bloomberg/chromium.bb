/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/storage/StorageEvent.h"

#include "modules/EventModules.h"
#include "modules/storage/Storage.h"
#include "modules/storage/StorageEventInit.h"

namespace blink {

StorageEvent* StorageEvent::Create() {
  return new StorageEvent;
}

StorageEvent::StorageEvent() = default;

StorageEvent::~StorageEvent() = default;

StorageEvent* StorageEvent::Create(const AtomicString& type,
                                   const String& key,
                                   const String& old_value,
                                   const String& new_value,
                                   const String& url,
                                   Storage* storage_area) {
  return new StorageEvent(type, key, old_value, new_value, url, storage_area);
}

StorageEvent* StorageEvent::Create(const AtomicString& type,
                                   const StorageEventInit& initializer) {
  return new StorageEvent(type, initializer);
}

StorageEvent::StorageEvent(const AtomicString& type,
                           const String& key,
                           const String& old_value,
                           const String& new_value,
                           const String& url,
                           Storage* storage_area)
    : Event(type, false, false),
      key_(key),
      old_value_(old_value),
      new_value_(new_value),
      url_(url),
      storage_area_(storage_area) {}

StorageEvent::StorageEvent(const AtomicString& type,
                           const StorageEventInit& initializer)
    : Event(type, initializer) {
  if (initializer.hasKey())
    key_ = initializer.key();
  if (initializer.hasOldValue())
    old_value_ = initializer.oldValue();
  if (initializer.hasNewValue())
    new_value_ = initializer.newValue();
  if (initializer.hasURL())
    url_ = initializer.url();
  if (initializer.hasStorageArea())
    storage_area_ = initializer.storageArea();
}

void StorageEvent::initStorageEvent(const AtomicString& type,
                                    bool can_bubble,
                                    bool cancelable,
                                    const String& key,
                                    const String& old_value,
                                    const String& new_value,
                                    const String& url,
                                    Storage* storage_area) {
  if (IsBeingDispatched())
    return;

  initEvent(type, can_bubble, cancelable);

  key_ = key;
  old_value_ = old_value;
  new_value_ = new_value;
  url_ = url;
  storage_area_ = storage_area;
}

const AtomicString& StorageEvent::InterfaceName() const {
  return EventNames::StorageEvent;
}

void StorageEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(storage_area_);
  Event::Trace(visitor);
}

}  // namespace blink
