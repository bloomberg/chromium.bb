// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_AUDIO_SOURCE_ADAPTER_H_
#define REMOTING_PROTOCOL_WEBRTC_AUDIO_SOURCE_ADAPTER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "third_party/webrtc/api/media_stream_interface.h"

namespace webrtc {
class AudioTrackSinkInterface;
}  // namespace webrtc

namespace remoting {

namespace protocol {

class AudioSource;

class WebrtcAudioSourceAdapter : public webrtc::AudioSourceInterface {
 public:
  explicit WebrtcAudioSourceAdapter(
      scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner);
  ~WebrtcAudioSourceAdapter() override;

  void Start(std::unique_ptr<AudioSource> audio_source);
  void Pause(bool pause);

  // webrtc::AudioSourceInterface implementation.
  SourceState state() const override;
  bool remote() const override;
  void RegisterAudioObserver(AudioObserver* observer) override;
  void UnregisterAudioObserver(AudioObserver* observer) override;
  void AddSink(webrtc::AudioTrackSinkInterface* sink) override;
  void RemoveSink(webrtc::AudioTrackSinkInterface* sink) override;

  // webrtc::NotifierInterface implementation.
  void RegisterObserver(webrtc::ObserverInterface* observer) override;
  void UnregisterObserver(webrtc::ObserverInterface* observer) override;

 private:
  class Core;

  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;

  // Core running on |audio_task_runner_|.
  std::unique_ptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(WebrtcAudioSourceAdapter);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_AUDIO_SOURCE_ADAPTER_H_
