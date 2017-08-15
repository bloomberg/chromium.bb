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

#ifndef SpeechRecognitionClientProxy_h
#define SpeechRecognitionClientProxy_h

#include <memory>
#include "modules/ModulesExport.h"
#include "modules/speech/SpeechRecognitionClient.h"
#include "platform/wtf/Compiler.h"
#include "platform/wtf/text/WTFString.h"
#include "public/web/WebSpeechRecognizerClient.h"

namespace blink {

class MediaStreamTrack;
class WebSpeechRecognizer;
class WebString;

class MODULES_EXPORT SpeechRecognitionClientProxy final
    : public SpeechRecognitionClient,
      public WebSpeechRecognizerClient {
 public:
  ~SpeechRecognitionClientProxy() override;

  // Constructing a proxy object with a 0 WebSpeechRecognizer is safe in
  // itself, but attempting to call start/stop/abort on it will crash.
  static std::unique_ptr<SpeechRecognitionClientProxy> Create(
      WebSpeechRecognizer*);

  // SpeechRecognitionClient:
  void Start(SpeechRecognition*,
             const SpeechGrammarList*,
             const String& lang,
             bool continuous,
             bool interim_results,
             unsigned long max_alternatives,
             MediaStreamTrack*) override;
  void Stop(SpeechRecognition*) override;
  void Abort(SpeechRecognition*) override;

  // WebSpeechRecognizerClient:
  void DidStartAudio(const WebSpeechRecognitionHandle&) override;
  void DidStartSound(const WebSpeechRecognitionHandle&) override;
  void DidEndSound(const WebSpeechRecognitionHandle&) override;
  void DidEndAudio(const WebSpeechRecognitionHandle&) override;
  void DidReceiveResults(
      const WebSpeechRecognitionHandle&,
      const WebVector<WebSpeechRecognitionResult>& new_final_results,
      const WebVector<WebSpeechRecognitionResult>& current_interim_results)
      override;
  void DidReceiveNoMatch(const WebSpeechRecognitionHandle&,
                         const WebSpeechRecognitionResult&) override;
  void DidReceiveError(const WebSpeechRecognitionHandle&,
                       const WebString& message,
                       WebSpeechRecognizerClient::ErrorCode) override;
  void DidStart(const WebSpeechRecognitionHandle&) override;
  void DidEnd(const WebSpeechRecognitionHandle&) override;

 private:
  SpeechRecognitionClientProxy(WebSpeechRecognizer*);

  WebSpeechRecognizer* recognizer_;
};

}  // namespace blink

#endif  // SpeechRecognitionClientProxy_h
