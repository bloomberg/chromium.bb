// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_AUDIO_SCHEDULER_H_
#define REMOTING_HOST_AUDIO_SCHEDULER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

namespace protocol {
class AudioStub;
}  // namespace protocol

class AudioCapturer;
class AudioEncoder;
class AudioPacket;

// AudioScheduler is responsible for fetching audio data from the AudioCapturer
// and encoding it before passing it to the AudioStub for delivery to the
// client. Audio is captured and encoded on the audio thread and then passed to
// AudioStub on the network thread.
class AudioScheduler : public base::RefCountedThreadSafe<AudioScheduler> {
 public:
  // Audio capture and encoding tasks are dispatched via the
  // |audio_task_runner|. |audio_stub| tasks are dispatched via the
  // |network_task_runner|. The caller must ensure that the |audio_capturer| and
  // |audio_stub| exist until the scheduler is stopped using Stop() method.
  static scoped_refptr<AudioScheduler> Create(
      scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      scoped_ptr<AudioCapturer> audio_capturer,
      scoped_ptr<AudioEncoder> audio_encoder,
      protocol::AudioStub* audio_stub);

  // Stops the recording session.
  void Stop();

  // Pauses or resumes audio on a running session. This leaves the audio
  // capturer running, and only affects whether or not the captured audio is
  // encoded and sent on the wire.
  void Pause(bool pause);

 private:
  friend class base::RefCountedThreadSafe<AudioScheduler>;

  AudioScheduler(
      scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      scoped_ptr<AudioCapturer> audio_capturer,
      scoped_ptr<AudioEncoder> audio_encoder,
      protocol::AudioStub* audio_stub);
  virtual ~AudioScheduler();

  // Called on the audio thread to start capturing.
  void StartOnAudioThread();

  // Called on the audio thread to stop capturing.
  void StopOnAudioThread();

  // Called on the audio thread when a new audio packet is available.
  void EncodeAudioPacket(scoped_ptr<AudioPacket> packet);

  // Called on the network thread to send a captured packet to the audio stub.
  void SendAudioPacket(scoped_ptr<AudioPacket> packet);

  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  scoped_ptr<AudioCapturer> audio_capturer_;

  scoped_ptr<AudioEncoder> audio_encoder_;

  protocol::AudioStub* audio_stub_;

  bool network_stopped_;

  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(AudioScheduler);
};

}  // namespace remoting

#endif  // REMOTING_HOST_AUDIO_SCHEDULER_H_
