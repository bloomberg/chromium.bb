// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_OBSERVER_H_
#define CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_OBSERVER_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

namespace content {

// The observer for observing audio focus events. This will not work on Android
// as it does not use the internal AudioFocusManager implementation.
class CONTENT_EXPORT AudioFocusObserver
    : public media_session::mojom::AudioFocusObserver {
 public:
  AudioFocusObserver();
  ~AudioFocusObserver() override;

  // The given media session gained audio focus with the specified type.
  void OnFocusGained(::media_session::mojom::MediaSessionInfoPtr,
                     media_session::mojom::AudioFocusType) override {}

  // The given media session lost audio focus.
  void OnFocusLost(::media_session::mojom::MediaSessionInfoPtr) override {}

 protected:
  // Called by subclasses to (un-)register the observer with AudioFocusManager.
  void RegisterAudioFocusObserver();
  void UnregisterAudioFocusObserver();

 private:
  void ConnectToService();
  media_session::mojom::AudioFocusManagerPtr audio_focus_ptr_;

  mojo::Binding<media_session::mojom::AudioFocusObserver> binding_;

  DISALLOW_COPY_AND_ASSIGN(AudioFocusObserver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_OBSERVER_H_
