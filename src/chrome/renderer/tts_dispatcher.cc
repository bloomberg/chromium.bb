// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/tts_dispatcher.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "chrome/common/tts_messages.h"
#include "chrome/common/tts_utterance_request.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/platform/web_speech_synthesis_utterance.h"
#include "third_party/blink/public/platform/web_speech_synthesis_voice.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"

using content::RenderThread;
using blink::WebSpeechSynthesizerClient;
using blink::WebSpeechSynthesisUtterance;
using blink::WebSpeechSynthesisVoice;
using blink::WebString;
using blink::WebVector;

int TtsDispatcher::next_utterance_id_ = 1;

TtsDispatcher::TtsDispatcher(WebSpeechSynthesizerClient* client)
    : synthesizer_client_(client) {
  RenderThread::Get()->AddObserver(this);
}

TtsDispatcher::~TtsDispatcher() {
  RenderThread::Get()->RemoveObserver(this);
}

bool TtsDispatcher::OnControlMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(TtsDispatcher, message)
    IPC_MESSAGE_HANDLER(TtsMsg_SetVoiceList, OnSetVoiceList)
    IPC_MESSAGE_HANDLER(TtsMsg_DidStartSpeaking, OnDidStartSpeaking)
    IPC_MESSAGE_HANDLER(TtsMsg_DidFinishSpeaking, OnDidFinishSpeaking)
    IPC_MESSAGE_HANDLER(TtsMsg_DidPauseSpeaking, OnDidPauseSpeaking)
    IPC_MESSAGE_HANDLER(TtsMsg_DidResumeSpeaking, OnDidResumeSpeaking)
    IPC_MESSAGE_HANDLER(TtsMsg_WordBoundary, OnWordBoundary)
    IPC_MESSAGE_HANDLER(TtsMsg_SentenceBoundary, OnSentenceBoundary)
    IPC_MESSAGE_HANDLER(TtsMsg_MarkerEvent, OnMarkerEvent)
    IPC_MESSAGE_HANDLER(TtsMsg_WasInterrupted, OnWasInterrupted)
    IPC_MESSAGE_HANDLER(TtsMsg_WasCancelled, OnWasCancelled)
    IPC_MESSAGE_HANDLER(TtsMsg_SpeakingErrorOccurred, OnSpeakingErrorOccurred)
  IPC_END_MESSAGE_MAP()

  // Always return false because there may be multiple TtsDispatchers
  // and we want them all to have a chance to handle this message.
  return false;
}

void TtsDispatcher::UpdateVoiceList() {
  RenderThread::Get()->Send(new TtsHostMsg_InitializeVoiceList());
}

void TtsDispatcher::Speak(const WebSpeechSynthesisUtterance& web_utterance) {
  int id = next_utterance_id_++;

  utterance_id_map_[id] = web_utterance;

  TtsUtteranceRequest utterance;
  utterance.id = id;
  utterance.text = web_utterance.GetText().Utf8();
  utterance.lang = web_utterance.Lang().Utf8();
  utterance.voice = web_utterance.Voice().Utf8();
  utterance.volume = web_utterance.Volume();
  utterance.rate = web_utterance.Rate();
  utterance.pitch = web_utterance.Pitch();
  RenderThread::Get()->Send(new TtsHostMsg_Speak(utterance));
}

void TtsDispatcher::Pause() {
  RenderThread::Get()->Send(new TtsHostMsg_Pause());
}

void TtsDispatcher::Resume() {
  RenderThread::Get()->Send(new TtsHostMsg_Resume());
}

void TtsDispatcher::Cancel() {
  RenderThread::Get()->Send(new TtsHostMsg_Cancel());
}

WebSpeechSynthesisUtterance TtsDispatcher::FindUtterance(int utterance_id) {
  const auto iter = utterance_id_map_.find(utterance_id);
  if (iter == utterance_id_map_.end())
    return WebSpeechSynthesisUtterance();
  return iter->second;
}

void TtsDispatcher::OnSetVoiceList(const std::vector<TtsVoice>& voices) {
  WebVector<WebSpeechSynthesisVoice> out_voices(voices.size());
  for (size_t i = 0; i < voices.size(); ++i) {
    out_voices[i] = WebSpeechSynthesisVoice();
    out_voices[i].SetVoiceURI(WebString::FromUTF8(voices[i].voice_uri));
    out_voices[i].SetName(WebString::FromUTF8(voices[i].name));
    out_voices[i].SetLanguage(WebString::FromUTF8(voices[i].lang));
    out_voices[i].SetIsLocalService(voices[i].local_service);
    out_voices[i].SetIsDefault(voices[i].is_default);
  }
  synthesizer_client_->SetVoiceList(out_voices);
}

void TtsDispatcher::OnDidStartSpeaking(int utterance_id) {
  if (utterance_id_map_.find(utterance_id) == utterance_id_map_.end())
    return;

  WebSpeechSynthesisUtterance utterance = FindUtterance(utterance_id);
  if (utterance.IsNull())
    return;

  synthesizer_client_->DidStartSpeaking(utterance);
}

void TtsDispatcher::OnDidFinishSpeaking(int utterance_id) {
  WebSpeechSynthesisUtterance utterance = FindUtterance(utterance_id);
  if (utterance.IsNull())
    return;

  synthesizer_client_->DidFinishSpeaking(utterance);
  utterance_id_map_.erase(utterance_id);
}

void TtsDispatcher::OnDidPauseSpeaking(int utterance_id) {
  WebSpeechSynthesisUtterance utterance = FindUtterance(utterance_id);
  if (utterance.IsNull())
    return;

  synthesizer_client_->DidPauseSpeaking(utterance);
}

void TtsDispatcher::OnDidResumeSpeaking(int utterance_id) {
  WebSpeechSynthesisUtterance utterance = FindUtterance(utterance_id);
  if (utterance.IsNull())
    return;

  synthesizer_client_->DidResumeSpeaking(utterance);
}

void TtsDispatcher::OnWordBoundary(int utterance_id, int char_index) {
  CHECK(char_index >= 0);

  WebSpeechSynthesisUtterance utterance = FindUtterance(utterance_id);
  if (utterance.IsNull())
    return;

  synthesizer_client_->WordBoundaryEventOccurred(
      utterance, static_cast<unsigned>(char_index));
}

void TtsDispatcher::OnSentenceBoundary(int utterance_id, int char_index) {
  CHECK(char_index >= 0);

  WebSpeechSynthesisUtterance utterance = FindUtterance(utterance_id);
  if (utterance.IsNull())
    return;

  synthesizer_client_->SentenceBoundaryEventOccurred(
      utterance, static_cast<unsigned>(char_index));
}

void TtsDispatcher::OnMarkerEvent(int utterance_id, int char_index) {
  // Not supported yet.
}

void TtsDispatcher::OnWasInterrupted(int utterance_id) {
  WebSpeechSynthesisUtterance utterance = FindUtterance(utterance_id);
  if (utterance.IsNull())
    return;

  // The web speech API doesn't support "interrupted".
  synthesizer_client_->DidFinishSpeaking(utterance);
  utterance_id_map_.erase(utterance_id);
}

void TtsDispatcher::OnWasCancelled(int utterance_id) {
  WebSpeechSynthesisUtterance utterance = FindUtterance(utterance_id);
  if (utterance.IsNull())
    return;

  // The web speech API doesn't support "cancelled".
  synthesizer_client_->DidFinishSpeaking(utterance);
  utterance_id_map_.erase(utterance_id);
}

void TtsDispatcher::OnSpeakingErrorOccurred(int utterance_id,
                                            const std::string& error_message) {
  WebSpeechSynthesisUtterance utterance = FindUtterance(utterance_id);
  if (utterance.IsNull())
    return;

  // The web speech API doesn't support an error message.
  synthesizer_client_->SpeakingErrorOccurred(utterance);
  utterance_id_map_.erase(utterance_id);
}
