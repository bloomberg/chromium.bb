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

#include "modules/speech/SpeechRecognitionClientProxy.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "core/dom/ExecutionContext.h"
#include "modules/speech/SpeechGrammarList.h"
#include "modules/speech/SpeechRecognition.h"
#include "modules/speech/SpeechRecognitionError.h"
#include "modules/speech/SpeechRecognitionResult.h"
#include "modules/speech/SpeechRecognitionResultList.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/web/WebSpeechGrammar.h"
#include "public/web/WebSpeechRecognitionHandle.h"
#include "public/web/WebSpeechRecognitionParams.h"
#include "public/web/WebSpeechRecognitionResult.h"
#include "public/web/WebSpeechRecognizer.h"

namespace blink {

SpeechRecognitionClientProxy::~SpeechRecognitionClientProxy() = default;

std::unique_ptr<SpeechRecognitionClientProxy>
SpeechRecognitionClientProxy::Create(WebSpeechRecognizer* recognizer) {
  return base::WrapUnique(new SpeechRecognitionClientProxy(recognizer));
}

void SpeechRecognitionClientProxy::Start(SpeechRecognition* recognition,
                                         const SpeechGrammarList* grammar_list,
                                         const String& lang,
                                         bool continuous,
                                         bool interim_results,
                                         unsigned long max_alternatives) {
  size_t length =
      grammar_list ? static_cast<size_t>(grammar_list->length()) : 0U;
  WebVector<WebSpeechGrammar> web_speech_grammars(length);
  for (unsigned long i = 0; i < length; ++i)
    web_speech_grammars[i] = grammar_list->item(i);

  WebSpeechRecognitionParams params(
      web_speech_grammars, lang, continuous, interim_results, max_alternatives,
      WebSecurityOrigin(
          recognition->GetExecutionContext()->GetSecurityOrigin()));
  recognizer_->Start(recognition, params, this);
}

void SpeechRecognitionClientProxy::Stop(SpeechRecognition* recognition) {
  recognizer_->Stop(recognition, this);
}

void SpeechRecognitionClientProxy::Abort(SpeechRecognition* recognition) {
  recognizer_->Abort(recognition, this);
}

void SpeechRecognitionClientProxy::DidStartAudio(
    const WebSpeechRecognitionHandle& handle) {
  SpeechRecognition* recognition(handle);
  recognition->DidStartAudio();
}

void SpeechRecognitionClientProxy::DidStartSound(
    const WebSpeechRecognitionHandle& handle) {
  SpeechRecognition* recognition(handle);
  recognition->DidStartSound();
  recognition->DidStartSpeech();
}

void SpeechRecognitionClientProxy::DidEndSound(
    const WebSpeechRecognitionHandle& handle) {
  SpeechRecognition* recognition(handle);
  recognition->DidEndSpeech();
  recognition->DidEndSound();
}

void SpeechRecognitionClientProxy::DidEndAudio(
    const WebSpeechRecognitionHandle& handle) {
  SpeechRecognition* recognition(handle);
  recognition->DidEndAudio();
}

void SpeechRecognitionClientProxy::DidReceiveResults(
    const WebSpeechRecognitionHandle& handle,
    const WebVector<WebSpeechRecognitionResult>& new_final_results,
    const WebVector<WebSpeechRecognitionResult>& current_interim_results) {
  SpeechRecognition* recognition(handle);

  HeapVector<Member<SpeechRecognitionResult>> final_results_vector(
      new_final_results.size());
  for (size_t i = 0; i < new_final_results.size(); ++i) {
    final_results_vector[i] =
        Member<SpeechRecognitionResult>(new_final_results[i]);
  }

  HeapVector<Member<SpeechRecognitionResult>> interim_results_vector(
      current_interim_results.size());
  for (size_t i = 0; i < current_interim_results.size(); ++i) {
    interim_results_vector[i] =
        Member<SpeechRecognitionResult>(current_interim_results[i]);
  }

  recognition->DidReceiveResults(final_results_vector, interim_results_vector);
}

void SpeechRecognitionClientProxy::DidReceiveNoMatch(
    const WebSpeechRecognitionHandle& handle,
    const WebSpeechRecognitionResult& result) {
  SpeechRecognition* recognition(handle);
  recognition->DidReceiveNoMatch(result);
}

void SpeechRecognitionClientProxy::DidReceiveError(
    const WebSpeechRecognitionHandle& handle,
    const WebString& message,
    WebSpeechRecognizerClient::ErrorCode code) {
  SpeechRecognition* recognition(handle);
  SpeechRecognitionError::ErrorCode error_code =
      static_cast<SpeechRecognitionError::ErrorCode>(code);
  recognition->DidReceiveError(
      SpeechRecognitionError::Create(error_code, message));
}

void SpeechRecognitionClientProxy::DidStart(
    const WebSpeechRecognitionHandle& handle) {
  SpeechRecognition* recognition(handle);
  recognition->DidStart();
}

void SpeechRecognitionClientProxy::DidEnd(
    const WebSpeechRecognitionHandle& handle) {
  SpeechRecognition* recognition(handle);
  recognition->DidEnd();
}

SpeechRecognitionClientProxy::SpeechRecognitionClientProxy(
    WebSpeechRecognizer* recognizer)
    : recognizer_(recognizer) {}

}  // namespace blink
