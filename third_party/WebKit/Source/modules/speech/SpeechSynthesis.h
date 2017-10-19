/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef SpeechSynthesis_h
#define SpeechSynthesis_h

#include "core/dom/ContextLifecycleObserver.h"
#include "modules/EventTargetModules.h"
#include "modules/ModulesExport.h"
#include "modules/speech/SpeechSynthesisUtterance.h"
#include "modules/speech/SpeechSynthesisVoice.h"
#include "platform/heap/Handle.h"
#include "platform/speech/PlatformSpeechSynthesisUtterance.h"
#include "platform/speech/PlatformSpeechSynthesizer.h"

namespace blink {

class PlatformSpeechSynthesizerClient;

class MODULES_EXPORT SpeechSynthesis final
    : public EventTargetWithInlineData,
      public ContextClient,
      public PlatformSpeechSynthesizerClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(SpeechSynthesis);

 public:
  static SpeechSynthesis* Create(ExecutionContext*);

  bool pending() const;
  bool speaking() const;
  bool paused() const;

  void speak(SpeechSynthesisUtterance*);
  void cancel();
  void pause();
  void resume();

  const HeapVector<Member<SpeechSynthesisVoice>>& getVoices();

  // Used in testing to use a mock platform synthesizer
  void SetPlatformSynthesizer(PlatformSpeechSynthesizer*);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(voiceschanged);

  ExecutionContext* GetExecutionContext() const override {
    return ContextClient::GetExecutionContext();
  }

  virtual void Trace(blink::Visitor*);

 private:
  explicit SpeechSynthesis(ExecutionContext*);

  // PlatformSpeechSynthesizerClient override methods.
  void VoicesDidChange() override;
  void DidStartSpeaking(PlatformSpeechSynthesisUtterance*) override;
  void DidPauseSpeaking(PlatformSpeechSynthesisUtterance*) override;
  void DidResumeSpeaking(PlatformSpeechSynthesisUtterance*) override;
  void DidFinishSpeaking(PlatformSpeechSynthesisUtterance*) override;
  void SpeakingErrorOccurred(PlatformSpeechSynthesisUtterance*) override;
  void BoundaryEventOccurred(PlatformSpeechSynthesisUtterance*,
                             SpeechBoundary,
                             unsigned char_index) override;

  void StartSpeakingImmediately();
  void HandleSpeakingCompleted(SpeechSynthesisUtterance*, bool error_occurred);
  void FireEvent(const AtomicString& type,
                 SpeechSynthesisUtterance*,
                 unsigned long char_index,
                 const String& name);

  // Returns the utterance at the front of the queue.
  SpeechSynthesisUtterance* CurrentSpeechUtterance() const;

  Member<PlatformSpeechSynthesizer> platform_speech_synthesizer_;
  HeapVector<Member<SpeechSynthesisVoice>> voice_list_;
  HeapDeque<Member<SpeechSynthesisUtterance>> utterance_queue_;
  bool is_paused_;

  // EventTarget
  const AtomicString& InterfaceName() const override;
};

}  // namespace blink

#endif  // SpeechSynthesis_h
