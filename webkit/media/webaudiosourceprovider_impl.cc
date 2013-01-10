// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/webaudiosourceprovider_impl.h"

#include <vector>

#include "base/logging.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAudioSourceProviderClient.h"

using WebKit::WebVector;

namespace webkit_media {

WebAudioSourceProviderImpl::WebAudioSourceProviderImpl(
    const scoped_refptr<media::AudioRendererSink>& sink)
    : is_initialized_(false),
      channels_(0),
      sample_rate_(0),
      is_running_(false),
      renderer_(NULL),
      client_(NULL),
      sink_(sink) {
}

WebAudioSourceProviderImpl::~WebAudioSourceProviderImpl() {}

void WebAudioSourceProviderImpl::setClient(
    WebKit::WebAudioSourceProviderClient* client) {
  base::AutoLock auto_lock(sink_lock_);

  if (client && client != client_) {
    // Detach the audio renderer from normal playback.
    sink_->Stop();

    // The client will now take control by calling provideInput() periodically.
    client_ = client;

    if (is_initialized_) {
      // The client needs to be notified of the audio format, if available.
      // If the format is not yet available, we'll be notified later
      // when Initialize() is called.

      // Inform WebKit about the audio stream format.
      client->setFormat(channels_, sample_rate_);
    }
  } else if (!client && client_) {
    // Restore normal playback.
    client_ = NULL;
    // TODO(crogers): We should call sink_->Play() if we're
    // in the playing state.
  }
}

void WebAudioSourceProviderImpl::provideInput(
    const WebVector<float*>& audio_data, size_t number_of_frames) {
  DCHECK(client_);

  if (renderer_ && is_initialized_ && is_running_) {
    // Wrap WebVector as std::vector.
    std::vector<float*> v(audio_data.size());
    for (size_t i = 0; i < audio_data.size(); ++i)
      v[i] = audio_data[i];

    scoped_ptr<media::AudioBus> audio_bus = media::AudioBus::WrapVector(
        number_of_frames, v);

    // TODO(crogers): figure out if we should volume scale here or in common
    // WebAudio code.  In any case we need to take care of volume.
    renderer_->Render(audio_bus.get(), 0);
    return;
  }

  // Provide silence if the source is not running.
  for (size_t i = 0; i < audio_data.size(); ++i)
    memset(audio_data[i], 0, sizeof(*audio_data[0]) * number_of_frames);
}

void WebAudioSourceProviderImpl::Start() {
  base::AutoLock auto_lock(sink_lock_);
  if (!client_)
    sink_->Start();
  is_running_ = true;
}

void WebAudioSourceProviderImpl::Stop() {
  base::AutoLock auto_lock(sink_lock_);
  if (!client_)
    sink_->Stop();
  is_running_ = false;
}

void WebAudioSourceProviderImpl::Play() {
  base::AutoLock auto_lock(sink_lock_);
  if (!client_)
    sink_->Play();
  is_running_ = true;
}

void WebAudioSourceProviderImpl::Pause(bool flush) {
  base::AutoLock auto_lock(sink_lock_);
  if (!client_)
    sink_->Pause(flush);
  is_running_ = false;
}

bool WebAudioSourceProviderImpl::SetVolume(double volume) {
  base::AutoLock auto_lock(sink_lock_);
  if (!client_)
    sink_->SetVolume(volume);
  return true;
}

void WebAudioSourceProviderImpl::Initialize(
    const media::AudioParameters& params,
    RenderCallback* renderer) {
  base::AutoLock auto_lock(sink_lock_);
  CHECK(!is_initialized_);
  renderer_ = renderer;

  sink_->Initialize(params, renderer);

  // Keep track of the format in case the client hasn't yet been set.
  channels_ = params.channels();
  sample_rate_ = params.sample_rate();

  if (client_) {
    // Inform WebKit about the audio stream format.
    client_->setFormat(channels_, sample_rate_);
  }

  is_initialized_ = true;
}

}  // namespace webkit_media
