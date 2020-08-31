// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_audio_source_adapter.h"

#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/audio_source.h"

namespace remoting {
namespace protocol {

static const int kChannels = 2;
static const int kBytesPerSample = 2;

// Frame size expected by webrtc::AudioTrackSinkInterface.
static constexpr base::TimeDelta kAudioFrameDuration =
    base::TimeDelta::FromMilliseconds(10);

class WebrtcAudioSourceAdapter::Core {
 public:
  Core();
  ~Core();

  void Start(std::unique_ptr<AudioSource> audio_source);
  void Pause(bool pause);
  void AddSink(webrtc::AudioTrackSinkInterface* sink);
  void RemoveSink(webrtc::AudioTrackSinkInterface* sink);

 private:
  void OnAudioPacket(std::unique_ptr<AudioPacket> packet);

  std::unique_ptr<AudioSource> audio_source_;

  bool paused_ = false;

  int sampling_rate_ = 0;

  // webrtc::AudioTrackSinkInterface expects to get audio in 10ms frames (see
  // kAudioFrameDuration). AudioSource may generate AudioPackets for time
  // intervals that are not multiple of 10ms. In that case the left-over samples
  // are kept in |partial_frame_| until the next AudioPacket is captured by the
  // AudioSource.
  std::vector<uint8_t> partial_frame_;

  base::ObserverList<webrtc::AudioTrackSinkInterface>::Unchecked audio_sinks_;
  base::Lock audio_sinks_lock_;

  base::ThreadChecker thread_checker_;
};

WebrtcAudioSourceAdapter::Core::Core() {
  thread_checker_.DetachFromThread();
}

WebrtcAudioSourceAdapter::Core::~Core() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void WebrtcAudioSourceAdapter::Core::Start(
    std::unique_ptr<AudioSource> audio_source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  audio_source_ = std::move(audio_source);
  audio_source_->Start(
      base::Bind(&Core::OnAudioPacket, base::Unretained(this)));
}

void WebrtcAudioSourceAdapter::Core::Pause(bool pause) {
  DCHECK(thread_checker_.CalledOnValidThread());
  paused_ = pause;
}

void WebrtcAudioSourceAdapter::Core::AddSink(
    webrtc::AudioTrackSinkInterface* sink) {
  // Can be called on any thread.
  base::AutoLock lock(audio_sinks_lock_);
  audio_sinks_.AddObserver(sink);
}

void WebrtcAudioSourceAdapter::Core::RemoveSink(
    webrtc::AudioTrackSinkInterface* sink) {
  // Can be called on any thread.
  base::AutoLock lock(audio_sinks_lock_);
  audio_sinks_.RemoveObserver(sink);
}

void WebrtcAudioSourceAdapter::Core::OnAudioPacket(
    std::unique_ptr<AudioPacket> packet) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (paused_)
    return;

  DCHECK_EQ(packet->channels(), kChannels);
  DCHECK_EQ(packet->bytes_per_sample(), kBytesPerSample);

  if (sampling_rate_ != packet->sampling_rate()) {
    sampling_rate_ = packet->sampling_rate();
    partial_frame_.clear();
  }

  size_t samples_per_frame =
      kAudioFrameDuration * sampling_rate_ / base::TimeDelta::FromSeconds(1);
  size_t bytes_per_frame = kBytesPerSample * kChannels * samples_per_frame;

  const std::string& data = packet->data(0);

  size_t position = 0;

  base::AutoLock lock(audio_sinks_lock_);

  if (!partial_frame_.empty()) {
    size_t bytes_to_append =
        std::min(bytes_per_frame - partial_frame_.size(), data.size());
    position += bytes_to_append;
    partial_frame_.insert(partial_frame_.end(), data.data(),
                             data.data() + bytes_to_append);
    if (partial_frame_.size() < bytes_per_frame) {
      // Still don't have full frame.
      return;
    }

    // Here |partial_frame_| always contains a full frame.
    DCHECK_EQ(partial_frame_.size(), bytes_per_frame);

    for (auto& observer : audio_sinks_) {
      observer.OnData(&partial_frame_.front(), kBytesPerSample * 8,
                      sampling_rate_, kChannels, samples_per_frame);
    }
  }

  while (position + bytes_per_frame <= data.size()) {
    for (auto& observer : audio_sinks_) {
      observer.OnData(data.data() + position, kBytesPerSample * 8,
                      sampling_rate_, kChannels, samples_per_frame);
    }
    position += bytes_per_frame;
  }

  partial_frame_.assign(data.data() + position, data.data() + data.size());
}

WebrtcAudioSourceAdapter::WebrtcAudioSourceAdapter(
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner)
    : audio_task_runner_(std::move(audio_task_runner)), core_(new Core()) {}

WebrtcAudioSourceAdapter::~WebrtcAudioSourceAdapter() {
  audio_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

void WebrtcAudioSourceAdapter::Start(
    std::unique_ptr<AudioSource> audio_source) {
  audio_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::Start, base::Unretained(core_.get()),
                                std::move(audio_source)));
}

void WebrtcAudioSourceAdapter::Pause(bool pause) {
  audio_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::Pause, base::Unretained(core_.get()), pause));
}

WebrtcAudioSourceAdapter::SourceState WebrtcAudioSourceAdapter::state() const {
  return kLive;
}

bool WebrtcAudioSourceAdapter::remote() const {
  return false;
}

void WebrtcAudioSourceAdapter::RegisterAudioObserver(AudioObserver* observer) {}

void WebrtcAudioSourceAdapter::UnregisterAudioObserver(
    AudioObserver* observer) {}

void WebrtcAudioSourceAdapter::AddSink(webrtc::AudioTrackSinkInterface* sink) {
  core_->AddSink(sink);
}
void WebrtcAudioSourceAdapter::RemoveSink(
    webrtc::AudioTrackSinkInterface* sink) {
  core_->RemoveSink(sink);
}

void WebrtcAudioSourceAdapter::RegisterObserver(
    webrtc::ObserverInterface* observer) {}
void WebrtcAudioSourceAdapter::UnregisterObserver(
    webrtc::ObserverInterface* observer) {}

}  // namespace protocol
}  // namespace remoting
