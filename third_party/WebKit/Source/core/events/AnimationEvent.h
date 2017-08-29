/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
 */

#ifndef AnimationEvent_h
#define AnimationEvent_h

#include "core/dom/events/Event.h"
#include "core/events/AnimationEventInit.h"

namespace blink {

class AnimationEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AnimationEvent* Create() { return new AnimationEvent; }
  static AnimationEvent* Create(const AtomicString& type,
                                const String& animation_name,
                                double elapsed_time) {
    return new AnimationEvent(type, animation_name, elapsed_time);
  }
  static AnimationEvent* Create(const AtomicString& type,
                                const AnimationEventInit& initializer) {
    return new AnimationEvent(type, initializer);
  }

  ~AnimationEvent() override;

  const String& animationName() const;
  double elapsedTime() const;

  const AtomicString& InterfaceName() const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  AnimationEvent();
  AnimationEvent(const AtomicString& type,
                 const String& animation_name,
                 double elapsed_time);
  AnimationEvent(const AtomicString&, const AnimationEventInit&);

  String animation_name_;
  double elapsed_time_;
};

}  // namespace blink

#endif  // AnimationEvent_h
