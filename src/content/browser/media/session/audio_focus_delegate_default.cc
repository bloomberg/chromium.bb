// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/audio_focus_delegate.h"

#include "content/browser/media/session/audio_focus_manager.h"
#include "services/media_session/public/cpp/switches.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

namespace content {

using media_session::mojom::AudioFocusType;

namespace {

// AudioFocusDelegateDefault is the default implementation of
// AudioFocusDelegate which only handles audio focus between WebContents.
class AudioFocusDelegateDefault : public AudioFocusDelegate {
 public:
  explicit AudioFocusDelegateDefault(MediaSessionImpl* media_session);
  ~AudioFocusDelegateDefault() override;

  // AudioFocusDelegate implementation.
  bool RequestAudioFocus(AudioFocusType audio_focus_type) override;
  void AbandonAudioFocus() override;
  AudioFocusType GetCurrentFocusType() const override;

 private:
  // Weak pointer because |this| is owned by |media_session_|.
  MediaSessionImpl* media_session_;

  // The last requested AudioFocusType by the associated |media_session_|.
  AudioFocusType audio_focus_type_if_disabled_;
};

}  // anonymous namespace

AudioFocusDelegateDefault::AudioFocusDelegateDefault(
    MediaSessionImpl* media_session)
    : media_session_(media_session) {}

AudioFocusDelegateDefault::~AudioFocusDelegateDefault() = default;

bool AudioFocusDelegateDefault::RequestAudioFocus(
    AudioFocusType audio_focus_type) {
  audio_focus_type_if_disabled_ = audio_focus_type;

  if (!media_session::IsAudioFocusEnabled())
    return true;

  AudioFocusManager::GetInstance()->RequestAudioFocus(media_session_,
                                                      audio_focus_type);
  return true;
}

void AudioFocusDelegateDefault::AbandonAudioFocus() {
  AudioFocusManager::GetInstance()->AbandonAudioFocus(media_session_);
}

AudioFocusType AudioFocusDelegateDefault::GetCurrentFocusType() const {
  if (media_session::IsAudioFocusEnabled()) {
    return AudioFocusManager::GetInstance()->GetFocusTypeForSession(
        media_session_);
  }

  return audio_focus_type_if_disabled_;
}

// static
std::unique_ptr<AudioFocusDelegate> AudioFocusDelegate::Create(
    MediaSessionImpl* media_session) {
  return std::unique_ptr<AudioFocusDelegate>(
      new AudioFocusDelegateDefault(media_session));
}

}  // namespace content
