// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/tts_controller_impl.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "third_party/blink/public/platform/web_speech_synthesis_constants.h"

namespace content {

// Returns true if this event type is one that indicates an utterance
// is finished and can be destroyed.
bool IsFinalTtsEventType(TtsEventType event_type) {
  return (event_type == TTS_EVENT_END || event_type == TTS_EVENT_INTERRUPTED ||
          event_type == TTS_EVENT_CANCELLED || event_type == TTS_EVENT_ERROR);
}

//
// UtteranceContinuousParameters
//

UtteranceContinuousParameters::UtteranceContinuousParameters()
    : rate(blink::kWebSpeechSynthesisDoublePrefNotSet),
      pitch(blink::kWebSpeechSynthesisDoublePrefNotSet),
      volume(blink::kWebSpeechSynthesisDoublePrefNotSet) {}

//
// VoiceData
//

VoiceData::VoiceData() : remote(false), native(false) {}

VoiceData::VoiceData(const VoiceData& other) = default;

VoiceData::~VoiceData() {}

//
// Utterance
//

// static
int Utterance::next_utterance_id_ = 0;

Utterance::Utterance(BrowserContext* browser_context)
    : browser_context_(browser_context),
      id_(next_utterance_id_++),
      src_id_(-1),
      can_enqueue_(false),
      char_index_(0),
      finished_(false) {
  options_.reset(new base::DictionaryValue());
}

Utterance::~Utterance() {
  // It's an error if an Utterance is destructed without being finished,
  // unless |browser_context_| is nullptr because it's a unit test.
  DCHECK(finished_ || !browser_context_);
}

void Utterance::OnTtsEvent(TtsEventType event_type,
                           int char_index,
                           const std::string& error_message) {
  if (char_index >= 0)
    char_index_ = char_index;
  if (IsFinalTtsEventType(event_type))
    finished_ = true;

  if (event_delegate_)
    event_delegate_->OnTtsEvent(this, event_type, char_index, error_message);
  if (finished_)
    event_delegate_ = nullptr;
}

void Utterance::Finish() {
  finished_ = true;
}

void Utterance::set_options(const base::Value* options) {
  options_.reset(options->DeepCopy());
}

TtsController* TtsController::GetInstance() {
  return TtsControllerImpl::GetInstance();
}

//
// TtsControllerImpl
//

// static
TtsControllerImpl* TtsControllerImpl::GetInstance() {
  return base::Singleton<TtsControllerImpl>::get();
}

TtsControllerImpl::TtsControllerImpl()
    : delegate_(GetContentClient()->browser()->GetTtsControllerDelegate()) {}

TtsControllerImpl::~TtsControllerImpl() {}

void TtsControllerImpl::SpeakOrEnqueue(Utterance* utterance) {
  if (!delegate_)
    return;

  delegate_->SpeakOrEnqueue(utterance);
}

void TtsControllerImpl::Stop() {
  if (!delegate_)
    return;

  delegate_->Stop();
}

void TtsControllerImpl::Pause() {
  if (!delegate_)
    return;

  delegate_->Pause();
}

void TtsControllerImpl::Resume() {
  if (!delegate_)
    return;

  delegate_->Resume();
}

void TtsControllerImpl::OnTtsEvent(int utterance_id,
                                   TtsEventType event_type,
                                   int char_index,
                                   const std::string& error_message) {
  if (!delegate_)
    return;

  delegate_->OnTtsEvent(utterance_id, event_type, char_index, error_message);
}

void TtsControllerImpl::GetVoices(BrowserContext* browser_context,
                                  std::vector<VoiceData>* out_voices) {
  if (!delegate_)
    return;

  delegate_->GetVoices(browser_context, out_voices);
}

bool TtsControllerImpl::IsSpeaking() {
  if (!delegate_)
    return false;

  return delegate_->IsSpeaking();
}

void TtsControllerImpl::VoicesChanged() {
  if (!delegate_)
    return;

  delegate_->VoicesChanged();
}

void TtsControllerImpl::AddVoicesChangedDelegate(
    VoicesChangedDelegate* delegate) {
  if (!delegate_)
    return;

  delegate_->AddVoicesChangedDelegate(delegate);
}

void TtsControllerImpl::RemoveVoicesChangedDelegate(
    VoicesChangedDelegate* delegate) {
  if (!delegate_)
    return;

  delegate_->RemoveVoicesChangedDelegate(delegate);
}

void TtsControllerImpl::RemoveUtteranceEventDelegate(
    UtteranceEventDelegate* delegate) {
  if (!delegate_)
    return;

  delegate_->RemoveUtteranceEventDelegate(delegate);
}

void TtsControllerImpl::SetTtsEngineDelegate(TtsEngineDelegate* delegate) {
  if (!delegate_)
    return;

  delegate_->SetTtsEngineDelegate(delegate);
}

TtsEngineDelegate* TtsControllerImpl::GetTtsEngineDelegate() {
  if (!delegate_)
    return nullptr;

  return delegate_->GetTtsEngineDelegate();
}

}  // namespace content
