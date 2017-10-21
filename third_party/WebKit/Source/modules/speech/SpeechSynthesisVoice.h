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

#ifndef SpeechSynthesisVoice_h
#define SpeechSynthesisVoice_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/speech/PlatformSpeechSynthesisVoice.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class SpeechSynthesisVoice final
    : public GarbageCollectedFinalized<SpeechSynthesisVoice>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SpeechSynthesisVoice* Create(
      scoped_refptr<PlatformSpeechSynthesisVoice>);
  ~SpeechSynthesisVoice();

  const String& voiceURI() const { return platform_voice_->VoiceURI(); }
  const String& name() const { return platform_voice_->GetName(); }
  const String& lang() const { return platform_voice_->Lang(); }
  bool localService() const { return platform_voice_->LocalService(); }
  bool isDefault() const { return platform_voice_->IsDefault(); }

  PlatformSpeechSynthesisVoice* PlatformVoice() const {
    return platform_voice_.get();
  }

  void Trace(blink::Visitor* visitor) {}

 private:
  explicit SpeechSynthesisVoice(scoped_refptr<PlatformSpeechSynthesisVoice>);

  scoped_refptr<PlatformSpeechSynthesisVoice> platform_voice_;
};

}  // namespace blink

#endif  // SpeechSynthesisVoice_h
