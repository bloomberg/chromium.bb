// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_TTS_TTS_CONTROLLER_IMPL_H_
#define CHROMECAST_BROWSER_TTS_TTS_CONTROLLER_IMPL_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "chromecast/browser/tts/tts_controller.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

// Singleton class that manages text-to-speech for the TTS and TTS engine
// extension APIs, maintaining a queue of pending utterances and keeping
// track of all state.
class TtsControllerImpl : public TtsController {
 public:
  explicit TtsControllerImpl(std::unique_ptr<TtsPlatformImpl> platform_impl);
  ~TtsControllerImpl() override;

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
  void SetPlatformImpl(std::unique_ptr<TtsPlatformImpl> platform_impl) override;
  int QueueSize() override;

  std::string GetApplicationLocale() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(TtsControllerTest, TestGetMatchingVoice);
  FRIEND_TEST_ALL_PREFIXES(TtsControllerTest,
                           TestTtsControllerUtteranceDefaults);

  // Get the platform TTS implementation (or injected mock).
  TtsPlatformImpl* GetPlatformImpl();

  // Start speaking the given utterance. Will either take ownership of
  // |utterance| or delete it if there's an error. Returns true on success.
  void SpeakNow(Utterance* utterance);

  // Clear the utterance queue. If send_events is true, will send
  // TTS_EVENT_CANCELLED events on each one.
  void ClearUtteranceQueue(bool send_events);

  // Finalize and delete the current utterance.
  void FinishCurrentUtterance();

  // Start speaking the next utterance in the queue.
  void SpeakNextUtterance();

  // Given an utterance and a vector of voices, return the
  // index of the voice that best matches the utterance.
  int GetMatchingVoice(const Utterance* utterance,
                       std::vector<VoiceData>& voices);

  // Updates the utterance to have default values for rate, pitch, and
  // volume if they have not yet been set. On Chrome OS, defaults are
  // pulled from user prefs, and may not be the same as other platforms.
  void UpdateUtteranceDefaults(Utterance* utterance);

  // The current utterance being spoken.
  Utterance* current_utterance_;

  // Whether the queue is paused or not.
  bool paused_;

  // A queue of utterances to speak after the current one finishes.
  base::queue<Utterance*> utterance_queue_;

  // A pointer to the platform implementation of text-to-speech.
  std::unique_ptr<TtsPlatformImpl> platform_impl_;

  DISALLOW_COPY_AND_ASSIGN(TtsControllerImpl);
};

#endif  // CHROMECAST_BROWSER_TTS_TTS_CONTROLLER_IMPL_H_
