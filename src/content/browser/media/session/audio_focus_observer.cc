// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/audio_focus_observer.h"

#include "build/build_config.h"
#include "content/browser/media/session/audio_focus_manager.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace content {

AudioFocusObserver::AudioFocusObserver() : binding_(this) {}

void AudioFocusObserver::RegisterAudioFocusObserver() {
  if (observer_id_.has_value())
    return;

#if !defined(OS_ANDROID)
  // TODO(https://crbug.com/873320): Add support for Android.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  media_session::mojom::AudioFocusObserverPtr observer;
  binding_.Bind(mojo::MakeRequest(&observer));
  observer_id_ =
      AudioFocusManager::GetInstance()->AddObserver(std::move(observer));
#endif
}

void AudioFocusObserver::UnregisterAudioFocusObserver() {
  if (!observer_id_.has_value())
    return;

#if !defined(OS_ANDROID)
  // TODO(https://crbug.com/873320): Add support for Android.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  binding_.Close();
  AudioFocusManager::GetInstance()->RemoveObserver(observer_id_.value());
#endif

  observer_id_.reset();
}

AudioFocusObserver::~AudioFocusObserver() = default;

}  // namespace content
