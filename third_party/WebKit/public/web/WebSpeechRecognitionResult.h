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

#ifndef WebSpeechRecognitionResult_h
#define WebSpeechRecognitionResult_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebPrivatePtr.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"

namespace blink {

class SpeechRecognitionResult;

class WebSpeechRecognitionResult {
 public:
  WebSpeechRecognitionResult() {}
  WebSpeechRecognitionResult(const WebSpeechRecognitionResult& result) {
    Assign(result);
  }
  ~WebSpeechRecognitionResult() { Reset(); }

  BLINK_EXPORT void Assign(const WebVector<WebString>& transcripts,
                           const WebVector<float>& confidences,
                           bool final);
  BLINK_EXPORT void Assign(const WebSpeechRecognitionResult&);
  BLINK_EXPORT void Reset();

#if BLINK_IMPLEMENTATION
  operator SpeechRecognitionResult*() const;
#endif

 private:
  WebPrivatePtr<SpeechRecognitionResult> private_;
};

}  // namespace blink

#endif  // WebSpeechRecognitionResult_h
