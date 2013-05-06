// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/webaudiosourceprovider_impl.h"

#include <vector>

#include "base/logging.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAudioSourceProviderClient.h"

using WebKit::WebVector;

namespace webkit_media {

namespace {

// Simple helper class for Try() locks.  Lock is Try()'d on construction and
// must be checked via the locked() attribute.  If acquisition was successful
// the lock will be released upon destruction.
// TODO(dalecurtis): This should probably move to base/ if others start using
// this pattern.
class AutoTryLock {
 public:
  explicit AutoTryLock(base::Lock& lock)
      : lock_(lock),
        acquired_(lock_.Try()) {}

  bool locked() const { return acquired_; }

  ~AutoTryLock() {
    if (acquired_) {
      lock_.AssertAcquired();
      lock_.Release();
    }
  }

 private:
  base::Lock& lock_;
  const bool acquired_;
  DISALLOW_COPY_AND_ASSIGN(AutoTryLock);
};

}  // namespace

WebAudioSourceProviderImpl::WebAudioSourceProviderImpl(
    const scoped_refptr<media::AudioRendererSink>& sink)
    : channels_(0),
      sample_rate_(0),
      volume_(1.0),
      state_(kStopped),
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

    if (renderer_) {
      // The client needs to be notified of the audio format, if available.
      // If the format is not yet available, we'll be notified later
      // when Initialize() is called.

      // Inform WebKit about the audio stream format.
      client->setFormat(channels_, sample_rate_);
    }
  } else if (!client && client_) {
    // Restore normal playback.
    client_ = NULL;
    sink_->SetVolume(volume_);
    if (state_ >= kStarted)
      sink_->Start();
    if (state_ >= kPlaying)
      sink_->Play();
  }
}

void WebAudioSourceProviderImpl::provideInput(
    const WebVector<float*>& audio_data, size_t number_of_frames) {
  if (!bus_wrapper_ ||
      static_cast<size_t>(bus_wrapper_->channels()) != audio_data.size()) {
    bus_wrapper_ = media::AudioBus::CreateWrapper(audio_data.size());
  }

  bus_wrapper_->set_frames(number_of_frames);
  for (size_t i = 0; i < audio_data.size(); ++i)
    bus_wrapper_->SetChannelData(i, audio_data[i]);

  // Use a try lock to avoid contention in the real-time audio thread.
  AutoTryLock auto_try_lock(sink_lock_);
  if (!auto_try_lock.locked() || state_ != kPlaying) {
    // Provide silence if we failed to acquire the lock or the source is not
    // running.
    bus_wrapper_->Zero();
    return;
  }

  DCHECK(renderer_);
  DCHECK(client_);
  DCHECK_EQ(channels_, bus_wrapper_->channels());
  renderer_->Render(bus_wrapper_.get(), 0);
  bus_wrapper_->Scale(volume_);
}

void WebAudioSourceProviderImpl::Start() {
  base::AutoLock auto_lock(sink_lock_);
  DCHECK_EQ(state_, kStopped);
  state_ = kStarted;
  if (!client_)
    sink_->Start();
}

void WebAudioSourceProviderImpl::Stop() {
  base::AutoLock auto_lock(sink_lock_);
  state_ = kStopped;
  if (!client_)
    sink_->Stop();
}

void WebAudioSourceProviderImpl::Play() {
  base::AutoLock auto_lock(sink_lock_);
  DCHECK_EQ(state_, kStarted);
  state_ = kPlaying;
  if (!client_)
    sink_->Play();
}

void WebAudioSourceProviderImpl::Pause() {
  base::AutoLock auto_lock(sink_lock_);
  DCHECK(state_ == kPlaying || state_ == kStarted);
  state_ = kStarted;
  if (!client_)
    sink_->Pause();
}

bool WebAudioSourceProviderImpl::SetVolume(double volume) {
  base::AutoLock auto_lock(sink_lock_);
  volume_ = volume;
  if (!client_)
    sink_->SetVolume(volume);
  return true;
}

void WebAudioSourceProviderImpl::Initialize(
    const media::AudioParameters& params,
    RenderCallback* renderer) {
  base::AutoLock auto_lock(sink_lock_);
  CHECK(!renderer_);
  renderer_ = renderer;

  DCHECK_EQ(state_, kStopped);
  sink_->Initialize(params, renderer);

  // Keep track of the format in case the client hasn't yet been set.
  channels_ = params.channels();
  sample_rate_ = params.sample_rate();

  if (client_) {
    // Inform WebKit about the audio stream format.
    client_->setFormat(channels_, sample_rate_);
  }
}

}  // namespace webkit_media
