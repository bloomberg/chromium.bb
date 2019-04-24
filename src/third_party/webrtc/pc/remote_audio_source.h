/*
 *  Copyright 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_REMOTE_AUDIO_SOURCE_H_
#define PC_REMOTE_AUDIO_SOURCE_H_

#include <list>
#include <string>

#include "api/call/audio_sink.h"
#include "api/notifier.h"
#include "pc/channel.h"
#include "pc/playout_latency_interface.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/message_handler.h"

namespace rtc {
struct Message;
class Thread;
}  // namespace rtc

namespace webrtc {

// This class implements the audio source used by the remote audio track.
// This class works by configuring itself as a sink with the underlying media
// engine, then when receiving data will fan out to all added sinks.
class RemoteAudioSource : public Notifier<AudioSourceInterface>,
                          rtc::MessageHandler {
 public:
  explicit RemoteAudioSource(rtc::Thread* worker_thread);

  // Register and unregister remote audio source with the underlying media
  // engine.
  void Start(cricket::VoiceMediaChannel* media_channel, uint32_t ssrc);
  void Stop(cricket::VoiceMediaChannel* media_channel, uint32_t ssrc);

  // MediaSourceInterface implementation.
  MediaSourceInterface::SourceState state() const override;
  bool remote() const override;

  // AudioSourceInterface implementation.
  void SetVolume(double volume) override;
  void SetLatency(double latency) override;
  double GetLatency() const override;
  void RegisterAudioObserver(AudioObserver* observer) override;
  void UnregisterAudioObserver(AudioObserver* observer) override;

  void AddSink(AudioTrackSinkInterface* sink) override;
  void RemoveSink(AudioTrackSinkInterface* sink) override;

 protected:
  ~RemoteAudioSource() override;

 private:
  // These are callbacks from the media engine.
  class AudioDataProxy;
  void OnData(const AudioSinkInterface::Data& audio);
  void OnAudioChannelGone();

  void OnMessage(rtc::Message* msg) override;

  rtc::Thread* const main_thread_;
  rtc::Thread* const worker_thread_;
  std::list<AudioObserver*> audio_observers_;
  rtc::CriticalSection sink_lock_;
  std::list<AudioTrackSinkInterface*> sinks_;
  SourceState state_;
  // Allows to thread safely change playout latency. Handles caching cases if
  // |SetLatency| is called before start.
  rtc::scoped_refptr<PlayoutLatencyInterface> latency_;
};

}  // namespace webrtc

#endif  // PC_REMOTE_AUDIO_SOURCE_H_
