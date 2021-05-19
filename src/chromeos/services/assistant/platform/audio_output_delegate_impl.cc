// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_output_delegate_impl.h"
#include "chromeos/services/assistant/media_session/assistant_media_session.h"

namespace chromeos {
namespace assistant {

AudioOutputDelegateImpl::AudioOutputDelegateImpl(
    AssistantMediaSession* media_session)
    : media_session_(media_session) {}

AudioOutputDelegateImpl::~AudioOutputDelegateImpl() = default;

void AudioOutputDelegateImpl::Bind(
    mojo::PendingReceiver<AudioOutputDelegate> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void AudioOutputDelegateImpl::RequestAudioFocus(
    chromeos::libassistant::mojom::AudioOutputStreamType stream_type) {
  // TODO(wutao): Fix the libassistant behavior.
  // Currently this is called with |STREAM_TTS| and |STREAM_ALARM| when
  // requesting focus. When releasing focus it calls with |STREAM_MEDIA|.
  // libassistant media code path does not request focus.
  switch (stream_type) {
    case chromeos::libassistant::mojom::AudioOutputStreamType::kAlarmStream:
      media_session_->RequestAudioFocus(
          media_session::mojom::AudioFocusType::kGainTransientMayDuck);
      break;
    case chromeos::libassistant::mojom::AudioOutputStreamType::kTtsStream:
      media_session_->RequestAudioFocus(
          media_session::mojom::AudioFocusType::kGainTransient);
      break;
    case chromeos::libassistant::mojom::AudioOutputStreamType::kMediaStream:
      media_session_->AbandonAudioFocusIfNeeded();
      break;
  }
}

void AudioOutputDelegateImpl::AbandonAudioFocusIfNeeded() {
  media_session_->AbandonAudioFocusIfNeeded();
}

void AudioOutputDelegateImpl::AddMediaSessionObserver(
    mojo::PendingRemote<::media_session::mojom::MediaSessionObserver>
        observer) {
  media_session_->AddObserver(std::move(observer));
}

}  // namespace assistant
}  // namespace chromeos
