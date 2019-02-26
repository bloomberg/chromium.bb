// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/singleton.h"
#include "content/public/browser/tts_controller.h"

content::UtteranceContinuousParameters::UtteranceContinuousParameters()
    : rate(1.0), pitch(1.0), volume(1.0) {}

content::VoiceData::VoiceData() : remote(false), native(false) {}

content::VoiceData::VoiceData(const VoiceData& other) = default;

content::VoiceData::~VoiceData() {}

class MockTtsController : public content::TtsController {
 public:
  static MockTtsController* GetInstance() {
    return base::Singleton<MockTtsController>::get();
  }

  MockTtsController() {}

  bool IsSpeaking() override { return false; }

  void SpeakOrEnqueue(content::Utterance* utterance) override {}

  void Stop() override {}

  void Pause() override {}

  void Resume() override {}

  void OnTtsEvent(int utterance_id,
                  content::TtsEventType event_type,
                  int char_index,
                  const std::string& error_message) override {}

  void GetVoices(content::BrowserContext* browser_context,
                 std::vector<content::VoiceData>* out_voices) override {}

  void VoicesChanged() override {}

  void AddVoicesChangedDelegate(
      content::VoicesChangedDelegate* delegate) override {}

  void RemoveVoicesChangedDelegate(
      content::VoicesChangedDelegate* delegate) override {}

  void RemoveUtteranceEventDelegate(
      content::UtteranceEventDelegate* delegate) override {}

  void SetTtsEngineDelegate(content::TtsEngineDelegate* delegate) override {}

  content::TtsEngineDelegate* GetTtsEngineDelegate() override {
    return nullptr;
  }

 private:
  friend struct base::DefaultSingletonTraits<MockTtsController>;
  DISALLOW_COPY_AND_ASSIGN(MockTtsController);
};

// static
content::TtsController* content::TtsController::GetInstance() {
  return MockTtsController::GetInstance();
}
