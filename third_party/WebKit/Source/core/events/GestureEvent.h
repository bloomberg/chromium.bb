/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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
 */

#ifndef GestureEvent_h
#define GestureEvent_h

#include "core/CoreExport.h"
#include "core/dom/events/EventDispatcher.h"
#include "core/events/UIEventWithKeyState.h"
#include "public/platform/WebGestureEvent.h"

namespace blink {

class CORE_EXPORT GestureEvent final : public UIEventWithKeyState {
 public:
  static GestureEvent* Create(AbstractView*, const WebGestureEvent&);
  ~GestureEvent() override {}

  bool IsGestureEvent() const override;

  const AtomicString& InterfaceName() const override;

  const WebGestureEvent& NativeEvent() const { return native_event_; }

  virtual void Trace(blink::Visitor*);

 private:
  GestureEvent(const AtomicString&, AbstractView*, const WebGestureEvent&);

  WebGestureEvent native_event_;
};

DEFINE_EVENT_TYPE_CASTS(GestureEvent);

}  // namespace blink

#endif  // GestureEvent_h
