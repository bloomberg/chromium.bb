// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_TTS_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_SPEECH_TTS_CONTROLLER_IMPL_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "content/common/content_export.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_controller_delegate.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;

// Singleton class that manages text-to-speech for the TTS and TTS engine
// extension APIs, maintaining a queue of pending utterances and keeping
// track of all state.
class CONTENT_EXPORT TtsControllerImpl : public TtsController {
 public:
  // Get the single instance of this class.
  static TtsControllerImpl* GetInstance();

  // TtsController methods
  bool IsSpeaking() override;
  void SpeakOrEnqueue(Utterance* utterance) override;
  void Stop() override;
  void Pause() override;
  void Resume() override;
  void OnTtsEvent(int utterance_id,
                  TtsEventType event_type,
                  int char_index,
                  const std::string& error_message) override;
  void GetVoices(content::BrowserContext* browser_context,
                 std::vector<VoiceData>* out_voices) override;
  void VoicesChanged() override;
  void AddVoicesChangedDelegate(VoicesChangedDelegate* delegate) override;
  void RemoveVoicesChangedDelegate(VoicesChangedDelegate* delegate) override;
  void RemoveUtteranceEventDelegate(UtteranceEventDelegate* delegate) override;
  void SetTtsEngineDelegate(TtsEngineDelegate* delegate) override;
  TtsEngineDelegate* GetTtsEngineDelegate() override;

 protected:
  TtsControllerImpl();
  ~TtsControllerImpl() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(TtsControllerTest, TestTtsControllerShutdown);
  FRIEND_TEST_ALL_PREFIXES(TtsControllerTest, TestGetMatchingVoice);
  FRIEND_TEST_ALL_PREFIXES(TtsControllerTest,
                           TestTtsControllerUtteranceDefaults);

  friend struct base::DefaultSingletonTraits<TtsControllerImpl>;

  TtsControllerDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(TtsControllerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_TTS_CONTROLLER_IMPL_H_
