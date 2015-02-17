// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_AUDIO_PUMP_H_
#define REMOTING_HOST_AUDIO_PUMP_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"

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

// AudioPump is responsible for fetching audio data from the AudioCapturer
// and encoding it before passing it to the AudioStub for delivery to the
// client. Audio is captured and encoded on the audio thread and then passed to
// AudioStub on the network thread.
class AudioPump {
 public:
  // The caller must ensure that the |audio_stub| is not destroyed until the
  // pump is destroyed.
  AudioPump(scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
            scoped_ptr<AudioCapturer> audio_capturer,
            scoped_ptr<AudioEncoder> audio_encoder,
            protocol::AudioStub* audio_stub);
  virtual ~AudioPump();

  // Pauses or resumes audio on a running session. This leaves the audio
  // capturer running, and only affects whether or not the captured audio is
  // encoded and sent on the wire.
  void Pause(bool pause);

 private:
  class Core;

  // Called on the network thread to send a captured packet to the audio stub.
  void SendAudioPacket(scoped_ptr<AudioPacket> packet, int size);

  // Callback for BufferedSocketWriter.
  void OnPacketSent(int size);

  base::ThreadChecker thread_checker_;

  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;
  protocol::AudioStub* audio_stub_;

  scoped_ptr<Core> core_;

  base::WeakPtrFactory<AudioPump> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioPump);
};

}  // namespace remoting

#endif  // REMOTING_HOST_AUDIO_PUMP_H_
