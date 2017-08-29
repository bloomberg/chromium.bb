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

#ifndef StorageEvent_h
#define StorageEvent_h

#include "core/dom/events/Event.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class Storage;
class StorageEventInit;

class StorageEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static StorageEvent* Create();
  static StorageEvent* Create(const AtomicString& type,
                              const String& key,
                              const String& old_value,
                              const String& new_value,
                              const String& url,
                              Storage* storage_area);
  static StorageEvent* Create(const AtomicString&, const StorageEventInit&);
  ~StorageEvent() override;

  const String& key() const { return key_; }
  const String& oldValue() const { return old_value_; }
  const String& newValue() const { return new_value_; }
  const String& url() const { return url_; }
  Storage* storageArea() const { return storage_area_.Get(); }

  void initStorageEvent(const AtomicString& type,
                        bool can_bubble,
                        bool cancelable,
                        const String& key,
                        const String& old_value,
                        const String& new_value,
                        const String& url,
                        Storage* storage_area);

  // Needed once we support init<blank>EventNS
  // void initStorageEventNS(in DOMString namespaceURI, in DOMString typeArg,
  //     in boolean canBubbleArg, in boolean cancelableArg, in DOMString keyArg,
  //     in DOMString oldValueArg, in DOMString newValueArg,
  //     in DOMString urlArg, Storage storageAreaArg);

  const AtomicString& InterfaceName() const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  StorageEvent();
  StorageEvent(const AtomicString& type,
               const String& key,
               const String& old_value,
               const String& new_value,
               const String& url,
               Storage* storage_area);
  StorageEvent(const AtomicString&, const StorageEventInit&);

  String key_;
  String old_value_;
  String new_value_;
  String url_;
  Member<Storage> storage_area_;
};

}  // namespace blink

#endif  // StorageEvent_h
