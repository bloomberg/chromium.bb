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

#ifndef SpeechRecognition_h
#define SpeechRecognition_h

#include "core/dom/ContextLifecycleObserver.h"
#include "modules/EventTargetModules.h"
#include "modules/ModulesExport.h"
#include "modules/speech/SpeechGrammarList.h"
#include "modules/speech/SpeechRecognitionResult.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Compiler.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebPrivatePtr.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class MediaStreamTrack;
class Page;
class SpeechRecognitionController;
class SpeechRecognitionError;

class MODULES_EXPORT SpeechRecognition final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<SpeechRecognition>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(SpeechRecognition);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SpeechRecognition* Create(ExecutionContext*);
  ~SpeechRecognition() override;

  // SpeechRecognition.idl implemementation.
  // Attributes.
  SpeechGrammarList* grammars() { return grammars_; }
  void setGrammars(SpeechGrammarList* grammars) { grammars_ = grammars; }
  String lang() { return lang_; }
  void setLang(const String& lang) { lang_ = lang; }
  bool continuous() { return continuous_; }
  void setContinuous(bool continuous) { continuous_ = continuous; }
  bool interimResults() { return interim_results_; }
  void setInterimResults(bool interim_results) {
    interim_results_ = interim_results;
  }
  unsigned maxAlternatives() { return max_alternatives_; }
  void setMaxAlternatives(unsigned max_alternatives) {
    max_alternatives_ = max_alternatives;
  }
  MediaStreamTrack* audioTrack() { return audio_track_; }
  void setAudioTrack(MediaStreamTrack* audio_track) {
    audio_track_ = audio_track;
  }

  // Callable by the user.
  void start(ExceptionState&);
  void stopFunction();
  void abort();

  // Called by the SpeechRecognitionClient.
  void DidStartAudio();
  void DidStartSound();
  void DidStartSpeech();
  void DidEndSpeech();
  void DidEndSound();
  void DidEndAudio();
  void DidReceiveResults(
      const HeapVector<Member<SpeechRecognitionResult>>& new_final_results,
      const HeapVector<Member<SpeechRecognitionResult>>&
          current_interim_results);
  void DidReceiveNoMatch(SpeechRecognitionResult*);
  void DidReceiveError(SpeechRecognitionError*);
  void DidStart();
  void DidEnd();

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ScriptWrappable
  bool HasPendingActivity() const final;

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(audiostart);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(soundstart);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(speechstart);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(speechend);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(soundend);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(audioend);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(result);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(nomatch);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(start);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(end);

  virtual void Trace(blink::Visitor*);

 private:
  SpeechRecognition(Page*, ExecutionContext*);

  Member<SpeechGrammarList> grammars_;
  Member<MediaStreamTrack> audio_track_;
  String lang_;
  bool continuous_;
  bool interim_results_;
  unsigned long max_alternatives_;

  Member<SpeechRecognitionController> controller_;
  bool started_;
  bool stopping_;
  HeapVector<Member<SpeechRecognitionResult>> final_results_;
};

}  // namespace blink

#endif  // SpeechRecognition_h
