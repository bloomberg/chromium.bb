// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_TTS_PLATFORM_IMPL_H_
#define CHROME_BROWSER_SPEECH_TTS_PLATFORM_IMPL_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/speech/tts_platform.h"
#include "content/public/browser/tts_controller.h"

// Abstract platform implementation.
// TODO(katie): Move to content/browser/speech.
class TtsPlatformImpl : public TtsPlatform {
 public:
  // TtsPlatform overrides.
  bool LoadBuiltInTtsExtension(
      content::BrowserContext* browser_context) override;
  void WillSpeakUtteranceWithVoice(
      const content::Utterance* utterance,
      const content::VoiceData& voice_data) override;
  std::string GetError() override;
  void ClearError() override;
  void SetError(const std::string& error) override;

 protected:
  TtsPlatformImpl() {}

  // On some platforms this may be a leaky singleton - do not rely on the
  // destructor being called!  http://crbug.com/122026
  virtual ~TtsPlatformImpl() {}

  std::string error_;

  DISALLOW_COPY_AND_ASSIGN(TtsPlatformImpl);
};

#endif  // CHROME_BROWSER_SPEECH_TTS_PLATFORM_IMPL_H_
