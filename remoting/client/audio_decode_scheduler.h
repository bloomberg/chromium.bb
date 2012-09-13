// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_AUDIO_DECODE_SCHEDULER_H_
#define REMOTING_CLIENT_AUDIO_DECODE_SCHEDULER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/audio_stub.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

namespace protocol {
class SessionConfig;
}  // namespace protocol

class AudioDecoder;
class AudioPacket;
class AudioPlayer;

class AudioDecodeScheduler : public protocol::AudioStub {
 public:
  AudioDecodeScheduler(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> audio_decode_task_runner,
      scoped_ptr<AudioPlayer> audio_player);
  virtual ~AudioDecodeScheduler();

  // Initializes decoder with the information from the protocol config.
  void Initialize(const protocol::SessionConfig& config);

  // AudioStub implementation.
  virtual void ProcessAudioPacket(scoped_ptr<AudioPacket> packet,
                                  const base::Closure& done) OVERRIDE;

 private:
  class Core;

  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(AudioDecodeScheduler);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_AUDIO_DECODE_SCHEDULER_H_
