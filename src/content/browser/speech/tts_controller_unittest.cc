// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for the TTS Controller.

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "content/browser/speech/tts_controller_impl.h"
#include "content/public/browser/tts_controller_delegate.h"
#include "content/public/browser/tts_platform.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_speech_synthesis_constants.h"

namespace content {

class TtsControllerTest : public testing::Test {};

// Platform Tts implementation that does nothing.
class MockTtsPlatformImpl : public TtsPlatform {
 public:
  MockTtsPlatformImpl() {}
  virtual ~MockTtsPlatformImpl() {}
  bool PlatformImplAvailable() override { return true; }
  bool Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const VoiceData& voice,
             const UtteranceContinuousParameters& params) override {
    return true;
  }
  bool IsSpeaking() override { return false; }
  bool StopSpeaking() override { return true; }
  void Pause() override {}
  void Resume() override {}
  void GetVoices(std::vector<VoiceData>* out_voices) override {}
  bool LoadBuiltInTtsEngine(BrowserContext* browser_context) override {
    return false;
  }
  void WillSpeakUtteranceWithVoice(TtsUtterance* utterance,
                                   const VoiceData& voice_data) override {}
  void SetError(const std::string& error) override {}
  std::string GetError() override { return std::string(); }
  void ClearError() override {}
};

class MockTtsControllerDelegate : public TtsControllerDelegate {
 public:
  MockTtsControllerDelegate() {}
  ~MockTtsControllerDelegate() override {}

  int GetMatchingVoice(content::TtsUtterance* utterance,
                       std::vector<content::VoiceData>& voices) override {
    // Below 0 implies a "native" voice.
    return -1;
  }

  void UpdateUtteranceDefaultsFromPrefs(content::TtsUtterance* utterance,
                                        double* rate,
                                        double* pitch,
                                        double* volume) override {}

  void SetTtsEngineDelegate(content::TtsEngineDelegate* delegate) override {}

  content::TtsEngineDelegate* GetTtsEngineDelegate() override {
    return nullptr;
  }
};

// Subclass of TtsController with a public ctor and dtor.
class TtsControllerForTesting : public TtsControllerImpl {
 public:
  TtsControllerForTesting() {}
  ~TtsControllerForTesting() override {}
};

TEST_F(TtsControllerTest, TestTtsControllerShutdown) {
  MockTtsPlatformImpl platform_impl;
  TtsControllerForTesting* controller = new TtsControllerForTesting();
  MockTtsControllerDelegate* delegate = new MockTtsControllerDelegate();
  controller->delegate_ = delegate;

  controller->SetTtsPlatform(&platform_impl);

  TtsUtterance* utterance1 = TtsUtterance::Create(nullptr);
  utterance1->SetCanEnqueue(true);
  utterance1->SetSrcId(1);
  controller->SpeakOrEnqueue(utterance1);

  TtsUtterance* utterance2 = TtsUtterance::Create(nullptr);
  utterance2->SetCanEnqueue(true);
  utterance2->SetSrcId(2);
  controller->SpeakOrEnqueue(utterance2);

  // Make sure that deleting the controller when there are pending
  // utterances doesn't cause a crash.
  delete controller;

  // Clean up.
  delete delegate;
}

#if !defined(OS_CHROMEOS)
TEST_F(TtsControllerTest, TestTtsControllerUtteranceDefaults) {
  std::unique_ptr<TtsControllerForTesting> controller =
      std::make_unique<TtsControllerForTesting>();

  std::unique_ptr<TtsUtterance> utterance1 =
      base::WrapUnique(content::TtsUtterance::Create(nullptr));
  // Initialized to default (unset constant) values.
  EXPECT_EQ(blink::kWebSpeechSynthesisDoublePrefNotSet,
            utterance1->GetContinuousParameters().rate);
  EXPECT_EQ(blink::kWebSpeechSynthesisDoublePrefNotSet,
            utterance1->GetContinuousParameters().pitch);
  EXPECT_EQ(blink::kWebSpeechSynthesisDoublePrefNotSet,
            utterance1->GetContinuousParameters().volume);

  controller->UpdateUtteranceDefaults(utterance1.get());
  // Updated to global defaults.
  EXPECT_EQ(blink::kWebSpeechSynthesisDefaultTextToSpeechRate,
            utterance1->GetContinuousParameters().rate);
  EXPECT_EQ(blink::kWebSpeechSynthesisDefaultTextToSpeechPitch,
            utterance1->GetContinuousParameters().pitch);
  EXPECT_EQ(blink::kWebSpeechSynthesisDefaultTextToSpeechVolume,
            utterance1->GetContinuousParameters().volume);
}
#endif  // !defined(OS_CHROMEOS)

}  // namespace content
