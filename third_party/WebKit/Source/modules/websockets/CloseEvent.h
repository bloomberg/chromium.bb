/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CloseEvent_h
#define CloseEvent_h

#include "core/dom/events/Event.h"
#include "core/event_type_names.h"
#include "modules/EventModules.h"
#include "modules/websockets/CloseEventInit.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CloseEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CloseEvent* Create() { return new CloseEvent(); }

  static CloseEvent* Create(bool was_clean,
                            unsigned short code,
                            const String& reason) {
    return new CloseEvent(was_clean, code, reason);
  }

  static CloseEvent* Create(const AtomicString& type,
                            const CloseEventInit& initializer) {
    return new CloseEvent(type, initializer);
  }

  bool wasClean() const { return was_clean_; }
  unsigned short code() const { return code_; }
  String reason() const { return reason_; }

  // Event function.
  const AtomicString& InterfaceName() const override {
    return EventNames::CloseEvent;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { Event::Trace(visitor); }

 private:
  CloseEvent() : was_clean_(false), code_(0) {}

  CloseEvent(bool was_clean, int code, const String& reason)
      : Event(EventTypeNames::close, false, false),
        was_clean_(was_clean),
        code_(code),
        reason_(reason) {}

  CloseEvent(const AtomicString& type, const CloseEventInit& initializer);

  bool was_clean_;
  unsigned short code_;
  String reason_;
};

}  // namespace blink

#endif  // CloseEvent_h
