/*
 * Copyright (C) 2013 Apple Computer, Inc.  All rights reserved.
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

#include "platform/speech/PlatformSpeechSynthesizer.h"

#include "platform/exported/WebSpeechSynthesizerClientImpl.h"
#include "platform/speech/PlatformSpeechSynthesisUtterance.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebSpeechSynthesisUtterance.h"
#include "public/platform/WebSpeechSynthesizer.h"
#include "public/platform/WebSpeechSynthesizerClient.h"

namespace blink {

PlatformSpeechSynthesizer* PlatformSpeechSynthesizer::Create(
    PlatformSpeechSynthesizerClient* client) {
  PlatformSpeechSynthesizer* synthesizer =
      new PlatformSpeechSynthesizer(client);
  synthesizer->InitializeVoiceList();
  return synthesizer;
}

PlatformSpeechSynthesizer::PlatformSpeechSynthesizer(
    PlatformSpeechSynthesizerClient* client)
    : speech_synthesizer_client_(client) {
  web_speech_synthesizer_client_ =
      new WebSpeechSynthesizerClientImpl(this, client);
  web_speech_synthesizer_ = Platform::Current()->CreateSpeechSynthesizer(
      web_speech_synthesizer_client_);
}

PlatformSpeechSynthesizer::~PlatformSpeechSynthesizer() {}

void PlatformSpeechSynthesizer::Speak(
    PlatformSpeechSynthesisUtterance* utterance) {
  if (web_speech_synthesizer_ && web_speech_synthesizer_client_)
    web_speech_synthesizer_->Speak(WebSpeechSynthesisUtterance(utterance));
}

void PlatformSpeechSynthesizer::Pause() {
  if (web_speech_synthesizer_)
    web_speech_synthesizer_->Pause();
}

void PlatformSpeechSynthesizer::Resume() {
  if (web_speech_synthesizer_)
    web_speech_synthesizer_->Resume();
}

void PlatformSpeechSynthesizer::Cancel() {
  if (web_speech_synthesizer_)
    web_speech_synthesizer_->Cancel();
}

void PlatformSpeechSynthesizer::SetVoiceList(
    Vector<RefPtr<PlatformSpeechSynthesisVoice>>& voices) {
  voice_list_ = voices;
}

void PlatformSpeechSynthesizer::InitializeVoiceList() {
  if (web_speech_synthesizer_)
    web_speech_synthesizer_->UpdateVoiceList();
}

void PlatformSpeechSynthesizer::Trace(blink::Visitor* visitor) {
  visitor->Trace(speech_synthesizer_client_);
  visitor->Trace(web_speech_synthesizer_client_);
}

}  // namespace blink
