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

#ifndef SpeechRecognitionController_h
#define SpeechRecognitionController_h

#include "core/page/Page.h"
#include "modules/speech/SpeechRecognitionClient.h"
#include <memory>

namespace blink {

class MediaStreamTrack;

class SpeechRecognitionController final
    : public GarbageCollectedFinalized<SpeechRecognitionController>,
      public Supplement<Page> {
  USING_GARBAGE_COLLECTED_MIXIN(SpeechRecognitionController);

 public:
  virtual ~SpeechRecognitionController();

  void Start(SpeechRecognition* recognition,
             const SpeechGrammarList* grammars,
             const String& lang,
             bool continuous,
             bool interim_results,
             unsigned long max_alternatives,
             MediaStreamTrack* audio_track) {
    client_->Start(recognition, grammars, lang, continuous, interim_results,
                   max_alternatives, audio_track);
  }

  void Stop(SpeechRecognition* recognition) { client_->Stop(recognition); }
  void Abort(SpeechRecognition* recognition) { client_->Abort(recognition); }

  static SpeechRecognitionController* Create(
      std::unique_ptr<SpeechRecognitionClient>);
  static const char* SupplementName();
  static SpeechRecognitionController* From(Page* page) {
    return static_cast<SpeechRecognitionController*>(
        Supplement<Page>::From(page, SupplementName()));
  }

  virtual void Trace(blink::Visitor* visitor) {
    Supplement<Page>::Trace(visitor);
  }

 private:
  explicit SpeechRecognitionController(
      std::unique_ptr<SpeechRecognitionClient>);

  std::unique_ptr<SpeechRecognitionClient> client_;
};

}  // namespace blink

#endif  // SpeechRecognitionController_h
