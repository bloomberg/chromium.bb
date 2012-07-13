// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_AUDIO_SCHEDULER_H_
#define REMOTING_HOST_AUDIO_SCHEDULER_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

namespace protocol {
class AudioStub;
}  // namespace protocol

class AudioCapturer;
class AudioPacket;

// A class for controlling AudioCapturer and forwarding audio packets to the
// client.
//
// THREADING
//
// This class works on two threads: the capture and network threads.
// Any encoding that is done on the audio samples will be done on the capture
// thread.
//
// AudioScheduler is responsible for:
// 1. managing the AudioCapturer.
// 2. sending packets of audio samples over the network to the client.
class AudioScheduler : public base::RefCountedThreadSafe<AudioScheduler> {
 public:
  // Construct an AudioScheduler. TaskRunners are used for message passing
  // among the capturer and network threads. The caller is responsible for
  // ensuring that the |audio_capturer| and |audio_stub| outlive the
  // AudioScheduler.
  AudioScheduler(
      scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      AudioCapturer* audio_capturer,
      protocol::AudioStub* audio_stub);

  // Stop the recording session.
  void Stop(const base::Closure& done_task);

  // Called when a client disconnects.
  void OnClientDisconnected();

 private:
  friend class base::RefCountedThreadSafe<AudioScheduler>;
  virtual ~AudioScheduler();

  void NotifyAudioPacketCaptured(scoped_ptr<AudioPacket> packet);

  void DoStart();

  // Sends an audio packet to the client.
  void DoSendAudioPacket(scoped_ptr<AudioPacket> packet);

  // Signal network thread to cease activities.
  void DoStopOnNetworkThread(const base::Closure& done_task);

  // Called when an AudioPacket has been delivered to the client.
  void OnCaptureCallbackNotified();

  scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  AudioCapturer* audio_capturer_;

  protocol::AudioStub* audio_stub_;

  bool network_stopped_;

  DISALLOW_COPY_AND_ASSIGN(AudioScheduler);
};

}  // namespace remoting

#endif  // REMOTING_HOST_AUDIO_SCHEDULER_H_
