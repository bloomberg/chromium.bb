// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_TTS_PLATFORM_H_
#define CHROME_BROWSER_SPEECH_TTS_PLATFORM_H_

#include <string>

#include "base/macros.h"
#include "content/public/browser/tts_controller.h"

// Abstract class that defines the native platform TTS interface,
// subclassed by specific implementations on Win, Mac, etc.
// TODO(katie): Move to content/public/browser and most implementations into
// content/browser/speech. The tts_chromeos.cc implementation may need to remain
// in chrome/ due to ARC++ dependencies.
class TtsPlatform {
 public:
  static TtsPlatform* GetInstance();

  // Returns true if this platform implementation is supported and available.
  virtual bool PlatformImplAvailable() = 0;

  // Some platforms may provide a built-in TTS extension. Returns true
  // if the extension was not previously loaded and is now loading, and
  // false if it's already loaded or if there's no extension to load.
  // Will call TtsController::RetrySpeakingQueuedUtterances when
  // the extension finishes loading.
  virtual bool LoadBuiltInTtsExtension(
      content::BrowserContext* browser_context) = 0;

  // Speak the given utterance with the given parameters if possible,
  // and return true on success. Utterance will always be nonempty.
  // If rate, pitch, or volume are -1.0, they will be ignored.
  //
  // The TtsController will only try to speak one utterance at
  // a time. If it wants to interrupt speech, it will always call Stop
  // before speaking again.
  virtual bool Speak(int utterance_id,
                     const std::string& utterance,
                     const std::string& lang,
                     const content::VoiceData& voice,
                     const content::UtteranceContinuousParameters& params) = 0;

  // Stop speaking immediately and return true on success.
  virtual bool StopSpeaking() = 0;

  // Returns whether any speech is on going.
  virtual bool IsSpeaking() = 0;

  // Append information about voices provided by this platform implementation
  // to |out_voices|.
  virtual void GetVoices(std::vector<content::VoiceData>* out_voices) = 0;

  // Pause the current utterance, if any, until a call to Resume,
  // Speak, or StopSpeaking.
  virtual void Pause() = 0;

  // Resume speaking the current utterance, if it was paused.
  virtual void Resume() = 0;

  // Allows the platform to monitor speech commands and the voices used
  // for each one.
  virtual void WillSpeakUtteranceWithVoice(
      const content::Utterance* utterance,
      const content::VoiceData& voice_data) = 0;

  virtual std::string GetError() = 0;
  virtual void ClearError() = 0;
  virtual void SetError(const std::string& error) = 0;
};

#endif  // CHROME_BROWSER_SPEECH_TTS_PLATFORM_H_
