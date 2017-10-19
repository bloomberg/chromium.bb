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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/speech/SpeechRecognitionEvent.h"

namespace blink {

SpeechRecognitionEvent* SpeechRecognitionEvent::Create(
    const AtomicString& event_name,
    const SpeechRecognitionEventInit& initializer) {
  return new SpeechRecognitionEvent(event_name, initializer);
}

SpeechRecognitionEvent* SpeechRecognitionEvent::CreateResult(
    unsigned long result_index,
    const HeapVector<Member<SpeechRecognitionResult>>& results) {
  return new SpeechRecognitionEvent(
      EventTypeNames::result, result_index,
      SpeechRecognitionResultList::Create(results));
}

SpeechRecognitionEvent* SpeechRecognitionEvent::CreateNoMatch(
    SpeechRecognitionResult* result) {
  if (result) {
    HeapVector<Member<SpeechRecognitionResult>> results;
    results.push_back(result);
    return new SpeechRecognitionEvent(
        EventTypeNames::nomatch, 0,
        SpeechRecognitionResultList::Create(results));
  }

  return new SpeechRecognitionEvent(EventTypeNames::nomatch, 0, nullptr);
}

const AtomicString& SpeechRecognitionEvent::InterfaceName() const {
  return EventNames::SpeechRecognitionEvent;
}

SpeechRecognitionEvent::SpeechRecognitionEvent(
    const AtomicString& event_name,
    const SpeechRecognitionEventInit& initializer)
    : Event(event_name, initializer), result_index_(0) {
  if (initializer.hasResultIndex())
    result_index_ = initializer.resultIndex();
  if (initializer.hasResults())
    results_ = initializer.results();
}

SpeechRecognitionEvent::SpeechRecognitionEvent(
    const AtomicString& event_name,
    unsigned long result_index,
    SpeechRecognitionResultList* results)
    : Event(event_name, /*canBubble=*/false, /*cancelable=*/false),
      result_index_(result_index),
      results_(results) {}

SpeechRecognitionEvent::~SpeechRecognitionEvent() {}

void SpeechRecognitionEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(results_);
  Event::Trace(visitor);
}

}  // namespace blink
